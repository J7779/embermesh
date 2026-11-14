#include "host/stream.h"

#include <thread>

#include "host/util.h"

namespace ember_host {

struct TcpStream::DnsReq {
  std::mutex mu;
  Loop* loop = nullptr;      // nulled when the stream dies
  TcpStream* stream = nullptr;  // loop thread only
};

TcpStream::TcpStream(Loop& loop) : loop_(loop), alive_(std::make_shared<bool>(true)) {}

TcpStream::~TcpStream() {
  *alive_ = false;
  close();
}

void TcpStream::adopt(sock_t s, const SockAddr& peer) {
  close();
  sock_ = s;
  peer_ = peer;
  connected_ = true;
  last_activity_ = steady_ms();
  watch_ = loop_.watch(sock_, kPollIn, [this](short re) { on_io(re); });
}

void TcpStream::connect(const std::string& host, uint16_t port, uint32_t timeout_ms) {
  close();
  timeout_ms_ = timeout_ms;
  connecting_ = true;
  SockAddr a;
  if (resolve_numeric(host, port, &a) || (host == "localhost" && resolve_numeric("127.0.0.1", port, &a))) {
    start_connect(a);
    return;
  }
  // DNS on a worker thread; result posted back to the loop.
  auto req = std::make_shared<DnsReq>();
  req->loop = &loop_;
  req->stream = this;
  dns_ = req;
  std::thread([req, host, port] {
    SockAddr addr;
    std::string err;
    bool ok = resolve_blocking(host, port, &addr, &err);
    std::lock_guard<std::mutex> lk(req->mu);
    if (!req->loop) return;
    req->loop->post([req, ok, addr, err] {
      TcpStream* s = req->stream;
      if (!s) return;
      s->dns_.reset();
      if (!ok) {
        s->finish_connect(err.empty() ? "cannot resolve host" : err);
        return;
      }
      s->start_connect(addr);
    });
  }).detach();
  connect_timer_ = loop_.timer(timeout_ms_, [this] {
    connect_timer_ = 0;
    finish_connect("connect timeout");
  });
}

void TcpStream::start_connect(const SockAddr& a) {
  bool in_progress = false;
  std::string err;
  peer_ = a;
  sock_t s = tcp_connect_start(a, &in_progress, &err);
  if (s == kInvalidSock) {
    // Report asynchronously so callers never see callbacks from inside connect().
    std::weak_ptr<bool> alive = alive_;
    loop_.timer(0, [this, alive, err] {
      if (alive.expired() || !*alive.lock()) return;
      finish_connect(err);
    });
    return;
  }
  sock_ = s;
  watch_ = loop_.watch(sock_, kPollOut, [this](short re) { on_io(re); });
  if (!connect_timer_)
    connect_timer_ = loop_.timer(timeout_ms_, [this] {
      connect_timer_ = 0;
      finish_connect("connect timeout");
    });
  (void)in_progress;
}

void TcpStream::finish_connect(const std::string& err) {
  if (!connecting_) return;
  connecting_ = false;
  if (connect_timer_) {
    loop_.cancel(connect_timer_);
    connect_timer_ = 0;
  }
  if (!err.empty()) {
    close();
    auto cb = on_connect;
    if (cb) cb(err);
    return;
  }
  connected_ = true;
  last_activity_ = steady_ms();
  update_events();
  auto alive = alive_;
  if (on_connect) on_connect("");
  if (!*alive) return;
  if (pending()) flush();
}

void TcpStream::close() {
  if (dns_) {
    std::lock_guard<std::mutex> lk(dns_->mu);
    dns_->loop = nullptr;
    dns_->stream = nullptr;
    dns_.reset();
  }
  if (connect_timer_) {
    loop_.cancel(connect_timer_);
    connect_timer_ = 0;
  }
  if (watch_) {
    loop_.unwatch(watch_);
    watch_ = 0;
  }
  if (sock_ != kInvalidSock) {
    sock_close(sock_);
    sock_ = kInvalidSock;
  }
  connecting_ = false;
  connected_ = false;
  closing_ = false;
  paused_ = false;
  out_.clear();
  out_off_ = 0;
}

void TcpStream::close_after_flush() {
  if (!is_open()) return;
  closing_ = true;
  paused_ = true;
  if (!pending()) {
    fail("closed");
    return;
  }
  update_events();
}

void TcpStream::set_read_paused(bool p) {
  paused_ = p || closing_;
  update_events();
}

void TcpStream::write(const void* data, size_t len) {
  if (!is_open() || closing_ || len == 0) return;
  if (out_off_ > 0 && out_off_ == out_.size()) {
    out_.clear();
    out_off_ = 0;
  }
  // Try a direct write first when nothing is queued.
  const char* p = static_cast<const char*>(data);
  if (connected_ && pending() == 0) {
    long n = sock_send(sock_, p, len);
    if (n > 0) {
      bytes_out_ += uint64_t(n);
      last_activity_ = steady_ms();
      p += n;
      len -= size_t(n);
    } else if (n < 0) {
      int e = sock_error();
      if (!sock_would_block(e) && !sock_interrupted(e)) {
        // Defer the failure to the loop so callers never get on_close re-entrantly from write().
        std::weak_ptr<bool> alive = alive_;
        std::string why = sock_strerror(e);
        loop_.timer(0, [this, alive, why] {
          if (alive.expired() || !*alive.lock()) return;
          if (is_open()) fail(why);
        });
        return;
      }
    }
  }
  if (len) {
    if (out_off_ > 65536 && out_off_ * 2 > out_.size()) {
      out_.erase(0, out_off_);
      out_off_ = 0;
    }
    out_.append(p, len);
  }
  update_events();
}

void TcpStream::update_events() {
  if (!watch_) return;
  short ev = 0;
  if (connecting_) {
    ev = kPollOut;
  } else {
    if (!paused_) ev |= kPollIn;
    if (pending()) ev |= kPollOut;
  }
  loop_.set_events(watch_, ev);
}

void TcpStream::flush() {
  while (pending()) {
    long n = sock_send(sock_, out_.data() + out_off_, pending());
    if (n > 0) {
      out_off_ += size_t(n);
      bytes_out_ += uint64_t(n);
      last_activity_ = steady_ms();
      continue;
    }
    int e = sock_error();
    if (n < 0 && (sock_would_block(e) || sock_interrupted(e))) break;
    fail(n < 0 ? sock_strerror(e) : "write failed");
    return;
  }
  if (!pending()) {
    out_.clear();
    out_off_ = 0;
    if (closing_) {
      fail("closed");
      return;
    }
    update_events();
    auto alive = alive_;
    if (on_drain) on_drain();
    if (!*alive) return;
  } else {
    update_events();
  }
}

void TcpStream::fail(const std::string& reason) {
  if (!is_open()) return;
  close();
  if (on_close) on_close(reason);
}

void TcpStream::on_io(short re) {
  if (connecting_) {
    int e = sock_pending_error(sock_);
    if (e != 0 || (re & kPollErr)) {
      finish_connect("connect " + peer_.to_string() + ": " + sock_strerror(e ? e : sock_error()));
      return;
    }
    if (re & (kPollOut | kPollHup)) finish_connect("");
    return;
  }
  auto alive = alive_;
  if ((re & kPollOut) && pending()) {
    flush();
    if (!*alive || !is_open()) return;
  }
  if ((re & (kPollIn | kPollHup | kPollErr)) && !paused_) {
    uint8_t buf[65536];
    // Bounded number of reads per wakeup so one fast peer cannot starve others.
    for (int i = 0; i < 4; ++i) {
      long n = sock_recv(sock_, buf, sizeof buf);
      if (n > 0) {
        bytes_in_ += uint64_t(n);
        last_activity_ = steady_ms();
        if (on_data) on_data(buf, size_t(n));
        if (!*alive || !is_open() || paused_) return;
        if (size_t(n) < sizeof buf) break;
        continue;
      }
      if (n == 0) {
        fail("connection closed by peer");
        return;
      }
      int e = sock_error();
      if (sock_would_block(e) || sock_interrupted(e)) break;
      fail(sock_strerror(e));
      return;
    }
  } else if ((re & (kPollErr | kPollHup)) && paused_ && !pending()) {
    // Peer went away while we were not reading.
    if (re & kPollErr) fail("socket error");
  }
}

}  // namespace ember_host
