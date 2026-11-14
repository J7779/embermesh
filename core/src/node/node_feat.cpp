// EmberMesh node — emergency features (SOS, check-ins, position, bulletins), mesh methods, net
// plans, sneakernet bundles, the periodic sweep, and the engine callbacks behind them.
#include <algorithm>
#include <cstdio>

#include "base/codec.h"
#include "node/node.h"
#include "node/node_util.h"

namespace ember::node {

namespace {
constexpr size_t kMaxAlerts = 256;
constexpr size_t kMaxUnverifiedAlerts = 32;
constexpr size_t kMaxCheckins = 2000;
constexpr size_t kMaxBulletins = 1000;
constexpr size_t kMaxNetplans = 64;
constexpr uint64_t kAlertMaxAgeMs = 2 * 86400000ull;
constexpr size_t kMaxBundleBytes = 12u << 20;

const char* checkin_label(proto::CheckinStatus s) {
  switch (s) {
    case proto::CheckinStatus::NeedHelp: return "Need help";
    case proto::CheckinStatus::Injured: return "Injured";
    case proto::CheckinStatus::Moving: return "Moving";
    case proto::CheckinStatus::Sheltering: return "Sheltering";
    default: return "I'm OK";
  }
}

std::string position_text(const proto::Position& p) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "Position %.5f, %.5f", p.lat(), p.lon());
  return buf;
}

template <class M>
void evict_oldest(M& m, size_t cap, const char* ts_key) {
  while (m.size() > cap) {
    auto victim = m.begin();
    for (auto it = m.begin(); it != m.end(); ++it)
      if (it->second.get_int(ts_key) < victim->second.get_int(ts_key)) victim = it;
    m.erase(victim);
  }
}

RpcResult send_fail(const mesh::SendResult& r) {
  if (r.error == "too_large") return rpc_err("too_large", "too large for the mesh");
  if (r.error == "busy") return rpc_err("busy", "the mesh is busy, try again");
  if (r.error == "not_started") return rpc_err("busy", "the node clock is not set yet");
  if (r.error == "bad_params") return rpc_err("bad_params", "invalid request");
  return rpc_err("internal", "send failed: " + r.error);
}
}  // namespace

// --- SOS -----------------------------------------------------------------------------------------

Json Node::sos_status_json() {
  Json j = Json::object();
  bool active = eng_ ? eng_->sos_active() : own_sos_.get_bool("active");
  j.set("active", active);
  if (active) {
    j.set("since_ts", own_sos_.get_int("since_ts", eng_ ? static_cast<int64_t>(eng_->sos_since_ms()) : 0));
    j.set("text", own_sos_.get_string("text"));
    if (own_sos_.get("position").is_object()) j.set("position", own_sos_.get("position"));
  }
  j.set("sent", eng_ ? eng_->sos_sent() : 0);
  j.set("acked_by", own_sos_.get("acked_by").is_array() ? own_sos_.get("acked_by") : Json::array());
  return j;
}

RpcResult Node::m_sos_start(Params& p) {
  std::string text;
  int64_t interval = 60;
  const Json* pos_j = nullptr;
  if (!p.str("text", &text, 500, false) || !p.object("position", &pos_j, false) ||
      !p.integer("interval_s", &interval, 10, 3600, false))
    return p.fail();
  if (!utf8_valid(text)) return rpc_err("bad_params", "text must be valid UTF-8");
  if (text.empty()) text = "SOS - need help";
  std::optional<proto::Position> pos;
  proto::Position pp;
  std::string err;
  if (pos_j) {
    if (!json_to_position(*pos_j, &pp, &err)) return rpc_err("bad_params", err);
    pos = pp;
  } else if (own_position_.is_object() && settings_.get_string("position_sharing") != "off" &&
             json_to_position(own_position_, &pp)) {
    pos = pp;
  }
  bool was_active = eng_->sos_active();
  mesh::SendResult r = eng_->sos_start(text, pos, static_cast<uint32_t>(interval));
  if (!r.ok) return send_fail(r);
  uint64_t now = mesh_now();
  Json prev = own_sos_;
  own_sos_ = Json::object();
  own_sos_.set("active", true).set("text", text).set("interval_s", interval);
  own_sos_.set("since_ts", was_active && prev.get_int("since_ts") ? prev.get_int("since_ts") : static_cast<int64_t>(now));
  own_sos_.set("acked_by", was_active && prev.get("acked_by").is_array() ? prev.get("acked_by") : Json::array());
  if (pos) own_sos_.set("position", position_to_json(*pos, now));
  db_put_json("meta", B("sos"), own_sos_);
  if (!was_active) {
    Json m = new_message("ch:public", r.id.to_hex(), true, "sos", text);
    m.set("priority", "emergency").set("status", "sent");
    if (pos) m.set("position", position_to_json(*pos, now));
    store_message(m, false, true);
  }
  emit("sos.updated", sos_status_json());
  return rpc_ok(Json::object({{"active", true}, {"since_ts", own_sos_.get_int("since_ts")}}));
}

RpcResult Node::m_sos_stop(Params&) {
  if (eng_->sos_active()) eng_->sos_stop();
  own_sos_ = Json::object({{"active", false}});
  db_put_json("meta", B("sos"), own_sos_);
  emit("sos.updated", sos_status_json());
  return rpc_ok(Json::object({{"active", false}}));
}

RpcResult Node::m_sos_status(Params&) { return rpc_ok(sos_status_json()); }

RpcResult Node::m_sos_list(Params&) {
  std::vector<Json> v;
  for (auto& kv : alerts_) {
    Json a = kv.second;
    a.erase("pkt_id");
    v.push_back(std::move(a));
  }
  std::sort(v.begin(), v.end(), [](const Json& a, const Json& b) { return a.get_int("last_ts") > b.get_int("last_ts"); });
  Json arr = Json::array();
  for (auto& a : v) arr.push(std::move(a));
  return rpc_ok(Json::object({{"alerts", std::move(arr)}}));
}

RpcResult Node::m_sos_ack(Params& p) {
  std::string hex, text;
  if (!p.str("node_id", &hex, 16, true) || !p.str("text", &text, 200, false)) return p.fail();
  NodeId id;
  if (!parse_node_id(hex, id)) return rpc_err("bad_params", "node_id must be 16 hex characters");
  auto it = alerts_.find(id);
  if (it == alerts_.end()) return rpc_err("not_found", "no SOS alert from that node");
  mesh::SendResult r = eng_->sos_ack(id, utf8_sanitize(text), static_cast<uint32_t>(it->second.get_int("pkt_id")));
  if (!r.ok) return send_fail(r);
  it->second.set("acked", true);
  db_put_json("sos", nid_view(id), it->second);
  return rpc_ok();
}

void Node::on_sos(const mesh::RxInfo& rx, const proto::SosBody& body) {
  // Life safety: SOS from blocked contacts is still shown.
  if (!id_.valid() || rx.from == id_.node_id()) return;
  auto it = alerts_.find(rx.from);
  std::string name = peer_name(rx.from);
  if (!body.active) {
    if (it == alerts_.end()) return;
    // Only an authenticated cancel clears an alert (an unverified one only an unverified alert;
    // security review H1).
    if (!rx.authenticated && it->second.get_bool("signed_ok")) return;
    alerts_.erase(it);
    db_del("sos", nid_view(rx.from));
    add_system_message("ch:public", "SOS cancelled by " + (name.empty() ? node_id_hex(rx.from) : name));
    return;
  }
  bool is_new = it == alerts_.end();
  // An unverified repeat never downgrades a verified alert's text / position.
  if (!is_new && !rx.authenticated && it->second.get_bool("signed_ok")) return;
  if (is_new && !rx.authenticated) {
    // Unverified alerts have their own small table: they can never push verified alerts out
    // (security review C2). The oldest unverified alert makes room.
    size_t unv = 0;
    auto oldest = alerts_.end();
    for (auto i = alerts_.begin(); i != alerts_.end(); ++i)
      if (!i->second.get_bool("signed_ok")) {
        ++unv;
        if (oldest == alerts_.end() || i->second.get_int("last_ts") < oldest->second.get_int("last_ts")) oldest = i;
      }
    if (unv >= kMaxUnverifiedAlerts && oldest != alerts_.end()) {
      db_del("sos", nid_view(oldest->first));
      alerts_.erase(oldest);
    }
  }
  Json a = is_new ? Json::object({{"node_id", node_id_hex(rx.from)}, {"first_ts", rx.received_ms}, {"acked", false}})
                  : it->second;
  std::string text = utf8_sanitize(body.text);
  a.set("name", name).set("text", text).set("last_ts", rx.received_ms).set("hops", static_cast<int>(rx.hops));
  a.set("pkt_id", rx.id.pkt_id).set("signed_ok", rx.authenticated);
  if (body.pos) a.set("position", position_to_json(*body.pos, rx.created_ms ? rx.created_ms : rx.received_ms));
  alerts_[rx.from] = a;
  if (alerts_.size() > kMaxAlerts) {
    // Unverified first, then the oldest.
    auto victim = alerts_.end();
    for (auto i = alerts_.begin(); i != alerts_.end(); ++i) {
      if (i->first == rx.from) continue;
      if (victim == alerts_.end()) { victim = i; continue; }
      bool a1 = i->second.get_bool("signed_ok"), b1 = victim->second.get_bool("signed_ok");
      if (a1 != b1) { if (!a1) victim = i; continue; }
      if (i->second.get_int("last_ts") < victim->second.get_int("last_ts")) victim = i;
    }
    if (victim != alerts_.end()) {
      db_del("sos", nid_view(victim->first));
      alerts_.erase(victim);
    }
  }
  db_put_json("sos", nid_view(rx.from), a);
  Json ev = Json::object({{"node_id", node_id_hex(rx.from)}, {"name", name}, {"text", text}});
  if (body.pos) ev.set("position", a.get("position"));
  ev.set("ts", rx.created_ms ? rx.created_ms : rx.received_ms).set("hops", static_cast<int>(rx.hops));
  emit("sos.received", ev);
  if (is_new) {
    Json m = new_message("ch:public", rx.id.to_hex(), false, "sos", text);
    m.set("from", node_id_hex(rx.from)).set("from_name", name).set("priority", "emergency");
    m.set("ts", rx.received_ms).set("received_ts", rx.received_ms).set("hops", static_cast<int>(rx.hops));
    m.set("via_link", rx.via_link).set("signed_ok", rx.authenticated);
    if (body.pos) m.set("position", a.get("position"));
    store_message(m, true, true);
  }
}

void Node::on_sos_ack(const mesh::RxInfo& rx, const proto::SosAckBody& body) {
  if (!own_sos_.get_bool("active") || is_blocked(rx.from)) return;
  Json& acks = own_sos_.at_key("acked_by");
  if (!acks.is_array()) acks = Json::array();
  std::string hex = node_id_hex(rx.from);
  Json entry = Json::object({{"node_id", hex}, {"name", peer_name(rx.from)}, {"ts", rx.received_ms}});
  if (!body.text.empty()) entry.set("text", utf8_sanitize(body.text));
  bool replaced = false;
  for (Json& x : acks.items_mut())
    if (x.get_string("node_id") == hex) {
      x = entry;
      replaced = true;
    }
  if (!replaced && acks.size() < 64) acks.push(entry);
  db_put_json("meta", B("sos"), own_sos_);
  emit("sos.updated", sos_status_json());
}

// --- check-ins -----------------------------------------------------------------------------------

RpcResult Node::m_checkin_send(Params& p) {
  std::string st, note;
  const Json* pos_j = nullptr;
  if (!p.str("status", &st, 32, true) || !p.str("note", &note, proto::kMaxNote, false) ||
      !p.object("position", &pos_j, false))
    return p.fail();
  proto::CheckinBody b;
  if (!proto::parse_checkin_status(st, b.status))
    return rpc_err("bad_params", "status must be ok, need_help, injured, moving or sheltering");
  if (!utf8_valid(note)) return rpc_err("bad_params", "note must be valid UTF-8");
  b.note = note;
  proto::Position pp;
  std::string err;
  if (pos_j) {
    if (!json_to_position(*pos_j, &pp, &err)) return rpc_err("bad_params", err);
    b.pos = pp;
  } else if (own_position_.is_object() && settings_.get_string("position_sharing") != "off" &&
             json_to_position(own_position_, &pp)) {
    b.pos = pp;
  }
  mesh::SendOptions o;
  o.prio = b.status == proto::CheckinStatus::Ok ? proto::Prio::Normal : proto::Prio::High;
  mesh::SendResult r = eng_->send_checkin(b, o);
  if (!r.ok) return send_fail(r);
  uint64_t now = mesh_now();
  Json ci = Json::object({{"node_id", node_id_hex(id_.node_id())},
                          {"name", profile_.get_string("name")},
                          {"status", st},
                          {"note", note},
                          {"ts", now}});
  if (b.pos) ci.set("position", position_to_json(*b.pos, now));
  checkins_[id_.node_id()] = ci;
  db_put_json("checkins", nid_view(id_.node_id()), ci);
  Json m = new_message("ch:public", r.id.to_hex(), true, "checkin",
                       std::string("Check-in: ") + checkin_label(b.status) + (note.empty() ? "" : ". " + note));
  m.set("priority", prio_name(o.prio)).set("status", "sent");
  if (b.pos) m.set("position", ci.get("position"));
  store_message(m, false, true);
  return rpc_ok();
}

RpcResult Node::m_checkin_list(Params&) {
  std::vector<Json> v;
  for (auto& kv : checkins_) v.push_back(kv.second);
  std::sort(v.begin(), v.end(), [](const Json& a, const Json& b) { return a.get_int("ts") > b.get_int("ts"); });
  Json arr = Json::array();
  for (auto& a : v) arr.push(std::move(a));
  return rpc_ok(Json::object({{"checkins", std::move(arr)}}));
}

void Node::on_checkin(const mesh::RxInfo& rx, const proto::CheckinBody& body) {
  if (!id_.valid() || rx.from == id_.node_id() || is_blocked(rx.from)) return;
  uint64_t ts = rx.created_ms ? std::min(rx.created_ms, rx.received_ms + 300000) : rx.received_ms;
  auto old = checkins_.find(rx.from);
  if (old != checkins_.end() && old->second.get_int("ts") > static_cast<int64_t>(ts)) return;  // stale copy
  std::string name = peer_name(rx.from);
  std::string note = utf8_sanitize(body.note);
  Json ci = Json::object({{"node_id", node_id_hex(rx.from)},
                          {"name", name},
                          {"status", proto::checkin_status_name(body.status)},
                          {"note", note},
                          {"ts", ts}});
  if (body.pos) ci.set("position", position_to_json(*body.pos, ts));
  checkins_[rx.from] = ci;
  evict_oldest(checkins_, kMaxCheckins, "ts");
  db_put_json("checkins", nid_view(rx.from), ci);
  emit("checkin.received", ci);
  Json m = new_message("ch:public", rx.id.to_hex(), false, "checkin",
                       std::string("Check-in: ") + checkin_label(body.status) + (note.empty() ? "" : ". " + note));
  m.set("from", node_id_hex(rx.from)).set("from_name", name).set("ts", ts).set("received_ts", rx.received_ms);
  m.set("priority", prio_name(rx.prio)).set("hops", static_cast<int>(rx.hops)).set("via_link", rx.via_link);
  m.set("signed_ok", rx.authenticated);
  if (body.pos) m.set("position", ci.get("position"));
  store_message(m, body.status != proto::CheckinStatus::Ok, true);
}

// --- position ------------------------------------------------------------------------------------

RpcResult Node::m_position_set(Params& p) {
  proto::Position pp;
  std::string err;
  if (!json_to_position(p.raw(), &pp, &err)) return rpc_err("bad_params", err);
  int64_t ts = 0;
  if (!p.integer("ts", &ts, 0, INT64_MAX, false)) return p.fail();
  if (ts == 0) ts = static_cast<int64_t>(mesh_now());
  own_position_ = position_to_json(pp, static_cast<uint64_t>(ts));
  if (lock_ != Lock::NoStore) db_put_json("meta", B("position"), own_position_);
  if (eng_ && lock_ == Lock::Open && settings_.get_string("position_sharing") == "auto" &&
      (last_auto_position_ == 0 || mono_ - last_auto_position_ >= 600000)) {
    last_auto_position_ = mono_ ? mono_ : 1;
    mesh::SendOptions o;
    o.prio = proto::Prio::Low;
    eng_->send_position(mesh::Dest::broadcast(), pp, o);
  }
  return rpc_ok();
}

RpcResult Node::m_position_get(Params&) {
  Json j = Json::object();
  if (own_position_.is_object()) j.set("position", own_position_);
  return rpc_ok(j);
}

RpcResult Node::m_position_share(Params& p) {
  std::string conv;
  if (!p.str("conv", &conv, 64, true)) return p.fail();
  RpcResult c = check_conv(conv, true);
  if (!c.ok) return c;
  if (conv[0] == 'i') return rpc_err("unsupported", "positions cannot be shared with interop stations");
  proto::Position pp;
  if (!own_position_.is_object() || !json_to_position(own_position_, &pp))
    return rpc_err("not_found", "no position fix yet (position.set)");
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
  mesh::SendResult r = eng_->send_position(dest, pp);
  if (!r.ok) return send_fail(r);
  Json m = new_message(conv, r.id.to_hex(), true, "position", position_text(pp));
  m.set("position", own_position_).set("encrypted", encrypted);
  mesh::MsgStatus st = eng_->status(r.id);
  if (st != mesh::MsgStatus::Unknown) m.set("status", mesh::msg_status_name(st));
  store_message(m, false, true);
  return rpc_ok(Json::object({{"message", m}}));
}

void Node::on_position(const mesh::RxInfo& rx, const proto::Position& pos) {
  if (!id_.valid() || rx.from == id_.node_id() || is_blocked(rx.from)) return;
  if (rx.dest.type == mesh::Dest::Broadcast) return;  // beacons only update the peer table
  std::string conv = conv_for_rx(rx);
  if (conv.empty()) return;
  uint64_t ts = rx.created_ms ? std::min(rx.created_ms, rx.received_ms + 300000) : rx.received_ms;
  Json m = new_message(conv, rx.id.to_hex(), false, "position", position_text(pos));
  m.set("from", node_id_hex(rx.from)).set("from_name", peer_name(rx.from)).set("ts", ts);
  m.set("received_ts", rx.received_ms).set("position", position_to_json(pos, ts));
  m.set("encrypted", rx.encrypted).set("signed_ok", rx.authenticated).set("hops", static_cast<int>(rx.hops));
  m.set("via_link", rx.via_link);
  store_message(m, true, true);
}

// --- bulletins -----------------------------------------------------------------------------------

RpcResult Node::m_bulletins_list(Params& p) {
  std::string cat;
  if (!p.str("category", &cat, 32, false)) return p.fail();
  proto::BulletinCategory c;
  if (!cat.empty() && !proto::parse_bulletin_category(cat, c)) return rpc_err("bad_params", "unknown category");
  int64_t now = static_cast<int64_t>(mesh_now());
  std::vector<Json> v;
  for (auto& kv : bulletins_) {
    const Json& b = kv.second;
    if (b.get_int("expires_ts") <= now) continue;
    if (!cat.empty() && b.get_string("category") != cat) continue;
    v.push_back(b);
  }
  std::sort(v.begin(), v.end(), [](const Json& a, const Json& b) { return a.get_int("ts") > b.get_int("ts"); });
  Json arr = Json::array();
  for (auto& b : v) arr.push(std::move(b));
  return rpc_ok(Json::object({{"bulletins", std::move(arr)}}));
}

RpcResult Node::m_bulletins_post(Params& p) {
  std::string cat, title, text;
  const Json* pos_j = nullptr;
  int64_t expires = settings_.get_int("store_forward_days", 3) * 86400;
  if (!p.str("category", &cat, 32, true) || !p.str("title", &title, proto::kMaxShortField, true) ||
      !p.str("text", &text, proto::kMaxBulletinText, false) || !p.object("position", &pos_j, false) ||
      !p.integer("expires_s", &expires, 60, 30 * 86400, false))
    return p.fail();
  proto::BulletinBody b;
  if (!proto::parse_bulletin_category(cat, b.category)) return rpc_err("bad_params", "unknown category");
  if (!utf8_valid(title) || !utf8_valid(text)) return rpc_err("bad_params", "title/text must be valid UTF-8");
  std::string t = title;
  while (!t.empty() && (t.back() == ' ' || t.back() == '\n')) t.pop_back();
  if (t.empty()) return rpc_err("bad_params", "title is required");
  b.title = t;
  b.text = text;
  proto::Position pp;
  std::string err;
  if (pos_j) {
    if (!json_to_position(*pos_j, &pp, &err)) return rpc_err("bad_params", err);
    b.pos = pp;
  }
  mesh::SendResult r = eng_->post_bulletin(b, static_cast<uint32_t>(expires));
  if (!r.ok) return send_fail(r);
  uint64_t now = mesh_now();
  Json bl = Json::object({{"id", r.id.to_hex()},
                          {"from", node_id_hex(id_.node_id())},
                          {"from_name", profile_.get_string("name")},
                          {"ts", now},
                          {"expires_ts", now + static_cast<uint64_t>(expires) * 1000},
                          {"category", cat},
                          {"title", t},
                          {"text", text},
                          {"mine", true}});
  if (b.pos) bl.set("position", position_to_json(*b.pos, now));
  bulletins_[bl.get_string("id")] = bl;
  db_put_json("bulletins", B(bl.get_string("id")), bl);
  emit("bulletin.new", Json::object({{"bulletin", bl}}));
  return rpc_ok(Json::object({{"bulletin", bl}}));
}

RpcResult Node::m_bulletins_delete(Params& p) {
  std::string id;
  if (!p.str("id", &id, 24, true)) return p.fail();
  auto it = bulletins_.find(id);
  if (it == bulletins_.end()) return rpc_err("not_found", "no such bulletin");
  if (!it->second.get_bool("mine")) return rpc_err("denied", "only your own bulletins can be withdrawn");
  proto::MsgId mid;
  if (proto::MsgId::parse(id, mid)) eng_->delete_bulletin(mid.pkt_id);
  bulletins_.erase(it);
  db_del("bulletins", B(id));
  return rpc_ok();
}

void Node::on_bulletin(const mesh::RxInfo& rx, const proto::BulletinBody& body) {
  if (!id_.valid() || rx.from == id_.node_id() || is_blocked(rx.from)) return;
  if (body.is_delete) {
    if (!rx.authenticated) return;  // only the verified author withdraws (security review H1)
    proto::MsgId target{rx.from, body.delete_pkt_id};
    std::string id = target.to_hex();
    if (bulletins_.erase(id)) db_del("bulletins", B(id));
    return;
  }
  std::string id = rx.id.to_hex();
  if (rx.expires_ms <= mesh_now()) return;
  auto have = bulletins_.find(id);
  // An authenticated copy supersedes an unverified look-alike with the same id (M4).
  if (have != bulletins_.end() && (have->second.get_bool("signed_ok", true) || !rx.authenticated)) return;
  uint64_t ts = rx.created_ms ? std::min(rx.created_ms, rx.received_ms + 300000) : rx.received_ms;
  Json bl = Json::object({{"id", id},
                          {"from", node_id_hex(rx.from)},
                          {"from_name", peer_name(rx.from)},
                          {"ts", ts},
                          {"expires_ts", rx.expires_ms},
                          {"category", proto::bulletin_category_name(body.category)},
                          {"title", utf8_sanitize(body.title)},
                          {"text", utf8_sanitize(body.text)},
                          {"mine", false},
                          {"signed_ok", rx.authenticated}});
  if (body.pos) bl.set("position", position_to_json(*body.pos, ts));
  bulletins_[id] = bl;
  if (bulletins_.size() > kMaxBulletins) {
    // Unverified senders' bulletins go first, then the soonest to expire (M4).
    auto victim = bulletins_.end();
    for (auto it = bulletins_.begin(); it != bulletins_.end(); ++it) {
      if (it->second.get_bool("mine")) continue;
      if (victim == bulletins_.end()) { victim = it; continue; }
      bool a1 = it->second.get_bool("signed_ok", true), b1 = victim->second.get_bool("signed_ok", true);
      if (a1 != b1) { if (!a1) victim = it; continue; }
      if (it->second.get_int("expires_ts") < victim->second.get_int("expires_ts")) victim = it;
    }
    if (victim != bulletins_.end()) {
      db_del("bulletins", B(victim->first));
      bulletins_.erase(victim);
    }
  }
  if (bulletins_.count(id)) {
    db_put_json("bulletins", B(id), bl);
    emit("bulletin.new", Json::object({{"bulletin", bl}}));
  }
}

// --- mesh ----------------------------------------------------------------------------------------

RpcResult Node::m_mesh_peers(Params&) {
  Json arr = Json::array();
  if (eng_) {
    for (const mesh::PeerInfo& pi : eng_->peers()) {
      if (pi.id == id_.node_id()) continue;
      Json j = Json::object();
      std::string name = peer_name(pi.id);
      j.set("node_id", node_id_hex(pi.id)).set("name", name.empty() ? pi.name : name).set("callsign", pi.callsign);
      j.set("status", pi.status).set("direct", pi.direct);
      if (pi.hops != 0xFF)
        j.set("hops", static_cast<int>(pi.hops));
      else
        j.set("hops", Json());
      j.set("last_seen_ts", pi.last_seen_ms).set("has_keys", pi.has_keys).set("relay", pi.relay);
      Json links = Json::array();
      for (const auto& l : pi.links) {
        Json lj = Json::object({{"link_id", l.link_id}, {"last_rx_ts", l.last_rx_ms}});
        if (l.has_rssi) lj.set("rssi", l.rssi_dbm);
        if (l.has_snr) lj.set("snr", l.snr_db);
        links.push(std::move(lj));
      }
      j.set("links", std::move(links));
      if (pi.position) j.set("position", position_to_json(*pi.position, pi.position_ms));
      j.set("contact", contacts_.count(pi.id) > 0);
      arr.push(std::move(j));
    }
  }
  return rpc_ok(Json::object({{"peers", std::move(arr)}}));
}

RpcResult Node::m_mesh_stats(Params&) {
  mesh::MeshStats s = eng_ ? eng_->stats() : mesh::MeshStats();
  Json j = Json::object();
  j.set("stored_messages", s.stored_messages).set("stored_bytes", s.stored_bytes).set("relayed", s.relayed);
  j.set("dropped_duplicates", s.dropped_duplicates).set("dropped_ttl", s.dropped_ttl);
  j.set("tx_frames", s.tx_frames).set("rx_frames", s.rx_frames).set("rx_bad", s.rx_bad);
  j.set("uptime_ms", mono_ - created_mono_);
  j.set("dropped_expired", s.dropped_expired).set("dropped_rate", s.dropped_rate).set("dropped_auth", s.dropped_auth);
  j.set("suppressed", s.suppressed).set("delivered", s.delivered).set("acks_received", s.acks_received);
  j.set("retries", s.retries).set("sync_rounds", s.sync_rounds).set("store_evicted", s.store_evicted);
  j.set("outbox_dropped", out_dropped_);
  if (kv_) {
    store::StoreStats ss = kv_->stats();
    j.set("db_records", ss.records).set("db_file_bytes", ss.file_bytes).set("db_live_bytes", ss.live_bytes);
  }
  Json pl = Json::array();
  for (const auto& l : s.per_link) {
    Json o = Json::object({{"link_id", l.link_id},
                           {"airtime_ms", l.airtime_ms},
                           {"duty_used_permille", l.duty_used_permille},
                           {"queue", l.queue}});
    o.set("tx_frames", l.tx_frames).set("rx_frames", l.rx_frames).set("tx_bytes", l.tx_bytes).set("rx_bytes", l.rx_bytes);
    o.set("rx_bad", l.rx_bad).set("blocked_legal", l.blocked_legal).set("blocked_no_callsign", l.blocked_no_callsign);
    o.set("dropped_queue_full", l.dropped_queue_full).set("neighbours", l.neighbours);
    pl.push(std::move(o));
  }
  j.set("per_link", std::move(pl));
  return rpc_ok(j);
}

RpcResult Node::m_mesh_ping(Params& p) {
  std::string hex;
  if (!p.str("node_id", &hex, 16, true)) return p.fail();
  NodeId id;
  if (!parse_node_id(hex, id)) return rpc_err("bad_params", "node_id must be 16 hex characters");
  if (id == id_.node_id()) return rpc_err("bad_params", "cannot ping yourself");
  if (!eng_->ping(id)) return rpc_err("not_found", "unknown node (its keys have not been heard yet)");
  return rpc_ok(Json::object({{"sent", true}}));
}

RpcResult Node::m_mesh_announce(Params&) {
  eng_->announce_now();
  return rpc_ok();
}

void Node::on_pong(const NodeId& from, uint32_t rtt_ms, uint8_t hops) {
  emit("mesh.pong", Json::object({{"node_id", node_id_hex(from)}, {"rtt_ms", rtt_ms}, {"hops", static_cast<int>(hops)}}));
}

void Node::on_time(uint64_t mesh_ms, mesh::TimeSource source) {
  emit("time.updated", Json::object({{"time_ms", mesh_ms}, {"source", mesh::time_source_name(source)}}));
}

// --- net plans -----------------------------------------------------------------------------------

RpcResult Node::m_netplan_get(Params&) {
  Json j = Json::object();
  if (own_netplan_.is_object() && !own_netplan_.get("entries").empty()) j.set("plan", own_netplan_);
  std::vector<Json> v;
  for (auto& kv : netplans_) v.push_back(kv.second);
  std::sort(v.begin(), v.end(), [](const Json& a, const Json& b) { return a.get_int("ts") > b.get_int("ts"); });
  Json arr = Json::array();
  for (auto& x : v) arr.push(std::move(x));
  j.set("heard", std::move(arr));
  j.set("modes", netplan_modes());
  return rpc_ok(j);
}

RpcResult Node::m_netplan_set(Params& p) {
  proto::NetPlanBody np;
  std::string err;
  if (!json_to_netplan(p.raw(), &np, &err)) return rpc_err("bad_params", err);
  mesh::SendResult r = eng_->set_netplan(np, 1800);
  if (!r.ok && !np.entries.empty()) return send_fail(r);
  Json plan = Json::object({{"from", node_id_hex(id_.node_id())},
                            {"from_name", profile_.get_string("name")},
                            {"ts", mesh_now()},
                            {"entries", netplan_entries_json(np)}});
  if (np.entries.empty()) {
    own_netplan_ = Json();
    db_del("meta", B("netplan"));
  } else {
    own_netplan_ = plan;
    db_put_json("meta", B("netplan"), plan);
  }
  return rpc_ok(Json::object({{"plan", plan}}));
}

void Node::on_netplan(const mesh::RxInfo& rx, const proto::NetPlanBody& body) {
  if (!id_.valid() || rx.from == id_.node_id() || is_blocked(rx.from)) return;
  uint64_t ts = rx.created_ms ? std::min(rx.created_ms, rx.received_ms + 300000) : rx.received_ms;
  Json plan = Json::object({{"from", node_id_hex(rx.from)},
                            {"from_name", peer_name(rx.from)},
                            {"ts", ts},
                            {"entries", netplan_entries_json(body)}});
  if (body.entries.empty()) {
    if (netplans_.erase(rx.from)) db_del("netplans", nid_view(rx.from));
    return;
  }
  netplans_[rx.from] = plan;
  evict_oldest(netplans_, kMaxNetplans, "ts");
  db_put_json("netplans", nid_view(rx.from), plan);
  emit("netplan.heard", plan);
}

// --- bundles -------------------------------------------------------------------------------------

RpcResult Node::m_bundle_export(Params& p) {
  std::string scope = "all", conv, hex;
  int64_t since = 0, max_bytes = 0;
  if (!p.str("scope", &scope, 16, false) || !p.str("conv", &conv, 64, false) || !p.str("node_id", &hex, 16, false) ||
      !p.integer("since_ts", &since, 0, INT64_MAX, false) ||
      !p.integer("max_bytes", &max_bytes, 0, static_cast<int64_t>(kMaxBundleBytes) * 4, false))
    return p.fail();
  mesh::BundleFilter f;
  if (scope == "all") {
    f.scope = mesh::BundleFilter::All;
  } else if (scope == "conv") {
    if (conv.empty()) return rpc_err("bad_params", "conv is required for scope conv");
    RpcResult c = check_conv(conv, false);
    if (!c.ok) return c;
    f.scope = mesh::BundleFilter::ToDest;
    if (conv[0] == 'd') {
      NodeId id;
      parse_node_id(conv.substr(3), id);
      f.dest = mesh::Dest::to_node(id);
    } else if (conv == "ch:public") {
      f.dest = mesh::Dest::broadcast();
    } else if (conv[0] == 'c') {
      f.dest = mesh::Dest::to_channel(channels_[conv.substr(3)].key.id);
    } else {
      return rpc_err("bad_params", "interop conversations are not carried in bundles");
    }
  } else if (scope == "for_node") {
    NodeId id;
    if (!parse_node_id(hex, id)) return rpc_err("bad_params", "node_id (16 hex characters) is required for scope for_node");
    f.scope = mesh::BundleFilter::ForNode;
    f.node = id;
  } else {
    return rpc_err("bad_params", "scope must be all, conv or for_node");
  }
  f.since_ms = static_cast<uint64_t>(since);
  f.max_bytes = max_bytes > 0 ? std::min<size_t>(static_cast<size_t>(max_bytes), kMaxBundleBytes) : kMaxBundleBytes;
  uint32_t count = 0;
  Bytes b = eng_->export_bundle(f, &count);
  return rpc_ok(Json::object({{"data_b64", base64_encode(b)}, {"count", count}, {"bytes", b.size()}}));
}

RpcResult Node::m_bundle_import(Params& p) {
  std::string b64;
  if (!p.str("data_b64", &b64, (kMaxBundleBytes / 3 + 1) * 4 + 64, true, false)) return p.fail();
  Bytes data;
  if (!base64_decode_lenient(b64, data) || data.empty()) return rpc_err("bad_params", "data_b64 is not valid base64");
  mesh::BundleImportResult r = eng_->import_bundle(data);
  if (!r.ok) return rpc_err("bad_params", "not a valid EmberMesh bundle");
  return rpc_ok(Json::object(
      {{"imported", r.imported}, {"duplicates", r.duplicates}, {"for_me", r.for_me}, {"rejected", r.rejected}}));
}

// --- periodic sweep ------------------------------------------------------------------------------

void Node::sweep() {
  if (lock_ == Lock::NoStore || !kv_) return;
  int64_t now = static_cast<int64_t>(mesh_now());
  for (auto it = bulletins_.begin(); it != bulletins_.end();) {
    if (it->second.get_int("expires_ts") <= now) {
      db_del("bulletins", B(it->first));
      it = bulletins_.erase(it);
    } else {
      ++it;
    }
  }
  for (auto it = alerts_.begin(); it != alerts_.end();) {
    if (it->second.get_int("last_ts") + static_cast<int64_t>(kAlertMaxAgeMs) < now) {
      db_del("sos", nid_view(it->first));
      it = alerts_.erase(it);
    } else {
      ++it;
    }
  }
  // Expired messages (messages.send expires_s).
  store::ScanSpec s;
  ByteWriter up;
  up.u64be(static_cast<uint64_t>(now));
  s.set_upper(up.data());
  s.keys_only = true;
  s.limit = 256;
  std::vector<Bytes> keys;
  kv_->scan("mexp", s, [&](ByteView k, ByteView, uint64_t) {
    keys.push_back(k.to_bytes());
    return true;
  });
  std::set<std::string> touched;
  for (auto& k : keys) {
    if (k.size() == 20) {
      Bytes id(k.begin() + 8, k.end()), mk;
      if (db_get("mid", id, &mk)) {
        Json m;
        if (db_get_json("msg", mk, &m)) touched.insert(m.get_string("conv"));
        store::WriteBatch b;
        b.del("msg", mk);
        b.del("mid", id);
        db_write(b);
      }
    }
    db_del("mexp", k);
  }
  for (auto& c : touched) emit_conversation(c);
}

}  // namespace ember::node
