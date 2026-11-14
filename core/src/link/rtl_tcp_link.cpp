// EmberMesh core / link — "rtl_tcp": receive-only SDR monitor. The endpoint is a TCP connection
// to an rtl_tcp server (RTL-SDR dongle). The driver:
//   * skips the 12-byte server header ("RTL0", tuner type u32be, gain count u32be). The header may
//     arrive split across chunks or after stale bytes; if it never arrives (endpoint strips it,
//     connection joined mid-stream) the driver assumes raw IQ after kHeaderWaitMs / kSearchMax
//     bytes and goes Up anyway (status.header_missing),
//   * goes to Error only for a server that is clearly something else (HTTP/SSH banner), and then
//     retries on its own every kRetryMs on the same connection (re-sends tuning, re-reads),
//   * sends the 5-byte tuner commands itself (cmd u8 + param u32be): 0x02 sample rate,
//     0x01 centre frequency, 0x05 ppm correction, 0x03 gain mode (0 auto / 1 manual),
//     0x04 gain (tenths of dB), 0x08 RTL AGC off,
//   * NBFM-demodulates the unsigned 8-bit IQ (dsp/nbfm.h), tuned `tune_offset_hz` away from the
//     DC spike, and feeds the audio to the selected modem (same framing as the audio driver).
// It never transmits: caps().mtu == 0 and send() always returns false.
#include <cstring>
#include <memory>

#include "dsp/nbfm.h"
#include "link/drivers.h"
#include "link/modem_link.h"

namespace ember {
namespace link {
namespace {

constexpr uint32_t kAudioRate = 24000;
constexpr size_t kHeader = 12;
constexpr size_t kSearchMax = 64 * 1024;   // bytes buffered while looking for "RTL0"
constexpr uint64_t kHeaderWaitMs = 2000;   // then assume a headerless IQ stream
constexpr uint64_t kRetryMs = 5000;        // Error -> Connecting again

Schema rtl_schema() {
  Schema s = {
      num("frequency_hz", "Frequency", 144390000, 24000000, 1766000000,
          "Frequency to listen on, in Hz (144390000 = APRS in North America, 144800000 = APRS in Europe).", "Hz"),
      select("sample_rate", "SDR sample rate", "240000",
             {{"240000", "240 kHz (lowest CPU, recommended)"},
              {"1024000", "1.024 MHz"},
              {"2048000", "2.048 MHz"}},
             "How fast the dongle samples. 240 kHz is plenty for one channel and is easiest on phones.").adv(),
      num("tune_offset_hz", "Tuning offset", 50000, 0, 400000,
          "Tune the dongle this far below the channel to avoid its centre spike. 0 = tune directly.", "Hz").adv(),
      real("gain_db", "RF gain", 0, 0, 50, 0.1,
           "Receiver gain in dB; 0 = automatic. Raise it for weak signals, lower it near strong transmitters.", "dB"),
      num("ppm", "Frequency correction", 0, -200, 200,
          "Crystal error of your dongle in parts per million (find it with a known station).", "ppm").adv(),
      select("channel_bw", "Channel width", "12500",
             {{"12500", "12.5 kHz narrow FM (most radios)"}, {"25000", "25 kHz wide FM"}},
             "Match the transmitters you want to hear.").adv(),
  };
  append_modem_fields(s, false);
  return s;
}

void put_cmd(Bytes& out, uint8_t cmd, uint32_t v) {
  out.push_back(cmd);
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

const char* tuner_name(uint32_t t) {
  switch (t) {
    case 1: return "E4000";
    case 2: return "FC0012";
    case 3: return "FC0013";
    case 4: return "FC2580";
    case 5: return "R820T";
    case 6: return "R828D";
    default: return "unknown";
  }
}

class RtlTcpLink final : public ModemLinkBase {
 public:
  explicit RtlTcpLink(LinkHost& h) : ModemLinkBase(h) {}

  bool configure(const Json& c, std::string* error) {
    if (!configure_modem(c, false, error)) return false;
    caps_.mtu = 0;  // receive only
    caps_.cost = 255;
    freq_ = static_cast<uint32_t>(c.get_int("frequency_hz"));
    int64_t sr = 240000;
    parse_int(c.get_string("sample_rate"), &sr);
    rate_ = static_cast<uint32_t>(sr);
    offset_ = static_cast<uint32_t>(c.get_int("tune_offset_hz"));
    gain_ = c.get_double("gain_db");
    ppm_ = static_cast<int32_t>(c.get_int("ppm"));
    dsp::NbfmConfig n;
    n.iq_rate = rate_;
    n.audio_rate = kAudioRate;
    n.offset_hz = offset_;
    n.channel_bw_hz = c.get_string("channel_bw") == "25000" ? 25000 : 12500;
    n.deviation_hz = n.channel_bw_hz == 25000 ? 5000 : 3000;
    if (offset_ + n.channel_bw_hz / 2 >= rate_ / 2.0) {
      if (error) *error = "tune_offset_hz: too large for the sample rate";
      return false;
    }
    if (!demod_.init(n)) {
      if (error) *error = "SDR demodulator could not be set up for these settings";
      return false;
    }
    return true;
  }

  const char* type() const override { return "rtl_tcp"; }
  bool send(ByteView, uint8_t) override {
    ++st_.tx_refused;
    return false;
  }

  void on_io_state(bool connected) override {
    connected_ = connected;
    demod_.reset();
    if (!connected) {
      set_state(LinkState::Down);
      return;
    }
    start(now());
  }

  void on_io_bytes(ByteView d) override {
    if (d.empty()) return;
    st_.io_rx_bytes += d.size();
    if (state_ == LinkState::Error) return;  // tick() retries
    if (!have_header_) {
      // Look for the header without trusting chunk boundaries; keep at most kSearchMax bytes.
      size_t room = kSearchMax - pre_.size();
      size_t take = d.size() < room ? d.size() : room;
      pre_.insert(pre_.end(), d.begin(), d.begin() + take);
      d = d.sub(take);
      if (!scan_header()) {
        if (state_ == LinkState::Error) return;
        if (pre_.size() < kSearchMax) return;
        go_headerless();
      }
      // pre_ now holds IQ that followed the header (or the whole headerless prefix).
      Bytes iq;
      iq.swap(pre_);
      demodulate(ByteView(iq));
    }
    demodulate(d);
  }

  void tick(uint64_t t) override {
    if (connected_ && state_ == LinkState::Error && t >= error_at_ + kRetryMs) start(t);
    if (connected_ && state_ == LinkState::Connecting && !have_header_ && !pre_.empty() &&
        t >= connect_ms_ + kHeaderWaitMs) {
      go_headerless();
      Bytes iq;
      iq.swap(pre_);
      demodulate(ByteView(iq));
    }
    tick_text(t);
  }

  std::string status_json() const override {
    Json j = base_status();
    j.set("frequency_hz", freq_).set("sample_rate", rate_).set("tune_offset_hz", offset_);
    j.set("gain_db", gain_).set("ppm", ppm_).set("iq_bytes", iq_bytes_);
    if (have_header_ && !header_missing_) j.set("tuner", tuner_name(tuner_)).set("tuner_gains", gains_);
    j.set("header_missing", header_missing_).set("retries", retries_);
    j.set("channel_power", demod_.channel_power());
    add_meter_status(j, false);
    j.set("modem_status", modem_status());
    j.set("receive_only", true).set("caps", caps_json(caps_));
    return j.dump();
  }

  bool reconfigure(const std::string& cfg, std::string* error) override {
    Json c;
    if (!parse_config(rtl_schema(), cfg, &c, error)) return false;
    RtlTcpLink tmp(host_);
    if (!tmp.configure(c, error)) return false;
    configure(c, error);
    if (state_ == LinkState::Up || state_ == LinkState::Connecting) send_tuning();
    return true;
  }

 private:
  void start(uint64_t t) {
    pre_.clear();
    have_header_ = header_missing_ = false;
    demod_.reset();
    connect_ms_ = t;
    set_state(LinkState::Connecting);
    send_tuning();
  }

  // Finds "RTL0" + 8 bytes in pre_. On success parses it, leaves only the following bytes in pre_
  // and goes Up. Returns false while incomplete (or after setting Error for a foreign server).
  bool scan_header() {
    static const char kMagic[4] = {'R', 'T', 'L', '0'};
    size_t n = pre_.size();
    for (size_t i = 0; i + 4 <= n; ++i) {
      if (pre_[i] != 'R' || std::memcmp(pre_.data() + i, kMagic, 4) != 0) continue;
      if (n - i < kHeader) return false;  // rest of the header still to come
      ByteReader r{ByteView(pre_).sub(i, kHeader)};
      r.skip(4);
      tuner_ = r.u32be();
      gains_ = r.u32be();
      pre_.erase(pre_.begin(), pre_.begin() + static_cast<std::ptrdiff_t>(i + kHeader));
      have_header_ = true;
      set_state(LinkState::Up);
      last_error_.clear();
      return true;
    }
    // A text banner is a different service on that port: say so instead of demodulating it.
    for (const char* banner : {"HTTP/", "SSH-", "<!DOC", "<html"}) {
      size_t bl = std::strlen(banner);
      if (n >= bl && std::memcmp(pre_.data(), banner, bl) == 0) {
        set_state(LinkState::Error, "not an rtl_tcp server (bad header)");
        error_at_ = now();
        ++retries_;
        pre_.clear();
        return false;
      }
    }
    return false;
  }

  void go_headerless() {
    have_header_ = true;
    header_missing_ = true;
    set_state(LinkState::Up);
  }

  void demodulate(ByteView d) {
    // Process in bounded pieces so the audio scratch buffer stays small.
    const size_t kPiece = 16384;
    for (size_t off = 0; off < d.size(); off += kPiece) {
      ByteView p = d.sub(off, kPiece);
      audio_.clear();
      demod_.process_u8(p.data(), p.size(), audio_);
      iq_bytes_ += p.size();
      if (!audio_.empty()) rx_float(audio_.data(), audio_.size(), kAudioRate);
    }
  }

  void send_tuning() {
    Bytes b;
    put_cmd(b, 0x02, rate_);
    put_cmd(b, 0x01, freq_ - offset_);
    put_cmd(b, 0x05, static_cast<uint32_t>(ppm_));
    if (gain_ <= 0) {
      put_cmd(b, 0x03, 0);
    } else {
      put_cmd(b, 0x03, 1);
      put_cmd(b, 0x04, static_cast<uint32_t>(gain_ * 10 + 0.5));
    }
    put_cmd(b, 0x08, 0);
    io_write(ByteView(b));
  }

  uint32_t freq_ = 144390000, rate_ = 240000, offset_ = 50000;
  double gain_ = 0;
  int32_t ppm_ = 0;
  dsp::NbfmDemod demod_;
  Bytes pre_;  // bytes received before the header was found
  bool connected_ = false, have_header_ = false, header_missing_ = false;
  uint64_t connect_ms_ = 0, error_at_ = 0, retries_ = 0;
  uint32_t tuner_ = 0, gains_ = 0;
  uint64_t iq_bytes_ = 0;
  std::vector<float> audio_;
};

std::unique_ptr<Link> create_rtl(const Json& c, LinkHost& h, std::string* error) {
  auto l = std::make_unique<RtlTcpLink>(h);
  if (!l->configure(c, error)) return nullptr;
  return l;
}

}  // namespace

const LinkTypeInfo& rtl_tcp_type() {
  static const LinkTypeInfo t = {
      "rtl_tcp",
      "SDR receiver (rtl_tcp)",
      "Listen-only: a cheap RTL-SDR dongle running rtl_tcp monitors a frequency and decodes mesh packets, "
      "APRS or keyboard modes. It cannot transmit.",
      {"rtl_tcp", "tcp"},
      rtl_schema,
      create_rtl,
  };
  return t;
}

}  // namespace link
}  // namespace ember
