#include "proto/relay.h"

#include <cstring>

namespace ember::proto::relay {

namespace {

const char kHex[] = "0123456789abcdef";

crypto::Key32 keyed(const crypto::Key32& key, std::string_view label, const uint8_t* extra = nullptr,
                    size_t extra_len = 0) {
  Bytes m(label.begin(), label.end());
  if (extra) m.insert(m.end(), extra, extra + extra_len);
  return crypto::blake2b_256(ByteView(m), ByteView(key.data(), key.size()));
}

void be32(uint8_t out[4], uint32_t v) {
  out[0] = static_cast<uint8_t>(v >> 24);
  out[1] = static_cast<uint8_t>(v >> 16);
  out[2] = static_cast<uint8_t>(v >> 8);
  out[3] = static_cast<uint8_t>(v);
}

void put_host(Bytes& b, const std::string& host) {
  b.push_back(static_cast<uint8_t>(host.size()));
  b.insert(b.end(), host.begin(), host.end());
}

Bytes ad_for(const std::string& host, const std::string& topic) {
  static const char kAd[] = "ember-relay-v1";
  Bytes ad(kAd, kAd + sizeof kAd - 1);
  ad.push_back(kVersion);
  put_host(ad, host);
  ad.insert(ad.end(), topic.begin(), topic.end());
  return ad;
}

const size_t kClasses[] = {128, 256, 512, 1024, 2048, 4096};

}  // namespace

crypto::Key32 pair_root(const crypto::Key32& k_pair) { return keyed(k_pair, "ember-relay-pair-v1"); }
crypto::Key32 channel_root(const crypto::Key32& k_chan) { return keyed(k_chan, "ember-relay-chan-v1"); }

uint32_t epoch_of(uint64_t unix_ms) { return static_cast<uint32_t>(unix_ms / 86400000ull); }

std::string canonical_host(const std::string& url) {
  std::string rest;
  std::string port = "443";
  if (url.rfind("wss://", 0) == 0) rest = url.substr(6);
  else if (url.rfind("ws://", 0) == 0) { rest = url.substr(5); port = "80"; }
  else return std::string();
  rest = rest.substr(0, rest.find('/'));
  std::string host = rest;
  if (!rest.empty() && rest[0] == '[') {
    size_t e = rest.find(']');
    if (e == std::string::npos) return std::string();
    host = rest.substr(1, e - 1);
    if (e + 1 < rest.size() && rest[e + 1] == ':') port = rest.substr(e + 2);
  } else {
    size_t colon = rest.rfind(':');
    if (colon != std::string::npos) { host = rest.substr(0, colon); port = rest.substr(colon + 1); }
  }
  if (host.empty() || port.empty() || host.size() > 200 || port.size() > 5) return std::string();
  for (char& ch : host)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  return host + ":" + port;
}

std::string topic(const crypto::Key32& root, uint32_t epoch, const std::string& host) {
  static const char kLabel[] = "ember-relay-topic-v1";
  uint8_t e[4];
  be32(e, epoch);
  Bytes m(kLabel, kLabel + sizeof kLabel - 1);
  m.insert(m.end(), e, e + 4);
  put_host(m, host);
  uint8_t h[16];
  crypto::blake2b(h, sizeof h, ByteView(m), ByteView(root.data(), root.size()));
  std::string out(kTopicLen, '0');
  for (size_t i = 0; i < 16; ++i) {
    out[2 * i] = kHex[h[i] >> 4];
    out[2 * i + 1] = kHex[h[i] & 15];
  }
  return out;
}

crypto::Key32 frame_key(const crypto::Key32& root, uint32_t epoch, const std::string& host) {
  uint8_t e[4];
  be32(e, epoch);
  Bytes x(e, e + 4);
  put_host(x, host);
  return keyed(root, "ember-relay-aead-v1", x.data(), x.size());
}

size_t padded_size(size_t len) {
  for (size_t c : kClasses)
    if (len + kPlainOverhead <= c) return c;
  return 0;
}

Bytes seal(const crypto::Key32& k, const std::string& host, const std::string& topic, const uint8_t rnd[16],
           uint64_t sent_ms, ByteView frame) {
  if (frame.empty() || frame.size() > kMaxFrame) return Bytes();
  size_t n = padded_size(frame.size());
  Bytes p(n, 0);
  for (int i = 0; i < 8; ++i) p[static_cast<size_t>(i)] = static_cast<uint8_t>(sent_ms >> (56 - 8 * i));
  p[8] = static_cast<uint8_t>(frame.size() >> 8);
  p[9] = static_cast<uint8_t>(frame.size());
  std::memcpy(p.data() + kPlainOverhead, frame.data(), frame.size());
  // Hedged nonce: random bytes AND the plaintext (PROTOCOL §12).
  static const char kNl[] = "ember-relay-nonce-v1";
  Bytes nm(kNl, kNl + sizeof kNl - 1);
  nm.insert(nm.end(), rnd, rnd + 16);
  nm.insert(nm.end(), p.begin(), p.begin() + 8);  // u64be sent_ms
  nm.insert(nm.end(), p.begin(), p.end());
  uint8_t nonce[kNonceLen];
  crypto::blake2b(nonce, sizeof nonce, ByteView(nm), ByteView(k.data(), k.size()));
  crypto::wipe(nm);
  Bytes ad = ad_for(host, topic);
  Bytes ct = crypto::seal(k, nonce, ByteView(ad), ByteView(p));
  crypto::wipe(p);
  Bytes out;
  out.reserve(kHeaderLen + ct.size());
  out.push_back(kVersion);
  out.insert(out.end(), nonce, nonce + kNonceLen);
  out.insert(out.end(), ct.begin(), ct.end());
  return out;
}

bool open(const crypto::Key32& k, const std::string& host, const std::string& topic, ByteView msg, Opened* out) {
  if (msg.size() < kHeaderLen + kTagLen || msg[0] != kVersion) return false;
  size_t body = msg.size() - kHeaderLen - kTagLen;
  bool size_ok = false;
  for (size_t c : kClasses) size_ok = size_ok || body == c;
  if (!size_ok) return false;
  Bytes ad = ad_for(host, topic);
  Bytes p;
  if (!crypto::open(k, msg.data() + 1, ByteView(ad), msg.sub(kHeaderLen), p)) return false;
  if (p.size() != body) return false;
  uint64_t sent = 0;
  for (int i = 0; i < 8; ++i) sent = sent << 8 | p[static_cast<size_t>(i)];
  size_t len = static_cast<size_t>(p[8]) << 8 | p[9];
  if (len == 0 || len > body - kPlainOverhead) { crypto::wipe(p); return false; }
  for (size_t i = kPlainOverhead + len; i < p.size(); ++i)
    if (p[i] != 0) { crypto::wipe(p); return false; }
  // Canonical padding class: the smallest that fits (one encoding per frame).
  if (padded_size(len) != body) { crypto::wipe(p); return false; }
  if (out) {
    out->frame.assign(p.begin() + static_cast<long>(kPlainOverhead), p.begin() + static_cast<long>(kPlainOverhead + len));
    out->sent_ms = sent;
    std::memcpy(out->nonce, msg.data() + 1, kNonceLen);
  }
  crypto::wipe(p);
  return true;
}

}  // namespace ember::proto::relay
