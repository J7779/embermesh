// Internet relay derivations and sealing (docs/PROTOCOL.md §12). Every vector here was produced by
// an independent reference (Python: hashlib.blake2b and an XChaCha20-Poly1305 written from RFC 8439
// and draft-irtf-cfrg-xchacha, itself checked against those documents' vectors), so the full sealed
// message pins the framing, the hedged nonce, the AD and the AEAD together.
#include <string>

#include "doctest.h"
#include "proto/relay.h"

using namespace ember;
namespace relay = ember::proto::relay;

namespace {
crypto::Key32 seq_key(uint8_t start) {
  crypto::Key32 k;
  for (size_t i = 0; i < k.size(); ++i) k[i] = static_cast<uint8_t>(start + i);
  return k;
}
std::string hex(const crypto::Key32& k) { return to_hex(ByteView(k.data(), k.size())); }
const std::string kHost = "broker.example.org:8084";
}  // namespace

TEST_SUITE("proto") {

TEST_CASE("relay: key and topic derivations match the reference vectors") {
  crypto::Key32 kpair = seq_key(0), kchan = seq_key(32);
  crypto::Key32 rp = relay::pair_root(kpair), rc = relay::channel_root(kchan);
  CHECK(hex(rp) == "f2ed9d1556599423401b6154e15a81c09c13353b10f68fa2656e4f980d456451");
  CHECK(hex(rc) == "56525207dbb953de803f3d1f7093580b532e71484e08bf53f4de058ca2834acd");
  CHECK(relay::epoch_of(1763078400000ull) == 20406);
  CHECK(relay::epoch_of(1763078400000ull - 1) == 20405);
  CHECK(relay::topic(rp, 20406, kHost) == "b61f9b3d043471379e905d4aefbe672c");
  CHECK(relay::topic(rp, 20407, kHost) == "9e8c34977c9269c6d0dd31226b015feb");
  CHECK(relay::topic(rc, 20406, kHost) == "0a9ec8333ccee061f6f92007dea7837f");
  CHECK(relay::topic(rc, 20407, kHost) == "af251244b409e280a7c9b444aaf1fa1f");
  CHECK(hex(relay::frame_key(rp, 20406, kHost)) == "ad93f89b41b4e5a1caf5ac9506e10b5dfc67a74a1e0c0304aa45ea3492b1f124");
  CHECK(hex(relay::frame_key(rp, 20407, kHost)) == "0368ee5df4a47c0fc44677b6430f45735418219c2776c02782d8a0985467654a");
  // M2: bound to the broker — the same pair meets on a different topic elsewhere.
  CHECK(relay::topic(rp, 20406, "broker.example.org:443") == "65132cb6b60682b977ed2036b0bf23e1");
  CHECK(hex(relay::pair_root(kpair)) != hex(relay::channel_root(kpair)));
}

TEST_CASE("relay: canonical broker host") {
  CHECK(relay::canonical_host("wss://Broker.Example.ORG:8084/mqtt") == "broker.example.org:8084");
  CHECK(relay::canonical_host("wss://broker.example.org/mqtt") == "broker.example.org:443");
  CHECK(relay::canonical_host("wss://broker.example.org") == "broker.example.org:443");
  CHECK(relay::canonical_host("ws://192.0.2.7:1883/") == "192.0.2.7:1883");
  CHECK(relay::canonical_host("ws://hub.local/mqtt") == "hub.local:80");
  CHECK(relay::canonical_host("wss://[FE80::1]:9001/mqtt") == "fe80::1:9001");
  CHECK(relay::canonical_host("https://broker.example.org") == "");
  CHECK(relay::canonical_host("wss://") == "");
  CHECK(relay::canonical_host("wss://[fe80::1/x") == "");
  CHECK(relay::canonical_host("wss://" + std::string(201, 'a')) == "");
}

TEST_CASE("relay: L3 — the full sealed message matches the independent reference") {
  crypto::Key32 rp = relay::pair_root(seq_key(0));
  crypto::Key32 k = relay::frame_key(rp, 20406, kHost);
  std::string t = relay::topic(rp, 20406, kHost);
  uint8_t rnd[16];
  for (int i = 0; i < 16; ++i) rnd[i] = static_cast<uint8_t>(0xA0 + i);
  Bytes f = {0x80, 1, 2, 3, 4, 0x55};
  Bytes m = relay::seal(k, kHost, t, rnd, 1763078400123ull, ByteView(f));
  CHECK(m.size() == 169);
  CHECK(to_hex(ByteView(m)) ==
        "01941851c18244f015e3fb407c67ac3afbe0314c04e8e8de5e8675641a30898fb20648e7b612dd39ced4ca6e2b2b715a6c06f72fc3d1d9"
        "61b0d3194caea5ff4afb2af87a087dcd51a6527c97aafb7e96ee4c0e65bc64b63a705324ccee72fddeb5eff86236deb91b3a1bbb23a1e0d7"
        "7a2e6ef60895ad018400d10870c77d8aff354da1666193ee5d563caf099a6e13dcf1daeaf0e32674b63d1d0c7b2c33da638a5fd8fef5b6e1"
        "8c77");
  relay::Opened o;
  REQUIRE(relay::open(k, kHost, t, ByteView(m), &o));
  CHECK(o.frame == f);
  CHECK(o.sent_ms == 1763078400123ull);
  // L1: same random bytes, different plaintext → a different nonce.
  Bytes m2 = relay::seal(k, kHost, t, rnd, 1763078400123ull, ByteView(Bytes{0x80, 9}));
  CHECK(Bytes(m.begin() + 1, m.begin() + 25) != Bytes(m2.begin() + 1, m2.begin() + 25));
}

TEST_CASE("relay: padding classes and size limits") {
  crypto::Key32 rp = relay::pair_root(seq_key(0));
  crypto::Key32 k = relay::frame_key(rp, 20406, kHost);
  std::string t = relay::topic(rp, 20406, kHost);
  uint8_t rnd[16] = {1};
  relay::Opened o;
  for (size_t len : {1u, 118u, 119u, 246u, 247u, 4086u}) {
    Bytes fr(len, 0x42);
    Bytes s = relay::seal(k, kHost, t, rnd, 1, ByteView(fr));
    REQUIRE(!s.empty());
    CHECK(s.size() - 41 == relay::padded_size(len));
    CHECK(relay::open(k, kHost, t, ByteView(s), &o));
    CHECK(o.frame == fr);
  }
  CHECK(relay::seal(k, kHost, t, rnd, 1, ByteView(Bytes(4087, 1))).empty());
  CHECK(relay::seal(k, kHost, t, rnd, 1, ByteView()).empty());
}

TEST_CASE("relay: open rejects tampering, wrong topic, host or epoch, bad sizes") {
  crypto::Key32 root = relay::pair_root(seq_key(0));
  crypto::Key32 k = relay::frame_key(root, 20406, kHost);
  std::string t = relay::topic(root, 20406, kHost);
  uint8_t rnd[16] = {7};
  Bytes m = relay::seal(k, kHost, t, rnd, 5, ByteView(Bytes{1, 2, 3}));
  relay::Opened o;
  CHECK(relay::open(k, kHost, t, ByteView(m), &o));
  CHECK(!relay::open(k, kHost, relay::topic(root, 20407, kHost), ByteView(m), &o));  // AD binds the topic
  CHECK(!relay::open(k, "other.example.org:8084", t, ByteView(m), &o));             // and the broker
  CHECK(!relay::open(relay::frame_key(root, 20407, kHost), kHost, t, ByteView(m), &o));
  for (size_t i = 0; i < m.size(); ++i) {
    Bytes x = m;
    x[i] ^= 0x01;
    CHECK(!relay::open(k, kHost, t, ByteView(x), &o));
  }
  for (size_t n : {0u, 1u, 24u, 40u, 41u, 100u, 168u}) {
    Bytes x(m.begin(), m.begin() + static_cast<long>(std::min(n, m.size())));
    CHECK(!relay::open(k, kHost, t, ByteView(x), &o));
  }
  Bytes longer = m;
  longer.push_back(0);
  CHECK(!relay::open(k, kHost, t, ByteView(longer), &o));
}

}  // TEST_SUITE
