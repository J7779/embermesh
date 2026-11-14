// EmberMesh core / dsp — end-to-end model of a voice handheld (or mobile) radio path, for testing
// the audio link presets (link/README.md "Handheld radios"). Unlike ChannelSim (a linear channel),
// this models what makes cheap FM handhelds hard for data:
//
//   device audio --[acoustic TX: phone speaker -> radio mic: band-limit, room reverb, room noise]
//     --> keying: VOX (envelope > threshold, opens `vox_attack_ms` after audio starts, holds for
//         `vox_hang_ms` after it stops)  |  PTT line (key-up delay)  |  operator presses PTT by hand
//         (reaction delay on and off)
//     --> TX audio: mic gain, pre-emphasis, hard limiter (deviation limit), 3 kHz splatter filter
//     --> RX: squelch opens `squelch_open_ms` after the carrier appears; FM noise (white before
//         de-emphasis) at `fm_snr_db`; de-emphasis; 300-3000 Hz voice passband; RX AGC/volume;
//         when the carrier drops a `squelch_tail_ms` burst of full noise, then silence (squelch shut)
//     --[acoustic RX: radio speaker -> device mic: band-limit, reverb, room noise]--> device audio
//
// All randomness is seeded; everything runs at one sample rate.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ember {
namespace dsp {

enum class Keying : uint8_t { Vox, Ptt, Manual };

struct RadioPathParams {
  uint64_t seed = 1;
  Keying keying = Keying::Vox;
  // VOX (on the mic signal after any acoustic path)
  double vox_threshold = 0.02;  // envelope (linear, full scale = 1) needed to key
  double vox_attack_ms = 400;   // keying delay after the audio exceeds the threshold
  double vox_hang_ms = 600;     // stays keyed this long after the audio drops
  // PTT line / operator
  double ptt_delay_ms = 150;      // Ptt: interface + transmitter key-up; Manual: operator reaction
  double ptt_release_ms = 50;     // Manual: operator lets go this late after the app says stop
  // Transmitter
  double mic_gain = 1.0;          // device level -> deviation (1 = full deviation at full scale)
  bool preemphasis = true;
  double limiter = 1.0;           // hard clip after pre-emphasis (deviation limit)
  // Receiver
  double squelch_open_ms = 150;
  double squelch_tail_ms = 180;
  double fm_snr_db = 25;          // channel noise in the received audio (before de-emphasis)
  bool deemphasis = true;
  bool rx_agc = true;             // speaker-volume style normalisation to ~0.25 RMS
  double rx_gain = 1.0;
  // Acoustic coupling (both directions when enabled)
  bool acoustic = false;
  double acoustic_lo_hz = 400, acoustic_hi_hz = 4000;  // small speaker / phone mic band
  double reverb_mix = 0.25;       // 0 = none; Schroeder reverb, ~ 300 ms decay
  double room_noise = 0.01;       // RMS of background noise at each microphone
};

struct RadioPathStats {
  uint32_t key_ups = 0;           // transmitter key-ups (VOX openings / PTT presses)
  double keyed_ms = 0;            // total time the transmitter was keyed
  double first_key_ms = -1;       // when the transmitter first keyed (ms from start)
};

class RadioPath {
 public:
  RadioPath(const RadioPathParams& p, double fs);
  // Runs `tx` (device output samples, nominal full scale 1.0) through the path. `ptt` (same length,
  // may be empty for VOX) is the PTT line the app drives (1 = keyed). `trail` samples of silence
  // are appended so the squelch tail is included. Returns what the far device records.
  std::vector<float> run(const std::vector<float>& tx, const std::vector<uint8_t>& ptt, size_t trail = 0);
  const RadioPathStats& stats() const { return st_; }

 private:
  RadioPathParams p_;
  double fs_;
  RadioPathStats st_;
};

}  // namespace dsp
}  // namespace ember
