// SPDX-License-Identifier: MIT
// Minimal MQTT 3.1.1 broker for the hub's /mqtt WebSocket (see mqtt_broker.h).
#include "host/mqtt_broker.h"

#include <algorithm>

#include "host/util.h"
#include "host/ws.h"

namespace ember_host {

namespace mq = ember::mqtt;

namespace {
struct Bucket {
  double tokens, cap, rate;
  uint64_t last;
  Bucket(double c, double r) : tokens(c), cap(c), rate(r), last(steady_ms()) {}
  bool take(double n) {
    uint64_t now = steady_ms();
    tokens = std::min(cap, tokens + double(now - last) * rate / 1000.0);
    last = now;
    if (tokens < n) return false;
    tokens -= n;
    return true;
  }
};
std::string to_str(const ember::Bytes& b) { return std::string(b.begin(), b.end()); }
}  // namespace

struct MqttBroker::Client {
  std::string source;
  explicit Client(const Options& o)
      : dec(o.max_packet), pubs(o.publish_per_s * 2, o.publish_per_s), bytes(o.bytes_per_s * 2, o.bytes_per_s),
        opened(steady_ms()) {}
  mq::Decoder dec;
  Bucket pubs, bytes;
  bool connected = false;
  std::set<std::string> subs;
  uint64_t opened;
  uint64_t dropping_since = 0;
  uint64_t abuse_window = 0, abuse_count = 0;
};

void MqttBroker::attach(HttpServer& server, const std::string& path) {
  server_ = &server;
  path_ = path;
  HttpServer::WsRoute r;
  r.max_message = opt_.max_packet + 64;
  r.max_pending = 512 * 1024;
  r.subprotocols = {"mqtt"};
  r.authorize = [this](const HttpRequest& req, const SockAddr& peer) {
    std::string src = peer.ip();
    if (peer.is_loopback()) {  // a reverse proxy on this machine: the client is the last XFF entry
      std::string xff = req.header_or("x-forwarded-for");
      if (!xff.empty()) {
        std::string last = trim(xff.substr(xff.rfind(',') == std::string::npos ? 0 : xff.rfind(',') + 1));
        if (!last.empty() && last.size() <= 64) src = last;
      }
    }
    if (clients_ >= opt_.max_clients) return 503;
    auto it = per_source_.find(src);
    if (it != per_source_.end() && it->second >= opt_.max_per_source) {
      ++refused_source_;
      return 429;
    }
    pending_source_ = src;
    return 0;
  };
  r.on_open = [this](WsConn& c) { on_open(c); };
  r.on_message = [this](WsConn& c, uint8_t op, std::string& d) { on_message(c, op, d); };
  r.on_close = [this](WsConn& c) { on_close(c); };
  server.add_ws_route(path, r);
}

void MqttBroker::on_open(WsConn& c) {
  Client* cl = new Client(opt_);
  cl->source = pending_source_.empty() ? c.peer().ip() : pending_source_;
  pending_source_.clear();
  ++per_source_[cl->source];
  c.user = cl;
  ++clients_;
  ++clients_total_;
}

void MqttBroker::on_close(WsConn& c) {
  Client* cl = static_cast<Client*>(c.user);
  if (!cl) return;
  for (const auto& t : cl->subs) {
    auto it = subs_.find(t);
    if (it == subs_.end()) continue;
    it->second.erase(&c);
    if (it->second.empty()) subs_.erase(it);
  }
  auto ps = per_source_.find(cl->source);
  if (ps != per_source_.end() && --ps->second == 0) per_source_.erase(ps);
  delete cl;
  c.user = nullptr;
  --clients_;
}

bool MqttBroker::topic_ok(const std::string& t) {
  // Exactly the relay topic format: 32 lowercase hex characters. No wildcards (they would let one
  // client watch everyone's traffic), no `$` system topics, nothing else.
  if (t.size() != 32) return false;
  for (char ch : t)
    if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'))) return false;
  return true;
}

void MqttBroker::on_message(WsConn& c, uint8_t op, std::string& data) {
  Client* cl = static_cast<Client*>(c.user);
  if (!cl) return;
  if (op != kWsBinary) {
    ++protocol_errors_;
    c.close(kWsCloseUnsupported, "MQTT uses binary frames");
    return;
  }
  uint64_t now = steady_ms();
  if (!cl->connected && now - cl->opened > opt_.connect_timeout_ms) {
    c.close(kWsClosePolicy, "CONNECT expected");
    return;
  }
  if (!cl->dec.feed(ember::ByteView(reinterpret_cast<const uint8_t*>(data.data()), data.size()))) {
    ++protocol_errors_;
    c.close(kWsCloseProtocol, "malformed MQTT");
    return;
  }
  mq::Packet p;
  for (int i = 0; i < 256 && c.open(); ++i) {
    if (!cl->dec.next(&p)) break;
    handle(c, *cl, p);
    if (!c.open() || !c.user) return;
  }
  if (cl->dec.failed()) {
    ++protocol_errors_;
    c.close(kWsCloseProtocol, "malformed MQTT");
  }
}

void MqttBroker::handle(WsConn& c, Client& cl, mq::Packet& p) {
  auto send = [&c](const ember::Bytes& b) { return c.send_binary(b.data(), b.size(), false); };
  if (!cl.connected) {
    if (p.type != mq::Type::Connect) {
      ++protocol_errors_;
      c.close(kWsCloseProtocol, "CONNECT expected");
      return;
    }
    cl.connected = true;
    send(mq::connack(false, 0));  // accepted; credentials (if any) are ignored
    return;
  }
  switch (p.type) {
    case mq::Type::Connect:  // a second CONNECT is a protocol violation (MQTT-3.1.0-2)
      ++protocol_errors_;
      c.close(kWsCloseProtocol, "duplicate CONNECT");
      return;
    case mq::Type::Pingreq:
      send(mq::pingresp());
      return;
    case mq::Type::Disconnect:
      c.close(kWsCloseNormal, "");
      return;
    case mq::Type::Subscribe: {
      std::vector<uint8_t> codes;
      for (const auto& f : p.filters) {
        bool ok = topic_ok(f.first) && (cl.subs.count(f.first) || cl.subs.size() < opt_.max_subscriptions);
        if (ok) {
          cl.subs.insert(f.first);
          subs_[f.first].insert(&c);
          codes.push_back(0);  // granted QoS 0
        } else {
          ++refused_subs_;
          codes.push_back(0x80);  // failure
        }
      }
      send(mq::suback(p.packet_id, codes));
      return;
    }
    case mq::Type::Unsubscribe: {
      for (const auto& f : p.filters) {
        if (!cl.subs.erase(f.first)) continue;
        auto it = subs_.find(f.first);
        if (it != subs_.end()) {
          it->second.erase(&c);
          if (it->second.empty()) subs_.erase(it);
        }
      }
      send(mq::unsuback(p.packet_id));
      return;
    }
    case mq::Type::Publish: {
      if (p.qos != 0 || !topic_ok(p.topic)) {  // QoS > 0 ignored; other topics denied
        ++denied_publish_;
        return;
      }
      ++msgs_in_;
      uint64_t now = steady_ms();
      bool allowed = cl.pubs.take(1) && cl.bytes.take(double(p.payload.size()));
      if (allowed) {
        TopicRate& tr = topic_rate_[p.topic];
        if (tr.last == 0) tr.tokens = opt_.topic_publish_per_s * 2;
        tr.tokens = std::min(opt_.topic_publish_per_s * 2, tr.tokens + double(now - tr.last) * opt_.topic_publish_per_s / 1000.0);
        tr.last = now;
        if (tr.tokens >= 1) tr.tokens -= 1;
        else allowed = false;
        if (topic_rate_.size() > 8192) topic_rate_.clear();  // bounded; budgets restart
      }
      if (!allowed) {
        ++dropped_rate_;
        if (now - cl.abuse_window > 10000) {
          cl.abuse_window = now;
          cl.abuse_count = 0;
        }
        if (++cl.abuse_count > opt_.abuse_drops) {
          ++evicted_;
          c.close(kWsClosePolicy, "rate limit");
        }
        return;
      }
      // Retain and dup flags are ignored: nothing is ever stored.
      ember::Bytes pkt = mq::publish(p.topic, p.payload);
      deliver(p.topic, to_str(pkt), &c);
      return;
    }
    default:  // PUBACK/PUBREC/... SUBACK etc. from a client: not part of this subset
      ++protocol_errors_;
      c.close(kWsCloseProtocol, "unsupported packet");
      return;
  }
}

void MqttBroker::deliver(const std::string& topic, const std::string& packet, WsConn* from) {
  auto it = subs_.find(topic);
  if (it == subs_.end()) return;
  uint64_t now = steady_ms();
  std::vector<WsConn*> evict;
  for (WsConn* s : it->second) {
    if (!s->open()) continue;
    Client* cl = static_cast<Client*>(s->user);
    if (s->send_binary(packet.data(), packet.size(), true)) {
      ++msgs_out_;
      if (cl) cl->dropping_since = 0;
      continue;
    }
    ++dropped_slow_;
    if (!cl) continue;
    if (!cl->dropping_since) cl->dropping_since = now;
    if (now - cl->dropping_since > opt_.slow_evict_ms) evict.push_back(s);
  }
  (void)from;
  for (WsConn* s : evict) {
    ++evicted_;
    s->close(kWsClosePolicy, "too slow");
  }
}

Json MqttBroker::stats() const {
  Json s = Json::object();
  s.set("clients", double(clients_));
  s.set("clients_total", double(clients_total_));
  s.set("topics", double(subs_.size()));
  s.set("messages_in", double(msgs_in_));
  s.set("messages_out", double(msgs_out_));
  s.set("dropped_rate", double(dropped_rate_));
  s.set("dropped_slow", double(dropped_slow_));
  s.set("refused_subscriptions", double(refused_subs_));
  s.set("evicted", double(evicted_));
  s.set("protocol_errors", double(protocol_errors_));
  s.set("denied_publish", double(denied_publish_));
  s.set("refused_source", double(refused_source_));
  return s;
}

}  // namespace ember_host
