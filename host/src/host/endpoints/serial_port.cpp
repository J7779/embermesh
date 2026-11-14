#include "host/endpoints/serial_port.h"

#include <cerrno>
#include <cmath>
#include <cstring>

#include "host/endpoint.h"
#include "host/endpoints/ptt.h"
#include "host/util.h"

#ifdef _WIN32
#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <IOKit/serial/ioss.h>
#endif
#if defined(__linux__) || defined(__ANDROID__)
#include <asm/ioctls.h>
#endif
#endif

namespace ember_host {

// ---- config --------------------------------------------------------------------------------------

namespace {

// Reads an optional integer field. Missing -> def. Present but not an integer -> error.
bool read_int_field(const Json& cfg, const char* key, int64_t def, int64_t* out, std::string* err) {
  const Json& v = cfg.get(key);
  if (v.is_null()) {
    *out = def;
    return true;
  }
  if (v.is_string() && trim(v.as_string()).empty()) {
    *out = def;
    return true;
  }
  double d = cfg_num(cfg, key, std::nan(""));
  if (std::isnan(d) || v.is_bool() || d != std::floor(d) || std::fabs(d) > 1e12) {
    if (err) *err = std::string("invalid ") + key + ": " + v.dump();
    return false;
  }
  *out = int64_t(d);
  return true;
}

bool read_level(const Json& cfg, const char* key, const char* def, std::string* out, std::string* err) {
  const Json& v = cfg.get(key);
  if (v.is_null()) {
    *out = def;
    return true;
  }
  if (v.is_bool()) {
    *out = v.as_bool() ? "on" : "off";
    return true;
  }
  std::string s = to_lower(trim(cfg_str(cfg, key)));
  if (s.empty()) {
    *out = def;
    return true;
  }
  if (s == "on" || s == "high" || s == "true" || s == "1") {
    *out = "on";
    return true;
  }
  if (s == "off" || s == "low" || s == "false" || s == "0") {
    *out = "off";
    return true;
  }
  if (err) *err = std::string("invalid ") + key + " (expected on/off): " + v.dump();
  return false;
}

}  // namespace

bool serial_config_from_json(const Json& cfg, SerialConfig* out, std::string* err) {
  if (!cfg.is_object()) {
    if (err) *err = "serial config must be an object";
    return false;
  }
  SerialConfig c;
  c.port = trim(cfg_str(cfg, "port"));
  if (c.port.empty()) {
    if (err) *err = "serial port is required (e.g. /dev/ttyUSB0, /dev/cu.usbserial-1410, COM3)";
    return false;
  }
  if (c.port.size() > 255 || c.port.find('\0') != std::string::npos) {
    if (err) *err = "invalid serial port name";
    return false;
  }
  int64_t v;
  // Accept "baud_custom" (advanced override) or "baud".
  const char* baud_key = "baud";
  if (!cfg.get("baud_custom").is_null() && !trim(cfg_str(cfg, "baud_custom")).empty()) baud_key = "baud_custom";
  if (!read_int_field(cfg, baud_key, 9600, &v, err)) return false;
  if (v < 50 || v > 12000000) {
    if (err) *err = "baud rate out of range (50..12000000): " + std::to_string(v);
    return false;
  }
  c.baud = uint32_t(v);
  if (!read_int_field(cfg, "data_bits", 8, &v, err)) return false;
  if (v < 5 || v > 8) {
    if (err) *err = "data_bits must be 5..8";
    return false;
  }
  c.data_bits = int(v);
  if (!read_int_field(cfg, "stop_bits", 1, &v, err)) return false;
  if (v != 1 && v != 2) {
    if (err) *err = "stop_bits must be 1 or 2";
    return false;
  }
  c.stop_bits = int(v);

  std::string p = to_lower(trim(cfg_str(cfg, "parity", "none")));
  if (p.empty() || p == "none" || p == "n") c.parity = 'N';
  else if (p == "even" || p == "e") c.parity = 'E';
  else if (p == "odd" || p == "o") c.parity = 'O';
  else if (p == "mark" || p == "m") c.parity = 'M';
  else if (p == "space" || p == "s") c.parity = 'S';
  else {
    if (err) *err = "invalid parity (none/even/odd/mark/space): " + p;
    return false;
  }

  std::string f = to_lower(trim(cfg_str(cfg, "flow", "none")));
  if (f.empty() || f == "none" || f == "off") c.flow = "none";
  else if (f == "rtscts" || f == "rts/cts" || f == "hardware" || f == "crtscts") c.flow = "rtscts";
  else if (f == "xonxoff" || f == "xon/xoff" || f == "software") c.flow = "xonxoff";
  else {
    if (err) *err = "invalid flow control (none/rtscts/xonxoff): " + f;
    return false;
  }
  if (!read_level(cfg, "dtr", "on", &c.dtr, err)) return false;
  if (!read_level(cfg, "rts", "on", &c.rts, err)) return false;
  c.exclusive = cfg_bool(cfg, "exclusive", true);
  *out = c;
  return true;
}

bool posix_standard_baud(uint32_t baud, unsigned long* constant) {
#ifdef _WIN32
  (void)baud;
  (void)constant;
  return false;
#else
  struct Map {
    uint32_t baud;
    unsigned long c;
  };
  static const Map table[] = {
      {50, B50},       {75, B75},       {110, B110},     {134, B134},     {150, B150},
      {200, B200},     {300, B300},     {600, B600},     {1200, B1200},   {1800, B1800},
      {2400, B2400},   {4800, B4800},   {9600, B9600},   {19200, B19200}, {38400, B38400},
#ifdef B7200
      {7200, B7200},
#endif
#ifdef B14400
      {14400, B14400},
#endif
#ifdef B28800
      {28800, B28800},
#endif
#ifdef B57600
      {57600, B57600},
#endif
#ifdef B76800
      {76800, B76800},
#endif
#ifdef B115200
      {115200, B115200},
#endif
#ifdef B230400
      {230400, B230400},
#endif
#ifdef B460800
      {460800, B460800},
#endif
#ifdef B500000
      {500000, B500000},
#endif
#ifdef B576000
      {576000, B576000},
#endif
#ifdef B921600
      {921600, B921600},
#endif
#ifdef B1000000
      {1000000, B1000000},
#endif
#ifdef B1152000
      {1152000, B1152000},
#endif
#ifdef B1500000
      {1500000, B1500000},
#endif
#ifdef B2000000
      {2000000, B2000000},
#endif
#ifdef B2500000
      {2500000, B2500000},
#endif
#ifdef B3000000
      {3000000, B3000000},
#endif
#ifdef B3500000
      {3500000, B3500000},
#endif
#ifdef B4000000
      {4000000, B4000000},
#endif
  };
  for (const auto& m : table) {
    if (m.baud == baud) {
      *constant = m.c;
      return true;
    }
  }
  return false;
#endif
}

// ---- POSIX ---------------------------------------------------------------------------------------
#ifndef _WIN32

// termios2 + BOTHER for arbitrary baud rates on Linux. <asm/termbits.h> cannot be included next to
// <termios.h> (both define struct termios), and TCGETS2/TCSETS2 from <asm/ioctls.h> expand to
// sizeof(struct termios2), which is then incomplete. So the struct is defined here with the kernel's
// asm-generic layout and the ioctl numbers are rebuilt from it. Only for architectures that use that
// layout (x86, arm, arm64, riscv, loongarch); mips/sparc/alpha/powerpc differ and fall back to an
// error for non-standard rates.
#if (defined(__linux__) || defined(__ANDROID__)) && defined(TCGETS2) && defined(TCSETS2) &&          \
    (defined(__x86_64__) || defined(__i386__) || defined(__arm__) || defined(__aarch64__) ||          \
     defined(__riscv) || defined(__loongarch__))
#define EMBER_HAVE_TERMIOS2 1
struct ember_termios2 {
  tcflag_t c_iflag;
  tcflag_t c_oflag;
  tcflag_t c_cflag;
  tcflag_t c_lflag;
  cc_t c_line;
  cc_t c_cc[19];
  speed_t c_ispeed;
  speed_t c_ospeed;
};
#ifndef BOTHER
#define BOTHER 0010000
#endif
#ifndef IBSHIFT
#define IBSHIFT 16
#endif
static_assert(sizeof(ember_termios2) == 44, "kernel asm-generic struct termios2 layout");
#define EMBER_TCGETS2 _IOR('T', 0x2A, struct ember_termios2)
#define EMBER_TCSETS2 _IOW('T', 0x2B, struct ember_termios2)
#endif

SerialPort::SerialPort(Loop* loop) : loop_(loop), alive_(std::make_shared<bool>(true)) {}

SerialPort::~SerialPort() {
  *alive_ = false;
  close();
}

bool SerialPort::is_open() const { return fd_ >= 0; }
size_t SerialPort::pending() const { return out_.size() - out_off_; }

static bool set_modem_line(int fd, int bit, bool on, std::string* err) {
  int b = bit;
  if (::ioctl(fd, on ? TIOCMBIS : TIOCMBIC, &b) != 0) {
    if (err) *err = std::string("modem line ioctl: ") + std::strerror(errno);
    return false;
  }
  return true;
}

bool SerialPort::set_rts(bool on, std::string* err) {
  if (fd_ < 0) {
    if (err) *err = "port not open";
    return false;
  }
  return set_modem_line(fd_, TIOCM_RTS, on, err);
}

bool SerialPort::set_dtr(bool on, std::string* err) {
  if (fd_ < 0) {
    if (err) *err = "port not open";
    return false;
  }
  return set_modem_line(fd_, TIOCM_DTR, on, err);
}

bool SerialPort::open(const SerialConfig& cfg, bool lines_only, std::string* err) {
  close();
  lines_only_ = lines_only;
  path_ = cfg.port;
  if (!lines_only && !loop_) {
    if (err) *err = "internal: serial port without event loop";
    return false;
  }
  int fd = ::open(cfg.port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    if (err) *err = "open " + cfg.port + ": " + std::strerror(errno);
    return false;
  }
  if (!::isatty(fd)) {
    ::close(fd);
    if (err) *err = cfg.port + " is not a serial device";
    return false;
  }
  if (cfg.exclusive && ::ioctl(fd, TIOCEXCL) != 0) {
    EH_LOGD("serial %s: TIOCEXCL failed: %s", cfg.port.c_str(), std::strerror(errno));
  }
  struct termios t;
  if (::tcgetattr(fd, &t) != 0) {
    int e = errno;
    ::close(fd);
    if (err) *err = "tcgetattr " + cfg.port + ": " + std::strerror(e);
    return false;
  }
  ::cfmakeraw(&t);
  t.c_cflag |= (CLOCAL | CREAD);
  t.c_cflag &= ~tcflag_t(CSIZE);
  switch (cfg.data_bits) {
    case 5: t.c_cflag |= CS5; break;
    case 6: t.c_cflag |= CS6; break;
    case 7: t.c_cflag |= CS7; break;
    default: t.c_cflag |= CS8; break;
  }
  if (cfg.stop_bits == 2) t.c_cflag |= CSTOPB;
  else t.c_cflag &= ~tcflag_t(CSTOPB);
  t.c_cflag &= ~tcflag_t(PARENB | PARODD);
#ifdef CMSPAR
  t.c_cflag &= ~tcflag_t(CMSPAR);
#endif
  t.c_iflag &= ~tcflag_t(INPCK | ISTRIP);
  switch (cfg.parity) {
    case 'E': t.c_cflag |= PARENB; break;
    case 'O': t.c_cflag |= PARENB | PARODD; break;
    case 'M':
    case 'S':
#ifdef CMSPAR
      t.c_cflag |= PARENB | CMSPAR;
      if (cfg.parity == 'M') t.c_cflag |= PARODD;
      break;
#else
      ::close(fd);
      if (err) *err = "mark/space parity is not supported on this platform";
      return false;
#endif
    default: break;
  }
#ifdef CRTSCTS
  t.c_cflag &= ~tcflag_t(CRTSCTS);
  if (cfg.flow == "rtscts") t.c_cflag |= CRTSCTS;
#else
  if (cfg.flow == "rtscts") {
    ::close(fd);
    if (err) *err = "hardware flow control not supported on this platform";
    return false;
  }
#endif
  t.c_iflag &= ~tcflag_t(IXON | IXOFF | IXANY);
  if (cfg.flow == "xonxoff") t.c_iflag |= IXON | IXOFF;
  // HUPCL drops DTR on close — leaves a DTR-keyed transmitter unkeyed when we exit.
  t.c_cflag |= HUPCL;
  t.c_cc[VMIN] = 0;
  t.c_cc[VTIME] = 0;

  unsigned long bconst = 0;
  bool standard = posix_standard_baud(cfg.baud, &bconst);
  if (standard) {
    ::cfsetispeed(&t, speed_t(bconst));
    ::cfsetospeed(&t, speed_t(bconst));
  } else {
    // Placeholder speed; the real non-standard rate is applied below.
    ::cfsetispeed(&t, B38400);
    ::cfsetospeed(&t, B38400);
  }
  if (::tcsetattr(fd, TCSANOW, &t) != 0) {
    int e = errno;
    ::close(fd);
    if (err) *err = "tcsetattr " + cfg.port + ": " + std::strerror(e);
    return false;
  }
  if (!standard) {
#if defined(__APPLE__) && defined(IOSSIOSPEED)
    speed_t sp = speed_t(cfg.baud);
    if (::ioctl(fd, IOSSIOSPEED, &sp) != 0) {
      int e = errno;
      ::close(fd);
      if (err) *err = "non-standard baud " + std::to_string(cfg.baud) + " (IOSSIOSPEED): " + std::strerror(e);
      return false;
    }
#elif defined(EMBER_HAVE_TERMIOS2)
    struct ember_termios2 t2;
    if (::ioctl(fd, EMBER_TCGETS2, &t2) != 0) {
      int e = errno;
      ::close(fd);
      if (err) *err = "non-standard baud (TCGETS2): " + std::string(std::strerror(e));
      return false;
    }
    t2.c_cflag &= ~tcflag_t(CBAUD);
    t2.c_cflag |= BOTHER;
    t2.c_cflag &= ~tcflag_t(CBAUD << IBSHIFT);
    t2.c_cflag |= tcflag_t(BOTHER) << IBSHIFT;
    t2.c_ispeed = speed_t(cfg.baud);
    t2.c_ospeed = speed_t(cfg.baud);
    if (::ioctl(fd, EMBER_TCSETS2, &t2) != 0) {
      int e = errno;
      ::close(fd);
      if (err) *err = "non-standard baud " + std::to_string(cfg.baud) + " (TCSETS2): " + std::strerror(e);
      return false;
    }
#else
    ::close(fd);
    if (err) *err = "non-standard baud rate " + std::to_string(cfg.baud) + " is not supported on this platform";
    return false;
#endif
  }
  ::tcflush(fd, TCIOFLUSH);
  fd_ = fd;
  // Initial control-line levels. Failures are normal on ptys / some USB adapters.
  std::string lerr;
  if (!set_modem_line(fd_, TIOCM_DTR, cfg.dtr == "on", &lerr))
    EH_LOGD("serial %s: DTR: %s", cfg.port.c_str(), lerr.c_str());
  if (cfg.flow != "rtscts" && !set_modem_line(fd_, TIOCM_RTS, cfg.rts == "on", &lerr))
    EH_LOGD("serial %s: RTS: %s", cfg.port.c_str(), lerr.c_str());
  if (!lines_only) {
    watch_ = loop_->watch(fd_, kPollIn, [this](short re) { on_io(re); });
  }
  return true;
}

void SerialPort::close() {
  if (watch_ && loop_) loop_->unwatch(watch_);
  watch_ = 0;
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  out_.clear();
  out_off_ = 0;
  overflow_logged_ = false;
}

void SerialPort::fail(const std::string& why) {
  if (!is_open()) return;
  close();
  if (on_error) on_error(why);
}

void SerialPort::update_events() {
  if (!watch_) return;
  loop_->set_events(watch_, short(kPollIn | (pending() ? kPollOut : 0)));
}

void SerialPort::write(const uint8_t* data, size_t len) {
  if (fd_ < 0 || lines_only_ || len == 0) return;
  if (pending() + len > kMaxPending) {
    dropped_ += len;
    if (!overflow_logged_) {
      EH_LOGW("serial %s: output buffer full (%zu bytes queued), dropping data", path_.c_str(), pending());
      overflow_logged_ = true;
    }
    return;
  }
  if (out_off_ > 0 && out_off_ == out_.size()) {
    out_.clear();
    out_off_ = 0;
  }
  out_.append(reinterpret_cast<const char*>(data), len);
  flush();
}

void SerialPort::flush() {
  while (fd_ >= 0 && pending()) {
    ssize_t n = ::write(fd_, out_.data() + out_off_, pending());
    if (n > 0) {
      out_off_ += size_t(n);
      continue;
    }
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) break;
    // Report asynchronously: write() must never re-enter the owner through on_error.
    std::string why = std::string("write ") + path_ + ": " + (n < 0 ? std::strerror(errno) : "no progress");
    std::weak_ptr<bool> alive = alive_;
    loop_->timer(0, [this, alive, why] {
      if (alive.expired() || !*alive.lock()) return;
      fail(why);
    });
    return;
  }
  if (!pending()) {
    out_.clear();
    out_off_ = 0;
    overflow_logged_ = false;
  }
  update_events();
}

void SerialPort::on_io(short re) {
  auto alive = alive_;
  if ((re & kPollOut) && pending()) {
    flush();
    if (!*alive || fd_ < 0) return;
  }
  if (re & (kPollIn | kPollHup | kPollErr)) {
    uint8_t buf[4096];
    for (int i = 0; i < 8; ++i) {
      ssize_t n = ::read(fd_, buf, sizeof buf);
      if (n > 0) {
        if (on_data) on_data(buf, size_t(n));
        if (!*alive || fd_ < 0) return;
        if (size_t(n) < sizeof buf) break;
        continue;
      }
      if (n == 0) {
        // poll() said readable but nothing came: the device hung up / was unplugged.
        if (i == 0) fail("serial device " + path_ + " closed");
        return;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
        if (i == 0 && (re & (kPollHup | kPollErr))) fail("serial device " + path_ + " hung up");
        return;
      }
      fail(std::string("read ") + path_ + ": " + std::strerror(errno));
      return;
    }
  }
}

#else  // ---- Win32 (UNTESTED) -------------------------------------------------------------------

struct SerialPort::Win {
  HANDLE h = INVALID_HANDLE_VALUE;
  HANDLE stop = nullptr;  // manual-reset event
  std::thread reader, writer;
  std::mutex mu;
  std::condition_variable cv;
  std::deque<std::string> queue;
  size_t queued = 0;
  bool quit = false;
  // Liveness for posted callbacks: set false (loop thread) in close().
  std::shared_ptr<std::atomic<bool>> live = std::make_shared<std::atomic<bool>>(true);
};

static std::wstring widen_utf8(const std::string& s) {
  if (s.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(size_t(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), &w[0], n);
  return w;
}

static std::string win_err(DWORD e) {
  char buf[256] = {0};
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, e, 0, buf, sizeof buf, nullptr);
  std::string s = buf;
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
  return s.empty() ? ("error " + std::to_string(e)) : s;
}

SerialPort::SerialPort(Loop* loop) : loop_(loop), alive_(std::make_shared<bool>(true)) {}

SerialPort::~SerialPort() {
  *alive_ = false;
  close();
}

bool SerialPort::is_open() const { return win_ && win_->h != INVALID_HANDLE_VALUE; }

size_t SerialPort::pending() const {
  if (!win_) return 0;
  std::lock_guard<std::mutex> lk(win_->mu);
  return win_->queued;
}

bool SerialPort::set_rts(bool on, std::string* err) {
  if (!is_open()) {
    if (err) *err = "port not open";
    return false;
  }
  if (!EscapeCommFunction(win_->h, on ? SETRTS : CLRRTS)) {
    if (err) *err = "EscapeCommFunction(RTS): " + win_err(GetLastError());
    return false;
  }
  return true;
}

bool SerialPort::set_dtr(bool on, std::string* err) {
  if (!is_open()) {
    if (err) *err = "port not open";
    return false;
  }
  if (!EscapeCommFunction(win_->h, on ? SETDTR : CLRDTR)) {
    if (err) *err = "EscapeCommFunction(DTR): " + win_err(GetLastError());
    return false;
  }
  return true;
}

bool SerialPort::open(const SerialConfig& cfg, bool lines_only, std::string* err) {
  close();
  lines_only_ = lines_only;
  path_ = cfg.port;
  if (!lines_only && !loop_) {
    if (err) *err = "internal: serial port without event loop";
    return false;
  }
  std::string dev = cfg.port;
  if (dev.rfind("\\\\.\\", 0) != 0) dev = "\\\\.\\" + dev;  // required for COM10+
  HANDLE h = CreateFileW(widen_utf8(dev).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    if (err) *err = "open " + cfg.port + ": " + win_err(GetLastError());
    return false;
  }
  DCB dcb;
  std::memset(&dcb, 0, sizeof dcb);
  dcb.DCBlength = sizeof dcb;
  if (!GetCommState(h, &dcb)) {
    if (err) *err = "GetCommState " + cfg.port + ": " + win_err(GetLastError());
    CloseHandle(h);
    return false;
  }
  dcb.BaudRate = cfg.baud;  // any rate the driver accepts
  dcb.fBinary = TRUE;
  dcb.ByteSize = BYTE(cfg.data_bits);
  dcb.StopBits = cfg.stop_bits == 2 ? TWOSTOPBITS : ONESTOPBIT;
  switch (cfg.parity) {
    case 'E': dcb.Parity = EVENPARITY; break;
    case 'O': dcb.Parity = ODDPARITY; break;
    case 'M': dcb.Parity = MARKPARITY; break;
    case 'S': dcb.Parity = SPACEPARITY; break;
    default: dcb.Parity = NOPARITY; break;
  }
  dcb.fParity = dcb.Parity != NOPARITY;
  dcb.fOutxCtsFlow = cfg.flow == "rtscts";
  dcb.fRtsControl = cfg.flow == "rtscts" ? RTS_CONTROL_HANDSHAKE
                                         : (cfg.rts == "on" ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE);
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = cfg.dtr == "on" ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;
  dcb.fDsrSensitivity = FALSE;
  dcb.fOutX = dcb.fInX = cfg.flow == "xonxoff";
  dcb.fErrorChar = FALSE;
  dcb.fNull = FALSE;
  dcb.fAbortOnError = FALSE;
  if (!SetCommState(h, &dcb)) {
    if (err) *err = "SetCommState " + cfg.port + " (baud " + std::to_string(cfg.baud) + "): " + win_err(GetLastError());
    CloseHandle(h);
    return false;
  }
  COMMTIMEOUTS to;
  std::memset(&to, 0, sizeof to);
  // Return as soon as any byte arrives, or after 100 ms with nothing.
  to.ReadIntervalTimeout = MAXDWORD;
  to.ReadTotalTimeoutMultiplier = MAXDWORD;
  to.ReadTotalTimeoutConstant = 100;
  to.WriteTotalTimeoutConstant = 0;
  to.WriteTotalTimeoutMultiplier = 0;
  SetCommTimeouts(h, &to);
  PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

  win_ = std::make_shared<Win>();
  win_->h = h;
  if (lines_only) return true;
  win_->stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  auto w = win_;
  Loop* loop = loop_;
  std::string path = path_;
  auto post_error = [w, loop, this](const std::string& why) {
    auto live = w->live;
    loop->post([this, live, why] {
      if (!live->load()) return;
      fail(why);
    });
  };
  w->reader = std::thread([w, loop, this, post_error, path] {
    OVERLAPPED ov;
    std::memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::vector<uint8_t> buf(4096);
    while (true) {
      ResetEvent(ov.hEvent);
      DWORD n = 0;
      if (!ReadFile(w->h, buf.data(), DWORD(buf.size()), &n, &ov)) {
        DWORD e = GetLastError();
        if (e != ERROR_IO_PENDING) {
          if (WaitForSingleObject(w->stop, 0) != WAIT_OBJECT_0) post_error("read " + path + ": " + win_err(e));
          break;
        }
        HANDLE hs[2] = {ov.hEvent, w->stop};
        DWORD r = WaitForMultipleObjects(2, hs, FALSE, INFINITE);
        if (r != WAIT_OBJECT_0) {
          CancelIoEx(w->h, &ov);
          GetOverlappedResult(w->h, &ov, &n, TRUE);
          break;
        }
        if (!GetOverlappedResult(w->h, &ov, &n, FALSE)) {
          DWORD e2 = GetLastError();
          if (WaitForSingleObject(w->stop, 0) != WAIT_OBJECT_0) post_error("read " + path + ": " + win_err(e2));
          break;
        }
      }
      if (WaitForSingleObject(w->stop, 0) == WAIT_OBJECT_0) break;
      if (n > 0) {
        std::string chunk(reinterpret_cast<char*>(buf.data()), n);
        auto live = w->live;
        loop->post([this, live, chunk] {
          if (!live->load()) return;
          if (on_data) on_data(reinterpret_cast<const uint8_t*>(chunk.data()), chunk.size());
        });
      }
    }
    CloseHandle(ov.hEvent);
  });
  w->writer = std::thread([w, post_error, path] {
    OVERLAPPED ov;
    std::memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    while (true) {
      std::string item;
      {
        std::unique_lock<std::mutex> lk(w->mu);
        w->cv.wait(lk, [&] { return w->quit || !w->queue.empty(); });
        if (w->quit) break;
        item = std::move(w->queue.front());
        w->queue.pop_front();
      }
      size_t off = 0;
      bool failed = false;
      while (off < item.size()) {
        ResetEvent(ov.hEvent);
        DWORD n = 0;
        if (!WriteFile(w->h, item.data() + off, DWORD(item.size() - off), &n, &ov)) {
          DWORD e = GetLastError();
          if (e != ERROR_IO_PENDING) {
            post_error("write " + path + ": " + win_err(e));
            failed = true;
            break;
          }
          HANDLE hs[2] = {ov.hEvent, w->stop};
          DWORD r = WaitForMultipleObjects(2, hs, FALSE, INFINITE);
          if (r != WAIT_OBJECT_0) {
            CancelIoEx(w->h, &ov);
            GetOverlappedResult(w->h, &ov, &n, TRUE);
            failed = true;
            break;
          }
          if (!GetOverlappedResult(w->h, &ov, &n, FALSE)) {
            post_error("write " + path + ": " + win_err(GetLastError()));
            failed = true;
            break;
          }
        }
        off += n;
      }
      {
        std::lock_guard<std::mutex> lk(w->mu);
        w->queued -= item.size();
      }
      if (failed) break;
    }
    CloseHandle(ov.hEvent);
  });
  return true;
}

void SerialPort::write(const uint8_t* data, size_t len) {
  if (!is_open() || lines_only_ || len == 0) return;
  std::lock_guard<std::mutex> lk(win_->mu);
  if (win_->queued + len > kMaxPending) {
    dropped_ += len;
    EH_LOGW("serial %s: output buffer full, dropping %zu bytes", path_.c_str(), len);
    return;
  }
  win_->queue.emplace_back(reinterpret_cast<const char*>(data), len);
  win_->queued += len;
  win_->cv.notify_one();
}

void SerialPort::close() {
  if (!win_) return;
  auto w = win_;
  win_.reset();
  w->live->store(false);
  {
    std::lock_guard<std::mutex> lk(w->mu);
    w->quit = true;
  }
  w->cv.notify_all();
  if (w->stop) SetEvent(w->stop);
  if (w->h != INVALID_HANDLE_VALUE) CancelIoEx(w->h, nullptr);
  if (w->reader.joinable()) w->reader.join();
  if (w->writer.joinable()) w->writer.join();
  if (w->h != INVALID_HANDLE_VALUE) CloseHandle(w->h);
  if (w->stop) CloseHandle(w->stop);
  w->h = INVALID_HANDLE_VALUE;
}

void SerialPort::fail(const std::string& why) {
  if (!is_open()) return;
  close();
  if (on_error) on_error(why);
}

#endif

// ---- enumeration ---------------------------------------------------------------------------------

#ifdef _WIN32
std::vector<SerialDeviceInfo> enumerate_serial_ports() {
  std::vector<SerialDeviceInfo> r;
  HKEY key;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", 0, KEY_READ, &key) != ERROR_SUCCESS)
    return r;
  for (DWORD i = 0;; ++i) {
    wchar_t name[256];
    DWORD name_len = 256;
    wchar_t data[256];
    DWORD data_len = sizeof data;
    DWORD type = 0;
    LONG rc = RegEnumValueW(key, i, name, &name_len, nullptr, &type, reinterpret_cast<BYTE*>(data), &data_len);
    if (rc == ERROR_NO_MORE_ITEMS) break;
    if (rc != ERROR_SUCCESS || type != REG_SZ) continue;
    data[255] = 0;
    char com[256] = {0}, dev[256] = {0};
    WideCharToMultiByte(CP_UTF8, 0, data, -1, com, sizeof com - 1, nullptr, nullptr);
    WideCharToMultiByte(CP_UTF8, 0, name, int(name_len), dev, sizeof dev - 1, nullptr, nullptr);
    SerialDeviceInfo d;
    d.path = com;
    d.name = com;
    std::string devs = dev;
    std::string hint;
    if (devs.find("USBSER") != std::string::npos) hint = "USB CDC serial";
    else if (devs.find("VCP") != std::string::npos) hint = "FTDI USB serial";
    else if (devs.find("Silabser") != std::string::npos) hint = "Silicon Labs CP210x";
    else if (devs.find("CH341") != std::string::npos || devs.find("wch") != std::string::npos) hint = "WCH CH340";
    else if (devs.find("BthModem") != std::string::npos) hint = "Bluetooth serial";
    d.detail = hint.empty() ? devs : hint + " (" + devs + ")";
    if (!hint.empty()) d.name = std::string(com) + " — " + hint;
    r.push_back(d);
  }
  RegCloseKey(key);
  return r;
}
#else

#if defined(__linux__)
static std::string read_small(const std::string& path) {
  std::string s;
  if (!read_file(path, &s, 4096)) return std::string();
  return trim(s);
}
#endif

#if !defined(__APPLE__)
static std::string link_target_basename(const std::string& link) {
  char buf[1024];
  ssize_t n = ::readlink(link.c_str(), buf, sizeof buf - 1);
  if (n <= 0) return std::string();
  std::string t(buf, size_t(n));
  size_t s = t.find_last_of('/');
  return s == std::string::npos ? t : t.substr(s + 1);
}
#endif

std::vector<SerialDeviceInfo> enumerate_serial_ports() {
  std::vector<SerialDeviceInfo> r;
  std::vector<std::string> names = list_dir("/dev");
#if defined(__APPLE__)
  for (const auto& n : names) {
    if (!starts_with(n, "cu.")) continue;
    SerialDeviceInfo d;
    d.path = "/dev/" + n;
    std::string base = n.substr(3);
    d.name = base;
    d.detail = "callout device (use this, not /dev/tty." + base + ")";
    if (base == "Bluetooth-Incoming-Port") d.detail = "macOS Bluetooth placeholder (usually not a radio)";
    else if (base == "debug-console") d.detail = "macOS debug console (not a radio)";
    else if (base.find("usbserial") != std::string::npos) d.name = base + " (USB serial)";
    else if (base.find("usbmodem") != std::string::npos) d.name = base + " (USB modem / CDC)";
    else if (base.find("SLAB") != std::string::npos) d.name = base + " (CP210x)";
    else if (base.find("wchusbserial") != std::string::npos) d.name = base + " (CH340)";
    r.push_back(d);
  }
#else
  // Friendly names from /dev/serial/by-id (udev).
  std::vector<std::pair<std::string, std::string>> by_id;  // target basename -> by-id name
  for (const auto& id : list_dir("/dev/serial/by-id")) {
    std::string t = link_target_basename("/dev/serial/by-id/" + id);
    if (!t.empty()) by_id.emplace_back(t, id);
  }
  auto add = [&](const std::string& n) {
    SerialDeviceInfo d;
    d.path = "/dev/" + n;
    d.name = n;
    for (const auto& b : by_id)
      if (b.first == n) d.detail = b.second;
#if defined(__linux__)
    std::string sys = "/sys/class/tty/" + n + "/device";
    std::string product = read_small(sys + "/../product");
    if (product.empty()) product = read_small(sys + "/product");
    std::string manu = read_small(sys + "/../manufacturer");
    if (manu.empty()) manu = read_small(sys + "/manufacturer");
    if (!product.empty()) d.name = n + " — " + (manu.empty() ? product : manu + " " + product);
    else if (starts_with(n, "ttyAMA") || n == "ttyS0") d.name = n + " (on-board UART)";
    else if (starts_with(n, "rfcomm")) d.name = n + " (Bluetooth SPP)";
#endif
    r.push_back(d);
  };
  for (const auto& n : names) {
    if (starts_with(n, "ttyUSB") || starts_with(n, "ttyACM") || starts_with(n, "ttyAMA") || starts_with(n, "rfcomm") ||
        starts_with(n, "ttyTHS") || starts_with(n, "cuaU") || starts_with(n, "ttyU")) {
      add(n);
    } else if (n == "ttyS0" || n == "ttyS1" || n == "ttyS2" || n == "ttyS3") {
      if (is_dir("/sys/class/tty/" + n + "/device")) add(n);
    }
  }
  for (const char* alias : {"serial0", "serial1"}) {
    std::string t = link_target_basename(std::string("/dev/") + alias);
    if (t.empty()) continue;
    SerialDeviceInfo d;
    d.path = std::string("/dev/") + alias;
    d.name = std::string(alias) + " (Raspberry Pi UART alias)";
    d.detail = "-> /dev/" + t;
    r.push_back(d);
  }
#endif
  return r;
}
#endif

// ---- PTT via serial control lines ----------------------------------------------------------------

namespace {

class SerialLinePtt : public PttControl {
 public:
  SerialLinePtt(std::string port, std::string line, bool invert)
      : port_name_(std::move(port)), line_(std::move(line)), invert_(invert), port_(nullptr) {}
  bool start(std::string* err) {
    SerialConfig c;
    c.port = port_name_;
    c.exclusive = false;
    // Start unkeyed on both lines that we drive.
    const char* idle = invert_ ? "on" : "off";
    c.dtr = (line_ == "dtr" || line_ == "both") ? idle : "on";
    c.rts = (line_ == "rts" || line_ == "both") ? idle : "on";
    if (!port_.open(c, true, err)) return false;
    set(false);
    return true;
  }
  void set(bool on) override {
    bool level = on != invert_;
    std::string err;
    if ((line_ == "rts" || line_ == "both") && !port_.set_rts(level, &err))
      EH_LOGW("PTT %s RTS: %s", port_name_.c_str(), err.c_str());
    if ((line_ == "dtr" || line_ == "both") && !port_.set_dtr(level, &err))
      EH_LOGW("PTT %s DTR: %s", port_name_.c_str(), err.c_str());
  }
  bool ok() const override { return port_.is_open(); }
  std::string describe() const override { return "serial " + line_ + " on " + port_name_ + (invert_ ? " (inverted)" : ""); }

 private:
  std::string port_name_, line_;
  bool invert_;
  SerialPort port_;
};

}  // namespace

std::unique_ptr<PttControl> make_serial_line_ptt(const std::string& port, const std::string& line, bool invert,
                                                 std::string* err) {
  std::string l = to_lower(trim(line));
  if (l != "rts" && l != "dtr" && l != "both") {
    if (err) *err = "PTT line must be rts, dtr or both";
    return nullptr;
  }
  if (trim(port).empty()) {
    if (err) *err = "PTT serial port is required";
    return nullptr;
  }
  auto p = std::make_unique<SerialLinePtt>(trim(port), l, invert);
  if (!p->start(err)) return nullptr;
  return p;
}

}  // namespace ember_host
