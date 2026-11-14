// EmberMesh core / link — shared plumbing for every link driver:
//   * Field / Schema: the Field-schema (docs/RPC_API.md) used by links.types, plus a strict config
//     parser that fills defaults and range-checks hostile JSON.
//   * TxQueue: bounded 8-level strict-priority frame queue with back-pressure.
//   * LinkBase: common state, counters, status JSON and host helpers.
//   * Pacer: airtime pacing for half-duplex modems / radios that must not be overrun.
//   * LinkTypeInfo: registry entry (see registry.cpp and drivers.h).
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/json.h"
#include "ember/bytes.h"
#include "ember/link.h"

namespace ember {
namespace link {

// --- Field schema --------------------------------------------------------------------------------

struct FieldOption {
  std::string value;
  std::string label;
};

struct Field {
  std::string key;
  std::string label;
  std::string type = "text";  // number | text | select | bool | textarea | password
  Json dflt;                  // default value (number / bool / string)
  std::string unit;
  bool has_min = false, has_max = false;
  double min = 0, max = 0, step = 0;  // step 0 = not advertised; integer fields use step 1
  bool integer = false;               // numbers must be integral
  std::vector<FieldOption> options;   // select
  std::string help;
  bool required = false;
  bool advanced = false;
  size_t max_len = 256;  // text / textarea / password: longest accepted string (bytes)

  Field& adv() { advanced = true; return *this; }
  Field& req() { required = true; return *this; }
  Field& len(size_t n) { max_len = n; return *this; }
  Field& with_unit(const char* u) { unit = u; return *this; }
  Json to_json() const;
};

using Schema = std::vector<Field>;

// Integer number field.
Field num(const char* key, const char* label, int64_t dflt, int64_t min, int64_t max, const char* help,
          const char* unit = "");
// Real number field.
Field real(const char* key, const char* label, double dflt, double min, double max, double step,
           const char* help, const char* unit = "");
Field boolean(const char* key, const char* label, bool dflt, const char* help);
Field text(const char* key, const char* label, const char* dflt, const char* help, size_t max_len = 256);
Field select(const char* key, const char* label, const char* dflt, std::vector<FieldOption> options,
             const char* help);

Json schema_json(const Schema& s);

// Parses `config_json` (empty / whitespace = {}) against the schema: missing or null keys take the
// default, present keys must have the right type (numbers in range and integral where required,
// selects one of their options, strings valid UTF-8 within max_len). Unknown keys are ignored.
// On success *out is an object holding every schema key. Returns false + *error otherwise.
bool parse_config(const Schema& schema, const std::string& config_json, Json* out, std::string* error);

// --- priority queue ------------------------------------------------------------------------------

constexpr uint8_t kPriorities = 8;

class TxQueue {
 public:
  TxQueue(size_t max_frames = 64, size_t max_bytes = 64 * 1024) : max_frames_(max_frames), max_bytes_(max_bytes) {}
  void set_limits(size_t max_frames, size_t max_bytes);

  // Queues a copy. When full, newer frames of strictly lower priority (higher number) are evicted to
  // make room (counted in evicted()); if that is not possible the frame is refused (false).
  bool push(ByteView frame, uint8_t priority);
  // Highest priority first, FIFO within a priority.
  bool pop(Bytes* frame, uint8_t* priority = nullptr);
  const Bytes* peek(uint8_t* priority = nullptr) const;
  // Pops only if the head frame is no larger than max_size (used to batch frames into one burst).
  bool pop_if_fits(size_t max_size, Bytes* frame, uint8_t* priority = nullptr);
  void clear();

  size_t size() const { return frames_; }
  size_t bytes() const { return bytes_; }
  bool empty() const { return frames_ == 0; }
  uint64_t evicted() const { return evicted_; }
  size_t max_frames() const { return max_frames_; }

 private:
  std::deque<Bytes> q_[kPriorities];
  size_t frames_ = 0, bytes_ = 0;
  size_t max_frames_, max_bytes_;
  uint64_t evicted_ = 0;
};

// --- common driver base --------------------------------------------------------------------------

struct LinkStats {
  uint64_t tx_frames = 0, tx_bytes = 0;    // mesh frames handed to the medium
  uint64_t rx_frames = 0, rx_bytes = 0;    // mesh frames delivered to the node
  uint64_t rx_errors = 0;                  // malformed / failed-check / oversize input units
  uint64_t tx_refused = 0;                 // send() returned false
  uint64_t io_tx_bytes = 0, io_rx_bytes = 0;  // raw endpoint bytes
  uint64_t interop_rx = 0, interop_tx = 0;
  uint64_t airtime_ms = 0;                 // estimated time on air (radio drivers)
  uint64_t last_rx_ms = 0;
};

class LinkBase : public Link {
 public:
  LinkState state() const override { return state_; }
  size_t tx_queue_depth() const override { return q_.size(); }

 protected:
  explicit LinkBase(LinkHost& host) : host_(host) {}

  // State transition; `error` (may be null) becomes last_error when given.
  void set_state(LinkState s, const char* error = nullptr);
  void set_error(const std::string& e) { last_error_ = e; }
  // Delivers a mesh frame to the node (counts it).
  void deliver(ByteView frame, const RxMeta& meta);
  void io_write(ByteView bytes);
  void interop(const InteropText& t);
  void event(const char* name, const Json& obj);
  uint64_t now() const { return host_.now_ms(); }
  uint8_t random_u8();
  uint32_t random_u32();
  // Common send() front half: rejects empty / oversize frames and full queues, counting refusals.
  bool enqueue(ByteView frame, uint8_t priority, size_t mtu);
  // Base status object: state, last_error, counters, queue.
  Json base_status() const;
  static const char* state_name(LinkState s);

  LinkHost& host_;
  LinkState state_ = LinkState::Down;
  std::string last_error_;
  TxQueue q_;
  LinkStats st_;
};

Json caps_json(const LinkCaps& c);

// Airtime (ms, rounded up) for `bytes` at `bps`.
uint64_t airtime_ms(size_t bytes, uint32_t bps);

// Write pacing: after a burst of `bytes` at `bps` (+ gap_ms), the next burst may start at ready_at().
class Pacer {
 public:
  bool ready(uint64_t now) const { return now >= next_; }
  void spend(uint64_t now, size_t bytes, uint32_t bps, uint32_t gap_ms);
  void reset() { next_ = 0; }
  uint64_t ready_at() const { return next_; }

 private:
  uint64_t next_ = 0;
};

// Small deterministic duty-cycle tracker: sliding window of transmissions (bounded).
class DutyCycle {
 public:
  explicit DutyCycle(uint32_t window_ms = 3600 * 1000) : window_(window_ms) {}
  void set_window(uint32_t ms) { window_ = ms; }
  void add(uint64_t now, uint32_t ms);
  // Airtime used in the window ending at now.
  uint64_t used(uint64_t now);
  // True if `ms` more airtime keeps the window at or below permille/1000.
  bool allows(uint64_t now, uint32_t ms, uint16_t permille);

 private:
  struct E { uint64_t t; uint32_t ms; };
  std::deque<E> e_;
  uint64_t sum_ = 0;
  uint32_t window_;
};

// --- registry entry ------------------------------------------------------------------------------

using CreateFn = std::unique_ptr<Link> (*)(const Json& config, LinkHost& host, std::string* error);

struct LinkTypeInfo {
  const char* type;
  const char* title;
  const char* description;
  std::vector<const char*> endpoint_kinds;
  Schema (*schema)();
  CreateFn create;  // config already validated by parse_config(schema())
  // Optional: fills preset-dependent values into `cfg` (already parsed, defaults filled) for every
  // key the caller did not give explicitly in `given` (the raw config object). Explicit keys win.
  void (*apply_defaults)(const Json& given, Json* cfg) = nullptr;
};

// Full effective configuration of a link: every schema key, defaults filled, the type's preset
// (apply_defaults) applied for keys not given in `config_json`. Returns false + *error for an
// unknown type or invalid config.
bool effective_config(const std::string& type, const std::string& config_json, Json* out, std::string* error);

// --- misc helpers --------------------------------------------------------------------------------

// Parses a printable escape string such as "KISS ON\rRESTART\r" ("\r", "\n", "\t", "\\", "\xHH").
bool unescape_text(const std::string& in, Bytes* out);
// Trims ASCII whitespace.
std::string trim(const std::string& s);
// Splits on a separator, trimming parts, dropping empty ones.
std::vector<std::string> split_list(const std::string& s, char sep);
// Converts arbitrary bytes into printable UTF-8 for interop text: CR/LF/TAB and U+2028/2029 become
// spaces; C0/C1 controls, DEL and bidi override/isolate characters are dropped; invalid UTF-8 is
// repaired; the result is cut to max_bytes on a code point boundary.
std::string printable_text(ByteView b, size_t max_bytes = 512);
// Bounds applied to every received InteropText by LinkBase::interop().
constexpr size_t kMaxInteropTextBytes = 512;
constexpr size_t kMaxInteropAddrBytes = 32;
// Strict decimal integer parse of a whole string (optional leading '-', <= 19 digits, in int64
// range; anything else, including overflow, returns false). No exceptions, no UB.
bool parse_int(const std::string& s, int64_t* out);

// Standard base64 (RFC 4648, with padding).
std::string base64_encode(ByteView b);
bool base64_decode(const std::string& s, Bytes* out, size_t max_out);

}  // namespace link
}  // namespace ember
