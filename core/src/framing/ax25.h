// EmberMesh framing — AX.25 v2.2 frames (address field, control, PID, info) and FCS helpers.
//
// Byte layout of a frame as carried inside KISS (no flags, no FCS, no bit stuffing — bit-level
// HDLC/NRZI is the modem's job):
//
//   dst(7) src(7) [digi(7) x 0..8] control(1) [pid(1)] info(0..N)
//
// Each address is 6 ASCII characters (A-Z, 0-9, space padded) shifted left one bit, followed by
// an SSID byte  C/H R R S S S S X : bit7 = C (command/response) for dst/src or H ("has been
// repeated") for digipeaters, bits 6-5 reserved (normally 1), bits 4-1 SSID 0..15, bit0 = 1 on
// the last address of the field. On air the frame is followed by the FCS: CRC-16/X-25 over all
// bytes, transmitted low byte first (append_fcs / check_fcs).
//
// decode() is strict and safe on arbitrary input: wrong extension bits, bad characters, embedded
// spaces, empty callsigns, more than 8 digipeaters, a missing control/PID byte, or an info field
// longer than `max_info` are all rejected.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ember/bytes.h"

namespace ember::ax25 {

constexpr size_t kMaxDigis = 8;
constexpr size_t kMaxCallLen = 6;
constexpr size_t kAddrLen = 7;
constexpr uint8_t kControlUI = 0x03;     // UI frame, P/F = 0
constexpr uint8_t kPidNoLayer3 = 0xF0;   // used by APRS and plain text
constexpr size_t kDefaultMaxInfo = 2048;  // generous; AX.25 N1 default is 256

struct Address {
  std::string call;     // 1..6 characters A-Z / 0-9 (upper case)
  uint8_t ssid = 0;     // 0..15
  bool flag = false;    // C bit (dst/src) or H bit (digipeater)
  uint8_t reserved = 3; // the two RR bits as received (round-tripped on encode)

  bool valid() const;
  // "N0CALL", "N0CALL-7" (SSID 0 is omitted). The flag is not included.
  std::string to_string() const;
  friend bool operator==(const Address& a, const Address& b) {
    return a.call == b.call && a.ssid == b.ssid;  // identity: flags/reserved bits ignored
  }
  friend bool operator!=(const Address& a, const Address& b) { return !(a == b); }
};

// Parses "CALL", "CALL-SSID" (SSID 0..15, no leading '+', at most 2 digits) with an optional
// trailing '*' which sets `flag`. Lower case letters are upper-cased. Returns false if invalid.
bool parse_address(std::string_view text, Address* out);
// Convenience: returns a default Address (call empty, invalid) on failure.
Address make_address(std::string_view text);

// Encodes one 7-byte address. Returns false if the address is invalid.
bool encode_address(const Address& a, bool last, uint8_t out[kAddrLen]);
// Decodes one 7-byte address; *last receives the extension bit. Returns false if malformed.
bool decode_address(const uint8_t in[kAddrLen], Address* out, bool* last);

enum class CommandResponse : uint8_t { Command, Response, Legacy };

struct Frame {
  Address dst;
  Address src;
  std::vector<Address> digis;  // 0..8 entries, flag = H bit
  uint8_t control = kControlUI;
  uint8_t pid = kPidNoLayer3;   // only meaningful when has_pid()
  Bytes info;

  // I frames and UI frames carry a PID byte (modulo-8 control field).
  bool has_pid() const { return (control & 0x01) == 0 || (control & 0xEF) == 0x03; }
  bool is_ui() const { return (control & 0xEF) == 0x03; }
  bool poll_final() const { return (control & 0x10) != 0; }
  // AX.25 v2: dst.C=1/src.C=0 is a command, dst.C=0/src.C=1 a response; equal bits = v1/legacy.
  CommandResponse command_response() const;
  void set_command(bool command) { dst.flag = command; src.flag = !command; }
  // Index of the first digipeater whose H bit is clear (the next station to repeat), or -1.
  int next_digi() const;
};

// Builds a UI frame (control 0x03, PID 0xF0 by default), marked as a command.
Frame make_ui(const Address& dst, const Address& src, const std::vector<Address>& digis, ByteView info,
              uint8_t pid = kPidNoLayer3);

// Serialises a frame without FCS. Returns false (out cleared) if an address is invalid or there are
// more than 8 digipeaters. A PID byte is written iff frame.has_pid().
bool encode(const Frame& f, Bytes* out);
Bytes encode(const Frame& f);  // empty on failure

// Parses a frame without FCS (as delivered by KISS). Returns false on malformed input.
bool decode(ByteView in, Frame* out, size_t max_info = kDefaultMaxInfo);

// FCS = CRC-16/X-25 over the frame, transmitted low byte first.
uint16_t fcs(ByteView frame_without_fcs);
void append_fcs(Bytes& frame);
// True if the last two bytes are the correct FCS of the preceding bytes (needs >= 3 bytes).
bool check_fcs(ByteView frame_with_fcs);
// check_fcs() + decode() of the bytes before the FCS.
bool decode_with_fcs(ByteView in, Frame* out, size_t max_info = kDefaultMaxInfo);

}  // namespace ember::ax25
