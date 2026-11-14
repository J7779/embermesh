// EmberMesh core / link — shared receive path + configuration for drivers built on a soft modem
// (modem/modem.h): the "audio" driver (sound card / radio audio) and the "rtl_tcp" SDR receiver.
//
// Framing on top of the modem:
//   HDLC modems (afsk1200, afsk300, psk2400, psk4800): AX.25 UI frames exactly like the kiss driver
//     (link/ax25_carrier.h), so AFSK1200 interoperates with KISS TNC users and APRS gear.
//   robust*: AX.25 when the band is amateur (callsign required), otherwise the bare mesh frame
//     (the robust waveform has its own CRC-32).
//   Keyboard modes (cw, rtty, psk31, dtmf): no mesh frames; text in both directions as interop.
#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "dsp/level.h"
#include "link/ax25_carrier.h"
#include "link/link_util.h"
#include "modem/modem.h"

namespace ember {
namespace link {

// Schema fields shared by audio and rtl_tcp (tx = include transmit-related fields).
void append_modem_fields(Schema& s, bool tx);

class ModemLinkBase : public LinkBase {
 public:
  LinkCaps caps() const override { return caps_; }
  bool channel_busy() const override { return rxm_ && rxm_->dcd(); }

 protected:
  explicit ModemLinkBase(LinkHost& h) : LinkBase(h) { ax_.set_pseudo_seed(random_u32()); }

  // Reads the shared fields; fills caps_ (except fields the subclass overrides). tx: transmitter present.
  bool configure_modem(const Json& c, bool tx, std::string* error);
  std::unique_ptr<modem::Modem> make_modem(uint32_t rate, std::string* error) const;

  // Receive audio at `rate` (modem recreated on rate change).
  void rx_pcm16(const int16_t* x, size_t n, uint32_t rate);
  void rx_float(const float* x, size_t n, uint32_t rate);
  void drain_rx();
  // Flush accumulated keyboard-mode text after a pause (call from tick()).
  void tick_text(uint64_t now);
  // Called with frames produced by the AX.25/APRS layer (acks); default drops them.
  virtual void ax25_output(Ax25Carrier::Output& out);
  Json modem_status() const;
  const char* level_hint() const;
  // Adds the flat level-meter fields ("Audio link status fields" in docs/RPC_API.md) to `j`.
  void add_meter_status(Json& j, bool tx_active) const;

  bool frames_mode() const { return info_.frames; }
  bool text_mode() const { return !info_.frames && info_.text; }

  std::string modem_name_;
  modem::ModemOptions opts_;
  modem::ModemInfo info_;
  LinkCaps caps_;
  bool ax25_ = true;
  bool amateur_ = true;
  Ax25Carrier ax_;
  std::unique_ptr<modem::Modem> rxm_;
  uint32_t rx_rate_ = 0;
  dsp::LevelMeter meter_;
  bool has_snr_ = false;
  float last_snr_ = 0;
  uint64_t rx_samples_ = 0;
  bool have_rx_audio_ = false;   // any receive audio block seen
  uint64_t rx_audio_ms_ = 0;     // time of the last receive audio block
  uint64_t dup_frames_ = 0;
  std::deque<std::pair<uint32_t, uint64_t>> recent_;  // dedup of repeated frames
  std::string text_acc_;
  uint64_t text_last_ms_ = 0;
};

}  // namespace link
}  // namespace ember
