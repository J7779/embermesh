#include "host/ws_client.h"

#include "host/util.h"

namespace ember_host {

WsClient::WsClient(Loop& loop, Options opt) : loop_(loop), opt_(std::move(opt)), stream_(loop), alive_(std::make_shared<bool>(true)) {
  stream_.on_connect = [this](const std::string& e) { on_connected(e); };
  stream_.on_data = [this](const uint8_t* d, size_t n) { on_bytes(d, n); };
  stream_.on_close = [this](const std::string& why) {
    if (state_ == Idle) return;
    finish(peer_close_code_, peer_close_reason_.empty() ? why : peer_close_reason_);
  };
}

WsClient::~WsClient() {
  *alive_ = false;
  abort();
}

bool WsClient::connect(const std::string& url, std::string* err) {
  abort();
  std::string host;
  uint16_t port = 80;
  std::string e;
  if (!ws_parse_url(url, &host, &port, &path_, &e)) {
    if (err) *err = e;
    return false;
  }
  host_header_ = (host.find(':') != std::string::npos ? "[" + host + "]" : host) + (port == 80 ? "" : ":" + std::to_string(port));
  key_ = ws_make_client_key();
  head_buf_.clear();
  WsDecoder::Options o;
  o.expect_masked = false;
  o.max_message = opt_.max_message;
  dec_ = WsDecoder(o);
  peer_close_code_ = kWsCloseAbnormal;
  peer_close_reason_.clear();
  close_sent_ = false;
  close_deadline_ = 0;
  state_ = Connecting;
  stream_.connect(host, port, opt_.connect_timeout_ms);
  return true;
}

void WsClient::on_connected(const std::string& err) {
  if (!err.empty()) {
    finish(kWsCloseAbnormal, err);
    return;
  }
  state_ = Handshake;
  last_input_ = steady_ms();
  stream_.write(ws_client_handshake_request(host_header_, path_, key_, opt_.origin));
  schedule_tick();
}

void WsClient::on_bytes(const uint8_t* d, size_t n) {
  last_input_ = steady_ms();
  if (state_ == Handshake) {
    head_buf_.append(reinterpret_cast<const char*>(d), n);
    size_t end = head_buf_.find("\r\n\r\n");
    if (end == std::string::npos) {
      if (head_buf_.size() > 16384) {
        stream_.close();
        finish(kWsCloseAbnormal, "handshake response too large");
      }
      return;
    }
    std::string why;
    if (!ws_check_server_response(std::string_view(head_buf_).substr(0, end + 4), key_, &why)) {
      stream_.close();
      finish(kWsCloseAbnormal, why);
      return;
    }
    std::string rest = head_buf_.substr(end + 4);
    head_buf_.clear();
    state_ = Open;
    last_ping_ = steady_ms();
    auto alive = alive_;
    if (on_open) on_open();
    if (!*alive || state_ != Open) return;
    if (!rest.empty()) dec_.feed(reinterpret_cast<const uint8_t*>(rest.data()), rest.size());
  } else if (state_ == Open || state_ == Closing) {
    dec_.feed(d, n);
  } else {
    return;
  }
  auto alive = alive_;
  while (state_ == Open || state_ == Closing) {
    WsEvent ev = dec_.next();
    switch (ev.kind) {
      case WsEvent::None:
        return;
      case WsEvent::Message:
        if (state_ == Open && on_message) on_message(ev.opcode, ev.payload);
        if (!*alive) return;
        break;
      case WsEvent::Ping:
        send_frame(kWsPong, ev.payload.data(), ev.payload.size());
        break;
      case WsEvent::Pong:
        break;
      case WsEvent::Close: {
        peer_close_code_ = ev.close_code;
        peer_close_reason_ = ev.payload;
        if (!close_sent_) {
          close_sent_ = true;
          std::string p = ev.close_code == kWsCloseNoStatus ? std::string() : ws_close_payload(ev.close_code, {});
          send_frame(kWsClose, p.data(), p.size());
        }
        state_ = Closing;
        stream_.close_after_flush();
        return;
      }
      case WsEvent::Error: {
        if (!close_sent_) {
          close_sent_ = true;
          std::string p = ws_close_payload(ev.close_code, ev.error);
          send_frame(kWsClose, p.data(), p.size());
        }
        peer_close_code_ = kWsCloseAbnormal;
        peer_close_reason_ = "protocol error: " + ev.error;
        state_ = Closing;
        stream_.close_after_flush();
        return;
      }
    }
  }
}

bool WsClient::send_frame(uint8_t op, const void* data, size_t len) {
  if (!stream_.is_open()) return false;
  uint8_t mask[4];
  random_bytes(mask, 4);
  stream_.write(ws_encode_frame(op, data, len, true, mask));
  return true;
}

bool WsClient::send_text(std::string_view t) {
  if (state_ != Open || close_sent_) return false;
  return send_frame(kWsText, t.data(), t.size());
}

bool WsClient::send_binary(const void* data, size_t len) {
  if (state_ != Open || close_sent_) return false;
  return send_frame(kWsBinary, data, len);
}

void WsClient::close(uint16_t code, std::string_view reason) {
  if (state_ == Connecting || state_ == Handshake) {
    abort();
    return;
  }
  if (state_ != Open) return;
  close_sent_ = true;
  std::string p = ws_close_payload(code, reason);
  send_frame(kWsClose, p.data(), p.size());
  state_ = Closing;
  close_deadline_ = steady_ms() + 3000;
}

void WsClient::abort() {
  state_ = Idle;
  if (tick_timer_) {
    loop_.cancel(tick_timer_);
    tick_timer_ = 0;
  }
  stream_.close();
}

void WsClient::finish(uint16_t code, const std::string& reason) {
  if (state_ == Idle) return;
  state_ = Idle;
  if (tick_timer_) {
    loop_.cancel(tick_timer_);
    tick_timer_ = 0;
  }
  stream_.close();
  if (on_close) on_close(code, reason);
}

void WsClient::schedule_tick() {
  if (tick_timer_) return;
  auto alive = alive_;
  tick_timer_ = loop_.timer(1000, [this, alive] {
    if (!*alive) return;
    tick_timer_ = 0;
    tick();
  });
}

void WsClient::tick() {
  if (state_ == Idle) return;
  uint64_t now = steady_ms();
  if (now - last_input_ > opt_.timeout_ms) {
    finish(kWsCloseAbnormal, "timeout");
    return;
  }
  if (state_ == Closing && close_deadline_ && now > close_deadline_) {
    finish(peer_close_code_, "close handshake timeout");
    return;
  }
  if (state_ == Open && now - last_ping_ >= opt_.ping_interval_ms) {
    last_ping_ = now;
    send_frame(kWsPing, "ember", 5);
  }
  schedule_tick();
}

}  // namespace ember_host
