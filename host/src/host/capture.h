// Native capture sources for the core's Morse reader (RPC_API.md §6, host.capture.*): microphone
// PCM from a sound card (miniaudio, EMBER_HOST_AUDIO) or from a raw s16le PCM file / FIFO (any POSIX
// build: `arecord -t raw -f S16_LE -r 8000 -c 1 > fifo`, test fixtures). Camera capture is done
// by the browser/web view, never natively by this library.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace ember_host {

class PcmSource {
 public:
  virtual ~PcmSource() = default;  // stops; no callback runs after the destructor returns
};

// Samples arrive on a capture thread (mono s16 at `rate`). on_error may be called once, from any
// thread, when the source fails or ends (end of file: "end of input").
using PcmFn = std::function<void(const int16_t* samples, size_t count)>;
using PcmErrFn = std::function<void(const std::string& error)>;

// Raw s16le mono file or FIFO. Regular files are delivered at the real-time rate.
std::unique_ptr<PcmSource> open_pcm_file_source(const std::string& path, uint32_t rate, PcmFn on_pcm, PcmErrFn on_error,
                                                std::string* err);

#ifdef EMBER_HOST_HAVE_AUDIO
// Sound-card input by (part of) its name ("" = system default). Opening is asynchronous and bounded
// by timeout_ms (the macOS microphone prompt can block it): on_error explains a failure/timeout.
std::unique_ptr<PcmSource> open_mic_source(const std::string& device, uint32_t rate, uint32_t timeout_ms, PcmFn on_pcm,
                                           PcmErrFn on_error);
#endif

}  // namespace ember_host
