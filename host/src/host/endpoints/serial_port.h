// Portable serial port: POSIX termios (Linux termios2 / macOS IOSSIOSPEED for non-standard baud
// rates) and Win32 (overlapped I/O on reader/writer threads — UNTESTED, no Windows toolchain was
// available when this was written).
//
// Threading: all methods on the loop thread. Data and errors are delivered on the loop thread
// (POSIX: from the Loop's poll watch; Windows: posted from the reader/writer threads).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "host/json.h"
#include "host/loop.h"

namespace ember_host {

struct SerialConfig {
  std::string port;
  uint32_t baud = 9600;
  int data_bits = 8;
  char parity = 'N';  // N / E / O / M (mark) / S (space)
  int stop_bits = 1;
  std::string flow = "none";  // "none" | "rtscts" | "xonxoff"
  std::string dtr = "on";     // initial DTR level: "on" | "off"
  std::string rts = "on";     // initial RTS level: "on" | "off" (ignored with rtscts flow)
  bool exclusive = true;      // TIOCEXCL on POSIX (Windows ports are always exclusive)
};

// Validates and maps an endpoint config object (as sent by the schema-driven UI form; numbers may
// arrive as strings). Returns false and sets *err on any invalid value.
bool serial_config_from_json(const Json& cfg, SerialConfig* out, std::string* err);

// Maps a baud rate to its POSIX B* constant (as unsigned long). False if the rate has no standard
// constant on this platform (it may still work as a non-standard rate). Always false on Windows.
bool posix_standard_baud(uint32_t baud, unsigned long* constant);

struct SerialDeviceInfo {
  std::string path;    // what to put in config.port: /dev/cu.usbserial-1410, /dev/ttyUSB0, COM3
  std::string name;    // friendly name when cheaply available
  std::string detail;  // extra info (driver, by-id name, tty counterpart...)
};
std::vector<SerialDeviceInfo> enumerate_serial_ports();

class SerialPort {
 public:
  // `loop` may be null only for lines_only ports (PTT keying without data I/O).
  explicit SerialPort(Loop* loop);
  ~SerialPort();
  SerialPort(const SerialPort&) = delete;
  SerialPort& operator=(const SerialPort&) = delete;

  // Opens and configures the port. lines_only: no reads/writes, just modem-control lines.
  bool open(const SerialConfig& cfg, bool lines_only, std::string* err);
  void close();  // never calls callbacks
  bool is_open() const;

  // Buffers and writes. Beyond kMaxPending queued bytes new data is dropped (and logged).
  void write(const uint8_t* data, size_t len);
  size_t pending() const;
  uint64_t dropped_bytes() const { return dropped_; }

  bool set_rts(bool on, std::string* err = nullptr);
  bool set_dtr(bool on, std::string* err = nullptr);

  std::function<void(const uint8_t* data, size_t len)> on_data;
  std::function<void(const std::string& error)> on_error;  // port failed / unplugged; already closed

  static constexpr size_t kMaxPending = size_t(1) << 20;

 private:
  void fail(const std::string& why);
  Loop* loop_;
  uint64_t dropped_ = 0;
  bool lines_only_ = false;
  std::string path_;
  std::shared_ptr<bool> alive_;
#ifdef _WIN32
  struct Win;
  std::shared_ptr<Win> win_;
#else
  void on_io(short re);
  void update_events();
  void flush();
  int fd_ = -1;
  uint64_t watch_ = 0;
  std::string out_;
  size_t out_off_ = 0;
  bool overflow_logged_ = false;
#endif
};

}  // namespace ember_host
