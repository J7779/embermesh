#include "crypto/identity.h"

#include <cstring>

#include "base/codec.h"
#include "base/json.h"
#include "base/uri.h"

namespace ember {

namespace {

// 256 distinct, concrete, easy-to-say English words (no homophones among them). Index = byte.
const char* const kWords[256] = {
    "acid",    "acorn",   "actor",   "adult",    "agent",   "alarm",   "album",   "alert",
    "alien",   "alpha",   "amber",   "angle",    "ankle",   "apple",   "apron",   "arena",
    "arrow",   "atlas",   "attic",   "audio",    "award",   "bacon",   "badge",   "bagel",
    "baker",   "bamboo",  "banjo",   "barn",     "basin",   "basket",  "beach",   "beard",
    "bench",   "berry",   "bison",   "blade",    "blanket", "blimp",   "bloom",   "blues",
    "bonus",   "boot",    "bottle",  "boxer",    "brain",   "brass",   "bread",   "brick",
    "bridge",  "brush",   "bubble",  "bucket",   "buffalo", "bugle",   "cabin",   "cactus",
    "camel",   "candle",  "canoe",   "canyon",   "carbon",  "cargo",   "carpet",  "castle",
    "cedar",   "cello",   "chalk",   "cherry",   "chess",   "chimney", "cider",   "cinema",
    "circus",  "clam",    "cliff",   "clock",    "cloud",   "clover",  "cobra",   "cocoa",
    "comet",   "coral",   "cotton",  "cougar",   "cowboy",  "coyote",  "crab",    "crane",
    "crater",  "crayon",  "cricket", "crystal",  "cupcake", "curtain", "cymbal",  "dagger",
    "daisy",   "dancer",  "delta",   "denim",    "desert",  "diamond", "diesel",  "dinner",
    "dolphin", "donkey",  "dragon",  "drum",     "duck",    "eagle",   "easel",   "echo",
    "eclipse", "elbow",   "elephant", "elk",     "ember",   "emerald", "engine",  "falcon",
    "feather", "fern",    "ferry",   "fiddle",   "film",    "finger",  "flag",    "flame",
    "flute",   "forest",  "fossil",  "fountain", "fox",     "galaxy",  "garden",  "garlic",
    "gazelle", "geyser",  "giant",   "ginger",   "giraffe", "glacier", "glove",   "goat",
    "gold",    "gorilla", "granite", "grape",    "gravel",  "guitar",  "hammer",  "harbor",
    "harp",    "hawk",    "hazel",   "helmet",   "hermit",  "honey",   "hornet",  "horse",
    "hotel",   "husky",   "igloo",   "indigo",   "iris",    "island",  "ivory",   "jacket",
    "jaguar",  "jasmine", "jelly",   "jester",   "jigsaw",  "judge",   "jungle",  "kayak",
    "kettle",  "kidney",  "kiwi",    "koala",    "ladder",  "lagoon",  "lantern", "laser",
    "lemon",   "leopard", "lettuce", "lilac",    "lime",    "lion",    "lizard",  "lobster",
    "locket",  "lotus",   "lumber",  "magnet",   "mango",   "maple",   "marble",  "market",
    "meadow",  "melon",   "mermaid", "meteor",   "mirror",  "mitten",  "monkey",  "moose",
    "mosaic",  "motor",   "muffin",  "museum",   "mustang", "napkin",  "nectar",  "needle",
    "nickel",  "noodle",  "nugget",  "oasis",    "ocean",   "olive",   "onion",   "orbit",
    "orchid",  "otter",   "oven",    "owl",      "oyster",  "paddle",  "panda",   "panther",
    "paper",   "parrot",  "peach",   "peanut",   "pebble",  "pelican", "pencil",  "pepper",
    "piano",   "pickle",  "pigeon",  "pillow",   "pilot",   "pine",    "pirate",  "pizza",
    "planet",  "plum",    "pony",    "popcorn",  "potato",  "pretzel", "prism",   "pumpkin",
};

bool valid_callsign(std::string_view cs) {
  if (cs.size() > 16) return false;
  for (char c : cs) {
    bool ok = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool set_err(std::string* e, const char* m) {
  if (e) *e = m;
  return false;
}

}  // namespace

std::string node_id_hex(const NodeId& id) { return to_hex(ByteView(id.data(), id.size())); }

bool parse_node_id(std::string_view hex, NodeId& out) {
  if (hex.size() != 16) return false;
  Bytes b;
  if (!from_hex(hex, b) || b.size() != 8) return false;
  std::memcpy(out.data(), b.data(), 8);
  return true;
}

NodeId PublicKeys::node_id() const {
  crypto::Blake2b h(32);
  h.update(std::string_view("ember-node-id-v1")).update(sign_pk.data(), 32).update(dh_pk.data(), 32);
  crypto::Key32 d = h.final32();
  NodeId id;
  std::memcpy(id.data(), d.data(), 8);
  return id;
}

const char* fingerprint_word(uint8_t index) { return kWords[index]; }

int fingerprint_word_index(std::string_view word) {
  if (word.size() > 16) return -1;
  std::string lower;
  for (char c : word) lower.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
  for (int i = 0; i < 256; ++i)
    if (lower == kWords[i]) return i;
  return -1;
}

std::vector<std::string> fingerprint_words(const PublicKeys& keys, size_t count) {
  crypto::Blake2b h(32);
  h.update(std::string_view("ember-fp-v1")).update(keys.sign_pk.data(), 32).update(keys.dh_pk.data(), 32);
  crypto::Key32 d = h.final32();
  std::vector<std::string> out;
  if (count > 32) count = 32;
  for (size_t i = 0; i < count; ++i) out.emplace_back(kWords[d[i]]);
  return out;
}

// --- ContactCard ------------------------------------------------------------------------------

std::string ContactCard::to_uri() const {
  EmberUri u;
  u.kind = "card";
  u.add("id", node_id_hex(node_id()));
  Bytes k(keys.sign_pk.begin(), keys.sign_pk.end());
  k.insert(k.end(), keys.dh_pk.begin(), keys.dh_pk.end());
  u.add("k", base64url_encode(k));
  if (!name.empty()) u.add("n", name);
  if (!callsign.empty()) u.add("cs", callsign);
  return u.to_string();
}

bool ContactCard::parse(std::string_view uri, ContactCard& out, std::string* error) {
  out = ContactCard();
  EmberUri u;
  if (!EmberUri::parse(uri, u, error)) return false;
  if (u.kind != "card") return set_err(error, "not a contact card");
  const std::string* id = u.find("id");
  const std::string* k = u.find("k");
  if (!id || !k) return set_err(error, "card is missing id or keys");
  NodeId nid;
  if (!parse_node_id(*id, nid)) return set_err(error, "bad node id");
  Bytes kb;
  if (!base64url_decode(*k, kb) || kb.size() != 64) return set_err(error, "bad keys");
  ContactCard c;
  std::memcpy(c.keys.sign_pk.data(), kb.data(), 32);
  std::memcpy(c.keys.dh_pk.data(), kb.data() + 32, 32);
  if (c.keys.node_id() != nid) return set_err(error, "node id does not match keys");
  c.name = u.get("n");
  if (c.name.size() > 64 || !utf8_valid(c.name)) return set_err(error, "bad name");
  c.callsign = u.get("cs");
  if (!valid_callsign(c.callsign)) return set_err(error, "bad callsign");
  out = std::move(c);
  return true;
}

// --- Identity -------------------------------------------------------------------------------------

Identity::Identity() = default;
Identity::~Identity() { wipe_all(); }
Identity::Identity(const Identity& o) { *this = o; }
Identity& Identity::operator=(const Identity& o) {
  if (this == &o) return *this;
  valid_ = o.valid_;
  seed_ = o.seed_;
  sign_sk_ = o.sign_sk_;
  dh_sk_ = o.dh_sk_;
  pub_ = o.pub_;
  node_id_ = o.node_id_;
  return *this;
}

void Identity::wipe_all() {
  crypto::wipe(seed_);
  crypto::wipe(sign_sk_);
  crypto::wipe(dh_sk_);
  valid_ = false;
}

void Identity::derive() {
  crypto::Key32 sign_seed = crypto::blake2b_256(ByteView(std::string_view("ember-sign-v1")),
                                                ByteView(seed_.data(), 32));
  crypto::ed25519_keypair(sign_seed, sign_sk_, pub_.sign_pk);
  crypto::wipe(sign_seed);
  dh_sk_ = crypto::blake2b_256(ByteView(std::string_view("ember-dh-v1")), ByteView(seed_.data(), 32));
  crypto::x25519_public(pub_.dh_pk, dh_sk_);
  node_id_ = pub_.node_id();
  valid_ = true;
}

Identity Identity::generate(crypto::Csprng& rng) {
  Identity id;
  rng.fill(id.seed_);
  id.derive();
  return id;
}

Identity Identity::from_seed(const crypto::Key32& seed) {
  Identity id;
  id.seed_ = seed;
  id.derive();
  return id;
}

void Identity::sign(ByteView msg, uint8_t sig[64]) const { crypto::ed25519_sign(sig, sign_sk_, msg); }

crypto::Sig64 Identity::sign(ByteView msg) const {
  crypto::Sig64 s;
  sign(msg, s.data());
  return s;
}

bool Identity::dh(const crypto::Key32& their, crypto::Key32& shared) const {
  return crypto::x25519(shared, dh_sk_, their);
}

Bytes Identity::serialize_secret() const {
  Bytes b;
  if (!valid_) return b;
  b.push_back(1);
  b.insert(b.end(), seed_.begin(), seed_.end());
  return b;
}

bool Identity::deserialize_secret(ByteView data, Identity& out) {
  if (data.size() != 33 || data[0] != 1) return false;
  crypto::Key32 s;
  std::memcpy(s.data(), data.data() + 1, 32);
  out = from_seed(s);
  crypto::wipe(s);
  return true;
}

std::string Identity::export_backup(std::string_view passphrase, crypto::Csprng& rng, ByteView extra,
                                    const crypto::KdfParams& params) const {
  if (!valid_ || !params.valid() || extra.size() > 65536) return "";
  ByteWriter w;
  w.u8(1).varint(params.mem_kib).u8(static_cast<uint8_t>(params.passes));
  uint8_t salt[16], nonce[24];
  rng.fill(salt, 16);
  rng.fill(nonce, 24);
  w.bytes(salt, 16).bytes(nonce, 24);
  Bytes header = w.take();
  crypto::Key32 key;
  if (!crypto::argon2id(key.data(), 32, ByteView(passphrase), ByteView(salt, 16), params)) return "";
  Bytes pt(seed_.begin(), seed_.end());
  pt.insert(pt.end(), extra.begin(), extra.end());
  Bytes ct = crypto::seal(key, nonce, header, pt);
  crypto::wipe(pt);
  crypto::wipe(key);
  header.insert(header.end(), ct.begin(), ct.end());
  EmberUri u;
  u.kind = "backup";
  u.add("v", "1").add("d", base64url_encode(header));
  return u.to_string();
}

bool Identity::import_backup(std::string_view text, std::string_view passphrase, Identity& out,
                             Bytes* extra, std::string* error, uint32_t max_mem_kib) {
  EmberUri u;
  if (!EmberUri::parse(text, u, error)) return false;
  if (u.kind != "backup" || u.get("v") != "1") return set_err(error, "not an identity backup");
  Bytes blob;
  if (!base64url_decode(u.get("d"), blob)) return set_err(error, "corrupt backup");
  ByteReader r{ByteView(blob)};
  uint8_t ver = r.u8();
  uint64_t mem = r.varint();
  uint8_t passes = r.u8();
  ByteView salt = r.bytes(16);
  ByteView nonce = r.bytes(24);
  size_t header_len = r.pos();
  ByteView ct = r.rest();
  if (!r.ok() || ver != 1) return set_err(error, "corrupt backup");
  crypto::KdfParams p;
  p.mem_kib = mem > 0xFFFFFFFFu ? 0 : static_cast<uint32_t>(mem);
  p.passes = passes;
  // A hostile backup must not demand absurd work (security review L8): memory capped by the
  // caller (default 64 MiB), passes at 8.
  if (!p.valid() || p.mem_kib > max_mem_kib || p.passes > 8) return set_err(error, "unsupported backup parameters");
  if (ct.size() < 32 + crypto::kMacSize) return set_err(error, "corrupt backup");
  crypto::Key32 key;
  if (!crypto::argon2id(key.data(), 32, ByteView(passphrase), salt, p)) return set_err(error, "kdf failed");
  Bytes pt;
  bool ok = crypto::open(key, nonce.data(), ByteView(blob.data(), header_len), ct, pt);
  crypto::wipe(key);
  if (!ok) return set_err(error, "wrong passphrase or corrupt backup");
  crypto::Key32 seed;
  std::memcpy(seed.data(), pt.data(), 32);
  out = from_seed(seed);
  crypto::wipe(seed);
  if (extra) extra->assign(pt.begin() + 32, pt.end());
  crypto::wipe(pt);
  return true;
}

ContactCard Identity::card(std::string_view name, std::string_view callsign) const {
  ContactCard c;
  c.keys = pub_;
  c.name = std::string(utf8_truncate(name, 64));
  c.callsign = valid_callsign(callsign) ? std::string(callsign) : std::string();
  return c;
}

}  // namespace ember
