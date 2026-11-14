// EmberMesh core / link — "audio": soft modem over the platform audio endpoint (radio audio via
// cable / interface, or acoustic coupling). See modem_link.h for the framing per modem.
//
// Transmit sequence (CSMA, p-persistence):
//   queue non-empty -> wait while the receive modem reports DCD -> every slot_ms draw a random byte;
//   send if it is <= persistence -> link_event("ptt",{"on":true}) -> next pull_audio() calls get:
//   optional VOX wake-up tone, modem preamble (txdelay), the burst's frames, tail (txtail) ->
//   link_event("ptt",{"on":false}) right after the last sample has been handed out.
// pull_audio() returns exactly `max` samples while a transmission is in progress, less only on
// the last call of a transmission, and 0 when idle. RX audio is ignored while transmitting.
#include <cmath>
#include <memory>

#include "link/drivers.h"
#include "link/modem_link.h"

namespace ember {
namespace link {
namespace {

constexpr uint64_t kKeyTimeoutMs = 2000;  // ptt on but nobody pulls audio

// Presets (link/README.md "Handheld radios"): each fills these keys unless the caller gave them.
struct PresetValue {
  const char* key;
  const char* text;  // for string fields, else nullptr
  double num;        // numbers
  int flag;          // bools: 0/1, -1 = not a bool
};
struct AudioPreset {
  const char* id;
  const char* label;
  std::vector<PresetValue> values;
};

const std::vector<AudioPreset>& audio_presets() {
  static const std::vector<AudioPreset> kPresets = {
      {"custom", "Custom (set everything yourself)", {}},
      {"handheld_vox", "Handheld radio keyed by its own VOX (audio cable)",
       {{"modem", "afsk1200", 0, -1}, {"vox_tone_ms", nullptr, 400, -1}, {"vox_tone_hz", nullptr, 1200, -1},
        {"txdelay_ms", nullptr, 600, -1}, {"txtail_ms", nullptr, 150, -1}, {"tx_level", nullptr, 0.35, -1},
        {"csma", nullptr, 0, 1}, {"persistence", nullptr, 128, -1}, {"slottime_ms", nullptr, 100, -1},
        {"max_frames_per_tx", nullptr, 8, -1}}},
      {"handheld_ptt", "Handheld radio, PTT keyed by a serial line or interface (AIOC, Digirig)",
       {{"modem", "afsk1200", 0, -1}, {"vox_tone_ms", nullptr, 0, -1}, {"txdelay_ms", nullptr, 450, -1},
        {"txtail_ms", nullptr, 50, -1}, {"tx_level", nullptr, 0.4, -1}, {"csma", nullptr, 0, 1},
        {"persistence", nullptr, 128, -1}, {"slottime_ms", nullptr, 100, -1}, {"max_frames_per_tx", nullptr, 6, -1}}},
      {"acoustic", "No cable: device speaker/microphone held to the radio, PTT by hand",
       {{"modem", "robust125", 0, -1}, {"vox_tone_ms", nullptr, 0, -1}, {"txdelay_ms", nullptr, 1500, -1},
        {"txtail_ms", nullptr, 400, -1}, {"tx_level", nullptr, 0.8, -1}, {"csma", nullptr, 0, 1},
        {"persistence", nullptr, 128, -1}, {"slottime_ms", nullptr, 200, -1}, {"max_frames_per_tx", nullptr, 2, -1}}},
      {"sound_card", "Mobile / base radio with a data port or USB sound card",
       {{"modem", "afsk1200", 0, -1}, {"vox_tone_ms", nullptr, 0, -1}, {"txdelay_ms", nullptr, 300, -1},
        {"txtail_ms", nullptr, 50, -1}, {"tx_level", nullptr, 0.3, -1}, {"csma", nullptr, 0, 1},
        {"persistence", nullptr, 63, -1}, {"slottime_ms", nullptr, 100, -1}, {"max_frames_per_tx", nullptr, 4, -1}}},
  };
  return kPresets;
}

Schema audio_schema() {
  Schema s;
  std::vector<FieldOption> opts;
  for (const AudioPreset& p : audio_presets()) opts.push_back({p.id, p.label});
  s.push_back(select("preset", "Radio setup", "custom", opts,
                     "How the device is connected to the radio. A preset picks the modem, lead-in, tail, level and "
                     "channel-access settings; any field you set yourself wins."));
  append_modem_fields(s, true);
  return s;
}

// Fills preset values into `cfg` for keys the caller did not give (null counts as not given).
void apply_audio_preset(const Json& given, Json* cfg) {
  std::string id = cfg->get_string("preset", "custom");
  for (const AudioPreset& p : audio_presets()) {
    if (id != p.id) continue;
    for (const PresetValue& v : p.values) {
      const Json* g = given.find(v.key);
      if (g && !g->is_null()) continue;
      if (v.text) cfg->set(v.key, v.text);
      else if (v.flag >= 0) cfg->set(v.key, v.flag != 0);
      else if (v.num == std::floor(v.num)) cfg->set(v.key, static_cast<int64_t>(v.num));
      else cfg->set(v.key, v.num);
    }
  }
}

class AudioLink final : public ModemLinkBase {
 public:
  explicit AudioLink(LinkHost& h) : ModemLinkBase(h) {}
  ~AudioLink() override {
    if (tx_ != Tx::Idle) event("ptt", Json::object({{"on", false}}));
  }

  bool configure(const Json& c, std::string* error) {
    if (!configure_modem(c, true, error)) return false;
    preset_ = c.get_string("preset", "custom");
    csma_ = c.get_bool("csma");
    persistence_ = static_cast<uint8_t>(c.get_int("persistence"));
    slot_ms_ = static_cast<uint32_t>(c.get_int("slottime_ms"));
    vox_ms_ = static_cast<uint32_t>(c.get_int("vox_tone_ms"));
    vox_hz_ = static_cast<double>(c.get_int("vox_tone_hz"));
    tx_level_ = c.get_double("tx_level");
    max_frames_ = static_cast<size_t>(c.get_int("max_frames_per_tx"));
    repeat_ = static_cast<size_t>(c.get_int("repeat"));
    size_t qf = static_cast<size_t>(c.get_int("queue_frames"));
    q_.set_limits(qf, qf * (caps_.mtu + 64u));
    txm_.reset();
    tx_rate_ = 0;
    return true;
  }

  const char* type() const override { return "audio"; }
  bool channel_busy() const override { return tx_ != Tx::Idle || rx_dcd(host_.now_ms()); }

  bool send(ByteView frame, uint8_t priority) override {
    if (!frames_mode()) { ++st_.tx_refused; return false; }
    if (no_callsign()) {
      last_error_ = "set your callsign before transmitting on amateur bands";
      ++st_.tx_refused;
      return false;
    }
    if (!enqueue(frame, priority, caps_.mtu)) return false;
    maybe_start(now());
    return true;
  }

  bool send_interop_text(const InteropText& msg) override {
    if (text_mode()) {
      if (msg.text.empty() || text_out_.size() >= 8) return false;
      if (!msg.network.empty() && msg.network != modem_name_) return false;
      text_out_.push_back(std::string(utf8_truncate(msg.text, 500)));
      ++st_.interop_tx;
      maybe_start(now());
      return true;
    }
    if (!ax25_ || no_callsign()) return false;
    Ax25Carrier::Output out;
    if (!ax_.send_text(msg, now(), &out)) return false;
    ++st_.interop_tx;
    ax25_output(out);
    return true;
  }

  void on_io_state(bool connected) override {
    if (!connected) {
      abort_tx("audio device closed");
      set_state(LinkState::Down);
    } else {
      set_state(LinkState::Up);
    }
  }

  void on_audio(const int16_t* samples, size_t count, uint32_t rate) override {
    if (!samples || count == 0) return;
    if (rate < 8000 || rate > 192000) {
      last_error_ = "unsupported receive sample rate";
      return;
    }
    if (state_ != LinkState::Up) set_state(LinkState::Up);
    if (tx_ != Tx::Idle) return;  // half duplex: do not decode ourselves
    last_audio_ms_ = now();
    rx_pcm16(samples, count, rate);
  }

  size_t pull_audio(int16_t* out, size_t max, uint32_t rate) override {
    if (!out || max == 0) return 0;
    if (rate < 8000 || rate > 192000) return 0;
    if (tx_ == Tx::Idle) maybe_start(now());
    if (tx_ == Tx::Keyed) load_burst(rate);
    if (tx_ != Tx::Sending) return 0;
    size_t n = 0;
    const double amp = tx_level_ * 32767.0 * 0.9;
    while (n < max && vox_left_ > 0) {
      out[n++] = static_cast<int16_t>(amp * std::sin(vox_phase_));
      vox_phase_ += 2.0 * 3.14159265358979323846 * vox_hz_ / rate;
      if (vox_phase_ > 6.283185307179586) vox_phase_ -= 6.283185307179586;
      --vox_left_;
    }
    if (n < max) n += txm_->pull_pcm16(out + n, max - n);
    tx_samples_ += n;
    if (vox_left_ == 0 && !txm_->tx_pending()) {
      finish_tx(rate);
      return n;
    }
    for (size_t i = n; i < max; ++i) out[i] = 0;  // contract: exactly max while transmitting
    return max;
  }

  void tick(uint64_t t) override {
    tick_text(t);
    Ax25Carrier::Output out;
    ax_.tick(t, &out);
    ax25_output(out);
    if (tx_ == Tx::Keyed && t >= key_ms_ + kKeyTimeoutMs) abort_tx("no audio output: the platform did not pull samples");
    maybe_start(t);
  }

  std::string status_json() const override {
    Json j = base_status();
    add_meter_status(j, tx_ != Tx::Idle);
    j.set("modem_status", modem_status());
    j.set("transmitting", tx_ != Tx::Idle).set("ptt", tx_ != Tx::Idle).set("tx_rate", tx_rate_);
    j.set("transmissions", transmissions_).set("csma_deferrals", deferrals_).set("preset", preset_);
    j.set("caps", caps_json(caps_));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(audio_schema(), cfg, &c, error)) return false;
    Json given;
    if (!Json::parse(cfg, given) || !given.is_object()) given = Json::object();
    apply_audio_preset(given, &c);
    AudioLink tmp(host_);
    if (!tmp.configure(c, error)) return false;
    abort_tx("reconfigured");
    return configure(c, error);
  }

 protected:
  void ax25_output(Ax25Carrier::Output& out) override {
    for (auto& f : out.frames) aprs_out_.push_back(std::move(f));
    while (aprs_out_.size() > 16) aprs_out_.pop_front();
    for (auto& e : out.events) event(e.first.c_str(), e.second);
    out.frames.clear();
    out.events.clear();
  }

 private:
  enum class Tx : uint8_t { Idle, Keyed, Sending };

  // DCD from the receive modem; stale if the platform stopped delivering audio (no input = no
  // evidence of a busy channel, and a latched detector must not block transmissions forever).
  bool rx_dcd(uint64_t t) const { return rxm_ && rxm_->dcd() && t < last_audio_ms_ + 500; }

  bool no_callsign() const { return amateur_ && ax25_ && ax_.config().mycall.call == "N0CALL"; }

  bool have_work() const { return !q_.empty() || !aprs_out_.empty() || !text_out_.empty(); }

  void maybe_start(uint64_t t) {
    if (state_ != LinkState::Up || tx_ != Tx::Idle || !have_work()) return;
    if (t < next_slot_) return;
    if (csma_) {
      if (rx_dcd(t)) {
        ++deferrals_;
        next_slot_ = t + slot_ms_;
        return;
      }
      if (random_u8() > persistence_) {
        ++deferrals_;
        next_slot_ = t + slot_ms_;
        return;
      }
    }
    // Build the burst.
    burst_.clear();
    burst_text_.clear();
    size_t est_bytes = 0;
    while (!aprs_out_.empty() && burst_.size() < max_frames_) {
      est_bytes += aprs_out_.front().size();
      burst_.push_back(std::move(aprs_out_.front()));
      aprs_out_.pop_front();
    }
    while (!text_out_.empty()) {
      burst_text_.push_back(std::move(text_out_.front()));
      text_out_.pop_front();
    }
    while (burst_.size() < max_frames_ && !q_.empty()) {
      uint32_t est = static_cast<uint32_t>(airtime_ms(est_bytes + q_.peek()->size() + 20, caps_.bitrate_bps) +
                                           opts_.txdelay_ms);
      if (caps_.duty_cycle_permille < 1000 && !duty_.allows(t, est * static_cast<uint32_t>(repeat_),
                                                              caps_.duty_cycle_permille))
        break;
      Bytes f;
      q_.pop(&f);
      ++st_.tx_frames;
      st_.tx_bytes += f.size();
      Bytes w;
      if (ax25_) {
        if (!ax_.wrap(ByteView(f), &w)) continue;
      } else {
        w = std::move(f);
      }
      est_bytes += w.size();
      burst_.push_back(std::move(w));
    }
    if (burst_.empty() && burst_text_.empty()) {
      next_slot_ = t + 1000;  // duty-cycle budget exhausted: look again later
      return;
    }
    tx_ = Tx::Keyed;
    key_ms_ = t;
    event("ptt", Json::object({{"on", true}}));
  }

  void load_burst(uint32_t rate) {
    if (!txm_ || rate != tx_rate_) {
      std::string err;
      txm_ = make_modem(rate, &err);
      tx_rate_ = rate;
      if (!txm_) {
        abort_tx(("transmit: " + err).c_str());
        return;
      }
    }
    bool any = false;
    for (size_t r = 0; r < repeat_; ++r)
      for (const Bytes& b : burst_) any |= txm_->send_frame(ByteView(b));
    for (const std::string& s : burst_text_) any |= txm_->send_text(s);
    burst_.clear();
    burst_text_.clear();
    if (!any) {
      abort_tx("modem refused the frame");
      return;
    }
    vox_left_ = static_cast<size_t>(static_cast<uint64_t>(vox_ms_) * rate / 1000);
    vox_phase_ = 0;
    tx_samples_ = 0;
    tx_ = Tx::Sending;
  }

  void finish_tx(uint32_t rate) {
    uint64_t ms = rate ? tx_samples_ * 1000 / rate : 0;
    st_.airtime_ms += ms;
    if (caps_.duty_cycle_permille < 1000) duty_.add(now(), static_cast<uint32_t>(ms));
    ++transmissions_;
    tx_ = Tx::Idle;
    next_slot_ = now() + slot_ms_;  // let the channel settle / others get a turn
    event("ptt", Json::object({{"on", false}}));
  }

  void abort_tx(const char* why) {
    if (tx_ == Tx::Idle) return;
    if (txm_) txm_->tx_abort();
    burst_.clear();
    burst_text_.clear();
    tx_ = Tx::Idle;
    if (why) last_error_ = why;
    event("ptt", Json::object({{"on", false}}));
  }

  std::string preset_ = "custom";
  bool csma_ = true;
  uint8_t persistence_ = 63;
  uint32_t slot_ms_ = 100;
  uint32_t vox_ms_ = 0;
  double vox_hz_ = 1000, tx_level_ = 0.5, vox_phase_ = 0;
  size_t vox_left_ = 0;
  size_t max_frames_ = 4, repeat_ = 1;
  Tx tx_ = Tx::Idle;
  uint64_t key_ms_ = 0, next_slot_ = 0, last_audio_ms_ = 0;
  std::unique_ptr<modem::Modem> txm_;
  uint32_t tx_rate_ = 0;
  std::vector<Bytes> burst_;
  std::vector<std::string> burst_text_;
  std::deque<Bytes> aprs_out_;
  std::deque<std::string> text_out_;
  uint64_t tx_samples_ = 0, transmissions_ = 0, deferrals_ = 0;
  DutyCycle duty_;
};

std::unique_ptr<Link> create_audio(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<AudioLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& audio_type() {
  static const LinkTypeInfo t = {
      "audio",
      "Sound-card modem (radio audio)",
      "Turns mesh packets into sound for a radio connected by audio cable, a radio interface, or a phone "
      "held to the radio. Includes AFSK 1200 packet (works with APRS and KISS TNC users), robust "
      "error-corrected modes, and Morse/RTTY/PSK31/DTMF text.",
      {"audio", "audio_pipe"},
      audio_schema,
      create_audio,
      apply_audio_preset,
  };
  return t;
}

}  // namespace link
}  // namespace ember
