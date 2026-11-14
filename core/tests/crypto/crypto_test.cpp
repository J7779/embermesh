#include <algorithm>
#include <set>
#include <string>

#include "base/codec.h"
#include "base/uri.h"
#include "crypto/crypto.h"
#include "crypto/identity.h"
#include "crypto/rng.h"
#include "doctest.h"
#include "monocypher/monocypher.h"

using namespace ember;
using namespace ember::crypto;

namespace {
Bytes H(std::string_view hex) {
  Bytes b;
  REQUIRE(from_hex(hex, b));
  return b;
}
Key32 K(std::string_view hex) {
  Bytes b = H(hex);
  REQUIRE(b.size() == 32);
  Key32 k;
  std::copy(b.begin(), b.end(), k.begin());
  return k;
}
std::string hex(const uint8_t* p, size_t n) { return to_hex(ByteView(p, n)); }
KdfParams fast_kdf() { return KdfParams{8, 1}; }
}  // namespace

TEST_SUITE("crypto") {

TEST_CASE("BLAKE2b-512 RFC 7693 vector and SHA-512 FIPS vector") {
  uint8_t out[64];
  blake2b(out, 64, ByteView(std::string_view("abc")));
  CHECK(hex(out, 64) ==
        "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc95"
        "18d38aa8dbf1925ab92386edd4009923");
  sha512(out, ByteView(std::string_view("abc")));
  CHECK(hex(out, 64) ==
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd"
        "454d4423643ce80e2a9ac94fa54ca49f");
  // Incremental == one-shot, field() framing differs from plain concatenation.
  Blake2b h(32);
  h.update(std::string_view("a")).update(std::string_view("bc"));
  Key32 inc = h.final32();
  CHECK(inc == blake2b_256(ByteView(std::string_view("abc"))));
  Blake2b f1(32), f2(32);
  f1.field(ByteView(std::string_view("ab"))).field(ByteView(std::string_view("c")));
  f2.field(ByteView(std::string_view("a"))).field(ByteView(std::string_view("bc")));
  CHECK(f1.final32() != f2.final32());
  // Keyed differs from unkeyed.
  CHECK(blake2b_256(ByteView(std::string_view("abc")), ByteView(std::string_view("k"))) != inc);
}

TEST_CASE("X25519 RFC 7748 section 6.1") {
  Key32 a_sk = K("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a");
  Key32 b_sk = K("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb");
  Key32 a_pk, b_pk;
  x25519_public(a_pk, a_sk);
  x25519_public(b_pk, b_sk);
  CHECK(hex(a_pk.data(), 32) == "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a");
  CHECK(hex(b_pk.data(), 32) == "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f");
  Key32 s1, s2;
  CHECK(x25519(s1, a_sk, b_pk));
  CHECK(x25519(s2, b_sk, a_pk));
  CHECK(s1 == s2);
  CHECK(hex(s1.data(), 32) == "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742");
  // Low-order point (all zero) is rejected.
  Key32 zero{};
  Key32 s3;
  CHECK_FALSE(x25519(s3, a_sk, zero));
}

TEST_CASE("Ed25519 RFC 8032 test vectors 1 and 2") {
  {
    Key32 seed = K("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60");
    SignSecret sk;
    Key32 pk;
    ed25519_keypair(seed, sk, pk);
    CHECK(hex(pk.data(), 32) == "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    uint8_t sig[64];
    ed25519_sign(sig, sk, ByteView());
    CHECK(hex(sig, 64) ==
          "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b4"
          "6bd25bf5f0595bbe24655141438e7a100b");
    CHECK(ed25519_verify(sig, pk, ByteView()));
    sig[10] ^= 1;
    CHECK_FALSE(ed25519_verify(sig, pk, ByteView()));
  }
  {
    Key32 seed = K("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb");
    SignSecret sk;
    Key32 pk;
    ed25519_keypair(seed, sk, pk);
    CHECK(hex(pk.data(), 32) == "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c");
    Bytes msg = H("72");
    uint8_t sig[64];
    ed25519_sign(sig, sk, msg);
    CHECK(hex(sig, 64) ==
          "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d"
          "8c387b2eaeb4302aeeb00d291612bb0c00");
    CHECK(ed25519_verify(sig, pk, msg));
    Bytes other = H("73");
    CHECK_FALSE(ed25519_verify(sig, pk, other));
  }
}

TEST_CASE("XChaCha20-Poly1305 draft-irtf-cfrg-xchacha A.3.1") {
  std::string pt =
      "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, "
      "sunscreen would be it.";
  Bytes ad = H("50515253c0c1c2c3c4c5c6c7");
  Key32 key = K("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f");
  Bytes nonce = H("404142434445464748494a4b4c4d4e4f5051525354555657");
  Bytes ct = seal(key, nonce.data(), ad, ByteView(std::string_view(pt)));
  CHECK(to_hex(ByteView(ct).sub(0, pt.size())) ==
        "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb731c7f1b0b4aa6440bf3a82f4eda7e"
        "39ae64c6708c54c216cb96b72e1213b4522f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3"
        "fff921f9664c97637da9768812f615c68b13b52e");
  CHECK(to_hex(ByteView(ct).sub(pt.size())) == "c0875924c1c7987947deafd8780acf49");
  Bytes back;
  CHECK(open(key, nonce.data(), ad, ct, back));
  CHECK(ByteView(back).to_string() == pt);
  // Any single bit flip in ciphertext, tag or AD is rejected.
  for (size_t i = 0; i < ct.size(); i += 7) {
    Bytes bad = ct;
    bad[i] ^= 0x20;
    CHECK_FALSE(open(key, nonce.data(), ad, bad, back));
    CHECK(back.empty());
  }
  Bytes bad_ad = ad;
  bad_ad[0] ^= 1;
  CHECK_FALSE(open(key, nonce.data(), bad_ad, ct, back));
  CHECK_FALSE(open(key, nonce.data(), ad, ByteView(ct).sub(0, 15), back));
  // Empty plaintext works.
  Bytes e = seal(key, nonce.data(), ByteView(), ByteView());
  CHECK(e.size() == 16);
  CHECK(open(key, nonce.data(), ByteView(), e, back));
  CHECK(back.empty());
}

TEST_CASE("Argon2id RFC 9106 section 5.3 (via Monocypher) and wrapper bounds") {
  uint8_t pass[32], salt[16], secret[8], ad[12];
  std::memset(pass, 1, 32);
  std::memset(salt, 2, 16);
  std::memset(secret, 3, 8);
  std::memset(ad, 4, 12);
  crypto_argon2_config cfg{CRYPTO_ARGON2_ID, 32, 3, 4};
  crypto_argon2_inputs in{pass, salt, 32, 16};
  crypto_argon2_extras ex{secret, ad, 8, 12};
  std::vector<uint8_t> work(32 * 1024);
  uint8_t tag[32];
  crypto_argon2(tag, 32, work.data(), cfg, in, ex);
  CHECK(hex(tag, 32) == "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659");

  uint8_t o1[32], o2[32];
  CHECK(argon2id(o1, 32, ByteView(std::string_view("pw")), ByteView(salt, 16), fast_kdf()));
  CHECK(argon2id(o2, 32, ByteView(std::string_view("pw")), ByteView(salt, 16), fast_kdf()));
  CHECK(std::memcmp(o1, o2, 32) == 0);
  CHECK(argon2id(o2, 32, ByteView(std::string_view("pX")), ByteView(salt, 16), fast_kdf()));
  CHECK(std::memcmp(o1, o2, 32) != 0);
  CHECK(argon2id(o2, 32, ByteView(), ByteView(salt, 16), fast_kdf()));  // empty passphrase ok
  CHECK_FALSE(argon2id(o1, 32, ByteView(), ByteView(salt, 7), fast_kdf()));  // short salt
  CHECK_FALSE(argon2id(o1, 32, ByteView(), ByteView(salt, 16), KdfParams{4, 1}));
  CHECK_FALSE(argon2id(o1, 32, ByteView(), ByteView(salt, 16), KdfParams{8, 0}));
  CHECK_FALSE(argon2id(o1, 32, ByteView(), ByteView(salt, 16), KdfParams{KdfParams::kMaxMemKib + 1, 1}));
  CHECK_FALSE(argon2id(o1, 2, ByteView(), ByteView(salt, 16), fast_kdf()));
}

TEST_CASE("Csprng is deterministic, reseedable, and unbiased enough") {
  Csprng a(ByteView(std::string_view("seed material that is long enough!!")));
  Csprng b(ByteView(std::string_view("seed material that is long enough!!")));
  CHECK(a.seeded());
  CHECK(a.bytes(100) == b.bytes(100));
  CHECK(a.u64() == b.u64());
  a.seed(ByteView(std::string_view("more")));
  CHECK(a.bytes(16) != b.bytes(16));
  Csprng c;
  CHECK_FALSE(c.seeded());
  // Successive outputs differ (fast key erasure).
  CHECK(c.bytes(32) != c.bytes(32));
  // Large fills work and differ from small ones.
  Bytes big = c.bytes(10000);
  CHECK(big.size() == 10000);
  int counts[10] = {};
  for (int i = 0; i < 10000; ++i) counts[a.uniform(10)]++;
  for (int k = 0; k < 10; ++k) CHECK(counts[k] > 800);
  CHECK(a.uniform(0) == 0);
  CHECK(a.uniform(1) == 0);
  int bits = 0;
  for (uint8_t x : big)
    for (int b = 0; b < 8; ++b) bits += (x >> b) & 1;  // portable popcount (no __builtin_* on MSVC)
  CHECK(bits > 39000);
  CHECK(bits < 41000);
}

TEST_CASE("wordlist: 256 distinct lowercase words") {
  std::set<std::string> seen;
  for (int i = 0; i < 256; ++i) {
    const char* w = fingerprint_word(static_cast<uint8_t>(i));
    REQUIRE(w != nullptr);
    std::string s(w);
    CHECK(s.size() >= 3);
    CHECK(s.size() <= 8);
    for (char c : s) CHECK((c >= 'a' && c <= 'z'));
    CHECK(seen.insert(s).second);
    CHECK(fingerprint_word_index(s) == i);
  }
  CHECK(fingerprint_word_index("CASTLE") == fingerprint_word_index("castle"));
  CHECK(fingerprint_word_index("zebra-not") == -1);
}

TEST_CASE("Identity derivation, signing, DH, node id") {
  Csprng rng(ByteView(std::string_view("identity test seed 0123456789abcdef")));
  Identity a = Identity::generate(rng);
  Identity b = Identity::generate(rng);
  REQUIRE(a.valid());
  CHECK(a.node_id() != b.node_id());
  CHECK(a.node_id() == a.public_keys().node_id());
  CHECK(node_id_hex(a.node_id()).size() == 16);
  NodeId parsed;
  CHECK(parse_node_id(node_id_hex(a.node_id()), parsed));
  CHECK(parsed == a.node_id());
  CHECK_FALSE(parse_node_id("0123", parsed));
  CHECK_FALSE(parse_node_id("0123456789abcdeg", parsed));
  CHECK(node_id_from_u64(node_id_u64(a.node_id())) == a.node_id());

  auto sig = a.sign(ByteView(std::string_view("hello")));
  CHECK(ed25519_verify(sig.data(), a.sign_pk(), ByteView(std::string_view("hello"))));
  CHECK_FALSE(ed25519_verify(sig.data(), b.sign_pk(), ByteView(std::string_view("hello"))));
  Key32 s1, s2;
  CHECK(a.dh(b.dh_pk(), s1));
  CHECK(b.dh(a.dh_pk(), s2));
  CHECK(s1 == s2);

  // Deterministic from seed.
  Key32 seed = K("000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f");
  Identity x = Identity::from_seed(seed), y = Identity::from_seed(seed);
  CHECK(x.node_id() == y.node_id());
  CHECK(x.public_keys() == y.public_keys());
  CHECK(x.fingerprint().size() == 8);
  CHECK(x.fingerprint() == y.fingerprint());
  CHECK(x.fingerprint() != a.fingerprint());

  Bytes ser = x.serialize_secret();
  CHECK(ser.size() == 33);
  Identity z;
  CHECK(Identity::deserialize_secret(ser, z));
  CHECK(z.node_id() == x.node_id());
  ser[0] = 2;
  CHECK_FALSE(Identity::deserialize_secret(ser, z));
  CHECK_FALSE(Identity::deserialize_secret(ByteView(ser).sub(0, 10), z));
  Identity empty;
  CHECK_FALSE(empty.valid());
  CHECK(empty.serialize_secret().empty());
}

TEST_CASE("Identity backup round trip and hostile backups") {
  Csprng rng(ByteView(std::string_view("backup test seed 0123456789abcdef!!")));
  Identity a = Identity::generate(rng);
  std::string backup = a.export_backup("correct horse", rng, ByteView(std::string_view("{\"name\":\"Ann\"}")), fast_kdf());
  CHECK(backup.rfind("ember:backup?", 0) == 0);
  Identity b;
  Bytes extra;
  std::string err;
  CHECK(Identity::import_backup(backup, "correct horse", b, &extra, &err));
  CHECK(b.node_id() == a.node_id());
  CHECK(ByteView(extra).to_string() == "{\"name\":\"Ann\"}");
  CHECK_FALSE(Identity::import_backup(backup, "wrong", b, nullptr, &err));
  CHECK(err.find("passphrase") != std::string::npos);
  CHECK_FALSE(Identity::import_backup("garbage", "x", b));
  CHECK_FALSE(Identity::import_backup("ember:backup?v=1&d=AAAA", "x", b));
  CHECK_FALSE(Identity::import_backup("ember:backup?v=2&d=AAAA", "x", b));
  // A backup demanding absurd memory is refused before any allocation.
  ByteWriter w;
  w.u8(1).varint(0xFFFFFFFFull).u8(3);
  Bytes huge = w.take();
  huge.resize(huge.size() + 16 + 24 + 48);
  std::string hostile = "ember:backup?v=1&d=" + base64url_encode(huge);
  CHECK_FALSE(Identity::import_backup(hostile, "x", b, nullptr, &err));
  // Flip each character of the payload: never accepted, never crashes.
  for (size_t i = 19; i < backup.size(); i += 3) {
    std::string t = backup;
    t[i] = t[i] == 'A' ? 'B' : 'A';
    Identity c;
    CHECK_FALSE(Identity::import_backup(t, "correct horse", c));
  }
}

TEST_CASE("Contact card URI") {
  Csprng rng(ByteView(std::string_view("card test seed 0123456789abcdef!!!!")));
  Identity a = Identity::generate(rng);
  ContactCard c = a.card("Ann & Bob = café", "QX4ABC");
  std::string uri = c.to_uri();
  CHECK(uri.rfind("ember:card?id=" + node_id_hex(a.node_id()), 0) == 0);
  ContactCard p;
  std::string err;
  REQUIRE(ContactCard::parse(uri, p, &err));
  CHECK(p.keys == a.public_keys());
  CHECK(p.name == "Ann & Bob = café");
  CHECK(p.callsign == "QX4ABC");
  CHECK(ContactCard::parse("  " + uri + "\n", p));       // whitespace tolerated
  CHECK(ContactCard::parse("EMBER:" + uri.substr(6), p));  // scheme case-insensitive
  // Wrong id for the keys.
  std::string tampered = uri;
  size_t pos = tampered.find("id=") + 3;
  tampered[pos] = tampered[pos] == '0' ? '1' : '0';
  CHECK_FALSE(ContactCard::parse(tampered, p, &err));
  CHECK(err.find("match") != std::string::npos);
  CHECK_FALSE(ContactCard::parse("ember:card?id=00", p));
  CHECK_FALSE(ContactCard::parse("ember:invite?id=00", p));
  CHECK_FALSE(ContactCard::parse("http://x", p));
  CHECK_FALSE(ContactCard::parse(a.card("x").to_uri() + "&cs=bad%20call", p));
  ContactCard q = a.card("x", "lower");  // invalid callsign dropped
  CHECK(q.callsign.empty());
}

TEST_CASE("ember: URI parser") {
  EmberUri u;
  CHECK(EmberUri::parse("ember:invite?name=Camp%20North&k=abc", u));
  CHECK(u.kind == "invite");
  CHECK(u.get("name") == "Camp North");
  CHECK(u.get("missing", "d") == "d");
  CHECK(EmberUri::parse("ember://pair?x=1", u));
  CHECK(u.kind == "pair");
  CHECK(EmberUri::parse("ember:card", u));
  CHECK(u.params.empty());
  CHECK_FALSE(EmberUri::parse("ember:", u));
  CHECK_FALSE(EmberUri::parse("ember:Bad?x=1", u));
  CHECK_FALSE(EmberUri::parse("ember:a?x=%zz", u));
  CHECK_FALSE(EmberUri::parse("ember:a?x=%4", u));
  CHECK_FALSE(EmberUri::parse("ember:a?X=1", u));
  std::string many = "ember:a?";
  for (int i = 0; i < 40; ++i) many += "k=1&";
  CHECK_FALSE(EmberUri::parse(many, u));
  CHECK_FALSE(EmberUri::parse("ember:a?k=" + std::string(5000, 'x'), u));
  EmberUri b;
  b.kind = "t";
  b.add("v", "a b&c=d/é");
  CHECK(EmberUri::parse(b.to_string(), u));
  CHECK(u.get("v") == "a b&c=d/é");
  std::string out;
  CHECK(percent_decode("a+b", out));
  CHECK(out == "a+b");
}

}  // TEST_SUITE
