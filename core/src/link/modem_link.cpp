#include "link/modem_link.h"

#include <cmath>
#include <cstdio>

#include "ember/crc.h"

namespace ember {
namespace link {

namespace {

const char* modem_label(const std::string& n) {
  if (n == "afsk1200") return "AFSK 1200 - VHF/UHF packet, works with APRS gear";
  if (n == "afsk300") return "AFSK 300 - HF packet";
  if (n == "psk2400") return "PSK 2400 - fast, audio cable only";
  if (n == "psk4800") return "PSK 4800 - fastest, clean audio cable only";
  if (n == "robust250") return "Robust 250 - error-corrected, good audio";
  if (n == "robust125") return "Robust 125 - error-corrected";
  if (n == "robust62") return "Robust 62 - phone held to a radio, weak signals";
  if (n == "robust31") return "Robust 31 - weakest signals, very slow";
  if (n == "cw") return "Morse code (CW) - text only";
  if (n == "rtty") return "RTTY 45 baud - text only";
  if (n == "psk31") return "PSK31 - text only";
  if (n == "dtmf") return "DTMF touch tones - digits only";
  return nullptr;
}

bool is_hdlc(const std::string& n) { return n == "afsk1200" || n == "afsk300" || n == "psk2400" || n == "psk4800"; }

std::string fnum(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%g", v);
  return b;
}

}  // namespace

void append_modem_fields(Schema& s, bool tx) {
  std::vector<FieldOption> opts;
  for (const modem::ModemInfo& i : modem::modem_catalog()) {
    const char* l = modem_label(i.name);
    opts.push_back({i.name, l ? l : i.title});
  }
  s.push_back(select("modem", "Modem", "afsk1200", opts,
                     "How data is turned into sound. Everyone on the frequency must use the same modem. AFSK 1200 "
                     "is standard packet radio; Robust works through a phone held against a radio."));
  s.push_back(text("callsign", "Your callsign", "N0CALL",
                   "Your amateur radio callsign with optional -SSID (e.g. N0CALL-7). Required on amateur bands.", 12));
  s.push_back(select("licence", "Band / licence", "amateur",
                     {{"amateur", "Amateur radio (callsign, no encryption)"},
                      {"licence_free", "Licence-free or emergency use (encryption allowed)"}},
                     "Amateur bands forbid encryption and need your callsign on every packet."));
  s.push_back(boolean("aprs", "APRS messaging", true,
                      "With AFSK modems: show APRS messages, positions and status heard on the channel"
                      " and allow sending APRS messages."));
  s.push_back(text("modem_params", "Modem fine-tuning", "",
                   "Optional key=value pairs separated by commas, e.g. wpm=15 for CW or mark_hz=1600 for AFSK.", 200)
                  .adv());
  if (!tx) return;
  s.push_back(text("path", "Digipeater path", "",
                   "Comma-separated digipeaters for mesh packets (normally empty: the mesh relays by itself).", 80)
                  .adv());
  s.push_back(text("aprs_path", "APRS path", "WIDE1-1,WIDE2-1", "Digipeater path for APRS messages you send.", 80).adv());
  s.push_back(boolean("aprs_auto_ack", "Acknowledge APRS messages", true,
                      "Automatically confirm APRS messages addressed to your callsign.").adv());
  s.push_back(num("txdelay_ms", "TX delay", 300, 0, 5000,
                  "Time between keying the radio and the data. Increase if the far end misses the start of packets "
                  "(VOX and cheap handhelds need 300-600 ms).", "ms"));
  s.push_back(num("txtail_ms", "TX tail", 50, 0, 2000, "Extra time the radio stays keyed after the data.", "ms").adv());
  s.push_back(real("tx_level", "Transmit volume", 0.5, 0.01, 1.0, 0.01,
                   "Loudness of the generated audio (1 = full scale). Lower it if the radio's audio sounds distorted."));
  s.push_back(num("vox_tone_ms", "VOX wake-up tone", 0, 0, 2000,
                  "Play a tone this long before each transmission so a VOX radio keys up (0 = off).", "ms"));
  s.push_back(num("vox_tone_hz", "VOX tone pitch", 1000, 300, 3000, "Pitch of the wake-up tone.", "Hz").adv());
  s.push_back(boolean("csma", "Wait for a clear channel", true,
                      "Listen before transmitting and wait while someone else is sending (strongly recommended)."));
  s.push_back(num("persistence", "Persistence", 63, 1, 255,
                  "Chance (out of 255) of sending at each free slot; lower values reduce collisions on busy channels.")
                  .adv());
  s.push_back(num("slottime_ms", "Slot time", 100, 10, 2000, "Time between channel checks.", "ms").adv());
  s.push_back(num("max_frames_per_tx", "Packets per transmission", 4, 1, 16,
                  "How many queued packets are sent in one key-up.").adv());
  s.push_back(num("repeat", "Send each packet", 1, 1, 3,
                  "Transmit every packet this many times (1 = once). Repetition helps on very lossy paths; "
                  "duplicates are discarded by the receiver.").adv());
  s.push_back(num("mtu", "Largest frame", 0, 0, 2048, "Largest mesh frame per packet. 0 = automatic for the modem.",
                  "bytes").adv());
  s.push_back(num("duty_cycle_permille", "Duty-cycle limit", 1000, 1, 1000,
                  "Legal transmit time limit in thousandths (1000 = none).", "‰").adv());
  s.push_back(num("cost", "Routing cost", 0, 0, 255, "Lower is preferred. 0 = automatic from the modem speed.").adv());
  s.push_back(num("queue_frames", "Queue size", 32, 1, 256, "Frames waiting to be sent before new ones are refused.",
                  "frames").adv());
}

bool ModemLinkBase::configure_modem(const Json& c, bool tx, std::string* error) {
  auto fail = [&](const std::string& m) {
    if (error) *error = m;
    return false;
  };
  std::string name = c.get_string("modem", "afsk1200");
  modem::KeyValues kv;
  for (const std::string& p : split_list(c.get_string("modem_params"), ',')) {
    size_t eq = p.find('=');
    if (eq == std::string::npos || eq == 0) return fail("modem_params: expected key=value, got '" + p + "'");
    kv[trim(p.substr(0, eq))] = trim(p.substr(eq + 1));
  }
  amateur_ = c.get_string("licence", "amateur") == "amateur";
  bool ax25 = is_hdlc(name) || (name.compare(0, 6, "robust") == 0 && amateur_);
  if (tx) {
    kv["txdelay_ms"] = std::to_string(c.get_int("txdelay_ms", 300));
    kv["txtail_ms"] = std::to_string(c.get_int("txtail_ms", 50));
    kv["tx_level"] = fnum(c.get_double("tx_level", 0.5));
    kv["max_frames_per_tx"] = std::to_string(c.get_int("max_frames_per_tx", 4) * c.get_int("repeat", 1));
  }
  if (ax25 && !kv.count("min_frame")) kv["min_frame"] = "15";
  modem::ModemOptions o;
  std::string merr;
  if (!modem::parse_options(kv, &o, &merr)) return fail("modem_params: " + merr);
  auto probe = modem::create_modem(name, 8000, o, &merr);
  if (!probe) return fail("modem: " + merr);
  info_ = probe->info();
  modem_name_ = name;
  opts_ = o;
  ax25_ = ax25 && info_.frames;

  Ax25Config ac;
  if (!parse_callsign(c.get_string("callsign", "N0CALL"), &ac.mycall, error)) return false;
  if (!parse_path(c.get_string("path"), &ac.path, error)) return false;
  if (!parse_path(c.get_string("aprs_path", "WIDE1-1,WIDE2-1"), &ac.aprs_path, error)) return false;
  ac.aprs = c.get_bool("aprs", true) && (name == "afsk1200" || name == "afsk300");
  ac.auto_ack = tx && c.get_bool("aprs_auto_ack", true);
  ac.max_info = info_.max_frame;
  ac.amateur = amateur_;
  ax_.configure(ac);

  LinkCaps k;
  k.bitrate_bps = info_.bitrate_bps ? info_.bitrate_bps : 1;
  k.broadcast = true;
  k.half_duplex = true;
  k.reliable = false;
  k.encryption_allowed = !amateur_;
  k.requires_station_id = amateur_;
  size_t overhead = ax25_ ? ax_.overhead() : 0;
  if (!info_.frames) {
    k.mtu = 0;  // keyboard mode: interop text only
  } else {
    size_t room = info_.max_frame > overhead ? info_.max_frame - overhead : 0;
    size_t want = static_cast<size_t>(c.get_int("mtu", 0));
    if (want == 0) want = name.compare(0, 6, "robust") == 0 ? 128 : 256;
    if (want > room) {
      if (c.get_int("mtu", 0) != 0) return fail("mtu: this modem carries at most " + std::to_string(room) + " bytes");
      want = room;
    }
    if (want < 32) return fail("mtu: must be at least 32");
    k.mtu = static_cast<uint16_t>(want);
    // Robust modes: a false header (noise passing the CRC-8) claiming a long payload holds DCD busy
    // for the whole claimed airtime (minutes at robust31). Accept only lengths this channel uses:
    // mtu + framing overhead, at least the auto mtu (128) so stations with a smaller manual mtu
    // still hear the default. An explicit modem_params max_frame wins.
    if (name.compare(0, 6, "robust") == 0 && !kv.count("max_frame")) {
      size_t mf = (want > 128 ? want : 128) + overhead;
      if (mf < info_.max_frame) {
        o.max_frame = static_cast<uint16_t>(mf);
        probe = modem::create_modem(name, 8000, o, &merr);
        if (!probe) return fail("modem: " + merr);
        info_ = probe->info();
        opts_ = o;
        ac.max_info = info_.max_frame;
        ax_.configure(ac);
      }
    }
  }
  k.duty_cycle_permille = static_cast<uint16_t>(c.get_int("duty_cycle_permille", 1000));
  int64_t cost = c.get_int("cost", 0);
  uint32_t b = k.bitrate_bps;
  k.cost = static_cast<uint8_t>(cost ? cost : b >= 4800 ? 30 : b >= 2400 ? 35 : b >= 1200 ? 40 : b >= 250 ? 60 : 80);
  caps_ = k;
  rxm_.reset();  // recreated with the new options at the next audio block
  rx_rate_ = 0;
  return true;
}

std::unique_ptr<modem::Modem> ModemLinkBase::make_modem(uint32_t rate, std::string* error) const {
  return modem::create_modem(modem_name_, rate, opts_, error);
}

void ModemLinkBase::rx_pcm16(const int16_t* x, size_t n, uint32_t rate) {
  if (!x || n == 0) return;
  if (!rxm_ || rate != rx_rate_) {
    std::string err;
    rxm_ = make_modem(rate, &err);
    rx_rate_ = rate;
    meter_.init(rate);
    if (!rxm_) {
      last_error_ = "receive: " + err;
      return;
    }
  }
  have_rx_audio_ = true;
  rx_audio_ms_ = now();
  if (!rxm_) return;
  rx_samples_ += n;
  for (size_t i = 0; i < n; ++i) meter_.push(x[i] / 32768.0f);
  rxm_->rx_pcm16(x, n);
  drain_rx();
}

void ModemLinkBase::rx_float(const float* x, size_t n, uint32_t rate) {
  if (!x || n == 0) return;
  if (!rxm_ || rate != rx_rate_) {
    std::string err;
    rxm_ = make_modem(rate, &err);
    rx_rate_ = rate;
    meter_.init(rate);
    if (!rxm_) {
      last_error_ = "receive: " + err;
      return;
    }
  }
  have_rx_audio_ = true;
  rx_audio_ms_ = now();
  if (!rxm_) return;
  rx_samples_ += n;
  meter_.push(x, n);
  rxm_->rx(x, n);
  drain_rx();
}

void ModemLinkBase::drain_rx() {
  modem::RxItem it;
  uint64_t t = now();
  while (rxm_ && rxm_->pop(&it)) {
    if (it.q.has_snr) {
      has_snr_ = true;
      last_snr_ = it.q.snr_db;
    }
    if (it.kind == modem::RxKind::Text) {
      text_acc_ += it.text;
      text_last_ms_ = t;
      if (text_acc_.find('\n') != std::string::npos || text_acc_.size() >= 200) tick_text(t + 1000000);
      continue;
    }
    RxMeta m;
    m.has_snr = it.q.has_snr;
    m.snr_db = it.q.snr_db;
    m.corrected_errors = static_cast<uint8_t>(it.q.corrected > 255 ? 255 : it.q.corrected);
    Bytes mesh;
    if (ax25_) {
      InteropText txt;
      Ax25Carrier::Output out;
      switch (ax_.on_frame(ByteView(it.data), t, &mesh, &txt, &out)) {
        case Ax25Carrier::Rx::Mesh: break;
        case Ax25Carrier::Rx::Interop: interop(txt); break;
        case Ax25Carrier::Rx::Bad: ++st_.rx_errors; break;
        default: break;
      }
      ax25_output(out);
      if (mesh.empty()) continue;
    } else {
      mesh = std::move(it.data);
      if (mesh.empty()) continue;
    }
    // drop repeats (the "repeat" option or digipeated copies) seen within 3 s
    uint32_t h = crc32(ByteView(mesh));
    while (!recent_.empty() && (recent_.front().second + 3000 <= t || recent_.size() > 32)) recent_.pop_front();
    bool dup = false;
    for (const auto& r : recent_) dup |= r.first == h;
    if (dup) {
      ++dup_frames_;
      continue;
    }
    recent_.emplace_back(h, t);
    deliver(ByteView(mesh), m);
  }
}

void ModemLinkBase::tick_text(uint64_t t) {
  if (text_acc_.empty() || (t < text_last_ms_ + 3000 && text_acc_.find('\n') == std::string::npos &&
                            text_acc_.size() < 200))
    return;
  std::string s = printable_text(ByteView(std::string_view(text_acc_)), 400);
  text_acc_.clear();
  s = trim(s);
  if (s.empty()) return;
  InteropText m;
  m.text = s;
  m.network = modem_name_;
  interop(m);
}

void ModemLinkBase::ax25_output(Ax25Carrier::Output& out) { (void)out; }

const char* ModemLinkBase::level_hint() const {
  if (!rxm_ || rx_samples_ == 0 || !have_rx_audio_) return "no_audio";
  uint64_t t = now();
  if (t > rx_audio_ms_ && t - rx_audio_ms_ > 2000) return "no_audio";
  if (meter_.clip_fraction() > 0.001f) return "clipping";
  if (meter_.peak_dbfs() > -1.0f) return "too_loud";
  if (meter_.rms_dbfs() < -50.0f) return "too_quiet";
  return "ok";
}

namespace {
// dB value for the meter: rounded to 0.1, floor -120 (also for -inf / NaN).
double meter_db(float v) {
  if (!(v > -120.0f)) return -120.0;
  if (v > 20.0f) v = 20.0f;
  return std::round(static_cast<double>(v) * 10.0) / 10.0;
}
double round1(double v) { return std::isfinite(v) ? std::round(v * 10.0) / 10.0 : 0.0; }
}  // namespace

void ModemLinkBase::add_meter_status(Json& j, bool tx_active) const {
  const bool live = rxm_ && rx_samples_ > 0;
  j.set("rx_level_dbfs", live ? meter_db(meter_.rms_dbfs()) : -120.0);
  j.set("rx_peak_dbfs", live ? meter_db(meter_.peak_dbfs()) : -120.0);
  double clip = live ? round1(static_cast<double>(meter_.clip_fraction()) * 100.0) : 0.0;
  if (clip < 0) clip = 0;
  if (clip > 100) clip = 100;
  j.set("clip_percent", clip).set("clipping", live && meter_.clip_fraction() > 0.001f);
  j.set("snr_db", has_snr_ ? Json(round1(static_cast<double>(last_snr_))) : Json());
  uint64_t t = now();
  bool fresh = have_rx_audio_ && (t <= rx_audio_ms_ || t - rx_audio_ms_ <= 500);
  j.set("dcd", fresh && rxm_ && rxm_->dcd());
  j.set("tx_active", tx_active);
  uint64_t ok = 0, bad = st_.rx_errors;
  if (rxm_) {
    ok = rxm_->stats().rx_frames;
    bad += rxm_->stats().rx_bad;
  }
  j.set("frames_ok", ok).set("frames_bad", bad);
  j.set("rx_audio_age_ms", have_rx_audio_ ? Json(t > rx_audio_ms_ ? t - rx_audio_ms_ : uint64_t{0}) : Json());
  j.set("last_error", last_error_);
  j.set("level_hint", level_hint());
}

Json ModemLinkBase::modem_status() const {
  Json j = Json::object();
  j.set("modem", modem_name_).set("modem_bitrate_bps", info_.bitrate_bps);
  if (info_.frames) j.set("max_frame", info_.max_frame);
  j.set("framing", ax25_ ? "ax25" : info_.frames ? "raw" : "text");
  j.set("rx_rate", rx_rate_);
  if (rxm_) {
    j.set("rx_level_dbfs", meter_.rms_dbfs()).set("rx_peak_dbfs", meter_.peak_dbfs());
    j.set("rx_clip_fraction", meter_.clip_fraction()).set("dcd", rxm_->dcd());
    const modem::ModemStats& s = rxm_->stats();
    j.set("modem_rx_frames", s.rx_frames).set("modem_rx_bad", s.rx_bad).set("modem_rx_chars", s.rx_chars);
  }
  j.set("level_hint", level_hint());
  if (has_snr_) j.set("last_snr_db", last_snr_);
  j.set("duplicates", dup_frames_);
  if (ax25_ || info_.frames) j.set("ax25", ax_.status());
  return j;
}

}  // namespace link
}  // namespace ember
