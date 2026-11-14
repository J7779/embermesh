#include "proto/secure.h"

#include <cstring>

#include "base/json.h"
#include "ember/crc.h"

namespace ember::proto {

namespace {

const std::string_view kSigDomain = "ember-sig-v1";

void nonce_for(const Bytes& ad, uint8_t nonce[24]) {
  crypto::Blake2b h(24);
  h.update(std::string_view("ember-nonce-v2")).update(ad);
  h.final(nonce);
}

// Hedged nonce salt: unique when the RNG is good, and still distinct for distinct plaintexts (or
// keys) when it is not, so a repeated header can never reuse a nonce for different messages.
void make_salt(Header& h, const crypto::Key32& k, ByteView plaintext, ByteView entropy) {
  h.salt = {};
  ByteWriter w(header_size(h));
  Header z = h;
  z.ttl = 0;
  z.hops = 0;
  write_header(w, z);
  crypto::Blake2b b(8, ByteView(k.data(), 32));
  b.update(std::string_view("ember-salt-v2"));
  uint8_t el = static_cast<uint8_t>(entropy.size() > 64 ? 64 : entropy.size());
  b.u8(el).update(entropy.sub(0, el)).update(w.data()).update(plaintext);
  b.final(h.salt.data());
}

Bytes sig_message(const Bytes& ad, ByteView body) {
  Bytes m;
  m.reserve(kSigDomain.size() + ad.size() + body.size());
  m.insert(m.end(), kSigDomain.begin(), kSigDomain.end());
  m.insert(m.end(), ad.begin(), ad.end());
  m.insert(m.end(), body.begin(), body.end());
  return m;
}

Bytes header_ad(const Header& h) {
  Header z = h;
  z.ttl = 0;
  z.hops = 0;
  ByteWriter w(header_size(h));
  write_header(w, z);
  return w.take();
}

void mac_for(const crypto::Key32& k, const Bytes& ad, ByteView body, uint8_t out[16]) {
  crypto::Blake2b h(16, ByteView(k.data(), 32));
  h.update(std::string_view("ember-mac-v1")).update(ad).update(body);
  h.final(out);
}

uint32_t u32_of(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

Bytes assemble(const Header& h, ByteView body, ByteView tag) {
  if (header_size(h) + body.size() + tag.size() > kMaxPacket) return {};
  ByteWriter w(header_size(h) + body.size() + tag.size());
  write_header(w, h);
  w.bytes(body).bytes(tag);
  return w.take();
}

}  // namespace

bool pair_key(const Identity& me, const crypto::Key32& their, crypto::Key32& out) {
  crypto::Key32 shared;
  if (!me.dh(their, shared)) return false;
  const crypto::Key32& mine = me.dh_pk();
  bool mine_first = std::memcmp(mine.data(), their.data(), 32) < 0;
  crypto::Blake2b h(32, ByteView(shared.data(), 32));
  h.update(std::string_view("ember-pair-v1"));
  h.update(mine_first ? mine.data() : their.data(), 32);
  h.update(mine_first ? their.data() : mine.data(), 32);
  out = h.final32();
  crypto::wipe(shared);
  return true;
}

AckSecret ack_secret(const crypto::Key32& k, const NodeId& origin, uint32_t pkt_id) {
  crypto::Blake2b h(8, ByteView(k.data(), 32));
  uint8_t id[4] = {static_cast<uint8_t>(pkt_id >> 24), static_cast<uint8_t>(pkt_id >> 16),
                   static_cast<uint8_t>(pkt_id >> 8), static_cast<uint8_t>(pkt_id)};
  h.update(std::string_view("ember-ack-v1")).update(origin.data(), 8).update(id, 4);
  AckSecret s;
  h.final(s.data());
  return s;
}

AckCommit ack_commit(const AckSecret& secret) {
  crypto::Blake2b h(8);
  h.update(std::string_view("ember-ackc-v1")).update(secret.data(), 8);
  AckCommit c;
  h.final(c.data());
  return c;
}

std::string normalize_channel_name(std::string_view name) {
  std::string out;
  bool space = false;
  for (char c : name) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { space = !out.empty(); continue; }
    if (space) { out.push_back(' '); space = false; }
    out.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
  }
  return std::string(utf8_truncate(out, 32));
}

ChannelKey channel_from_key(std::string_view name, const crypto::Key32& key) {
  ChannelKey ch;
  ch.name = normalize_channel_name(name);
  ch.encrypted = true;
  ch.key = key;
  uint8_t d[4];
  crypto::blake2b(d, 4, ByteView(std::string_view("ember-chan-id-v1")), ByteView(key.data(), 32));
  ch.id = u32_of(d);
  return ch;
}

ChannelKey derive_channel_key(std::string_view name, std::string_view passphrase) {
  std::string n = normalize_channel_name(name);
  std::string salt = "ember-channel-v1:" + n;
  crypto::Key32 k{};
  crypto::argon2id(k.data(), 32, ByteView(passphrase), ByteView(std::string_view(salt)), crypto::kChannelKdf);
  ChannelKey ch = channel_from_key(n, k);
  crypto::wipe(k);
  return ch;
}

ChannelKey open_channel(std::string_view name) {
  ChannelKey ch;
  ch.name = normalize_channel_name(name);
  ch.encrypted = false;
  std::string m = "ember-open-channel-v1:" + ch.name;
  uint8_t d[4];
  crypto::blake2b(d, 4, ByteView(std::string_view(m)));
  ch.id = u32_of(d);
  return ch;
}

Bytes seal_none(Header h, ByteView body) {
  h.sec = Sec::None;
  return assemble(h, body, ByteView());
}

Bytes seal_signed(Header h, ByteView body, const Identity& me) {
  h.sec = Sec::Signed;
  Bytes ad = header_ad(h);
  crypto::Sig64 sig = me.sign(sig_message(ad, body));
  return assemble(h, body, ByteView(sig.data(), 64));
}

Bytes seal_pair_mac(Header h, ByteView body, const crypto::Key32& k) {
  h.sec = Sec::PairMac;
  Bytes ad = header_ad(h);
  uint8_t mac[16];
  mac_for(k, ad, body, mac);
  return assemble(h, body, ByteView(mac, 16));
}

Bytes seal_pair_aead(Header h, ByteView body, const crypto::Key32& k, ByteView entropy) {
  h.sec = Sec::PairAead;
  make_salt(h, k, body, entropy);
  Bytes ad = header_ad(h);
  uint8_t nonce[24];
  nonce_for(ad, nonce);
  Bytes ct = crypto::seal(k, nonce, ad, body);
  if (header_size(h) + ct.size() > kMaxPacket) return {};
  ByteWriter w;
  write_header(w, h);
  w.bytes(ct);
  return w.take();
}

Bytes seal_chan(Header h, ByteView body, const ChannelKey& ch, const Identity& me, ByteView entropy) {
  h.sec = Sec::ChanAead;
  h.dst_type = DestType::Channel;
  h.channel = ch.id;
  make_salt(h, ch.key, body, entropy);
  Bytes ad = header_ad(h);
  crypto::Sig64 sig = me.sign(sig_message(ad, body));
  Bytes pt = body.to_bytes();
  pt.insert(pt.end(), sig.begin(), sig.end());
  uint8_t nonce[24];
  nonce_for(ad, nonce);
  Bytes ct = crypto::seal(ch.key, nonce, ad, pt);
  crypto::wipe(pt);
  if (header_size(h) + ct.size() > kMaxPacket) return {};
  ByteWriter w;
  write_header(w, h);
  w.bytes(ct);
  return w.take();
}

bool verify_signed(const PacketView& p, const crypto::Key32& sign_pk) {
  if (p.h.sec != Sec::Signed || p.tag.size() != 64) return false;
  Bytes ad = auth_header_bytes(p);
  return crypto::ed25519_verify(p.tag.data(), sign_pk, sig_message(ad, p.body));
}

bool verify_pair_mac(const PacketView& p, const crypto::Key32& k) {
  if (p.h.sec != Sec::PairMac || p.tag.size() != 16) return false;
  Bytes ad = auth_header_bytes(p);
  uint8_t mac[16];
  mac_for(k, ad, p.body, mac);
  return ct_equal(ByteView(mac, 16), p.tag);
}

bool open_pair_aead(const PacketView& p, const crypto::Key32& k, Bytes& body) {
  body.clear();
  if (p.h.sec != Sec::PairAead) return false;
  Bytes ad = auth_header_bytes(p);
  uint8_t nonce[24];
  nonce_for(ad, nonce);
  ByteView ct_mac = p.raw.sub(p.auth_header.size());
  return crypto::open(k, nonce, ad, ct_mac, body);
}

bool open_chan(const PacketView& p, const ChannelKey& ch, Bytes& body, crypto::Sig64& inner_sig) {
  body.clear();
  if (p.h.sec != Sec::ChanAead || !ch.encrypted || p.h.channel != ch.id) return false;
  Bytes ad = auth_header_bytes(p);
  uint8_t nonce[24];
  nonce_for(ad, nonce);
  ByteView ct_mac = p.raw.sub(p.auth_header.size());
  Bytes pt;
  if (!crypto::open(ch.key, nonce, ad, ct_mac, pt)) return false;
  if (pt.size() < 64) return false;
  std::memcpy(inner_sig.data(), pt.data() + pt.size() - 64, 64);
  body.assign(pt.begin(), pt.end() - 64);
  crypto::wipe(pt);
  return true;
}

bool verify_chan_sig(const PacketView& p, ByteView body, const crypto::Sig64& sig, const crypto::Key32& sign_pk) {
  Bytes ad = auth_header_bytes(p);
  return crypto::ed25519_verify(sig.data(), sign_pk, sig_message(ad, body));
}

Bytes encode_bundle(const std::vector<Bytes>& packets) {
  ByteWriter w;
  w.str("EMB1").varint(packets.size());
  for (const Bytes& p : packets) w.varint(p.size()).bytes(p);
  uint32_t crc = crc32(w.data());
  w.u32be(crc);
  return w.take();
}

bool decode_bundle(ByteView data, std::vector<ByteView>& packets) {
  packets.clear();
  if (data.size() < 9) return false;
  ByteView content = data.sub(0, data.size() - 4);
  ByteReader tr(data.sub(data.size() - 4));
  if (crc32(content) != tr.u32be()) return false;
  ByteReader r(content);
  if (r.bytes(4) != ByteView(std::string_view("EMB1"))) return false;
  uint64_t n = r.varint();
  if (!r.ok() || n > kMaxBundlePackets || n > content.size()) return false;
  packets.reserve(static_cast<size_t>(n));
  for (uint64_t i = 0; i < n; ++i) {
    uint64_t len = r.varint();
    if (!r.ok() || len > kMaxPacket) { packets.clear(); return false; }
    ByteView p = r.bytes(static_cast<size_t>(len));
    if (!r.ok()) { packets.clear(); return false; }
    packets.push_back(p);
  }
  if (!r.at_end()) { packets.clear(); return false; }
  return true;
}

}  // namespace ember::proto
