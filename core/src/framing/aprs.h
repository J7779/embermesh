// EmberMesh framing — APRS (Automatic Packet Reporting System, APRS101 + 1.1/1.2 addenda) helpers:
// enough to exchange text messages, acks, bulletins, positions and status with stock APRS radios
// (Kenwood TH-D7x/TM-D7x0, Yaesu FTM-xxx, Mobilinkd/APRSdroid/Direwolf/Xastir ...).
//
// APRS rides in AX.25 UI frames (control 0x03, PID 0xF0); the "info" field is ASCII text whose
// first character is the data type identifier. The builders below return the info field only;
// wrap it with ax25::make_ui(dst = "APZEMB" or similar tocall, src = operator callsign, path =
// e.g. WIDE1-1,WIDE2-1). All parsers are bounds-checked and accept arbitrary bytes safely.
//
// Supported:
//   ':'  messages  ":ADDRESSEE:text{id"  (addressee padded to 9), acks ":ADDRESSEE:ackid",
//        rejects ":ADDRESSEE:rejid", reply-acks "{MM}AA", bulletins/announcements "BLNx".
//   '!' '=' '/' '@'  positions, uncompressed ("4903.50N/07201.75W-") with ambiguity, optional
//        DDHHMMz/DDHHMM//HHMMSSh timestamp, CSE/SPD extension and /A=altitude comment; and
//        compressed base-91 ("/5L!!<*e7>7P[") with course/speed or altitude.
//   '`' '\'' (and legacy 0x1C/0x1D)  Mic-E positions (parse only; needs the AX.25 destination).
//   '>'  status reports.
//   TNC2 monitor format "SRC>DST,DIGI1*,DIGI2:info" (format + parse).
// Not supported (parsed as PacketType::Other): objects/items, weather, telemetry, third-party
// traffic, NMEA, Maidenhead-grid-only positions.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ember/bytes.h"
#include "framing/ax25.h"

namespace ember::aprs {

constexpr size_t kMaxMessageText = 67;  // APRS101 limit for message text
constexpr size_t kMaxInfo = 256;        // AX.25 N1; builders refuse to exceed it

struct Position {
  double lat = 0;  // degrees, + = north
  double lon = 0;  // degrees, + = east
  char symbol_table = '/';  // '/', '\\' or overlay 0-9 / A-Z
  char symbol_code = '>';   // e.g. '>' car, '-' house, '[' person, 'k' truck
  bool messaging = false;   // station accepts messages ('=' / '@'); Mic-E: always false
  bool compressed = false;
  bool mic_e = false;
  uint8_t ambiguity = 0;    // 0..4 trailing digits blanked (uncompressed / Mic-E only)
  std::string timestamp;    // raw 7 chars ("092345z") or empty
  bool has_course_speed = false;
  uint16_t course_deg = 0;  // 0..360 (0 = unknown in APRS convention, 360 = north)
  uint16_t speed_knots = 0;
  bool has_altitude = false;
  int32_t altitude_ft = 0;
  uint8_t mic_e_message = 0;  // Mic-E message bits (0..7), raw
  std::string comment;        // everything after the position/symbol (raw, extensions included)
};

enum class MessageKind : uint8_t { Text, Ack, Rej };

struct Message {
  std::string addressee;  // trimmed, e.g. "N0CALL-9", "BLN1"
  MessageKind kind = MessageKind::Text;
  std::string text;       // Text only
  std::string id;         // message number ("" = unnumbered); for Ack/Rej the acked id
  std::string reply_ack;  // APRS 1.1 reply-ack "{MM}AA" -> id "MM", reply_ack "AA"
  // "BLN0".."BLN9" bulletins, "BLNA".."BLNZ" announcements, "BLN?xxxxx" group bulletins.
  bool is_bulletin() const { return addressee.size() >= 4 && addressee.compare(0, 3, "BLN") == 0; }
};

enum class PacketType : uint8_t { Other, Position, Message, Status };

struct Packet {
  PacketType type = PacketType::Other;
  Position position;
  Message message;
  std::string status;
};

// --- builders (return false if an argument is invalid or the result would exceed kMaxInfo) ------

// ":ADDRESSEE:text{id". text: <= 67 chars, printable ASCII or UTF-8, none of '|' '~' '{'.
// id: "" or 1..5 alphanumerics. Addressee: 1..9 printable chars without ':' or spaces.
bool build_message(std::string_view addressee, std::string_view text, std::string_view id, std::string* out);
bool build_ack(std::string_view addressee, std::string_view id, std::string* out);
bool build_rej(std::string_view addressee, std::string_view id, std::string* out);
// Bulletin "BLN<line>" where line is '0'..'9' (bulletin) or 'A'..'Z' (announcement).
bool build_bulletin(char line, std::string_view text, std::string* out);
// Makes arbitrary chat text acceptable to build_message(): replaces forbidden/control characters
// with '?' (keeping valid UTF-8 sequences) and truncates to 67 bytes on a character boundary.
std::string sanitize_message_text(std::string_view text);

// Position report. Uses p.compressed to pick the format and p.timestamp (if 7 valid chars) for
// '/'/'@'. Uncompressed honours ambiguity, course/speed ("CSE/SPD") and altitude ("/A=").
bool build_position(const Position& p, std::string* out);
// ">text"
bool build_status(std::string_view text, std::string* out);

// --- parsers ---------------------------------------------------------------------------------

bool parse_message(std::string_view info, Message* out);
bool parse_position(std::string_view info, Position* out);   // '!', '=', '/', '@'
// Mic-E: `dest_call` is the AX.25 destination callsign (6 chars, SSID excluded).
bool parse_mic_e(std::string_view dest_call, std::string_view info, Position* out);
bool parse_status(std::string_view info, std::string* out);
// Dispatches on the data type identifier.
Packet parse(const ax25::Frame& frame);
Packet parse(std::string_view dest_call, std::string_view info);

// --- TNC2 monitor format -------------------------------------------------------------------

// "SRC>DST,DIGI1,DIGI2*:info" ('*' after the last digipeater whose H bit is set).
std::string format_tnc2(const ax25::Frame& f);
// Parses a TNC2 line (trailing CR/LF ignored) into a UI command frame. '*' on a digipeater sets
// the H bit on it and every earlier one. Returns false on invalid callsigns (e.g. APRS-IS q
// constructs such as "qAR" are not AX.25 addresses), > 8 digis, or info > kMaxInfo bytes.
bool parse_tnc2(std::string_view line, ax25::Frame* out);

// Base-91 helpers used by compressed positions and Mic-E altitude.
void base91_encode(uint32_t value, size_t digits, std::string* out);
bool base91_decode(std::string_view s, uint32_t* value);

}  // namespace ember::aprs
