// EmberMesh core / modem — software modems for any radio's audio path.
//
// A Modem turns audio into frames/text and back. It is the piece a link driver ("audio" link in
// core/src/link/) wraps behind ember::Link:
//
//   Link::on_audio(pcm, n, rate)        ->  modem->rx_pcm16(pcm, n)        (rate fixed at creation)
//   Link::pull_audio(out, max, rate)    ->  modem->pull_pcm16(out, max)    (pad the rest with 0 while
//                                                                            modem->tx_active())
//   Link::channel_busy()                ->  modem->channel_busy()          (DCD || transmitting)
//   Link::send(frame)                   ->  modem->send_frame(frame)
//   Link::send_interop_text(msg)        ->  modem->send_text(msg.text)      (CW / RTTY / PSK31 / DTMF)
//   received frames / text              <-  modem->pop(&item) after each rx call (or set_rx_callback)
//
// If the device sample rate changes, create a new modem (creation is cheap).
//
// Every modem accepts any device sample rate in [8000, 192000] Hz and internally resamples to a
// fixed processing rate chosen for that waveform (info().internal_rate), so CPU cost does not grow
// with 48 kHz sound cards. TX audio is generated at the internal rate and resampled back.
//
// Modems (see modem_catalog() and each header for the exact waveform):
//   afsk1200   Bell 202 AFSK 1200 bd, 1200/2200 Hz, AX.25 bit-level HDLC (NRZI, stuffing, FCS)  afsk.h
//   afsk300    HF packet AFSK 300 bd, 1600/1800 Hz (200 Hz shift), HDLC                          afsk.h
//   psk2400    DQPSK 1200 Bd on 1800 Hz, HDLC, scrambled: 2400 bit/s for clean cables          dpsk.h
//   psk4800    D8PSK 1600 Bd on 1800 Hz, HDLC, scrambled: 4800 bit/s for clean cables          dpsk.h
//   robust31 / robust62 / robust125 / robust250 (alias "robust" = robust62)
//              Ember robust MFSK-16 + K=7 r=1/2 Viterbi + interleaving + Costas sync, for acoustic
//              coupling and weak / HF signals                                                    robust.h
//   dtmf       DTMF digits as text                                                               dtmf.h
//   cw         Morse keyer (WPM + Farnsworth) and adaptive-speed decoder                         cw.h
//   psk31      BPSK31 varicode keyboard mode                                                     psk31.h
//   rtty       45.45 Bd ITA2/Baudot, 170 Hz shift                                                rtty.h
//
// Threading: none. All calls from one thread. No exceptions, no RTTI, no OS calls.
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "dsp/level.h"
#include "dsp/resampler.h"
#include "ember/bytes.h"

namespace ember {
namespace modem {

using KeyValues = std::map<std::string, std::string>;

// Options common to all modems. Mode-specific settings go in `params` (string key/value; each
// modem's header lists its keys). Unknown keys are rejected by create_modem() to catch typos.
struct ModemOptions {
  // Preamble before the first frame of a transmission: lets slow PTT / VOX key up and receivers
  // settle (HDLC flags, sync tones, PSK reversals, CW/DTMF silence). Clamped to [0, 5000].
  uint32_t txdelay_ms = 300;
  // Trailer after the last frame (flags / idle tones), keeps VOX from dropping early. [0, 2000].
  uint32_t txtail_ms = 30;
  // Peak amplitude of generated audio, (0, 1] (at the internal rate; resampling to the device rate
  // can overshoot by up to ~5%, output is always clamped to +-1).
  float tx_level = 0.5f;
  // Largest frame accepted by send_frame() and by the receiver; 0 = modem default (info().max_frame).
  uint16_t max_frame = 0;
  // Smallest frame the receiver reports (bytes, excluding FCS/CRC). Raising it (e.g. to 15 for
  // AX.25) further reduces the already small chance of false frames from noise.
  uint16_t min_frame = 1;
  // Frames sent back-to-back in one transmission (one preamble). [1, 64].
  uint8_t max_frames_per_tx = 8;
  size_t rx_queue_max = 64;   // oldest items dropped beyond this
  size_t tx_queue_max = 32;   // send_*() returns false beyond this
  KeyValues params;
};

// Builds ModemOptions from a flat key/value map (e.g. a JSON object flattened by the link driver).
// Recognised common keys: txdelay_ms, txtail_ms, tx_level, max_frame, min_frame, max_frames_per_tx,
// rx_queue_max, tx_queue_max. Everything else is copied into `params`. Returns false + *error on
// unparsable or out-of-range values.
bool parse_options(const KeyValues& kv, ModemOptions* out, std::string* error);

// --- param helpers (used by modems; exposed for link drivers and tests) --------------------------
bool param_double(const KeyValues& p, const char* key, double def, double lo, double hi, double* out, std::string* error);
bool param_bool(const KeyValues& p, const char* key, bool def, bool* out, std::string* error);
// Returns false + *error if `p` holds a key not in `allowed` (nullptr-terminated list).
bool check_keys(const KeyValues& p, const char* const* allowed, std::string* error);
// Strict number parse: optional sign, digits, optional fraction / exponent, nothing else.
bool parse_number(std::string_view s, double* out);

enum class RxKind : uint8_t { Frame = 0, Text = 1 };

struct RxQuality {
  bool has_snr = false;
  // Estimated SNR in dB. Data modems: see each header for the definition ("eye SNR" for HDLC
  // modems, channel SNR in 3 kHz for the robust modem).
  float snr_db = 0;
  float level_dbfs = -120;  // input RMS level at the time of decoding (dB re full-scale sine)
  float freq_offset_hz = 0; // measured carrier/tone offset if the modem tracks it
  uint16_t corrected = 0;   // bits/symbols repaired by FEC (robust), 0 otherwise
  uint8_t decoder = 0;      // which parallel decoder/slicer produced the frame first
};

struct RxItem {
  RxKind kind = RxKind::Frame;
  Bytes data;        // Frame: the frame without FCS/CRC
  std::string text;  // Text: decoded characters (UTF-8 / ASCII), possibly several per item
  RxQuality q;
};

struct ModemInfo {
  std::string name;
  std::string title;
  uint32_t bitrate_bps = 0;      // nominal line rate for airtime accounting (net for robust)
  uint16_t max_frame = 0;        // largest frame (bytes) for send_frame(); 0 if frames unsupported
  bool frames = false;           // send_frame()/RxKind::Frame supported
  bool text = false;             // send_text()/RxKind::Text supported
  float bw_lo_hz = 0, bw_hi_hz = 0;  // occupied audio band
  uint32_t internal_rate = 8000; // processing rate
};

struct ModemStats {
  uint64_t rx_samples = 0;     // device-rate samples received
  uint32_t rx_frames = 0;      // good frames delivered
  uint32_t rx_chars = 0;       // text characters delivered
  uint32_t rx_dropped = 0;     // items dropped because the RX queue was full
  uint32_t rx_bad = 0;         // frames detected but failing FCS/CRC (or header) checks
  uint32_t tx_frames = 0;      // frames / text items transmitted
  uint32_t tx_transmissions = 0;
};

class Modem {
 public:
  virtual ~Modem() = default;
  Modem(const Modem&) = delete;
  Modem& operator=(const Modem&) = delete;

  const ModemInfo& info() const { return info_; }
  const ModemOptions& options() const { return opt_; }
  uint32_t sample_rate() const { return rate_; }
  const ModemStats& stats() const { return stats_; }

  // --- receive --------------------------------------------------------------------------------
  // Feed received audio at sample_rate(). Any chunk size (including 0 / nullptr). NaN/Inf are
  // treated as 0; values are clamped to +-4.
  void rx(const float* x, size_t n);
  void rx_pcm16(const int16_t* x, size_t n);
  // Pops the oldest received item. Returns false if none.
  bool pop(RxItem* out);
  size_t rx_pending() const { return rxq_.size(); }
  // Optional: called synchronously (from inside rx()) for every item instead of queueing it.
  void set_rx_callback(std::function<void(const RxItem&)> cb) { cb_ = std::move(cb); }
  // Data carrier detect: a signal of this modem's kind is being received right now. Always false
  // while the input is below an absolute floor (kSilenceFloorDbfs, measured with DC removed over
  // ~20 ms): bit-exact digital silence, DC from a muted input, or a quiet noise floor never hold
  // CSMA off, at any sample rate, whatever the demodulator's internal state.
  bool dcd() const { return !rx_quiet() && detect(); }
  // True while the received (DC-removed) level is below kSilenceFloorDbfs.
  bool rx_quiet() const { return ac_ms_ < kSilenceFloorMs; }
  static constexpr float kSilenceFloorDbfs = -60.0f;  // re full-scale sine
  // Carrier sense for CSMA: DCD or we are transmitting.
  bool channel_busy() const { return dcd() || tx_active(); }

  // --- transmit -------------------------------------------------------------------------------
  // Queue a frame (1..max_frame bytes). Returns false if unsupported, too long/empty, or queue full.
  bool send_frame(ByteView frame);
  // Queue text for text modes (cw, psk31, rtty, dtmf). Frame modems treat text as a frame of its
  // bytes. Returns false if unsupported, empty, or contains nothing transmittable for this mode.
  bool send_text(std::string_view text);
  size_t tx_queue_depth() const { return txq_.size(); }
  // True from the first pulled sample of a transmission until its last sample has been pulled.
  bool tx_active() const { return active_ || txbuf_pos_ < txbuf_.size(); }
  // True if pull() would return samples (a transmission is active or queued).
  bool tx_pending() const { return tx_active() || !txq_.empty(); }
  // Writes up to `max` TX samples (sample_rate()) and returns how many. While a transmission is in
  // progress this always returns `max`; it returns less only at the very end of a transmission and
  // 0 when idle. Queued items are sent back-to-back in one transmission (up to max_frames_per_tx).
  size_t pull(float* out, size_t max);
  size_t pull_pcm16(int16_t* out, size_t max);
  // Drops the transmission in progress and everything queued.
  void tx_abort();

 protected:
  struct TxItem {
    bool is_text = false;
    Bytes data;
  };

  Modem(uint32_t device_rate, uint32_t internal_rate, const ModemOptions& opt);
  // Must be called by subclass create() after setting info_; returns false on bad rates.
  bool init_base();

  // Modem-specific carrier detection (see dcd()).
  virtual bool detect() const = 0;
  // Received audio at the internal rate, sanitised.
  virtual void demod(const float* x, size_t n) = 0;
  // Start a transmission from txq_ (pop what you send). Return false if nothing could be started.
  virtual bool tx_begin() = 0;
  // Generate up to max samples at the internal rate, nominal peak 1.0. Returning < max ends the
  // transmission.
  virtual size_t tx_generate(float* out, size_t max) = 0;
  // Validation hooks for send_*.
  virtual bool accept_text(std::string_view text) const { return info_.frames; }
  // Called by tx_abort().
  virtual void tx_reset() {}

  void emit_frame(ByteView data, RxQuality q);
  void emit_text(std::string_view text, RxQuality q);
  // Current input level (dB re full-scale sine) for RxQuality.
  float level_dbfs() const { return meter_.rms_dbfs(); }
  // Pops the next item for a transmission, false if none.
  bool take_tx(TxItem* out);
  uint16_t max_frame() const { return opt_.max_frame; }

  ModemInfo info_;
  ModemOptions opt_;
  ModemStats stats_;
  uint32_t rate_ = 8000, irate_ = 8000;
  uint64_t rx_internal_samples_ = 0;  // internal-rate samples demodulated so far

 private:
  void push_item(RxItem&& it);
  void flush_text();

  dsp::Resampler rx_rs_, tx_rs_;
  dsp::LevelMeter meter_;
  // Silence gate: mean square of the DC-removed input; 5e-7 = (0.7071 * 10^(-60/20))^2.
  static constexpr double kSilenceFloorMs = 5e-7;
  double dc_ = 0, ac_ms_ = 0, dc_a_ = 0, ms_a_ = 0;
  std::vector<float> scratch_, rs_out_, gen_;
  std::deque<RxItem> rxq_;
  std::deque<TxItem> txq_;
  std::function<void(const RxItem&)> cb_;
  std::string pending_text_;
  RxQuality pending_q_;
  std::vector<float> txbuf_;
  size_t txbuf_pos_ = 0;
  bool active_ = false;
};

// Creates a modem by name (see the list at the top of this file). `sample_rate` is the device
// rate (8000..192000). Returns nullptr and sets *error for unknown names, bad rates or bad options.
std::unique_ptr<Modem> create_modem(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                    std::string* error);
// Names accepted by create_modem(), in catalogue order (aliases excluded).
std::vector<std::string> modem_names();
// Info for every modem (as created at 8000 Hz with default options).
std::vector<ModemInfo> modem_catalog();

}  // namespace modem
}  // namespace ember
