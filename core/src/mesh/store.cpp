// DTN store and bounded recent-sets.
#include "mesh/impl.h"
#include <algorithm>

namespace ember::mesh {

uint64_t siphash24(uint64_t k0, uint64_t k1, const uint8_t* in, size_t n) {
  auto rotl = [](uint64_t x, int b) { return (x << b) | (x >> (64 - b)); };
  uint64_t v0 = 0x736f6d6570736575ull ^ k0, v1 = 0x646f72616e646f6dull ^ k1;
  uint64_t v2 = 0x6c7967656e657261ull ^ k0, v3 = 0x7465646279746573ull ^ k1;
  auto round = [&]() {
    v0 += v1; v1 = rotl(v1, 13); v1 ^= v0; v0 = rotl(v0, 32);
    v2 += v3; v3 = rotl(v3, 16); v3 ^= v2;
    v0 += v3; v3 = rotl(v3, 21); v3 ^= v0;
    v2 += v1; v1 = rotl(v1, 17); v1 ^= v2; v2 = rotl(v2, 32);
  };
  size_t i = 0;
  for (; i + 8 <= n; i += 8) {
    uint64_t m = 0;
    for (int j = 0; j < 8; ++j) m |= uint64_t(in[i + static_cast<size_t>(j)]) << (8 * j);
    v3 ^= m; round(); round(); v0 ^= m;
  }
  uint64_t b = uint64_t(n & 0xFF) << 56;
  for (size_t j = 0; i + j < n; ++j) b |= uint64_t(in[i + j]) << (8 * j);
  v3 ^= b; round(); round(); v0 ^= b;
  v2 ^= 0xFF;
  round(); round(); round(); round();
  return v0 ^ v1 ^ v2 ^ v3;
}

void ReplayGuard::put(const MsgId& m, uint64_t expires_ms, bool auth, std::vector<MsgId>* evicted) {
  if (!auth) {
    if (!map_.count(m)) unauth_.put(m, 1);
    return;
  }
  auto it = map_.find(m);
  if (it != map_.end()) {
    if (expires_ms <= it->second) return;
    by_exp_.erase({it->second, m});
    it->second = expires_ms;
    by_exp_.insert({expires_ms, m});
    return;
  }
  map_.emplace(m, expires_ms);
  by_exp_.insert({expires_ms, m});
  while (map_.size() > cap_auth_ && !by_exp_.empty()) {
    auto first = by_exp_.begin();
    if (evicted) evicted->push_back(first->second);
    map_.erase(first->second);
    by_exp_.erase(first);
  }
}

void ReplayGuard::prune(uint64_t now_ms, std::vector<MsgId>* removed) {
  while (!by_exp_.empty() && by_exp_.begin()->first <= now_ms) {
    auto first = by_exp_.begin();
    if (removed) removed->push_back(first->second);
    map_.erase(first->second);
    by_exp_.erase(first);
  }
}

bool RecentSet::insert(uint64_t k) {
  if (!set_.insert(k).second) return false;
  order_.push_back(k);
  while (order_.size() > cap_) {
    set_.erase(order_.front());
    order_.pop_front();
  }
  return true;
}

void RecentSet::set_capacity(size_t cap) {
  cap_ = cap ? cap : 1;
  while (order_.size() > cap_) {
    set_.erase(order_.front());
    order_.pop_front();
  }
}

const StoreEntry* DtnStore::get(uint64_t wire) const {
  auto it = map_.find(wire);
  return it == map_.end() ? nullptr : &it->second;
}
StoreEntry* DtnStore::get_mut(uint64_t wire) {
  auto it = map_.find(wire);
  return it == map_.end() ? nullptr : &it->second;
}

// Higher = more valuable (evicted last). Packets from unknown origins are the lowest class of
// all, whatever kind / priority they claim (security review C2).
int DtnStore::value_class(const StoreEntry& e) {
  if (e.trust == kTrustUnknown && !e.own()) return 1;
  int v;
  switch (e.h.kind) {
    case Kind::Sos: case Kind::SosAck: v = 60; break;
    case Kind::Ack: v = 45; break;
    case Kind::Announce: v = 40; break;
    case Kind::Text: case Kind::App: case Kind::Read: v = 35; break;
    case Kind::Bulletin: case Kind::NetPlan: v = 30; break;
    case Kind::Checkin: v = 25; break;
    case Kind::Position: v = 20; break;
    case Kind::File: v = 10; break;
    default: v = 5; break;
  }
  if (e.h.prio == Prio::Emergency) v += 20;
  else if (e.h.prio == Prio::High) v += 5;
  else if (e.h.prio == Prio::Low) v -= 5;
  if (e.own()) v += 100;  // our own undelivered traffic goes last
  return v;
}

bool DtnStore::evict_one(uint64_t now, int newv, bool unknown_only) {
  // Victim: expired first; else lowest value class, oldest received.
  const StoreEntry* victim = nullptr;
  int vv = 0;
  for (const auto& kv : map_) {
    const StoreEntry& c = kv.second;
    if (unknown_only && (c.trust != kTrustUnknown || c.own())) continue;
    if (c.expires_local <= now) { victim = &c; vv = -1; break; }
    int cv = value_class(c);
    if (!victim || cv < vv || (cv == vv && c.received_ms < victim->received_ms)) {
      victim = &c;
      vv = cv;
    }
  }
  if (!victim) return false;
  if (vv >= 0 && vv > newv) return false;  // newcomer is the least valuable
  evicted_tmp_.push_back(victim->wire);
  remove(victim->wire);
  return true;
}

bool DtnStore::put(StoreEntry&& e, uint64_t now, std::vector<uint64_t>* evicted) {
  if (map_.count(e.wire)) return true;
  size_t sz = e.raw.size() + 64;  // + bookkeeping estimate
  if (sz > max_bytes_ || max_packets_ == 0) return false;
  int newv = value_class(e);
  evicted_tmp_.clear();
  bool unknown = e.trust == kTrustUnknown && !e.own();
  bool ok = true;
  if (unknown) {
    // Unknown origins share at most 10 % of the quota; they only ever displace each other.
    size_t cap_n = std::max<size_t>(1, max_packets_ / 10), cap_b = std::max<size_t>(sz, max_bytes_ / 10);
    while (ok && (u_count_ + 1 > cap_n || u_bytes_ + sz > cap_b)) ok = evict_one(now, newv + 1, true);
  }
  while (ok && (bytes_ + sz > max_bytes_ || map_.size() + 1 > max_packets_)) ok = evict_one(now, newv, false);
  if (evicted) evicted->insert(evicted->end(), evicted_tmp_.begin(), evicted_tmp_.end());
  if (!ok) return false;
  MsgId m{e.h.origin, e.h.pkt_id};
  by_msg_.emplace(m, e.wire);
  if (e.replace_key) by_replace_[e.replace_key] = e.wire;
  by_sid_.emplace(e.sid, e.wire);
  bytes_ += sz;
  if (unknown) { u_count_++; u_bytes_ += sz; }
  ++gen_;
  uint64_t w = e.wire;
  map_.emplace(w, std::move(e));
  return true;
}

void DtnStore::set_trust(uint64_t wire, uint8_t t) {
  auto it = map_.find(wire);
  if (it == map_.end() || it->second.trust == t) return;
  StoreEntry& e = it->second;
  size_t sz = e.raw.size() + 64;
  bool was_u = e.trust == kTrustUnknown && !e.own();
  e.trust = t;
  bool is_u = e.trust == kTrustUnknown && !e.own();
  if (was_u && !is_u) { u_count_--; u_bytes_ -= sz; }
  if (!was_u && is_u) { u_count_++; u_bytes_ += sz; }
  ++gen_;
}

std::vector<uint64_t> DtnStore::by_sid(uint32_t sid) const {
  std::vector<uint64_t> out;
  auto r = by_sid_.equal_range(sid);
  for (auto i = r.first; i != r.second; ++i) out.push_back(i->second);
  return out;
}

bool DtnStore::remove(uint64_t wire) {
  auto it = map_.find(wire);
  if (it == map_.end()) return false;
  const StoreEntry& e = it->second;
  MsgId m{e.h.origin, e.h.pkt_id};
  auto range = by_msg_.equal_range(m);
  for (auto i = range.first; i != range.second; ++i) {
    if (i->second == wire) { by_msg_.erase(i); break; }
  }
  if (e.replace_key) {
    auto r = by_replace_.find(e.replace_key);
    if (r != by_replace_.end() && r->second == wire) by_replace_.erase(r);
  }
  auto sr = by_sid_.equal_range(e.sid);
  for (auto i = sr.first; i != sr.second; ++i)
    if (i->second == wire) { by_sid_.erase(i); break; }
  if (e.trust == kTrustUnknown && !e.own()) { u_count_--; u_bytes_ -= e.raw.size() + 64; }
  ++gen_;
  bytes_ -= e.raw.size() + 64;
  map_.erase(it);
  return true;
}

std::vector<uint64_t> DtnStore::find_msg(const MsgId& m) const {
  std::vector<uint64_t> out;
  auto range = by_msg_.equal_range(m);
  for (auto i = range.first; i != range.second; ++i) out.push_back(i->second);
  return out;
}

const StoreEntry* DtnStore::find_replace(uint64_t key) const {
  auto it = by_replace_.find(key);
  return it == by_replace_.end() ? nullptr : get(it->second);
}

void DtnStore::expired(uint64_t now, std::vector<uint64_t>& out) const {
  for (const auto& kv : map_)
    if (kv.second.expires_local <= now) out.push_back(kv.first);
}

}  // namespace ember::mesh
