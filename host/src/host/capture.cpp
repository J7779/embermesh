// PCM file / FIFO capture source (see capture.h). The sound-card source lives in endpoints/audio.cpp.
#include "host/capture.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "host/util.h"

#if !defined(_WIN32)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ember_host {

#if !defined(_WIN32)
namespace {
class FilePcmSource : public PcmSource {
 public:
  FilePcmSource(int fd, bool regular, uint32_t rate, PcmFn on_pcm, PcmErrFn on_error)
      : fd_(fd), regular_(regular), rate_(rate), on_pcm_(std::move(on_pcm)), on_error_(std::move(on_error)) {
    th_ = std::thread([this] { run(); });
  }
  ~FilePcmSource() override {
    stop_ = true;
    th_.join();
    ::close(fd_);
  }

 private:
  void run() {
    std::vector<uint8_t> carry;
    std::vector<int16_t> s;
    uint8_t buf[8192];
    uint64_t t0 = steady_ms();
    uint64_t delivered = 0;
    while (!stop_) {
      size_t want = sizeof buf;
      if (regular_) {
        uint64_t due = (steady_ms() - t0) * rate_ / 1000;
        if (due <= delivered) {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          continue;
        }
        want = std::min<size_t>(sizeof buf, size_t(due - delivered) * 2);
      } else {
        pollfd p{fd_, POLLIN, 0};
        int pr = ::poll(&p, 1, 100);
        if (pr <= 0) continue;
      }
      ssize_t r = ::read(fd_, buf, want);
      if (r < 0 && (errno == EAGAIN || errno == EINTR)) continue;
      if (r <= 0) {
        if (regular_ || r < 0) {
          if (on_error_) on_error_(r == 0 ? "end of input" : std::string("read error: ") + std::strerror(errno));
          return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // FIFO without a writer
        continue;
      }
      carry.insert(carry.end(), buf, buf + r);
      size_t n = carry.size() / 2;
      if (!n) continue;
      s.resize(n);
      for (size_t i = 0; i < n; ++i) s[i] = int16_t(uint16_t(carry[2 * i]) | uint16_t(carry[2 * i + 1]) << 8);
      carry.erase(carry.begin(), carry.begin() + long(n * 2));
      delivered += n;
      if (on_pcm_) on_pcm_(s.data(), n);
    }
  }
  int fd_;
  bool regular_;
  uint32_t rate_;
  PcmFn on_pcm_;
  PcmErrFn on_error_;
  std::atomic<bool> stop_{false};
  std::thread th_;
};
}  // namespace

std::unique_ptr<PcmSource> open_pcm_file_source(const std::string& path, uint32_t rate, PcmFn on_pcm, PcmErrFn on_error,
                                                std::string* err) {
  int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    if (err) *err = "open " + path + ": " + std::strerror(errno);
    return nullptr;
  }
  struct stat st;
  bool regular = ::fstat(fd, &st) == 0 && S_ISREG(st.st_mode);
  if (regular) ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) & ~O_NONBLOCK);
  return std::unique_ptr<PcmSource>(new FilePcmSource(fd, regular, rate ? rate : 8000, std::move(on_pcm), std::move(on_error)));
}
#else
std::unique_ptr<PcmSource> open_pcm_file_source(const std::string&, uint32_t, PcmFn, PcmErrFn, std::string* err) {
  if (err) *err = "PCM file capture is not implemented on Windows";
  return nullptr;
}
#endif

}  // namespace ember_host
