// SPDX-License-Identifier: MIT
// The hub's `/mqtt` endpoint: a minimal MQTT 3.1.1 broker over WebSocket (subprotocol `mqtt`) so a
// hub reachable from the internet is an internet relay with no third party (docs/CONNECTIVITY.md
// §4.12). It uses the core's codec (core/src/framing/mqtt.*) and implements only what relay links
// need: CONNECT/CONNACK, SUBSCRIBE/SUBACK, UNSUBSCRIBE/UNSUBACK, PUBLISH (QoS 0), PINGREQ/PINGRESP,
// DISCONNECT.
//
// It runs on its OWN listener and port with no other routes (CONNECTIVITY §4.12): exposing it to
// the internet never exposes the hub's web app, /ws or /rpc.
//
// Topics and topic filters must be exactly 32 lowercase hex characters (what relay links use): a
// SUBSCRIBE to anything else — `#`, `+`, `$…`, any other string — gets SUBACK 0x80, so nobody can
// watch other users' topics; a PUBLISH to anything else is denied (dropped and counted). Retain, will
// and QoS > 0 are ignored (QoS > 0 PUBLISHes are dropped); nothing is ever stored; usernames and
// passwords are ignored.
//
// Limits: <= max_clients connections, <= 4 per source address (behind a reverse proxy on this
// machine the source is the last X-Forwarded-For entry); per connection <= 256 subscriptions,
// packets <= 4400 bytes (a sealed relay frame is at most 4137 bytes), 100 PUBLISH/s and 512 KiB/s
// (token buckets), CONNECT before any other packet; per topic <= 50 PUBLISH/s across all publishers;
// a subscriber that stops reading has messages dropped (bounded buffer) and is disconnected after
// 15 s of drops; a client that keeps exceeding its rate is disconnected. It never logs topics,
// client ids or payloads.
#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>

#include "framing/mqtt.h"
#include "host/json.h"
#include "host/server.h"

namespace ember_host {

class MqttBroker {
 public:
  struct Options {
    size_t max_clients = 200;
    size_t max_per_source = 4;
    size_t max_subscriptions = 256;
    size_t max_packet = 4400;
    double publish_per_s = 100;
    double bytes_per_s = 512 * 1024;
    double topic_publish_per_s = 50;
    uint64_t connect_timeout_ms = 10000;
    uint64_t slow_evict_ms = 15000;
    uint64_t abuse_drops = 1000;  // rate-limited messages within 10 s before a client is dropped
  };
  explicit MqttBroker(Options o) : opt_(o) {}
  MqttBroker() : MqttBroker(Options()) {}
  // Registers the WebSocket route on the server (path "/mqtt").
  void attach(HttpServer& server, const std::string& path = "/mqtt");
  Json stats() const;
  size_t clients() const { return clients_; }

 private:
  struct Client;
  struct TopicRate {
    double tokens = 0;
    uint64_t last = 0;
  };
  void on_open(WsConn& c);
  void on_message(WsConn& c, uint8_t op, std::string& data);
  void on_close(WsConn& c);
  void handle(WsConn& c, Client& cl, ember::mqtt::Packet& p);
  void deliver(const std::string& topic, const std::string& packet, WsConn* from);
  bool topic_ok(const std::string& t);

  Options opt_;
  HttpServer* server_ = nullptr;
  std::string path_;
  std::map<std::string, std::set<WsConn*>> subs_;  // topic -> subscribers
  std::map<std::string, TopicRate> topic_rate_;
  std::map<std::string, size_t> per_source_;  // open connections per source address
  std::string pending_source_;                // set by authorize, taken by on_open
  size_t clients_ = 0;
  uint64_t msgs_in_ = 0, msgs_out_ = 0, dropped_rate_ = 0, dropped_slow_ = 0, refused_subs_ = 0, evicted_ = 0,
           protocol_errors_ = 0, clients_total_ = 0, denied_publish_ = 0, refused_source_ = 0;
};

}  // namespace ember_host
