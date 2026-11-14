// EmberMesh node — contacts, channels, conversations, messages, files, interop text, and the
// engine callbacks that feed them.
#include <algorithm>
#include <cstring>

#include "base/codec.h"
#include "base/uri.h"
#include "node/node.h"
#include "node/node_util.h"
#include "proto/secure.h"

namespace ember::node {

namespace {
constexpr size_t kMaxChannelMembers = 256;
constexpr size_t kMaxPendingRead = 64;
constexpr uint64_t kPeerEventIntervalMs = 5000;

Bytes conv_prefix(std::string_view conv) {
  Bytes p(conv.begin(), conv.end());
  p.push_back(0);
  return p;
}

std::string lower_ascii(std::string s) {
  for (char& c : s)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return s;
}

std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r')) ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' || s[b - 1] == '\r')) --b;
  return s.substr(a, b - a);
}

RpcResult send_error(const mesh::SendResult& r) {
  if (r.error == "too_large") return rpc_err("too_large", "too large for the mesh");
  if (r.error == "unknown_channel") return rpc_err("not_found", "not a member of that channel");
  if (r.error == "busy") return rpc_err("busy", "the mesh is busy, try again");
  if (r.error == "bad_params") return rpc_err("bad_params", "invalid message");
  if (r.error == "not_started") return rpc_err("busy", "the node clock is not set yet");
  return rpc_err("internal", "send failed: " + r.error);
}
}  // namespace

// --- conversation ids ----------------------------------------------------------------------------

bool Node::parse_conv(std::string_view conv, std::string* kind, std::string* rest) {
  if (conv.size() < 4 || conv.size() > 64 || conv[2] != ':') return false;
  std::string_view k = conv.substr(0, 2), r = conv.substr(3);
  if (k == "dm") {
    if (!valid_node_hex(r)) return false;
  } else if (k == "ch") {
    uint32_t x;
    if (r != "public" && !parse_hex32(r, &x)) return false;
    for (char c : r)
      if (c >= 'A' && c <= 'F') return false;
  } else if (k == "io") {
    if (r.empty() || r.size() > 32) return false;
    for (char c : r)
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
  } else {
    return false;
  }
  *kind = std::string(k);
  *rest = std::string(r);
  return true;
}

RpcResult Node::check_conv(const std::string& conv, bool for_send) {
  std::string kind, rest;
  if (!parse_conv(conv, &kind, &rest))
    return rpc_err("bad_params", "conv must be dm:<node_id>, ch:<channel id> or io:<network>");
  if (kind == "dm" && id_.valid() && rest == node_id_hex(id_.node_id()))
    return rpc_err("bad_params", "that is your own node id");
  if (kind == "ch" && !channels_.count(rest)) return rpc_err("not_found", "no such channel");
  (void)for_send;
  return rpc_ok();
}

Bytes Node::msg_key(std::string_view conv, uint64_t ts, uint64_t seq, const Bytes& id) {
  // conv \0 be64(ts) be64(seq) id12 — seq (the store's monotonic op sequence) keeps messages with
  // equal timestamps in arrival order; the id stays the last 12 bytes.
  Bytes k = conv_prefix(conv);
  for (int i = 7; i >= 0; --i) k.push_back(static_cast<uint8_t>(ts >> (8 * i)));
  for (int i = 7; i >= 0; --i) k.push_back(static_cast<uint8_t>(seq >> (8 * i)));
  k.insert(k.end(), id.begin(), id.end());
  return k;
}

std::string Node::conv_for_rx(const mesh::RxInfo& rx) {
  switch (rx.dest.type) {
    case mesh::Dest::Node: return "dm:" + node_id_hex(rx.from);
    case mesh::Dest::Channel: {
      std::string id = hex32(rx.dest.channel);
      return channels_.count(id) ? "ch:" + id : std::string();
    }
    default: return "ch:public";
  }
}

bool Node::is_blocked(const NodeId& id) const {
  auto it = contacts_.find(id);
  return it != contacts_.end() && it->second.get_bool("blocked");
}

std::string Node::peer_name(const NodeId& id) const {
  auto it = contacts_.find(id);
  if (it != contacts_.end() && !it->second.get_string("name").empty()) return it->second.get_string("name");
  if (eng_) {
    std::optional<mesh::PeerInfo> p = eng_->peer(id);
    if (p && !p->name.empty()) return p->name;
  }
  return "";
}

std::string Node::conv_title(const std::string& conv) const {
  std::string r = conv.substr(3);
  if (conv[0] == 'c') {
    auto it = channels_.find(r);
    return it != channels_.end() ? it->second.rec.get_string("name") : r;
  }
  if (conv[0] == 'i') {
    if (r == "aprs") return "APRS";
    if (r == "meshtastic") return "Meshtastic";
    if (r == "rtty" || r == "psk31" || r == "cw") {
      std::string u = r;
      for (char& c : u)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
      return u;
    }
    return r;
  }
  NodeId id;
  if (parse_node_id(r, id)) {
    std::string n = peer_name(id);
    if (!n.empty()) return n;
  }
  return "Node " + r.substr(0, 4) + "\xE2\x80\xA6" + r.substr(12);
}

Json& Node::conv_meta(const std::string& conv) {
  auto it = conv_meta_.find(conv);
  if (it != conv_meta_.end()) return it->second;
  Json& m = conv_meta_[conv];
  m = Json::object({{"unread", 0}, {"last_ts", 0}, {"muted", false}, {"v", 2}});
  return m;
}

// Table pressure never deletes history the user cares about (security review M3): only a
// conversation created by this version (v2) that the user never wrote in, with a sender who is not
// a contact, can be forgotten to make room, unverified-only ones first. Otherwise the new
// conversation is refused.
bool Node::conv_admit(const std::string& conv) {
  if (conv_meta_.count(conv) || conv_meta_.size() < cfg_.max_conversations) return true;
  std::string victim;
  int64_t oldest = INT64_MAX;
  bool victim_auth = true;
  for (auto& kv : conv_meta_) {
    const std::string& k = kv.first;
    const Json& m = kv.second;
    if (k.empty() || k[0] == 'c' || m.get_int("v") != 2 || m.get_bool("out")) continue;
    if (k.size() == 19 && k[0] == 'd') {
      NodeId id;
      if (parse_node_id(k.substr(3), id) && contacts_.count(id)) continue;
    }
    bool auth = m.get_bool("auth");
    int64_t t = m.get_int("last_ts");
    if (victim.empty() || (victim_auth && !auth) || (auth == victim_auth && t < oldest)) {
      victim = k;
      oldest = t;
      victim_auth = auth;
    }
  }
  if (victim.empty()) return false;
  delete_conversation_messages(victim);
  conv_meta_.erase(victim);
  db_del("conv", B(victim));
  return true;
}

bool Node::replace_unverified(const Json& msg) {
  if (!kv_ || !msg.get_bool("signed_ok")) return false;
  Bytes id;
  if (!id12_from_hex(msg.get_string("id"), &id)) return false;
  Bytes key;
  if (kv_->get("mid", id, &key) != store::Status::Ok) return false;
  Bytes old;
  if (kv_->get("msg", key, &old) != store::Status::Ok) return false;
  std::optional<Json> o = parse_json_bytes(old);
  if (!o || !o->is_object() || o->get_bool("signed_ok") || o->get_bool("outgoing") || o->get_string("conv") != msg.get_string("conv"))
    return false;
  Json m = msg;
  m.set("ts", o->get_int("ts"));  // the record key orders by ts: keep it
  std::string v = m.dump();
  if (!db_put("msg", key, ByteView(std::string_view(v)))) return false;
  emit("message.updated", Json::object({{"message", m}}));
  emit_conversation(m.get_string("conv"));
  return true;
}

void Node::save_conv_meta(const std::string& conv) {
  auto it = conv_meta_.find(conv);
  if (it != conv_meta_.end()) db_put_json("conv", B(conv), it->second);
}

Json Node::conversation_json(const std::string& conv) {
  Json j = Json::object();
  const char* kind = conv[0] == 'd' ? "dm" : conv[0] == 'c' ? "channel" : "interop";
  auto mit = conv_meta_.find(conv);
  Json meta = mit != conv_meta_.end() ? mit->second : Json::object();
  bool muted = meta.get_bool("muted");
  bool encrypted = conv[0] == 'd';
  if (conv[0] == 'c') {
    auto it = channels_.find(conv.substr(3));
    if (it != channels_.end()) {
      muted = it->second.rec.get_bool("muted");
      encrypted = it->second.rec.get_bool("encrypted");
    }
  }
  j.set("id", conv).set("kind", kind).set("title", conv_title(conv)).set("unread", meta.get_int("unread"));
  j.set("muted", muted).set("encrypted", encrypted);
  int64_t last_ts = meta.get_int("last_ts");
  if (kv_) {
    store::ScanSpec s;
    s.set_prefix(conv_prefix(conv));
    s.reverse = true;
    s.limit = 1;
    kv_->scan("msg", s, [&](ByteView, ByteView v, uint64_t) {
      std::optional<Json> m = Json::parse(std::string_view(reinterpret_cast<const char*>(v.data()), v.size()));
      if (m && m->is_object()) {
        last_ts = std::max<int64_t>(last_ts, m->get_int("ts"));
        j.set("last_message", std::move(*m));
      }
      return false;
    });
  }
  j.set("last_ts", last_ts);
  return j;
}

void Node::emit_conversation(const std::string& conv) {
  emit("conversation.updated", Json::object({{"conversation", conversation_json(conv)}}));
}

// --- message storage -----------------------------------------------------------------------------

std::string Node::local_message_id() {
  for (int i = 0; i < 16; ++i) {
    proto::MsgId m;
    m.origin = id_.node_id();
    m.pkt_id = rng_.u32() | 0x80000000u;
    if (!kv_ || !kv_->contains("mid", id12(m))) return m.to_hex();
  }
  proto::MsgId m;
  m.origin = id_.node_id();
  m.pkt_id = rng_.u32();
  return m.to_hex();
}

Json Node::new_message(const std::string& conv, const std::string& id_hex, bool outgoing, const std::string& kind,
                       const std::string& text) {
  uint64_t now = mesh_now();
  Json m = Json::object();
  m.set("id", id_hex).set("conv", conv);
  m.set("from", outgoing ? node_id_hex(id_.node_id()) : std::string());
  m.set("from_name", outgoing ? profile_.get_string("name") : std::string());
  m.set("outgoing", outgoing).set("ts", now).set("received_ts", now);
  m.set("text", text).set("kind", kind).set("status", outgoing ? "queued" : "received");
  m.set("priority", "normal").set("encrypted", false).set("signed_ok", true).set("hops", 0).set("via_link", 0);
  return m;
}

bool Node::store_message(const Json& msg, bool count_unread, bool emit_events) {
  if (!kv_) return false;
  Bytes id;
  if (!id12_from_hex(msg.get_string("id"), &id)) return false;
  if (kv_->contains("mid", id)) return replace_unverified(msg) && false;  // duplicate (dedupe by message id)
  std::string conv = msg.get_string("conv");
  if (!conv_admit(conv)) return false;
  uint64_t ts = static_cast<uint64_t>(std::max<int64_t>(0, msg.get_int("ts")));
  Bytes key = msg_key(conv, ts, kv_->last_sequence() + 1, id);
  store::WriteBatch b;
  std::string v = msg.dump();
  b.put("msg", key, ByteView(std::string_view(v)));
  b.put("mid", id, key);
  int64_t exp = msg.get_int("expires_ts");
  if (exp > 0) {
    ByteWriter ek;
    ek.u64be(static_cast<uint64_t>(exp)).bytes(id);
    b.put("mexp", ek.data(), ByteView());
  }
  if (!db_write(b)) return false;
  bool fresh = !conv_meta_.count(conv);
  Json& meta = conv_meta(conv);
  meta.set("last_ts", std::max<int64_t>(meta.get_int("last_ts"), static_cast<int64_t>(ts)));
  if (count_unread) meta.set("unread", meta.get_int("unread") + 1);
  if (msg.get_bool("outgoing") && !meta.get_bool("out")) { meta.set("out", true); fresh = true; }
  if (!msg.get_bool("outgoing") && msg.get_bool("signed_ok") && !meta.get_bool("auth")) { meta.set("auth", true); fresh = true; }
  if (fresh || count_unread) save_conv_meta(conv);  // last_ts alone is recomputed from the history
  if (emit_events) {
    emit("message.new", Json::object({{"message", msg}}));
    emit_conversation(conv);
    if (count_unread && conv[0] == 'c') emit_channel(conv.substr(3));
  }
  evict_messages();
  return true;
}

bool Node::load_message(const std::string& id_hex, Json* msg, Bytes* key) {
  Bytes id, k;
  if (!id12_from_hex(id_hex, &id) || !db_get("mid", id, &k)) return false;
  Json m;
  if (!db_get_json("msg", k, &m) || !m.is_object()) return false;
  *msg = std::move(m);
  if (key) *key = std::move(k);
  return true;
}

bool Node::save_message(const Bytes& key, const Json& msg) {
  std::string v = msg.dump();
  return db_put("msg", key, ByteView(std::string_view(v)));
}

void Node::set_message_status(const std::string& id_hex, const char* status, bool force) {
  Json m;
  Bytes key;
  if (!load_message(id_hex, &m, &key) || !m.get_bool("outgoing")) return;
  std::string cur = m.get_string("status");
  if (cur == status) return;
  if (!force) {
    int rc = status_rank(cur), rn = status_rank(status);
    if (std::strcmp(status, "failed") == 0) {
      if (rc >= 3) return;  // already delivered / read
    } else if (rn < 0) {
      return;
    } else if (cur != "failed" && rn <= rc) {
      return;  // never regress (e.g. "sent" after "delivered")
    }
  }
  m.set("status", status);
  if (!save_message(key, m)) return;
  emit("message.status", Json::object({{"id", m.get_string("id")}, {"conv", m.get_string("conv")}, {"status", status}}));
}

void Node::delete_conversation_messages(const std::string& conv) {
  if (!kv_) return;
  Bytes prefix = conv_prefix(conv);
  std::vector<Bytes> ids;
  store::ScanSpec s;
  s.set_prefix(prefix);
  s.keys_only = true;
  kv_->scan("msg", s, [&](ByteView k, ByteView, uint64_t) {
    if (k.size() >= prefix.size() + 20) ids.push_back(k.sub(k.size() - 12).to_bytes());
    return true;
  });
  store::WriteBatch b;
  for (auto& id : ids) {
    b.del("mid", id);
    if (kv_->contains("files", id)) b.del("files", id);
  }
  Bytes upper = prefix;
  upper.back() = 1;  // conv "\x01" sorts right after every "conv\0..." key
  b.delete_range("msg", prefix, upper);
  db_write(b);
}

void Node::evict_messages() {
  if (!kv_) return;
  store::NamespaceStats st0 = kv_->namespace_stats("msg");
  if (st0.records <= cfg_.max_messages && st0.bytes() <= cfg_.max_message_bytes) return;
  // Counts are computed once per call (security review M3: no per-conversation rescans per round).
  // Value order: channel / interop chatter first, then DMs with strangers the user never answered,
  // then everything else; the biggest first within a class.
  std::vector<std::pair<std::string, uint64_t>> convs;
  auto add = [&](const std::string& conv) {
    store::ScanSpec s;
    s.set_prefix(conv_prefix(conv));
    uint64_t n = kv_->count("msg", s);
    if (n) convs.push_back({conv, n});
  };
  for (auto& kv : conv_meta_) add(kv.first);
  for (auto& kv : channels_)
    if (!conv_meta_.count("ch:" + kv.first)) add("ch:" + kv.first);
  auto cls = [&](const std::string& conv) {
    if (conv[0] != 'd') return 0;
    auto it = conv_meta_.find(conv);
    bool out = it != conv_meta_.end() && it->second.get_bool("out");
    NodeId id;
    bool contact = conv.size() == 19 && parse_node_id(conv.substr(3), id) && contacts_.count(id);
    return out || contact ? 2 : 1;
  };
  for (int round = 0; round < 32 && !convs.empty(); ++round) {
    store::NamespaceStats st = kv_->namespace_stats("msg");
    if (st.records <= cfg_.max_messages && st.bytes() <= cfg_.max_message_bytes) return;
    size_t vi = 0;
    for (size_t i = 1; i < convs.size(); ++i) {
      int a = cls(convs[i].first), b = cls(convs[vi].first);
      if (a < b || (a == b && convs[i].second > convs[vi].second)) vi = i;
    }
    std::string victim = convs[vi].first;
    uint64_t best = convs[vi].second;
    size_t drop = std::max<size_t>(1, static_cast<size_t>(best / 10));
    store::ScanSpec s;
    s.set_prefix(conv_prefix(victim));
    s.keys_only = true;
    s.limit = drop;
    std::vector<Bytes> keys;
    kv_->scan("msg", s, [&](ByteView k, ByteView, uint64_t) {
      keys.push_back(k.to_bytes());
      return true;
    });
    store::WriteBatch b;
    for (auto& k : keys) {
      b.del("msg", k);
      if (k.size() >= 12) {
        Bytes id(k.end() - 12, k.end());
        b.del("mid", id);
        if (kv_->contains("files", id)) b.del("files", id);
      }
    }
    if (!db_write(b)) return;
    convs[vi].second -= std::min<uint64_t>(best, keys.size());
    if (convs[vi].second == 0 || keys.empty()) convs.erase(convs.begin() + static_cast<std::ptrdiff_t>(vi));
  }
}

void Node::add_system_message(const std::string& conv, const std::string& text) {
  Json m = new_message(conv, local_message_id(), false, "system", text);
  m.set("from", "");
  store_message(m, false, true);
}

// --- contacts ------------------------------------------------------------------------------------

Json Node::contact_json(const Json& rec) const {
  Json c = Json::object();
  std::string hex = rec.get_string("node_id");
  NodeId id{};
  parse_node_id(hex, id);
  std::optional<mesh::PeerInfo> peer;
  if (eng_) peer = eng_->peer(id);
  std::string name = rec.get_string("name");
  if (name.empty() && peer) name = peer->name;
  std::string cs = rec.get_string("callsign");
  if (cs.empty() && peer) cs = peer->callsign;
  c.set("node_id", hex).set("name", name).set("callsign", cs);
  c.set("verified", rec.get_bool("verified")).set("blocked", rec.get_bool("blocked")).set("favorite", rec.get_bool("favorite"));
  c.set("status", peer ? peer->status : std::string());
  c.set("last_seen_ts", peer ? peer->last_seen_ms : 0);
  if (peer && peer->hops != 0xFF)
    c.set("hops", static_cast<int>(peer->hops));
  else
    c.set("hops", Json());
  c.set("direct", peer ? peer->direct : false);
  const mesh::PeerLink* best = nullptr;
  if (peer)
    for (const auto& l : peer->links)
      if (!best || l.last_rx_ms > best->last_rx_ms) best = &l;
  c.set("via_link", best ? best->link_id : 0);
  // reach (RPC_API §11): the kind of link the contact was last heard on directly within 15 minutes,
  // preferring any non-relay path; never a delivery claim.
  {
    uint64_t now = eng_ ? eng_->now_ms() : 0;
    const mesh::PeerLink* direct = nullptr;
    const mesh::PeerLink* relay = nullptr;
    if (peer)
      for (const auto& l : peer->links) {
        if (now > l.last_rx_ms + 15 * 60000ull) continue;
        bool is_relay = relay_children_.count(l.link_id) > 0;
        const mesh::PeerLink*& slot = is_relay ? relay : direct;
        if (!slot || l.last_rx_ms > slot->last_rx_ms) slot = &l;
      }
    Json reach = Json::object();
    const mesh::PeerLink* use = direct ? direct : relay;
    std::string path = "none";
    if (use) {
      if (!direct) path = "relay";
      else {
        auto it = links_.find(use->link_id);
        const std::string t = it == links_.end() ? std::string() : it->second.type;
        if (it != links_.end() && it->second.auto_kind == "nearby") path = "nearby";
        else if (t == "kiss" || t == "audio" || t == "meshtastic" || t == "rnode" || t == "lora_at" || t == "rtl_tcp") path = "radio";
        else path = "network";
      }
      reach.set("link_id", use->link_id);
    }
    reach.set("path", path);
    if (peer && peer->last_seen_ms && now >= peer->last_seen_ms)
      reach.set("heard_s", static_cast<int64_t>((now - peer->last_seen_ms) / 1000));
    c.set("reach", std::move(reach));
  }
  if (best && best->has_rssi) c.set("rssi", best->rssi_dbm);
  if (best && best->has_snr) c.set("snr", best->snr_db);
  if (peer && peer->position) c.set("position", position_to_json(*peer->position, peer->position_ms));
  PublicKeys keys;
  bool have = contact_keys(rec, &keys);
  if (!have && peer && peer->has_keys) {
    keys = peer->keys;
    have = true;
  }
  Json fp = Json::array();
  if (have)
    for (auto& w : fingerprint_words(keys)) fp.push(w);
  c.set("fingerprint", std::move(fp));
  return c;
}

void Node::emit_contact(const NodeId& id) {
  auto it = contacts_.find(id);
  if (it == contacts_.end()) return;
  emit("contact.updated", Json::object({{"contact", contact_json(it->second)}}));
}

RpcResult Node::m_contacts_list(Params&) {
  Json a = Json::array();
  for (auto& kv : contacts_) a.push(contact_json(kv.second));
  return rpc_ok(Json::object({{"contacts", std::move(a)}}));
}

RpcResult Node::m_contacts_add(Params& p) {
  std::string card, hex, name;
  if (!p.str("card", &card, EmberUri::kMaxLength, false) || !p.str("node_id", &hex, 16, false) ||
      !p.str("name", &name, 64, false))
    return p.fail();
  if (!utf8_valid(name)) return rpc_err("bad_params", "name must be valid UTF-8");
  name = trim(name);
  NodeId id{};
  std::optional<ContactCard> cc;
  if (!card.empty()) {
    ContactCard c;
    std::string err;
    if (!ContactCard::parse(card, c, &err)) return rpc_err("bad_params", "not a valid ember: contact card: " + err);
    id = c.node_id();
    cc = c;
  } else {
    if (hex.empty()) return rpc_err("bad_params", "card or node_id is required");
    if (!parse_node_id(hex, id)) return rpc_err("bad_params", "node_id must be 16 hex characters");
  }
  if (id_.valid() && id == id_.node_id()) return rpc_err("bad_params", "that is your own card");
  auto it = contacts_.find(id);
  Json rec = it != contacts_.end() ? it->second : Json::object({{"node_id", node_id_hex(id)},
                                                                {"name", ""},
                                                                {"callsign", ""},
                                                                {"verified", false},
                                                                {"blocked", false},
                                                                {"favorite", false},
                                                                {"added_ts", mesh_now()}});
  if (cc) {
    PublicKeys old;
    if (contact_keys(rec, &old) && old != cc->keys) return rpc_err("bad_params", "card keys do not match the known contact");
    rec.set("keys", keys_b64(cc->keys));
    if (!cc->name.empty() && name.empty()) name = utf8_sanitize(cc->name);
    if (!cc->callsign.empty()) rec.set("callsign", cc->callsign);
    if (eng_ && !eng_->add_known_keys(cc->keys, name, cc->callsign))
      return rpc_err("bad_params", "card keys conflict with keys already known for this node");
  }
  if (!name.empty()) rec.set("name", name);
  if (rec.get_string("name").empty()) {
    std::string pn = peer_name(id);
    rec.set("name", pn);
  }
  contacts_[id] = rec;
  if (!db_put_json("contacts", nid_view(id), rec)) return rpc_err("io_error", "could not save contact");
  emit_contact(id);
  return rpc_ok(Json::object({{"contact", contact_json(rec)}}));
}

RpcResult Node::m_contacts_update(Params& p) {
  std::string hex, name;
  bool verified = false, blocked = false, favorite = false, hv = false, hb = false, hf = false;
  if (!p.str("node_id", &hex, 16, true) || !p.str("name", &name, 64, false) || !p.boolean("verified", &verified, &hv) ||
      !p.boolean("blocked", &blocked, &hb) || !p.boolean("favorite", &favorite, &hf))
    return p.fail();
  NodeId id;
  if (!parse_node_id(hex, id)) return rpc_err("bad_params", "node_id must be 16 hex characters");
  auto it = contacts_.find(id);
  if (it == contacts_.end()) return rpc_err("not_found", "no such contact");
  if (!utf8_valid(name)) return rpc_err("bad_params", "name must be valid UTF-8");
  name = trim(name);
  if (!name.empty()) it->second.set("name", name);
  if (hv) it->second.set("verified", verified);
  if (hb) it->second.set("blocked", blocked);
  if (hf) it->second.set("favorite", favorite);
  if (!db_put_json("contacts", nid_view(id), it->second)) return rpc_err("io_error", "could not save contact");
  emit_contact(id);
  if (!name.empty()) emit_conversation("dm:" + node_id_hex(id));
  return rpc_ok(Json::object({{"contact", contact_json(it->second)}}));
}

RpcResult Node::m_contacts_remove(Params& p) {
  std::string hex;
  if (!p.str("node_id", &hex, 16, true)) return p.fail();
  NodeId id;
  if (!parse_node_id(hex, id)) return rpc_err("bad_params", "node_id must be 16 hex characters");
  if (!contacts_.erase(id)) return rpc_err("not_found", "no such contact");
  db_del("contacts", nid_view(id));
  return rpc_ok();
}

// --- channels ------------------------------------------------------------------------------------

void Node::ensure_public_channel() {
  ChannelRec& c = channels_["public"];
  Json stored;
  bool muted = false;
  if (db_get_json("channels", B("public"), &stored)) muted = stored.get_bool("muted");
  c.rec = Json::object({{"id", "public"},
                        {"name", "Public"},
                        {"norm", "public"},
                        {"encrypted", false},
                        {"is_default", true},
                        {"muted", muted},
                        {"members", stored.get("members").is_array() ? stored.get("members") : Json::array()}});
  c.key = proto::ChannelKey();
}

Json Node::channel_json(const ChannelRec& c) {
  std::string id = c.rec.get_string("id");
  auto mit = conv_meta_.find("ch:" + id);
  Json j = Json::object();
  j.set("id", id).set("name", c.rec.get_string("name")).set("encrypted", c.rec.get_bool("encrypted"));
  j.set("is_default", c.rec.get_bool("is_default")).set("muted", c.rec.get_bool("muted"));
  j.set("unread", mit != conv_meta_.end() ? mit->second.get_int("unread") : 0);
  j.set("members_seen", static_cast<int64_t>(c.rec.get("members").size()) + 1);
  if (c.key.encrypted) {
    j.set("relay", c.rec.get_bool("relay"));
    j.set("relay_allowed", c.rec.get_bool("relay_allowed"));
  } else {
    j.set("relay", false).set("relay_allowed", false);
  }
  return j;
}

// Internet relay opt-in for a channel (CONNECTIVITY §4.8): only random-key or strong-passphrase channels.
void Node::mark_channel_relay_allowed(const std::string& id, bool allowed) {
  auto it = channels_.find(id);
  if (it == channels_.end() || !it->second.key.encrypted) return;
  if (allowed && !it->second.rec.get_bool("relay_allowed")) {
    it->second.rec.set("relay_allowed", true);
    db_put_json("channels", B(id), it->second.rec);
  }
}

std::string passphrase_strength(const std::string& pass) {
  bool lo = false, up = false, dg = false, ot = false;
  size_t words = 1;
  for (char c : pass) {
    lo |= c >= 'a' && c <= 'z';
    up |= c >= 'A' && c <= 'Z';
    dg |= c >= '0' && c <= '9';
    ot |= !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
    words += c == ' ';
  }
  int classes = lo + up + dg + ot;
  return (pass.size() >= 20 || (words >= 4 && pass.size() >= 16)) ? "strong" : (pass.size() >= 12 && classes >= 2) ? "fair" : "weak";
}

void Node::emit_channel(const std::string& id) {
  auto it = channels_.find(id);
  if (it != channels_.end()) emit("channel.updated", Json::object({{"channel", channel_json(it->second)}}));
}

bool Node::add_channel(const proto::ChannelKey& key, const std::string& display, Json* out) {
  std::string id = hex32(key.id);
  auto it = channels_.find(id);
  if (it != channels_.end()) {
    // Same 32-bit id but a different key (or open vs encrypted): a collision, not this channel
    // (security review L2). Never silently hand back the other channel.
    if (it->second.key.encrypted != key.encrypted || !ct_equal(ByteView(it->second.key.key.data(), 32), ByteView(key.key.data(), 32))) {
      *out = Json();
      return false;
    }
    *out = channel_json(it->second);
    return true;
  }
  ChannelRec c;
  c.key = key;
  c.rec = Json::object({{"id", id},
                        {"name", display},
                        {"norm", key.name},
                        {"encrypted", key.encrypted},
                        {"is_default", false},
                        {"muted", false},
                        {"members", Json::array()},
                        {"created_ts", mesh_now()}});
  if (key.encrypted) c.rec.set("key", base64_encode(ByteView(key.key.data(), key.key.size())));
  if (!db_put_json("channels", B(id), c.rec)) { *out = Json::object(); return false; }
  channels_[id] = c;
  if (eng_) eng_->join_channel(key);
  conv_meta("ch:" + id);
  save_conv_meta("ch:" + id);
  *out = channel_json(channels_[id]);
  emit("channel.updated", Json::object({{"channel", *out}}));
  emit_conversation("ch:" + id);
  return true;
}

RpcResult Node::m_channels_list(Params&) {
  Json a = Json::array();
  a.push(channel_json(channels_["public"]));
  for (auto& kv : channels_)
    if (kv.first != "public") a.push(channel_json(kv.second));
  return rpc_ok(Json::object({{"channels", std::move(a)}}));
}

namespace {
bool channel_name_ok(const std::string& name, std::string* display, std::string* err) {
  std::string t = trim(name);
  if (t.empty()) {
    *err = "name is required";
    return false;
  }
  if (!utf8_valid(t) || t.size() > 32) {
    *err = "name must be valid UTF-8, at most 32 bytes";
    return false;
  }
  if (proto::normalize_channel_name(t).empty()) {
    *err = "name must contain visible characters";
    return false;
  }
  *display = t;
  return true;
}
}  // namespace

RpcResult Node::m_channels_create(Params& p) {
  std::string name, pass, display, err;
  bool random_key = false;
  if (!p.str("name", &name, 64, true) || !p.str("passphrase", &pass, 256, false) || !p.boolean("random_key", &random_key))
    return p.fail();
  if (!channel_name_ok(name, &display, &err)) return rpc_err("bad_params", err);
  if (pass.empty() && !random_key && proto::normalize_channel_name(display) == "public")
    return rpc_err("bad_params", "the Public channel already exists");
  // Every packet of a passphrase channel lets an eavesdropper test guesses offline (review L2):
  // report how guessable the passphrase is, and offer random keys shared only by invite.
  std::string strength;
  if (!pass.empty()) strength = passphrase_strength(pass);
  proto::ChannelKey key;
  if (random_key) {
    crypto::Key32 k;
    rng_.fill(k);
    key = proto::channel_from_key(display, k);
    crypto::wipe(k);
  } else {
    key = pass.empty() ? proto::open_channel(display) : proto::derive_channel_key(display, pass);
  }
  crypto::wipe(pass.data(), pass.size());
  Json ch;
  if (!add_channel(key, display, &ch))
    return ch.is_null() ? rpc_err("busy", "a different channel already uses this channel id; choose another name or passphrase")
                        : rpc_err("io_error", "could not save channel");
  if (random_key) {
    channels_[hex32(key.id)].rec.set("random_key", true);
    db_put_json("channels", B(hex32(key.id)), channels_[hex32(key.id)].rec);
  }
  if (random_key || strength == "strong") {
    mark_channel_relay_allowed(hex32(key.id), true);
    ch = channel_json(channels_[hex32(key.id)]);
  }
  Json res = Json::object({{"channel", ch}});
  if (!strength.empty()) res.set("passphrase_strength", strength);
  return rpc_ok(res);
}

RpcResult Node::m_channels_invite(Params& p) {
  std::string id;
  if (!p.str("id", &id, 16, true)) return p.fail();
  auto it = channels_.find(id);
  if (it == channels_.end()) return rpc_err("not_found", "no such channel");
  EmberUri u;
  u.kind = "invite";
  u.add("v", "1").add("ch", id).add("n", it->second.rec.get_string("name"));
  if (it->second.key.encrypted)
    u.add("k", base64url_encode(ByteView(it->second.key.key.data(), it->second.key.key.size())));
  if (it->second.key.encrypted && it->second.rec.get_bool("random_key")) u.add("r", "1");  // relay-eligible key
  return rpc_ok(Json::object({{"invite", u.to_string()}}));
}

RpcResult Node::m_channels_join(Params& p) {
  std::string invite, name, pass;
  if (!p.str("invite", &invite, EmberUri::kMaxLength, false) || !p.str("name", &name, 64, false) ||
      !p.str("passphrase", &pass, 256, false))
    return p.fail();
  if (!invite.empty()) {
    EmberUri u;
    std::string err;
    if (!EmberUri::parse(invite, u, &err) || u.kind != "invite") return rpc_err("bad_params", "not a valid channel invite");
    std::string ch = u.get("ch"), display;
    if (ch == "public") return rpc_ok(Json::object({{"channel", channel_json(channels_["public"])}}));
    if (!channel_name_ok(u.get("n"), &display, &err)) return rpc_err("bad_params", "invite: " + err);
    proto::ChannelKey key;
    if (u.find("k")) {
      Bytes k;
      if (!base64url_decode(u.get("k"), k) || k.size() != 32) return rpc_err("bad_params", "invite key is corrupt");
      crypto::Key32 kk;
      std::copy(k.begin(), k.end(), kk.begin());
      key = proto::channel_from_key(display, kk);
      crypto::wipe(k);
    } else {
      key = proto::open_channel(display);
    }
    if (!ch.empty() && ch != hex32(key.id)) return rpc_err("bad_params", "invite is corrupt (channel id mismatch)");
    Json out;
    if (!add_channel(key, display, &out)) return rpc_err(out.is_null() ? "busy" : "io_error", out.is_null() ? "a different channel already uses this channel id" : "could not save channel");
    if (key.encrypted && u.get("r") == "1") {
      auto cit = channels_.find(hex32(key.id));
      if (cit != channels_.end()) { cit->second.rec.set("random_key", true); db_put_json("channels", B(hex32(key.id)), cit->second.rec); }
      mark_channel_relay_allowed(hex32(key.id), true);
      out = channel_json(channels_[hex32(key.id)]);
    }
    return rpc_ok(Json::object({{"channel", out}}));
  }
  std::string display, err;
  if (name.empty()) return rpc_err("bad_params", "invite or name is required");
  if (!channel_name_ok(name, &display, &err)) return rpc_err("bad_params", err);
  if (pass.empty() && proto::normalize_channel_name(display) == "public")
    return rpc_ok(Json::object({{"channel", channel_json(channels_["public"])}}));
  proto::ChannelKey key = pass.empty() ? proto::open_channel(display) : proto::derive_channel_key(display, pass);
  bool strong = !pass.empty() && passphrase_strength(pass) == "strong";
  crypto::wipe(pass.data(), pass.size());
  Json out;
  if (!add_channel(key, display, &out)) return rpc_err(out.is_null() ? "busy" : "io_error", out.is_null() ? "a different channel already uses this channel id" : "could not save channel");
  if (strong) {
    mark_channel_relay_allowed(hex32(key.id), true);
    out = channel_json(channels_[hex32(key.id)]);
  }
  return rpc_ok(Json::object({{"channel", out}}));
}

RpcResult Node::m_channels_update(Params& p) {
  std::string id;
  bool muted = false, hm = false, relay = false, hr = false;
  if (!p.str("id", &id, 16, true) || !p.boolean("muted", &muted, &hm) || !p.boolean("relay", &relay, &hr)) return p.fail();
  auto it = channels_.find(id);
  if (it == channels_.end()) return rpc_err("not_found", "no such channel");
  if (hr && relay && !(it->second.key.encrypted && it->second.rec.get_bool("relay_allowed")))
    return rpc_err("denied", "only channels with a random key or a strong passphrase can go over the internet relay");
  if (hm) it->second.rec.set("muted", muted);
  if (hr) it->second.rec.set("relay", relay);
  if (hm || hr) {
    db_put_json("channels", B(id), it->second.rec);
    if (hr) connectivity_tick(true);
  }
  Json out = channel_json(it->second);
  emit("channel.updated", Json::object({{"channel", out}}));
  emit_conversation("ch:" + id);
  return rpc_ok(Json::object({{"channel", out}}));
}

RpcResult Node::m_channels_leave(Params& p) {
  std::string id;
  if (!p.str("id", &id, 16, true)) return p.fail();
  auto it = channels_.find(id);
  if (it == channels_.end()) return rpc_err("not_found", "no such channel");
  if (it->second.rec.get_bool("is_default")) return rpc_err("denied", "the Public channel cannot be left");
  if (eng_) eng_->leave_channel(it->second.key.id);
  channels_.erase(it);
  db_del("channels", B(id));
  delete_conversation_messages("ch:" + id);
  conv_meta_.erase("ch:" + id);
  db_del("conv", B("ch:" + id));
  return rpc_ok();
}

// --- conversations / messages --------------------------------------------------------------------

RpcResult Node::m_conversations_list(Params&) {
  std::vector<Json> list;
  for (auto& kv : channels_) list.push_back(conversation_json("ch:" + kv.first));
  for (auto& kv : conv_meta_)
    if (kv.first[0] != 'c') list.push_back(conversation_json(kv.first));
  std::stable_sort(list.begin(), list.end(),
                   [](const Json& a, const Json& b) { return a.get_int("last_ts") > b.get_int("last_ts"); });
  Json a = Json::array();
  for (auto& j : list) a.push(std::move(j));
  return rpc_ok(Json::object({{"conversations", std::move(a)}}));
}

RpcResult Node::m_conversations_clear(Params& p) {
  std::string conv;
  if (!p.str("conv", &conv, 64, true)) return p.fail();
  RpcResult c = check_conv(conv, false);
  if (!c.ok) return c;
  delete_conversation_messages(conv);
  if (conv[0] == 'c') {
    Json& m = conv_meta(conv);
    m.set("unread", 0).set("last_ts", 0);
    save_conv_meta(conv);
    emit_channel(conv.substr(3));
  } else {
    conv_meta_.erase(conv);
    db_del("conv", B(conv));
  }
  emit_conversation(conv);
  return rpc_ok();
}

RpcResult Node::m_messages_list(Params& p) {
  std::string conv;
  int64_t before = 0, limit = 50;
  bool has_before = false;
  if (!p.str("conv", &conv, 64, true) || !p.integer("before_ts", &before, 0, INT64_MAX, false, &has_before) ||
      !p.integer("limit", &limit, 1, 1000, false))
    return p.fail();
  RpcResult c = check_conv(conv, false);
  if (!c.ok) return c;
  std::vector<Json> page;
  bool more = false;
  if (kv_) {
    store::ScanSpec s;
    Bytes prefix = conv_prefix(conv);
    s.set_prefix(prefix);
    if (has_before) {
      Bytes up = prefix;
      for (int i = 7; i >= 0; --i) up.push_back(static_cast<uint8_t>(static_cast<uint64_t>(before) >> (8 * i)));
      s.set_upper(up);
    }
    s.reverse = true;
    int64_t edge_ts = -1;
    kv_->scan("msg", s, [&](ByteView, ByteView v, uint64_t) {
      std::optional<Json> m = Json::parse(std::string_view(reinterpret_cast<const char*>(v.data()), v.size()));
      if (!m || !m->is_object()) return true;
      int64_t ts = m->get_int("ts");
      if (static_cast<int64_t>(page.size()) >= limit && ts != edge_ts) {
        more = true;
        return false;
      }
      // Messages sharing the oldest timestamp of the page are never split across pages.
      edge_ts = ts;
      page.push_back(std::move(*m));
      return true;
    });
  }
  Json a = Json::array();
  for (auto it = page.rbegin(); it != page.rend(); ++it) a.push(std::move(*it));
  return rpc_ok(Json::object({{"messages", std::move(a)}, {"more", more}}));
}

RpcResult Node::m_messages_send(Params& p) {
  std::string conv, text, prio_s = "normal";
  bool want_ack = true;
  int64_t ttl = 0, expires = 0;
  if (!p.str("conv", &conv, 64, true) || !p.str("text", &text, proto::kMaxText, true) ||
      !p.str("priority", &prio_s, 16, false) || !p.boolean("want_ack", &want_ack) ||
      !p.integer("ttl_hops", &ttl, 1, proto::kMaxTtl, false) ||
      !p.integer("expires_s", &expires, 1, 30 * 86400, false))
    return p.fail();
  if (!utf8_valid(text)) return rpc_err("bad_params", "text must be valid UTF-8");
  if (trim(text).empty()) return rpc_err("bad_params", "text is required");
  proto::Prio prio;
  if (!parse_prio(prio_s, &prio)) return rpc_err("bad_params", "priority must be low, normal, high or emergency");
  RpcResult c = check_conv(conv, true);
  if (!c.ok) return c;
  if (conv[0] == 'i') {
    // Reply into an interop conversation: send as plain text on the link it was last heard on.
    auto mit = conv_meta_.find(conv);
    Json meta = mit != conv_meta_.end() ? mit->second : Json::object();
    uint32_t link_id = static_cast<uint32_t>(meta.get_int("via_link"));
    LinkRec* r = link_by_id(link_id);
    if (!r) {
      for (auto& kv : links_)
        if (kv.second.last_interop_network == conv.substr(3)) r = &kv.second;
    }
    if (!r) return rpc_err("not_found", "no link carries this network; use interop.send with a link_id");
    Json q = Json::object({{"link_id", r->id}, {"text", text}, {"to", meta.get_string("reply_to")}});
    Params ip(q);
    return m_interop_send(ip);
  }
  mesh::Dest dest;
  bool encrypted = false;
  if (conv[0] == 'd') {
    NodeId id;
    parse_node_id(conv.substr(3), id);
    dest = mesh::Dest::to_node(id);
    encrypted = true;
  } else if (conv == "ch:public") {
    dest = mesh::Dest::broadcast();
  } else {
    ChannelRec& ch = channels_[conv.substr(3)];
    dest = mesh::Dest::to_channel(ch.key.id);
    encrypted = ch.key.encrypted;
  }
  mesh::SendOptions o;
  o.prio = prio;
  o.want_ack = want_ack;
  o.ttl_hops = static_cast<uint8_t>(ttl);
  o.lifetime_s = static_cast<uint32_t>(expires);
  mesh::SendResult r = eng_->send_text(dest, text, o);
  if (!r.ok) return send_error(r);
  Json m = new_message(conv, r.id.to_hex(), true, "text", text);
  m.set("priority", prio_name(prio)).set("encrypted", encrypted);
  if (expires) m.set("expires_ts", m.get_int("ts") + expires * 1000);
  mesh::MsgStatus st = eng_->status(r.id);
  if (st != mesh::MsgStatus::Unknown) m.set("status", mesh::msg_status_name(st));
  if (!store_message(m, false, true)) return rpc_err("io_error", "message could not be stored");
  return rpc_ok(Json::object({{"message", m}}));
}

RpcResult Node::m_messages_retry(Params& p) {
  std::string id;
  if (!p.str("id", &id, 24, true)) return p.fail();
  Json m;
  Bytes key;
  if (!load_message(id, &m, &key)) return rpc_err("not_found", "no such message");
  if (!m.get_bool("outgoing")) return rpc_err("bad_params", "only outgoing messages can be retried");
  std::string st = m.get_string("status");
  if (st == "delivered" || st == "read") return rpc_ok(Json::object({{"message", m}}));
  proto::MsgId mid;
  proto::MsgId::parse(id, mid);
  if (eng_->retry(mid)) {
    set_message_status(id, "queued", true);
  } else if (m.get_string("kind") == "text" && m.get_string("conv")[0] != 'i') {
    // The engine no longer tracks it (expired / evicted / broadcast): send a fresh copy and alias
    // the new packet id to this message so status updates land here.
    std::string conv = m.get_string("conv");
    mesh::Dest dest;
    if (conv[0] == 'd') {
      NodeId nid;
      parse_node_id(conv.substr(3), nid);
      dest = mesh::Dest::to_node(nid);
    } else if (conv == "ch:public") {
      dest = mesh::Dest::broadcast();
    } else {
      auto it = channels_.find(conv.substr(3));
      if (it == channels_.end()) return rpc_err("not_found", "channel was left");
      dest = mesh::Dest::to_channel(it->second.key.id);
    }
    mesh::SendOptions o;
    parse_prio(m.get_string("priority"), &o.prio);
    mesh::SendResult r = eng_->send_text(dest, m.get_string("text"), o);
    if (!r.ok) return send_error(r);
    db_put("mid", id12(r.id), key);
    set_message_status(id, "queued", true);
  } else {
    return rpc_err("unsupported", "this message can no longer be retried");
  }
  load_message(id, &m, nullptr);
  return rpc_ok(Json::object({{"message", m}}));
}

RpcResult Node::m_messages_mark_read(Params& p) {
  std::string conv;
  if (!p.str("conv", &conv, 64, true)) return p.fail();
  RpcResult c = check_conv(conv, false);
  if (!c.ok) return c;
  auto it = conv_meta_.find(conv);
  if (it == conv_meta_.end()) return rpc_ok();
  Json& meta = it->second;
  bool changed = meta.get_int("unread") != 0;
  meta.set("unread", 0);
  const Json& pend = meta.get("pending_read");
  if (conv[0] == 'd' && pend.is_array() && !pend.empty()) {
    if (eng_ && settings_.get_bool("read_receipts", true)) {
      NodeId peer;
      parse_node_id(conv.substr(3), peer);
      std::vector<uint32_t> ids;
      for (const Json& x : pend.items()) ids.push_back(static_cast<uint32_t>(x.as_int()));
      eng_->send_read_receipt(peer, ids);
    }
    meta.set("pending_read", Json::array());
    changed = true;
  }
  if (changed) {
    save_conv_meta(conv);
    emit_conversation(conv);
    if (conv[0] == 'c') emit_channel(conv.substr(3));
  }
  return rpc_ok();
}

RpcResult Node::m_messages_delete(Params& p) {
  std::string id;
  if (!p.str("id", &id, 24, true)) return p.fail();
  Json m;
  Bytes key, bid;
  if (!load_message(id, &m, &key)) return rpc_err("not_found", "no such message");
  id12_from_hex(id, &bid);
  store::WriteBatch b;
  b.del("msg", key);
  b.del("mid", bid);
  if (kv_ && kv_->contains("files", bid)) b.del("files", bid);
  if (!db_write(b)) return rpc_err("io_error", "could not delete");
  std::string st = m.get_string("status");
  if (eng_ && m.get_bool("outgoing") && (st == "queued" || st == "sent" || st == "relayed")) {
    proto::MsgId mid;
    if (proto::MsgId::parse(id, mid)) eng_->cancel(mid);
  }
  emit_conversation(m.get_string("conv"));
  return rpc_ok();
}

RpcResult Node::m_messages_search(Params& p) {
  std::string q;
  int64_t limit = 50;
  if (!p.str("query", &q, 256, true) || !p.integer("limit", &limit, 1, 500, false)) return p.fail();
  q = lower_ascii(trim(q));
  Json a = Json::array();
  if (q.empty() || !kv_) return rpc_ok(Json::object({{"messages", std::move(a)}}));
  std::vector<std::pair<int64_t, Json>> hits;
  kv_->scan("msg", store::ScanSpec(), [&](ByteView, ByteView v, uint64_t) {
    std::optional<Json> m = Json::parse(std::string_view(reinterpret_cast<const char*>(v.data()), v.size()));
    if (!m || !m->is_object()) return true;
    if (lower_ascii(m->get_string("text")).find(q) == std::string::npos &&
        lower_ascii(m->get_string("from_name")).find(q) == std::string::npos)
      return true;
    hits.emplace_back(m->get_int("ts"), std::move(*m));
    if (hits.size() > static_cast<size_t>(limit) * 4 + 256) {  // keep memory bounded
      std::sort(hits.begin(), hits.end(), [](auto& x, auto& y) { return x.first > y.first; });
      hits.resize(static_cast<size_t>(limit));
    }
    return true;
  });
  std::sort(hits.begin(), hits.end(), [](auto& x, auto& y) { return x.first > y.first; });
  for (size_t i = 0; i < hits.size() && i < static_cast<size_t>(limit); ++i) a.push(std::move(hits[i].second));
  return rpc_ok(Json::object({{"messages", std::move(a)}}));
}

// --- files ---------------------------------------------------------------------------------------

RpcResult Node::m_files_send(Params& p) {
  size_t max_file = eng_->config().max_file_bytes;
  std::string conv, name, mime = "application/octet-stream", b64;
  if (!p.str("conv", &conv, 64, true) || !p.str("name", &name, proto::kMaxShortField, true, false) ||
      !p.str("mime", &mime, proto::kMaxShortField, false) || !p.str("data_b64", &b64, (max_file + 2) / 3 * 4 + 8, true))
    return p.fail();
  if (!utf8_valid(name) || !utf8_valid(mime)) return rpc_err("bad_params", "name/mime must be valid UTF-8");
  if (mime.empty()) mime = "application/octet-stream";
  RpcResult c = check_conv(conv, true);
  if (!c.ok) return c;
  if (conv[0] == 'i') return rpc_err("unsupported", "files cannot be sent to interop stations");
  Bytes data;
  if (!base64_decode_lenient(b64, data)) return rpc_err("bad_params", "data_b64 is not valid base64");
  if (data.empty()) return rpc_err("bad_params", "file is empty");
  if (data.size() > max_file)
    return rpc_err("too_large", "files over " + std::to_string(max_file / 1024) + " KiB cannot be sent over the mesh");
  mesh::Dest dest;
  bool encrypted = false;
  if (conv[0] == 'd') {
    NodeId id;
    parse_node_id(conv.substr(3), id);
    dest = mesh::Dest::to_node(id);
    encrypted = true;
  } else if (conv == "ch:public") {
    dest = mesh::Dest::broadcast();
  } else {
    ChannelRec& ch = channels_[conv.substr(3)];
    dest = mesh::Dest::to_channel(ch.key.id);
    encrypted = ch.key.encrypted;
  }
  mesh::SendResult r = eng_->send_file(dest, name, mime, data);
  if (!r.ok) return send_error(r);
  Json m = new_message(conv, r.id.to_hex(), true, "file", name);
  m.set("encrypted", encrypted);
  m.set("file", Json::object({{"name", name}, {"mime", mime}, {"size", data.size()}, {"complete", true}}));
  mesh::MsgStatus st = eng_->status(r.id);
  if (st != mesh::MsgStatus::Unknown) m.set("status", mesh::msg_status_name(st));
  Bytes id = id12(r.id);
  db_put("files", id, data);
  ByteWriter fk;
  fk.u64be(mesh_now()).bytes(id);
  db_put("fage", fk.data(), ByteView());
  store_message(m, false, true);
  // Evict the oldest stored files beyond the quota.
  for (int i = 0; i < 64 && kv_ && kv_->namespace_stats("files").bytes() > cfg_.max_file_bytes_stored; ++i) {
    std::vector<Bytes> old;
    store::ScanSpec s;
    s.keys_only = true;
    s.limit = 1;
    kv_->scan("fage", s, [&](ByteView k, ByteView, uint64_t) {
      old.push_back(k.to_bytes());
      return false;
    });
    if (old.empty()) break;
    store::WriteBatch b;
    b.del("fage", old[0]);
    if (old[0].size() == 20) b.del("files", ByteView(old[0].data() + 8, 12));
    db_write(b);
  }
  return rpc_ok(Json::object({{"message", m}}));
}

RpcResult Node::m_files_get(Params& p) {
  std::string id;
  if (!p.str("id", &id, 24, true)) return p.fail();
  Bytes bid, data;
  if (!id12_from_hex(id, &bid)) return rpc_err("bad_params", "id must be a 24-character message id");
  if (!db_get("files", bid, &data)) return rpc_err("not_found", "file not available");
  Json m;
  std::string name = "file", mime = "application/octet-stream";
  if (load_message(id, &m, nullptr)) {
    name = m.get("file").get_string("name", name);
    mime = m.get("file").get_string("mime", mime);
  }
  return rpc_ok(Json::object({{"name", name}, {"mime", mime}, {"data_b64", base64_encode(data)}}));
}

// --- interop -------------------------------------------------------------------------------------

namespace {
std::string default_network(const std::string& type) {
  if (type == "kiss") return "aprs";
  if (type == "meshtastic") return "meshtastic";
  if (type == "audio") return "rtty";
  return type;
}
std::string clean_network(const std::string& n) {
  std::string out;
  for (char c : n) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') out.push_back(c);
    if (out.size() >= 32) break;
  }
  return out.empty() ? "radio" : out;
}
}  // namespace

RpcResult Node::m_interop_send(Params& p) {
  int64_t link_id = 0;
  std::string to, text;
  if (!p.integer("link_id", &link_id, 1, 0x7fffffff, true) || !p.str("to", &to, 64, false) ||
      !p.str("text", &text, 1000, true))
    return p.fail();
  if (!utf8_valid(text) || !utf8_valid(to)) return rpc_err("bad_params", "text must be valid UTF-8");
  if (trim(text).empty()) return rpc_err("bad_params", "text is required");
  LinkRec* r = link_by_id(static_cast<uint32_t>(link_id));
  if (!r) return rpc_err("not_found", "no such link");
  if (!r->link) return rpc_err("unsupported", "link driver unavailable: " + r->create_error);
  InteropText t;
  t.network = r->last_interop_network.empty() ? default_network(r->type) : r->last_interop_network;
  t.to = to;
  t.text = text;
  t.from = profile_.get_string("callsign").empty() ? profile_.get_string("name") : profile_.get_string("callsign");
  if (!r->link->send_interop_text(t)) return rpc_err("unsupported", "this link cannot send plain text to other stations");
  std::string net = clean_network(t.network);
  std::string conv = "io:" + net;
  Json m = new_message(conv, local_message_id(), true, "interop", text);
  m.set("status", "sent").set("via_link", r->id);
  m.set("interop", Json::object({{"network", net}, {"from", t.from}, {"to", to}}));
  store_message(m, false, true);
  if (r->type == "kiss" || r->type == "audio") {
    r->interop_pending.emplace_back(to, m.get_string("id"));
    while (r->interop_pending.size() > 32) r->interop_pending.pop_front();
  }
  Json& meta = conv_meta(conv);
  meta.set("via_link", r->id);
  save_conv_meta(conv);
  return rpc_ok(Json::object({{"message", m}}));
}

void Node::link_interop_text(Link& link, const InteropText& msg) {
  LinkRec* r = link_by_ptr(&link);
  if (!r || lock_ == Lock::NoStore || !id_.valid()) return;
  std::string net = clean_network(msg.network.empty() ? default_network(r->type) : msg.network);
  r->last_interop_network = net;
  std::string conv = "io:" + net;
  std::string text = utf8_sanitize(utf8_truncate(msg.text, proto::kMaxText));
  if (text.empty()) return;
  std::string from = utf8_sanitize(utf8_truncate(msg.from, 64)), to = utf8_sanitize(utf8_truncate(msg.to, 64));
  Json m = new_message(conv, local_message_id(), false, "interop", text);
  m.set("from", from).set("from_name", from).set("via_link", r->id).set("signed_ok", false);
  m.set("interop", Json::object({{"network", net}, {"from", from}, {"to", to}}));
  Json& meta = conv_meta(conv);
  meta.set("via_link", r->id);
  if (!from.empty()) meta.set("reply_to", from);
  store_message(m, true, true);
  save_conv_meta(conv);
}

// --- engine callbacks: text / file / status ------------------------------------------------------

void Node::on_text(const mesh::RxInfo& rx, const proto::TextBody& body) {
  if (!id_.valid() || rx.from == id_.node_id() || is_blocked(rx.from)) return;
  std::string conv = conv_for_rx(rx);
  if (conv.empty()) return;
  Json m = new_message(conv, rx.id.to_hex(), false, "text", utf8_sanitize(body.text));
  uint64_t ts = rx.created_ms ? rx.created_ms : rx.received_ms;
  if (ts > rx.received_ms + 300000) ts = rx.received_ms;  // sender's clock is in the future
  m.set("from", node_id_hex(rx.from)).set("from_name", peer_name(rx.from));
  m.set("ts", ts).set("received_ts", rx.received_ms).set("priority", prio_name(rx.prio));
  m.set("encrypted", rx.encrypted).set("signed_ok", rx.authenticated).set("hops", static_cast<int>(rx.hops));
  m.set("via_link", rx.via_link);
  if (body.reply_to) m.set("reply_to", body.reply_to->to_hex());
  if (!store_message(m, true, true)) return;
  Json& meta = conv_meta(conv);
  if (conv[0] == 'd') {
    Json pend = meta.get("pending_read").is_array() ? meta.get("pending_read") : Json::array();
    pend.push(rx.id.pkt_id);
    while (pend.size() > kMaxPendingRead) pend.items_mut().erase(pend.items_mut().begin());
    meta.set("pending_read", std::move(pend));
    save_conv_meta(conv);
  } else {
    auto it = channels_.find(conv.substr(3));
    if (it != channels_.end()) {
      std::string hex = node_id_hex(rx.from);
      Json& mem = it->second.rec.at_key("members");
      bool seen = false;
      for (const Json& x : mem.items()) seen = seen || x.as_string() == hex;
      if (!seen && mem.size() < kMaxChannelMembers) {
        mem.push(hex);
        db_put_json("channels", B(it->first), it->second.rec);
      }
    }
  }
}

void Node::on_file_progress(const proto::MsgId& file, const NodeId& from, uint32_t received, uint32_t total) {
  if (is_blocked(from)) return;
  emit("file.progress", Json::object({{"id", file.to_hex()}, {"received", received}, {"total", total}}));
}

void Node::on_file(const mesh::RxInfo& rx, const std::string& name, const std::string& mime, const Bytes& data) {
  if (!id_.valid() || rx.from == id_.node_id() || is_blocked(rx.from)) return;
  std::string conv = conv_for_rx(rx);
  if (conv.empty()) return;
  Bytes id = id12(rx.id);
  if (kv_ && kv_->contains("mid", id)) return;
  std::string nm = utf8_sanitize(name), mm = utf8_sanitize(mime);
  Json m = new_message(conv, rx.id.to_hex(), false, "file", nm);
  m.set("from", node_id_hex(rx.from)).set("from_name", peer_name(rx.from));
  m.set("ts", rx.created_ms ? std::min(rx.created_ms, rx.received_ms + 300000) : rx.received_ms);
  m.set("received_ts", rx.received_ms).set("priority", prio_name(rx.prio));
  m.set("encrypted", rx.encrypted).set("signed_ok", rx.authenticated).set("hops", static_cast<int>(rx.hops));
  m.set("via_link", rx.via_link);
  m.set("file", Json::object({{"name", nm}, {"mime", mm}, {"size", data.size()}, {"complete", true}}));
  db_put("files", id, data);
  ByteWriter fk;
  fk.u64be(rx.received_ms).bytes(id);
  db_put("fage", fk.data(), ByteView());
  emit("file.progress", Json::object({{"id", rx.id.to_hex()}, {"received", data.size()}, {"total", data.size()}}));
  store_message(m, true, true);
}

void Node::on_status(const proto::MsgId& msg, mesh::MsgStatus status) {
  if (status == mesh::MsgStatus::Unknown) return;
  set_message_status(msg.to_hex(), mesh::msg_status_name(status));
}

void Node::on_read(const NodeId& reader, const proto::MsgId& msg) {
  if (is_blocked(reader)) return;
  set_message_status(msg.to_hex(), "read");
}

void Node::on_announce(const mesh::RxInfo& rx, const proto::AnnounceBody& body) {
  auto it = contacts_.find(rx.from);
  if (it == contacts_.end()) return;
  PublicKeys k;
  if (!contact_keys(it->second, &k) && body.keys.node_id() == rx.from) {
    it->second.set("keys", keys_b64(body.keys));
    db_put_json("contacts", nid_view(rx.from), it->second);
  }
}

void Node::on_peer(const mesh::PeerInfo& peer, bool is_new) {
  if (peer_event_mono_.size() > 4096) peer_event_mono_.clear();
  auto it = peer_event_mono_.find(peer.id);
  if (!is_new && it != peer_event_mono_.end() && mono_ - it->second < kPeerEventIntervalMs) return;
  peer_event_mono_[peer.id] = mono_;
  const mesh::PeerLink* best = nullptr;
  for (const auto& l : peer.links)
    if (!best || l.last_rx_ms > best->last_rx_ms) best = &l;
  Json d = Json::object();
  d.set("node_id", node_id_hex(peer.id)).set("name", peer.name).set("link_id", best ? best->link_id : 0);
  if (peer.hops != 0xFF)
    d.set("hops", static_cast<int>(peer.hops));
  else
    d.set("hops", Json());
  if (best && best->has_rssi) d.set("rssi", best->rssi_dbm);
  if (best && best->has_snr) d.set("snr", best->snr_db);
  emit("peer.seen", d);
  if (contacts_.count(peer.id)) emit_contact(peer.id);
}

}  // namespace ember::node
