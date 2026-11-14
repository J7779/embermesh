// EmberMesh core / link — "relay": the internet relay (docs/CONNECTIVITY.md §4, PROTOCOL.md §12).
//
// One `relay` link = one MQTT 3.1.1 session over one WebSocket endpoint (`wss://…`, subprotocol
// `mqtt`) to one broker. The node gives it the sessions it serves (contact pairs and private
// channels, each with its relay root key); the link exposes one CHILD link per session. The node
// attaches the children (not the parent, whose mtu is 0) to the mesh engine, each with its own
// LinkScope, so reconciliation runs per contact and nothing outside a session's scope crosses it.
//
// The parent never connects while consent is off or while the wall clock is unknown (epochs are
// UTC days), subscribes to topic(E-1), topic(E), topic(E+1) of every session, publishes on
// topic(E), seals every frame (proto/relay.h) and checks replay, rates and the contact's hop id on
// receipt. Hostile brokers are expected: malformed MQTT closes the session (state error).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "ember/link.h"

namespace ember {
namespace link {

struct RelaySession {
  std::string key;           // stable id chosen by the node, e.g. "c:<node id hex>", "ch:<channel id>"
  bool channel = false;      // channel session (any member may send) vs contact pair
  crypto::Key32 root{};      // proto::relay::pair_root / channel_root
  uint8_t peer_hop[4] = {};  // pair sessions: the first 4 bytes of the contact's node id
};

struct RelayStats {
  uint64_t rx_messages = 0, tx_messages = 0, rx_bad_seal = 0, rx_replay = 0, rx_wrong_hop = 0, rx_own_echo = 0,
           rx_out_of_epoch = 0, rx_dropped_rate = 0, rx_unknown_topic = 0, rx_bad_publish = 0, reconnects = 0,
           breaker_trips = 0;
};

// Interface of a `relay` link, obtained with as_relay() (no RTTI in the core).
class RelayControl {
 public:
  virtual ~RelayControl() = default;
  virtual void set_consent(bool on) = 0;
  // Wall-clock UTC ms (0 = unknown). Epochs follow it; the link waits while it is unknown/insane.
  virtual void set_wall_clock(uint64_t unix_ms) = 0;
  // The first 4 bytes of our own node id: frames echoed back by the broker are dropped by it.
  virtual void set_own_hop(const uint8_t hop[4]) = 0;
  // Replaces the session list. Children whose key disappears are destroyed: the node must detach
  // them from the engine BEFORE calling this. Children of kept keys stay the same objects.
  virtual void set_sessions(const std::vector<RelaySession>& sessions) = 0;
  virtual size_t child_count() const = 0;
  virtual Link* child(size_t i) = 0;
  virtual const RelaySession& child_session(size_t i) const = 0;
  virtual const RelayStats& relay_stats() const = 0;
};

// The relay interface of `l` if it is a `relay` link, else null.
RelayControl* as_relay(Link* l);

}  // namespace link
}  // namespace ember
