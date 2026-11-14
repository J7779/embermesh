// CM108 / CM119 (and compatible: AIOC in CM108 mode, many "USB sound card with PTT" interfaces)
// HID GPIO transmitter keying. The sound chip exposes a HID device whose output report sets GPIO
// pins; interfaces wire one pin (usually GPIO3) to the radio's PTT.
//
// Output report (same as Direwolf / hamlib): [report id 0] 0x00, pin mask, pin values, 0x00.
// Linux: write() to /dev/hidrawN (needs read/write permission, e.g. a udev rule or the `plugdev`
// group). macOS: IOKit HID IOHIDDeviceSetReport. Other platforms: reported as unsupported.
// Built with EMBER_HOST_AUDIO (it only makes sense with the sound-card endpoint). Not tested on real
// hardware.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "host/endpoints/ptt.h"
#include "host/util.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#define EMBER_CM108_LINUX 1
#elif defined(__APPLE__) && TARGET_OS_OSX
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#define EMBER_CM108_MAC 1
#endif

namespace ember_host {

namespace {

// Known VID:PID of CM108-compatible chips and interfaces.
struct HidId {
  uint16_t vid, pid;
  const char* name;
};
const HidId kKnown[] = {
    {0x0d8c, 0x000c, "C-Media CM108"},  {0x0d8c, 0x000e, "C-Media CM108"},  {0x0d8c, 0x0008, "C-Media CM108"},
    {0x0d8c, 0x013c, "C-Media CM108"},  {0x0d8c, 0x0012, "C-Media CM108B"}, {0x0d8c, 0x013a, "C-Media CM119"},
    {0x0d8c, 0x0013, "C-Media CM119"},  {0x0d8c, 0x0014, "C-Media CM119B"}, {0x0d8c, 0x0139, "C-Media CM119A"},
    {0x0d8c, 0x0016, "C-Media CM119B"}, {0x1209, 0x7388, "AIOC (All-In-One-Cable)"}};

[[maybe_unused]] bool known_id(uint16_t vid, uint16_t pid) {
  for (const auto& k : kKnown)
    if (k.vid == vid && k.pid == pid) return true;
  return vid == 0x0d8c;  // other C-Media parts with GPIO
}

[[maybe_unused]] void make_report(uint8_t out[5], int gpio, bool on) {
  uint8_t mask = uint8_t(1u << (gpio - 1));
  out[0] = 0x00;  // report id
  out[1] = 0x00;
  out[2] = mask;  // pins to drive (direction)
  out[3] = on ? mask : 0x00;
  out[4] = 0x00;
}

#if defined(EMBER_CM108_LINUX)
// /sys/class/hidraw/hidrawN/device/uevent contains HID_ID=0003:00000D8C:0000000C
std::string find_hidraw_auto(std::string* what) {
  DIR* d = ::opendir("/sys/class/hidraw");
  if (!d) return std::string();
  std::string found;
  while (dirent* e = ::readdir(d)) {
    std::string n = e->d_name;
    if (n.compare(0, 6, "hidraw") != 0) continue;
    std::string ue;
    if (!read_file("/sys/class/hidraw/" + n + "/device/uevent", &ue, 8192)) continue;
    size_t p = ue.find("HID_ID=");
    if (p == std::string::npos) continue;
    unsigned bus = 0, vid = 0, pid = 0;
    if (std::sscanf(ue.c_str() + p + 7, "%x:%x:%x", &bus, &vid, &pid) != 3) continue;
    if (!known_id(uint16_t(vid), uint16_t(pid))) continue;
    found = "/dev/" + n;
    *what = format("%04x:%04x", vid, pid);
    break;
  }
  ::closedir(d);
  return found;
}

class Cm108Ptt : public PttControl {
 public:
  Cm108Ptt(int fd, std::string path, int gpio, bool invert) : fd_(fd), path_(std::move(path)), gpio_(gpio), invert_(invert) {
    set(false);
  }
  ~Cm108Ptt() override {
    set(false);
    ::close(fd_);
  }
  void set(bool on) override {
    uint8_t r[5];
    make_report(r, gpio_, on != invert_);
    ok_ = ::write(fd_, r, sizeof r) == ssize_t(sizeof r);
    if (!ok_) EH_LOGW("cm108 ptt %s: write failed: %s", path_.c_str(), std::strerror(errno));
  }
  bool ok() const override { return ok_; }
  std::string describe() const override { return format("CM108 GPIO%d on %s", gpio_, path_.c_str()); }

 private:
  int fd_;
  std::string path_;
  int gpio_;
  bool invert_;
  bool ok_ = true;
};
#endif

#if defined(EMBER_CM108_MAC)
uint16_t cf_int(IOHIDDeviceRef dev, CFStringRef key) {
  CFTypeRef v = IOHIDDeviceGetProperty(dev, key);
  int32_t n = 0;
  if (v && CFGetTypeID(v) == CFNumberGetTypeID()) CFNumberGetValue(static_cast<CFNumberRef>(v), kCFNumberSInt32Type, &n);
  return uint16_t(n);
}

class Cm108Ptt : public PttControl {
 public:
  Cm108Ptt(IOHIDManagerRef mgr, IOHIDDeviceRef dev, std::string what, int gpio, bool invert)
      : mgr_(mgr), dev_(dev), what_(std::move(what)), gpio_(gpio), invert_(invert) {
    CFRetain(dev_);
    set(false);
  }
  ~Cm108Ptt() override {
    set(false);
    IOHIDDeviceClose(dev_, kIOHIDOptionsTypeNone);
    CFRelease(dev_);
    IOHIDManagerClose(mgr_, kIOHIDOptionsTypeNone);
    CFRelease(mgr_);
  }
  void set(bool on) override {
    uint8_t r[5];
    make_report(r, gpio_, on != invert_);
    ok_ = IOHIDDeviceSetReport(dev_, kIOHIDReportTypeOutput, 0, r + 1, 4) == kIOReturnSuccess;
    if (!ok_) EH_LOGW("cm108 ptt %s: IOHIDDeviceSetReport failed", what_.c_str());
  }
  bool ok() const override { return ok_; }
  std::string describe() const override { return format("CM108 GPIO%d on %s", gpio_, what_.c_str()); }

 private:
  IOHIDManagerRef mgr_;
  IOHIDDeviceRef dev_;
  std::string what_;
  int gpio_;
  bool invert_;
  bool ok_ = true;
};
#endif

}  // namespace

std::vector<std::string> list_cm108_devices() {
  std::vector<std::string> r;
#if defined(EMBER_CM108_LINUX)
  DIR* d = ::opendir("/sys/class/hidraw");
  if (!d) return r;
  while (dirent* e = ::readdir(d)) {
    std::string n = e->d_name;
    if (n.compare(0, 6, "hidraw") != 0) continue;
    std::string ue;
    if (!read_file("/sys/class/hidraw/" + n + "/device/uevent", &ue, 8192)) continue;
    size_t p = ue.find("HID_ID=");
    unsigned bus = 0, vid = 0, pid = 0;
    if (p == std::string::npos || std::sscanf(ue.c_str() + p + 7, "%x:%x:%x", &bus, &vid, &pid) != 3) continue;
    if (known_id(uint16_t(vid), uint16_t(pid))) r.push_back(format("/dev/%s (%04x:%04x)", n.c_str(), vid, pid));
  }
  ::closedir(d);
#elif defined(EMBER_CM108_MAC)
  IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
  IOHIDManagerSetDeviceMatching(mgr, nullptr);
  CFSetRef set = IOHIDManagerCopyDevices(mgr);
  if (set) {
    CFIndex n = CFSetGetCount(set);
    std::vector<const void*> devs(static_cast<size_t>(n), nullptr);
    CFSetGetValues(set, devs.data());
    for (auto v : devs) {
      IOHIDDeviceRef dev = static_cast<IOHIDDeviceRef>(const_cast<void*>(v));
      uint16_t vid = cf_int(dev, CFSTR(kIOHIDVendorIDKey)), pid = cf_int(dev, CFSTR(kIOHIDProductIDKey));
      if (known_id(vid, pid)) r.push_back(format("%04x:%04x", vid, pid));
    }
    CFRelease(set);
  }
  CFRelease(mgr);
#endif
  return r;
}

std::unique_ptr<PttControl> make_cm108_ptt(const std::string& device, int gpio, bool invert, std::string* err) {
  if (gpio < 1 || gpio > 8) {
    *err = "ptt_gpio must be 1..8 (most interfaces: 3)";
    return nullptr;
  }
#if defined(EMBER_CM108_LINUX)
  std::string path = device, what;
  if (path.empty() || path == "auto") {
    path = find_hidraw_auto(&what);
    if (path.empty()) {
      *err = "no CM108/CM119-compatible HID device found under /sys/class/hidraw";
      return nullptr;
    }
  }
  int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
  if (fd < 0) {
    *err = "open " + path + ": " + std::strerror(errno) + " (needs write access: udev rule or plugdev group)";
    return nullptr;
  }
  return std::unique_ptr<PttControl>(new Cm108Ptt(fd, path, gpio, invert));
#elif defined(EMBER_CM108_MAC)
  // device: "auto" / "" or "VID:PID" in hex.
  unsigned want_vid = 0, want_pid = 0;
  if (!device.empty() && device != "auto" && std::sscanf(device.c_str(), "%x:%x", &want_vid, &want_pid) != 2) {
    *err = "ptt_device must be auto or VID:PID (hex), e.g. 0d8c:000c";
    return nullptr;
  }
  IOHIDManagerRef mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
  IOHIDManagerSetDeviceMatching(mgr, nullptr);
  IOHIDManagerOpen(mgr, kIOHIDOptionsTypeNone);
  CFSetRef set = IOHIDManagerCopyDevices(mgr);
  IOHIDDeviceRef pick = nullptr;
  std::string what;
  if (set) {
    CFIndex n = CFSetGetCount(set);
    std::vector<const void*> devs(static_cast<size_t>(n), nullptr);
    CFSetGetValues(set, devs.data());
    for (auto v : devs) {
      IOHIDDeviceRef dev = static_cast<IOHIDDeviceRef>(const_cast<void*>(v));
      uint16_t vid = cf_int(dev, CFSTR(kIOHIDVendorIDKey)), pid = cf_int(dev, CFSTR(kIOHIDProductIDKey));
      bool match = want_vid ? (vid == want_vid && pid == want_pid) : known_id(vid, pid);
      if (!match) continue;
      if (IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone) != kIOReturnSuccess) continue;
      pick = dev;
      what = format("HID %04x:%04x", vid, pid);
      break;
    }
  }
  std::unique_ptr<PttControl> r;
  if (pick) r.reset(new Cm108Ptt(mgr, pick, what, gpio, invert));
  if (set) CFRelease(set);
  if (!pick) {
    IOHIDManagerClose(mgr, kIOHIDOptionsTypeNone);
    CFRelease(mgr);
    *err = "no CM108/CM119-compatible HID device found (or it could not be opened)";
  }
  return r;
#else
  (void)device;
  (void)invert;
  *err = "CM108 HID PTT is not supported on this platform";
  return nullptr;
#endif
}

}  // namespace ember_host
