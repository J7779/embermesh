// EmberMesh core — the Link contract.
//
// A *Link* is a protocol driver that carries opaque mesh frames between this node and whoever is
// listening on some medium. A *Endpoint* is the platform I/O resource underneath it (serial port,
// TCP socket, BLE characteristic, sound card, WebRTC data channel ...). Endpoints live in platform
// code (Kotlin / Swift / JS / desktop host layer) and only move bytes or audio samples; every bit
// of protocol knowledge (KISS, AX.25, modems, Meshtastic, RNode ...) lives in Link drivers in the
// shared core so it is identical on every platform.
//
//   mesh engine  --send(frame)-->  Link driver  --link_io_write(bytes)-->  platform endpoint
//   mesh engine  <--link_frame_received--  Link driver  <--on_io_bytes / on_audio--  platform endpoint
//
// Threading: the core is single-threaded. All Link methods are called from the node's thread.
// No exceptions, no RTTI. Config and status cross this boundary as JSON text.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "ember/bytes.h"

namespace ember {

class Link;

struct RxMeta {
  bool has_rssi = false;
  bool has_snr = false;
  float rssi_dbm = 0;  // received signal strength, if the medium reports it
  float snr_db = 0;    // signal-to-noise ratio, if the medium/modem reports it
  uint8_t corrected_errors = 0;  // symbols/bits repaired by FEC for this frame (0 if n/a)
};

struct LinkCaps {
  uint16_t mtu = 200;           // largest mesh frame (bytes) accepted by send(); mesh fragments above this
  uint32_t bitrate_bps = 1200;  // nominal throughput, used for airtime accounting and scheduling
  bool broadcast = true;        // one transmission reaches every listener (radio, UDP multicast, hub bus)
  bool half_duplex = true;      // cannot receive while transmitting
  bool reliable = false;        // medium guarantees delivery + order (TCP, WebSocket, reliable WebRTC)
  // False on links where regulations forbid obscuring content (amateur radio bands). The mesh only
  // ever sends cleartext, signed payloads over such links and never forwards ciphertext onto them.
  bool encryption_allowed = true;
  bool requires_station_id = false;  // frames must carry the operator's callsign (AX.25 source address)
  uint16_t duty_cycle_permille = 1000;  // legal/advised max transmit duty cycle: 1000 = unlimited, 100 = 10%, 10 = 1%
  uint8_t cost = 10;  // routing preference; lower is preferred (LAN 1, BLE 3, LoRa 20, 1200bd AFSK 40, HF 80)
};

enum class LinkState : uint8_t { Down = 0, Connecting = 1, Up = 2, Error = 3 };

// Text heard from / sent to stations that do not speak the Ember mesh protocol
// (APRS messages, Meshtastic text channel, plain RTTY/PSK31/CW keyboard text ...).
struct InteropText {
  std::string from;     // foreign station id: callsign, Meshtastic node id, or "" if unknown (e.g. CW)
  std::string to;       // foreign addressee, or "" for broadcast / unaddressed
  std::string text;     // UTF-8
  std::string network;  // "aprs", "meshtastic", "rtty", "psk31", "cw", ...
};

// Services the node provides to link drivers.
class LinkHost {
 public:
  virtual ~LinkHost() = default;
  // A complete, integrity-checked mesh frame arrived on this link.
  virtual void link_frame_received(Link& link, ByteView frame, const RxMeta& meta) = 0;
  // Bytes to write to the platform endpoint. For datagram endpoints one call == one datagram.
  virtual void link_io_write(Link& link, ByteView bytes) = 0;
  // Foreign (non-Ember) text was heard.
  virtual void link_interop_text(Link& link, const InteropText& msg) = 0;
  // Driver-originated event forwarded to the app as "link.<name>", e.g. name="ptt",
  // json=R"({"on":true})". The node adds "link_id". State changes need no event: the node polls state().
  virtual void link_event(Link& link, const char* name, const std::string& json_object) = 0;
  virtual uint64_t now_ms() const = 0;
  virtual void random_bytes(uint8_t* out, size_t n) = 0;
};

class Link {
 public:
  virtual ~Link() = default;

  virtual const char* type() const = 0;   // registry name: "raw", "stream", "kiss", "audio", ...
  virtual LinkCaps caps() const = 0;      // may change after reconfigure()
  virtual LinkState state() const = 0;

  // Queue one mesh frame (size <= caps().mtu) for transmission. priority: 0 = highest (SOS) .. 7 = bulk.
  // Returns false if the frame is too large or the driver's queue is full (caller retries later).
  virtual bool send(ByteView frame, uint8_t priority) = 0;
  virtual size_t tx_queue_depth() const { return 0; }
  // Carrier sense: true while the medium is busy (someone else transmitting, or we are).
  virtual bool channel_busy() const { return false; }

  // --- input from the platform endpoint -------------------------------------------------------
  virtual void on_io_bytes(ByteView data) {}       // stream bytes, or exactly one datagram
  virtual void on_io_state(bool connected) {}      // endpoint opened / closed underneath us
  // Mono PCM received from the radio / microphone. Drivers must accept any rate >= 8000 Hz.
  virtual void on_audio(const int16_t* samples, size_t count, uint32_t sample_rate) {}
  // Fill `out` with up to `max` mono PCM samples to transmit. Returns the number written; 0 = idle.
  // While a transmission is in progress the driver must return exactly `max` (pad with silence).
  virtual size_t pull_audio(int16_t* out, size_t max, uint32_t sample_rate) { return 0; }

  virtual void tick(uint64_t now_ms) {}

  // Send plain text to non-Ember stations on this medium, if the driver supports it.
  virtual bool send_interop_text(const InteropText& msg) { return false; }

  // JSON object with driver-specific status/statistics for the UI.
  virtual std::string status_json() const { return "{}"; }
  // Apply new configuration (same schema as at creation). Returns false and sets *error on bad config.
  virtual bool reconfigure(const std::string& config_json, std::string* error) { return false; }
};

// --- driver registry (implemented in core/src/link/) --------------------------------------------

// Creates a link driver. `config_json` is a JSON object following the schema advertised by
// link_types_json() for that type. Returns nullptr and sets *error for unknown types / bad config.
std::unique_ptr<Link> create_link(const std::string& type, const std::string& config_json,
                                  LinkHost& host, std::string* error);

// JSON array describing every registered link type for schema-driven UI forms:
//   [{"type":"kiss","title":"KISS TNC","description":"...","endpoint_kinds":["serial","tcp","ble","bt_spp"],
//     "config":[ <Field>, ... ]}]            (Field schema: see docs/RPC_API.md)
std::string link_types_json();

}  // namespace ember
