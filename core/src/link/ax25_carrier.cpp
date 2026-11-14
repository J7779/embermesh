#include "link/ax25_carrier.h"

#include <cstdio>

#include "ember/crc.h"
#include "framing/aprs.h"
#include "link/link_util.h"

namespace ember {
namespace link {

namespace {

std::string upper(std::string s) {
  for (char& c : s)
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  return s;
}

std::string fmt_coord(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.5f", v);
  return b;
}

constexpr size_t kMaxPending = 16;
constexpr size_t kMaxRecent = 64;
constexpr uint64_t kDedupMs = 30000;
constexpr size_t kMaxParts = 4;
constexpr size_t kMaxSent = 128;  // own-echo memory (frames)

}  // namespace

bool parse_callsign(const std::string& text, ax25::Address* out, std::string* error) {
  ax25::Address a;
  std::string t = trim(text);
  if (t.empty() || t.back() == '*' || !ax25::parse_address(t, &a)) {
    if (error) *error = "callsign: '" + utf8_sanitize(utf8_truncate(t, 20)) +
                        "' is not a valid callsign (letters/digits, up to 6, optional -SSID 0..15)";
    return false;
  }
  a.flag = false;
  *out = a;
  return true;
}

bool parse_path(const std::string& text, std::vector<ax25::Address>* out, std::string* error) {
  out->clear();
  for (const std::string& p : split_list(text, ',')) {
    ax25::Address a;
    if (!ax25::parse_address(p, &a)) {
      if (error) *error = "path: '" + utf8_sanitize(utf8_truncate(p, 20)) + "' is not a valid digipeater";
      return false;
    }
    a.flag = false;
    out->push_back(a);
    if (out->size() > ax25::kMaxDigis) {
      if (error) *error = "path: at most 8 digipeaters";
      return false;
    }
  }
  return true;
}

void Ax25Carrier::set_pseudo_seed(uint32_t seed) {
  std::string call = "QX";
  uint32_t v = seed;
  for (int i = 0; i < 4; ++i) {
    call.push_back(static_cast<char>('A' + v % 26));
    v /= 26;
  }
  pseudo_.call = call;
  pseudo_.ssid = static_cast<uint8_t>((seed >> 27) & 15);
  pseudo_.flag = false;
  configure(cfg_);
}

void Ax25Carrier::configure(const Ax25Config& c) {
  cfg_ = c;
  if (!cfg_.amateur && cfg_.mycall.call == "N0CALL" && !pseudo_.call.empty()) cfg_.mycall = pseudo_;
}

uint32_t Ax25Carrier::frame_key(const ax25::Address& dst, const ax25::Address& src, ByteView info) {
  std::string a = dst.to_string() + ">" + src.to_string() + ":";
  return crc32(info, crc32(ByteView(std::string_view(a))));
}

void Ax25Carrier::note_sent(uint32_t key) {
  sent_.push_back(key);
  while (sent_.size() > kMaxSent) sent_.pop_front();
}

bool Ax25Carrier::sent_recently(uint32_t key) const {
  for (uint32_t k : sent_)
    if (k == key) return true;
  return false;
}

bool Ax25Carrier::wrap(ByteView mesh_frame, Bytes* out) {
  ax25::Address dst;
  dst.call = kMeshDestCall;
  ax25::Frame f = ax25::make_ui(dst, cfg_.mycall, cfg_.path, mesh_frame);
  if (!ax25::encode(f, out)) return false;
  note_sent(frame_key(f.dst, f.src, mesh_frame));
  return true;
}

bool Ax25Carrier::make_aprs(const std::string& info, Bytes* out) {
  ax25::Address dst;
  dst.call = kAprsToCall;
  ByteView iv{std::string_view(info)};
  ax25::Frame f = ax25::make_ui(dst, cfg_.mycall, cfg_.aprs_path, iv);
  if (!ax25::encode(f, out)) return false;
  note_sent(frame_key(f.dst, f.src, iv));
  return true;
}

bool Ax25Carrier::seen_recent(uint32_t h, uint64_t now) {
  while (!recent_.empty() && (recent_.front().second + kDedupMs <= now || recent_.size() > kMaxRecent))
    recent_.pop_front();
  for (const auto& r : recent_)
    if (r.first == h) return true;
  recent_.emplace_back(h, now);
  return false;
}

void Ax25Carrier::handle_ack(const std::string& from, const std::string& id, bool acked, Output* out) {
  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (it->id == id && upper(it->to) == upper(from)) {
      out->events.emplace_back("aprs_ack", Json::object({{"to", it->to}, {"id", it->id}, {"acked", acked}}));
      if (acked) ++aprs_acked_; else ++aprs_failed_;
      pending_.erase(it);
      return;
    }
  }
}

Ax25Carrier::Rx Ax25Carrier::on_frame(ByteView bytes, uint64_t now, Bytes* mesh, InteropText* text, Output* out) {
  ax25::Frame f;
  if (!ax25::decode(bytes, &f, cfg_.max_info > 256 ? cfg_.max_info : 256)) return Rx::Bad;
  if (!f.is_ui() || f.pid != ax25::kPidNoLayer3) {
    ++other_frames_;
    return Rx::Ignored;
  }
  if (f.dst.call == kMeshDestCall && f.dst.ssid == 0) {
    if (f.src == cfg_.mycall && sent_recently(frame_key(f.dst, f.src, ByteView(f.info)))) {
      // our own frame repeated by a digipeater (or heard back acoustically)
      ++own_echoes_;
      return Rx::Ignored;
    }
    if (f.info.empty()) return Rx::Bad;
    *mesh = std::move(f.info);
    return Rx::Mesh;
  }
  if (!cfg_.aprs) {
    ++other_frames_;
    return Rx::Ignored;
  }
  if (f.src == cfg_.mycall && sent_recently(frame_key(f.dst, f.src, ByteView(f.info)))) {
    ++own_echoes_;
    return Rx::Ignored;
  }
  std::string from = f.src.to_string();
  std::string key = from + ">" + ByteView(f.info).to_string();
  bool dup = seen_recent(crc32(ByteView(std::string_view(key))), now);
  aprs::Packet p = aprs::parse(f);
  std::string me = upper(cfg_.mycall.to_string());
  switch (p.type) {
    case aprs::PacketType::Message: {
      const aprs::Message& m = p.message;
      bool to_me = upper(m.addressee) == me;
      if (m.kind != aprs::MessageKind::Text) {
        if (to_me) handle_ack(from, m.id, m.kind == aprs::MessageKind::Ack, out);
        return Rx::Handled;
      }
      if (to_me) {
        if (!m.id.empty() && cfg_.auto_ack) {
          std::string info;
          Bytes fr;
          if (aprs::build_ack(from, m.id, &info) && make_aprs(info, &fr)) {
            out->frames.push_back(std::move(fr));
            ++aprs_acks_sent_;
          }
        }
        if (!m.reply_ack.empty()) handle_ack(from, m.reply_ack, true, out);
      }
      if (dup) return Rx::Handled;
      ++aprs_heard_;
      text->from = printable_text(ByteView(std::string_view(from)), 9);
      // APRS message text is at most 67 characters and the addressee 9 (APRS101 ch. 14).
      text->to = trim(printable_text(ByteView(std::string_view(m.addressee)), 9));
      text->text = printable_text(ByteView(std::string_view(m.text)), aprs::kMaxMessageText);
      text->network = "aprs";
      return Rx::Interop;
    }
    case aprs::PacketType::Position: {
      if (dup) return Rx::Handled;
      ++aprs_heard_;
      std::string s = "Position " + fmt_coord(p.position.lat) + ", " + fmt_coord(p.position.lon);
      if (p.position.has_altitude) s += " alt " + std::to_string(p.position.altitude_ft) + " ft";
      if (p.position.has_course_speed)
        s += " course " + std::to_string(p.position.course_deg) + " speed " + std::to_string(p.position.speed_knots) + " kn";
      std::string c = printable_text(ByteView(std::string_view(p.position.comment)), 200);
      if (!c.empty()) s += " - " + c;
      text->from = printable_text(ByteView(std::string_view(from)), 9);
      text->to = "";
      text->text = s;
      text->network = "aprs";
      return Rx::Interop;
    }
    case aprs::PacketType::Status: {
      if (dup) return Rx::Handled;
      ++aprs_heard_;
      text->from = printable_text(ByteView(std::string_view(from)), 9);
      text->to = "";
      text->text = "Status: " + printable_text(ByteView(std::string_view(p.status)), 200);
      text->network = "aprs";
      return Rx::Interop;
    }
    default:
      ++aprs_ignored_;
      return Rx::Ignored;
  }
}

bool Ax25Carrier::send_text(const InteropText& msg, uint64_t now, Output* out) {
  if (!cfg_.aprs || msg.text.empty()) return false;
  if (!msg.network.empty() && msg.network != "aprs") return false;
  std::string to = upper(trim(msg.to));
  if (to.size() > 9 || to.find(':') != std::string::npos || to.find(' ') != std::string::npos) return false;
  // Split into parts of <= 67 bytes on character boundaries.
  std::vector<std::string> parts;
  std::string_view rest(msg.text);
  while (!rest.empty() && parts.size() < kMaxParts) {
    std::string_view chunk = utf8_truncate(rest, aprs::kMaxMessageText);
    if (chunk.empty()) chunk = rest.substr(0, 1);  // invalid UTF-8 head byte: still advance
    std::string clean = aprs::sanitize_message_text(chunk);
    if (!trim(clean).empty()) parts.push_back(clean);
    rest.remove_prefix(chunk.size());
  }
  if (parts.empty()) return false;
  if (!to.empty() && pending_.size() + parts.size() > kMaxPending) return false;
  std::vector<Bytes> frames;
  std::vector<Pending> pend;
  for (const std::string& part : parts) {
    std::string info;
    if (to.empty()) {
      if (!aprs::build_bulletin('0', part, &info)) return false;
    } else {
      next_id_ = next_id_ % 99999 + 1;
      std::string id = std::to_string(next_id_);
      if (!aprs::build_message(to, part, id, &info)) return false;
      Pending p;
      p.to = to;
      p.id = id;
      p.interval_ms = cfg_.retry_ms;
      p.next_ms = now + cfg_.retry_ms;
      p.tries_left = cfg_.retries;
      if (!make_aprs(info, &p.frame)) return false;
      pend.push_back(p);
    }
    Bytes fr;
    if (!make_aprs(info, &fr)) return false;
    frames.push_back(std::move(fr));
  }
  for (auto& f : frames) out->frames.push_back(std::move(f));
  for (auto& p : pend) pending_.push_back(std::move(p));
  return true;
}

void Ax25Carrier::tick(uint64_t now, Output* out) {
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->next_ms > now) { ++it; continue; }
    if (it->tries_left == 0) {
      out->events.emplace_back("aprs_ack", Json::object({{"to", it->to}, {"id", it->id}, {"acked", false}}));
      ++aprs_failed_;
      it = pending_.erase(it);
      continue;
    }
    --it->tries_left;
    out->frames.push_back(it->frame);
    it->interval_ms = it->interval_ms > 0x7FFFFFFFu ? it->interval_ms : it->interval_ms * 2;
    it->next_ms = now + it->interval_ms;
    ++it;
  }
}

Json Ax25Carrier::status() const {
  Json j = Json::object();
  j.set("callsign", cfg_.mycall.to_string());
  j.set("aprs", cfg_.aprs).set("aprs_heard", aprs_heard_).set("aprs_ignored", aprs_ignored_);
  j.set("aprs_acks_sent", aprs_acks_sent_).set("aprs_acked", aprs_acked_).set("aprs_failed", aprs_failed_);
  j.set("aprs_outstanding", static_cast<uint64_t>(pending_.size()));
  j.set("own_echoes", own_echoes_).set("other_frames", other_frames_);
  return j;
}

}  // namespace link
}  // namespace ember
