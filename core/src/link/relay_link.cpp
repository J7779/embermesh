// EmberMesh core / link — "relay": MQTT 3.1.1 internet relay (see relay_link.h,
// docs/CONNECTIVITY.md §4, PROTOCOL.md §12).
#include "link/relay_link.h"

#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <set>

#include "framing/mqtt.h"
#include "link/drivers.h"
#include "proto/relay.h"

namespace ember {
namespace link {
namespace {

namespace rly = ember::proto::relay;

constexpr uint64_t kMinSaneUnixMs = 1704067200000ull;  // 2024-01-01: earlier clocks are "unset"
constexpr uint64_t kDayMs = 86400000ull;
constexpr uint64_t kOverlapMs = 3 * 3600000ull;        // subscribe E±1 only this close to midnight
constexpr size_t kMaxSessions = 512;
constexpr uint32_t kTopicRate = 50, kLinkRate = 200;   // messages per second
constexpr size_t kMaxSendersPerTopic = 64;
constexpr size_t kMaxNonces = 16384;                    // per relay link, all topics
constexpr uint64_t kNonceKeepMs = 30 * 60000ull;
constexpr uint64_t kReplaySlackMs = 600000;
constexpr uint64_t kConnackTimeoutMs = 20000, kSubackTimeoutMs = 30000;
constexpr size_t kTopicsPerSubscribe = 32;
constexpr size_t kMaxWsMessage = 8192;
constexpr size_t kMaxPacket = 4400;
constexpr uint32_t kBreakerThreshold = 20;              // bad seals + rate drops per minute and topic
constexpr uint64_t kBreakerMaxMs = 60 * 60000ull;

Schema relay_schema() {
  return {
      text("url", "Relay address", "", "The relay's WebSocket address (wss://…). The host connects it.", 256),
      num("keepalive_s", "Keep-alive", 240, 60, 1200,
          "About how often the relay connection is checked (randomised ±40 s). Shorter keeps carrier NAT open.", "s").adv(),
      text("username", "User name", "", "Only for a private relay that requires a login.", 128).adv(),
      text("password", "Password", "", "Only for a private relay that requires a login.", 128).adv(),
  };
}

bool is_topic_hex(const std::string& t) {
  if (t.size() != rly::kTopicLen) return false;
  for (char c : t)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

class RelayLink;

struct SenderState {
  uint64_t max_sent = 0;
  uint64_t last_ms = 0;
};

struct TopicState {
  std::map<uint32_t, SenderState> senders;  // by hop_src (u32be of the 4 bytes)
  uint64_t win_start = 0;
  uint32_t win_count = 0;
  uint64_t bad_start = 0;
  uint32_t bad_count = 0;
};

class RelayChild final : public Link {
 public:
  RelayChild(RelayLink* p, const RelaySession& s) : parent_(p), s_(s) {}
  const char* type() const override { return "relay_session"; }
  LinkCaps caps() const override;
  LinkState state() const override;
  bool send(ByteView frame, uint8_t priority) override;
  std::string status_json() const override;

  RelayLink* parent_;
  RelaySession s_;
  std::map<uint32_t, TopicState> topics_;  // epoch -> state
  uint64_t tx = 0, rx = 0;
};

struct TopicRef {
  RelayChild* child = nullptr;
  uint32_t epoch = 0;
};

class RelayLink final : public LinkBase, public RelayControl {
 public:
  explicit RelayLink(LinkHost& h) : LinkBase(h) {}

  bool configure(const Json& c, std::string* error) {
    url_ = c.get_string("url");
    host_name_ = rly::canonical_host(url_);
    if (host_name_.empty() && !url_.empty()) {
      if (error) *error = "url: a relay address wss://host[:port][/path] is required";
      return false;
    }
    keepalive_cfg_ = static_cast<uint16_t>(c.get_int("keepalive_s", 240));
    user_ = c.get_string("username");
    pass_ = c.get_string("password");
    return true;
  }

  // --- Link ---
  const char* type() const override { return "relay"; }
  LinkCaps caps() const override {
    LinkCaps k;
    k.mtu = 0;  // the parent carries nothing itself: its children (one per session) do
    k.bitrate_bps = 64000;
    k.broadcast = false;
    k.half_duplex = false;
    k.reliable = false;
    k.encryption_allowed = true;
    k.requires_station_id = false;
    k.cost = 30;
    return k;
  }
  bool send(ByteView, uint8_t) override { ++st_.tx_refused; return false; }

  void on_io_state(bool connected) override {
    connected_ = connected;
    reset_session();
    if (!connected) { set_state(LinkState::Down); return; }
    set_state(LinkState::Connecting);
    maybe_connect(now());
  }

  void on_io_bytes(ByteView d) override {
    if (d.empty()) return;
    st_.io_rx_bytes += d.size();
    if (phase_ == Phase::Idle) return;  // nothing asked for: ignore (never parse before CONNECT)
    if (d.size() > kMaxWsMessage) { protocol_error("relay message too large"); return; }
    last_rx_ = now();
    if (!dec_.feed(d)) { protocol_error(dec_.error()); return; }
    mqtt::Packet p;
    while (phase_ != Phase::Idle && dec_.next(&p)) handle(p);
    if (dec_.failed()) protocol_error(dec_.error());
  }

  void tick(uint64_t t) override {
    if (!connected_) return;
    maybe_connect(t);
    if (phase_ == Phase::WaitConnack && t >= sent_connect_ + kConnackTimeoutMs) {
      protocol_error("relay did not accept the connection in time");
      return;
    }
    if (phase_ != Phase::Up) return;
    for (const auto& kv : awaiting_)
      if (t >= kv.second + kSubackTimeoutMs) { protocol_error("relay did not answer a subscription"); return; }
    uint64_t ka = static_cast<uint64_t>(keepalive_s_) * 1000;
    if (ping_sent_ && t >= ping_sent_ + ka) { protocol_error("relay stopped answering"); return; }
    if (!ping_sent_ && t >= last_tx_ + ka * 9 / 10) { write(mqtt::pingreq()); ping_sent_ = t; }
    // Topic set: E always; E+1 in the last hours of the day, E-1 in the first; breaker pauses.
    if (window_key() != window_key_ || t >= next_pause_check_) refresh_topics();
  }

  std::string status_json() const override {
    Json j = base_status();
    j.set("url", url_);
    j.set("mqtt_state", phase_name());
    j.set("sessions", static_cast<uint64_t>(children_.size()));
    j.set("topics", static_cast<uint64_t>(subscribed_.size()));
    j.set("topics_paused", static_cast<uint64_t>(paused_.size()));
    j.set("topics_refused", static_cast<uint64_t>(refused_.size()));
    j.set("rx_messages", rs_.rx_messages).set("tx_messages", rs_.tx_messages);
    j.set("rx_bad_seal", rs_.rx_bad_seal).set("rx_replay", rs_.rx_replay).set("rx_wrong_hop", rs_.rx_wrong_hop);
    j.set("rx_own_echo", rs_.rx_own_echo).set("rx_out_of_epoch", rs_.rx_out_of_epoch);
    j.set("rx_dropped_rate", rs_.rx_dropped_rate).set("rx_unknown_topic", rs_.rx_unknown_topic);
    j.set("rx_bad_publish", rs_.rx_bad_publish).set("breaker_trips", rs_.breaker_trips);
    j.set("reconnects", rs_.reconnects);
    j.set("caps", caps_json(caps()));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(relay_schema(), cfg, &c, error)) return false;
    std::string old = host_name_;
    if (!configure(c, error)) return false;
    if (old != host_name_) { reset_session(); topic_map_.clear(); window_key_ = ~0ull; }
    return true;
  }

  // --- RelayControl ---
  void set_consent(bool on) override {
    if (on == consent_) return;
    consent_ = on;
    if (!on) {
      if (phase_ == Phase::Up || phase_ == Phase::WaitConnack) write(mqtt::disconnect());
      reset_session();
      set_state(connected_ ? LinkState::Connecting : LinkState::Down, "internet relay is off");
    }
  }
  void set_wall_clock(uint64_t ms) override { wall_ms_ = ms; }
  void set_own_hop(const uint8_t hop[4]) override { std::memcpy(own_hop_, hop, 4); }

  void set_sessions(const std::vector<RelaySession>& ss) override {
    std::map<std::string, const RelaySession*> want;
    for (const RelaySession& s : ss)
      if (want.size() < kMaxSessions && !s.key.empty()) want[s.key] = &s;
    for (size_t i = 0; i < children_.size();) {
      auto it = want.find(children_[i]->s_.key);
      bool same = it != want.end() && it->second->channel == children_[i]->s_.channel &&
                  it->second->root == children_[i]->s_.root &&
                  std::memcmp(it->second->peer_hop, children_[i]->s_.peer_hop, 4) == 0;
      if (!same) {
        drop_child_topics(children_[i].get());
        children_.erase(children_.begin() + static_cast<long>(i));
      } else {
        want.erase(it);
        ++i;
      }
    }
    for (const auto& kv : want) children_.push_back(std::make_unique<RelayChild>(this, *kv.second));
    window_key_ = ~0ull;  // force a recomputation
    refresh_topics();
  }

  size_t child_count() const override { return children_.size(); }
  Link* child(size_t i) override { return i < children_.size() ? children_[i].get() : nullptr; }
  const RelaySession& child_session(size_t i) const override { return children_[i]->s_; }
  const RelayStats& relay_stats() const override { return rs_; }

  // --- used by children ---
  bool up() const { return phase_ == Phase::Up; }
  LinkState child_state() const {
    if (phase_ == Phase::Up) return LinkState::Up;
    if (state_ == LinkState::Error) return LinkState::Error;
    return connected_ ? LinkState::Connecting : LinkState::Down;
  }

  bool publish_for(RelayChild& c, ByteView frame) {
    if (!up() || frame.empty() || frame.size() > rly::kMaxFrame || wall_ms_ < kMinSaneUnixMs) return false;
    uint32_t e = rly::epoch_of(wall_ms_);
    crypto::Key32 k = rly::frame_key(c.s_.root, e, host_name_);
    std::string t = rly::topic(c.s_.root, e, host_name_);
    uint8_t rnd[16];
    host_.random_bytes(rnd, sizeof rnd);
    Bytes m = rly::seal(k, host_name_, t, rnd, wall_ms_, frame);
    crypto::wipe(k.data(), k.size());
    if (m.empty()) return false;
    write(mqtt::publish(t, ByteView(m)));
    ++rs_.tx_messages;
    ++c.tx;
    st_.tx_frames++;
    st_.tx_bytes += frame.size();
    return true;
  }

 private:
  enum class Phase { Idle, WaitConnack, Up };

  const char* phase_name() const {
    if (!consent_) return "consent_required";
    if (!connected_) return "disconnected";
    if (wall_ms_ < kMinSaneUnixMs) return "waiting_for_time";
    return phase_ == Phase::Up ? "up" : "connecting";
  }

  void write(const Bytes& b) {
    if (b.empty()) return;
    io_write(ByteView(b));
    last_tx_ = now();
  }

  void reset_session() {
    phase_ = Phase::Idle;
    dec_.reset();
    subscribed_.clear();
    awaiting_.clear();
    pending_sub_.clear();
    ping_sent_ = 0;
  }

  void protocol_error(const char* why) {
    if (phase_ != Phase::Idle) write(mqtt::disconnect());
    reset_session();
    set_state(LinkState::Error, why);
    ++rs_.reconnects;
    retry_at_ = now() + 30000;
  }

  void maybe_connect(uint64_t t) {
    if (!connected_ || !consent_ || phase_ != Phase::Idle || wall_ms_ < kMinSaneUnixMs || t < retry_at_) return;
    if (host_name_.empty()) {  // created from defaults: nothing to connect to
      if (state_ != LinkState::Error) set_state(LinkState::Error, "no relay address configured");
      return;
    }
    // Empty client id with a clean session (MQTT 3.1.1, CONNECT payload, "Client Identifier"): nothing that names us.
    uint8_t r[1];
    host_.random_bytes(r, 1);
    int ka = keepalive_cfg_ - 40 + static_cast<int>(r[0] % 81);
    keepalive_s_ = static_cast<uint16_t>(ka < 60 ? 60 : ka > 1200 ? 1200 : ka);
    const std::string* u = user_.empty() ? nullptr : &user_;
    const std::string* pw = (user_.empty() || pass_.empty()) ? nullptr : &pass_;
    write(mqtt::connect("", keepalive_s_, true, u, pw));
    phase_ = Phase::WaitConnack;
    sent_connect_ = t;
    last_rx_ = t;
    if (state_ != LinkState::Connecting) set_state(LinkState::Connecting);
  }

  void handle(const mqtt::Packet& p) {
    switch (p.type) {
      case mqtt::Type::Connack:
        if (phase_ != Phase::WaitConnack) { protocol_error("unexpected CONNACK"); return; }
        if (p.return_code != 0) {
          protocol_error(p.return_code == 4 || p.return_code == 5 ? "relay refused the login" : "relay refused the connection");
          return;
        }
        phase_ = Phase::Up;
        set_state(LinkState::Up);
        last_error_.clear();
        window_key_ = ~0ull;
        refresh_topics();
        break;
      case mqtt::Type::Publish:
        if (phase_ != Phase::Up) { protocol_error("PUBLISH before CONNACK"); return; }
        if (p.qos != 0 || p.retain || p.dup) { ++rs_.rx_bad_publish; return; }  // M3: never retained/queued frames
        on_publish(p);
        break;
      case mqtt::Type::Suback: {
        auto it = awaiting_.find(p.packet_id);
        auto bt = batches_.find(p.packet_id);
        if (it == awaiting_.end() || bt == batches_.end()) { protocol_error("unexpected SUBACK"); return; }
        if (bt->second.size() != p.codes.size()) { protocol_error("SUBACK of the wrong length"); return; }
        for (size_t i = 0; i < p.codes.size(); ++i)
          if (p.codes[i] == 0x80) { refused_.insert(bt->second[i]); subscribed_.erase(bt->second[i]); }
        awaiting_.erase(it);
        batches_.erase(bt);
        break;
      }
      case mqtt::Type::Unsuback: {
        auto it = awaiting_.find(p.packet_id);
        if (it == awaiting_.end()) { protocol_error("unexpected UNSUBACK"); return; }
        awaiting_.erase(it);
        batches_.erase(p.packet_id);
        break;
      }
      case mqtt::Type::Pingresp:
        ping_sent_ = 0;
        break;
      default:
        protocol_error("unexpected MQTT packet from the relay");
        break;
    }
  }

  static uint32_t hop_of(ByteView f, bool* ok) {
    // Mesh frame (PROTOCOL §2): b0, [station id: u8 n + n bytes], hop_src[4].
    *ok = false;
    if (f.size() < 5) return 0;
    size_t off = 1;
    if (f[0] & 0x08) off = 2 + static_cast<size_t>(f[1]);
    if (f.size() < off + 4) return 0;
    *ok = true;
    return static_cast<uint32_t>(f[off]) << 24 | static_cast<uint32_t>(f[off + 1]) << 16 |
           static_cast<uint32_t>(f[off + 2]) << 8 | f[off + 3];
  }

  void breaker_hit(TopicState& ts, const std::string& topic, uint64_t t) {
    if (t - ts.bad_start >= 60000) { ts.bad_start = t; ts.bad_count = 0; }
    if (++ts.bad_count <= kBreakerThreshold) return;
    // Circuit breaker (M14): unsubscribe for 1, 2, 4 … 60 minutes.
    uint64_t& backoff = breaker_backoff_[topic];
    backoff = backoff ? std::min(backoff * 2, kBreakerMaxMs) : 60000;
    paused_[topic] = t + backoff;
    ++rs_.breaker_trips;
    ts.bad_count = 0;
    refresh_topics();
  }

  void on_publish(const mqtt::Packet& p) {
    ++rs_.rx_messages;
    uint64_t t = now();
    if (t - rate_start_ >= 1000) { rate_start_ = t; rate_count_ = 0; }
    if (++rate_count_ > kLinkRate) { ++rs_.rx_dropped_rate; return; }
    auto it = is_topic_hex(p.topic) && subscribed_.count(p.topic) ? topic_map_.find(p.topic) : topic_map_.end();
    if (it == topic_map_.end()) { ++rs_.rx_unknown_topic; return; }
    RelayChild& c = *it->second.child;
    uint32_t e = it->second.epoch;
    TopicState& ts = c.topics_[e];
    if (t - ts.win_start >= 1000) { ts.win_start = t; ts.win_count = 0; }
    if (++ts.win_count > kTopicRate) { ++rs_.rx_dropped_rate; breaker_hit(ts, p.topic, t); return; }
    crypto::Key32 k = rly::frame_key(c.s_.root, e, host_name_);
    rly::Opened o;
    bool ok = rly::open(k, host_name_, p.topic, ByteView(p.payload), &o);
    crypto::wipe(k.data(), k.size());
    if (!ok) { ++rs_.rx_bad_seal; ++st_.rx_errors; breaker_hit(ts, p.topic, t); return; }
    bool hok = false;
    uint32_t hop = hop_of(ByteView(o.frame), &hok);
    if (!hok) { ++rs_.rx_bad_seal; return; }
    uint32_t own = static_cast<uint32_t>(own_hop_[0]) << 24 | static_cast<uint32_t>(own_hop_[1]) << 16 |
                   static_cast<uint32_t>(own_hop_[2]) << 8 | own_hop_[3];
    if (hop == own) { ++rs_.rx_own_echo; return; }  // H2: before touching any replay state
    if (!c.s_.channel) {
      uint32_t peer = static_cast<uint32_t>(c.s_.peer_hop[0]) << 24 | static_cast<uint32_t>(c.s_.peer_hop[1]) << 16 |
                      static_cast<uint32_t>(c.s_.peer_hop[2]) << 8 | c.s_.peer_hop[3];
      if (hop != peer) { ++rs_.rx_wrong_hop; return; }
    }
    // Epoch bound: [start(e) - 1 day, start(e) + 2 days).
    uint64_t start = static_cast<uint64_t>(e) * kDayMs;
    if (o.sent_ms + kDayMs < start || o.sent_ms >= start + 2 * kDayMs) { ++rs_.rx_out_of_epoch; return; }
    // Replay: per (topic, sender) highest time, plus a capped nonce memory.
    std::string nk(reinterpret_cast<const char*>(o.nonce), 16);
    expire_nonces(t);
    if (nonce_set_.count(nk)) { ++rs_.rx_replay; return; }
    auto sit = ts.senders.find(hop);
    if (sit != ts.senders.end() && sit->second.max_sent > kReplaySlackMs && o.sent_ms < sit->second.max_sent - kReplaySlackMs) {
      ++rs_.rx_replay;
      return;
    }
    if (sit == ts.senders.end()) {
      if (ts.senders.size() >= kMaxSendersPerTopic) {
        auto victim = ts.senders.begin();
        for (auto i = ts.senders.begin(); i != ts.senders.end(); ++i)
          if (i->second.last_ms < victim->second.last_ms) victim = i;
        ts.senders.erase(victim);
      }
      sit = ts.senders.emplace(hop, SenderState()).first;
    }
    if (o.sent_ms > sit->second.max_sent) sit->second.max_sent = o.sent_ms;
    sit->second.last_ms = t;
    nonce_set_.insert(nk);
    nonces_.emplace_back(nk, t);
    ++c.rx;
    st_.rx_frames++;
    st_.rx_bytes += o.frame.size();
    st_.last_rx_ms = t;
    host_.link_frame_received(c, ByteView(o.frame), RxMeta());
  }

  void expire_nonces(uint64_t t) {
    while (!nonces_.empty() && (nonces_.size() > kMaxNonces || t - nonces_.front().second > kNonceKeepMs)) {
      nonce_set_.erase(nonces_.front().first);
      nonces_.pop_front();
    }
  }

  uint64_t window_key() const {
    if (wall_ms_ < kMinSaneUnixMs) return 0;
    uint64_t in_day = wall_ms_ % kDayMs;
    uint64_t part = in_day < kOverlapMs ? 1 : in_day >= kDayMs - kOverlapMs ? 2 : 0;
    return (wall_ms_ / kDayMs) * 4 + part + 1;
  }

  void drop_child_topics(RelayChild* c) {
    std::vector<std::string> gone;
    for (const auto& kv : topic_map_)
      if (kv.second.child == c) gone.push_back(kv.first);
    for (const std::string& t : gone) {
      topic_map_.erase(t);
      if (subscribed_.erase(t) && phase_ == Phase::Up) pending_unsub_.push_back(t);
    }
  }

  // Recomputes the wanted topic set (window, sessions, paused topics) and (un)subscribes the difference.
  void refresh_topics() {
    uint64_t t = now();
    window_key_ = window_key();
    next_pause_check_ = t + 15000;
    for (auto it = paused_.begin(); it != paused_.end();)
      it = t >= it->second ? paused_.erase(it) : std::next(it);
    std::map<std::string, TopicRef> want;
    if (wall_ms_ >= kMinSaneUnixMs) {
      uint32_t E = rly::epoch_of(wall_ms_);
      uint64_t in_day = wall_ms_ % kDayMs;
      std::vector<uint32_t> epochs = {E};
      if (in_day >= kDayMs - kOverlapMs) epochs.push_back(E + 1);
      if (in_day < kOverlapMs) epochs.push_back(E - 1);
      for (auto& c : children_) {
        for (uint32_t e : epochs) {
          std::string tp = rly::topic(c->s_.root, e, host_name_);
          if (!paused_.count(tp)) want[tp] = TopicRef{c.get(), e};
        }
        for (auto it = c->topics_.begin(); it != c->topics_.end();)
          it = (it->first + 1 < E || it->first > E + 1) ? c->topics_.erase(it) : std::next(it);
      }
    }
    for (const auto& kv : topic_map_)
      if (!want.count(kv.first) && subscribed_.erase(kv.first) && phase_ == Phase::Up) pending_unsub_.push_back(kv.first);
    topic_map_ = std::move(want);
    if (phase_ != Phase::Up) return;
    for (const auto& kv : topic_map_)
      if (!subscribed_.count(kv.first) && !refused_.count(kv.first)) pending_sub_.push_back(kv.first);
    flush_subscriptions();
  }

  void flush_subscriptions() {
    std::vector<std::string> batch;
    auto send_batch = [&](bool sub) {
      if (batch.empty()) return;
      uint16_t id = next_pid();
      write(sub ? mqtt::subscribe(id, batch) : mqtt::unsubscribe(id, batch));
      awaiting_[id] = now();
      if (sub) batches_[id] = batch;
      batch.clear();
    };
    for (const std::string& tp : pending_unsub_) {
      batch.push_back(tp);
      if (batch.size() >= kTopicsPerSubscribe) send_batch(false);
    }
    send_batch(false);
    pending_unsub_.clear();
    for (const std::string& tp : pending_sub_) {
      if (subscribed_.count(tp)) continue;
      subscribed_.insert(tp);
      batch.push_back(tp);
      if (batch.size() >= kTopicsPerSubscribe) send_batch(true);
    }
    send_batch(true);
    pending_sub_.clear();
  }

  uint16_t next_pid() {
    do { pid_ = static_cast<uint16_t>(pid_ % 65535 + 1); } while (awaiting_.count(pid_));
    return pid_;
  }

  std::string url_, host_name_, user_, pass_;
  uint16_t keepalive_cfg_ = 240, keepalive_s_ = 240;
  bool connected_ = false, consent_ = false;
  uint64_t wall_ms_ = 0;
  uint8_t own_hop_[4] = {};
  Phase phase_ = Phase::Idle;
  mqtt::Decoder dec_{kMaxPacket};
  uint64_t last_rx_ = 0, last_tx_ = 0, sent_connect_ = 0, retry_at_ = 0, ping_sent_ = 0;
  uint64_t window_key_ = ~0ull, next_pause_check_ = 0;
  std::vector<std::unique_ptr<RelayChild>> children_;
  std::map<std::string, TopicRef> topic_map_;
  std::set<std::string> subscribed_, refused_;
  std::map<std::string, uint64_t> paused_, breaker_backoff_;
  std::vector<std::string> pending_sub_, pending_unsub_;
  std::map<uint16_t, uint64_t> awaiting_;
  std::map<uint16_t, std::vector<std::string>> batches_;
  std::deque<std::pair<std::string, uint64_t>> nonces_;
  std::set<std::string> nonce_set_;
  uint16_t pid_ = 0;
  uint64_t rate_start_ = 0;
  uint32_t rate_count_ = 0;
  RelayStats rs_;
};

LinkCaps RelayChild::caps() const {
  LinkCaps k;
  k.mtu = 4000;
  k.bitrate_bps = 64000;     // slow enough that the engine never beacons it like a LAN link (M4)
  k.broadcast = s_.channel;  // a channel topic reaches every member; a pair topic one contact
  k.half_duplex = false;
  k.reliable = false;
  k.encryption_allowed = true;
  k.requires_station_id = false;
  k.cost = 30;
  return k;
}
LinkState RelayChild::state() const { return parent_->child_state(); }
bool RelayChild::send(ByteView frame, uint8_t) { return parent_->publish_for(*this, frame); }
std::string RelayChild::status_json() const {
  Json j = Json::object();
  j.set("type", "relay_session").set("key", s_.key).set("tx_messages", tx).set("rx_messages", rx);
  return j.dump();
}

std::unique_ptr<Link> create_relay(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<RelayLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

RelayControl* as_relay(Link* l) {
  if (!l || std::strcmp(l->type(), "relay") != 0) return nullptr;
  return static_cast<RelayLink*>(l);
}

const LinkTypeInfo& relay_type() {
  static const LinkTypeInfo t = {
      "relay",
      "Internet relay (MQTT)",
      "Reaches contacts at any distance through a relay server over the internet, when they cannot be "
      "reached otherwise. Messages are sealed end to end; the relay sees random-looking topics, sizes and "
      "times, and your IP address. Needs the internet relay to be switched on in Settings.",
      {"websocket"},
      relay_schema,
      create_relay,
  };
  return t;
}

}  // namespace link
}  // namespace ember
