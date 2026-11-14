#include <algorithm>
#include <vector>

#include "doctest.h"
#include "fec/interleave.h"
#include "fec/packet.h"
#include "fec/test_rng.h"

using namespace ember;
using namespace ember::fec;
using fec_test::Rng;

TEST_SUITE("fec") {

TEST_CASE("protect/recover: sizes and clean round trips at every boundary") {
  Rng rng(400);
  // single block up to 255 - nroots - 4 payload bytes
  CHECK(protected_size(0) == 10 + 4 + 16);
  CHECK(protected_size(235) == 10 + 255);
  CHECK(protected_size(236) == 10 + 2 * (120 + 16));  // two blocks of 120
  CHECK(protected_size(65535) > 65535);
  CHECK(protected_size(65536) == 0);
  CHECK(protected_size(10, ProtectParams{1, true}) == 0);
  CHECK(protected_size(10, ProtectParams{129, true}) == 0);
  CHECK(protect(Bytes(65536)).empty());
  for (uint8_t nroots : {uint8_t(2), uint8_t(16), uint8_t(32), uint8_t(128)})
    for (bool il : {false, true})
      for (size_t len : {size_t(0), size_t(1), size_t(2), size_t(250 - nroots), size_t(251 - nroots),
                         size_t(252 - nroots), size_t(1000), size_t(4096)}) {
        ProtectParams p{nroots, il};
        Bytes payload = rng.bytes(len);
        Bytes env = protect(payload, p);
        INFO("nroots=" << int(nroots) << " il=" << il << " len=" << len);
        REQUIRE(env.size() == protected_size(len, p));
        size_t total = 0;
        CHECK(peek_header(env, &total));
        CHECK(total == env.size());
        Bytes out;
        RecoverInfo info;
        REQUIRE(recover(env, &out, &info));
        CHECK(out == payload);
        CHECK(info.corrected == 0);
        CHECK(info.consumed == env.size());
        CHECK(info.nroots == nroots);
        // trailing modem padding is ignored
        env.push_back(0x55);
        CHECK(recover(env, &out));
        // truncation by one byte fails cleanly
        env.resize(env.size() - 2);
        CHECK_FALSE(recover(env, &out));
      }
  // max payload cap on the receive side
  Bytes env = protect(Bytes(5000, 1));
  Bytes out;
  CHECK_FALSE(recover(env, &out));  // default cap 4096
  CHECK(recover(env, &out, nullptr, 5000));
  CHECK_FALSE(recover(protect(Bytes(4097, 1)), &out));
  CHECK(recover(protect(Bytes(4096, 1)), &out));
  // the full 16-bit range works when the caller allows it
  Bytes big = rng.bytes(65535);
  CHECK(recover(protect(big), &out, nullptr, 65535));
  CHECK(out == big);
}

TEST_CASE("protect/recover: corrects t errors per block and reports them") {
  Rng rng(401);
  for (int iter = 0; iter < 30; ++iter) {
    Bytes payload = rng.bytes(1 + rng.below(700));
    ProtectParams p{16, false};  // non-interleaved: blocks are contiguous
    Bytes env = protect(payload, p);
    size_t len = payload.size() + 4;
    size_t blocks = (len + 238) / 239;
    size_t m = (len + blocks - 1) / blocks;
    size_t cw = m + 16;
    // 8 errors in every block + 3 in the header
    size_t injected = 0;
    for (size_t b = 0; b < blocks; ++b) {
      std::vector<size_t> pos;
      while (pos.size() < 8) {
        size_t q = rng.below(cw);
        if (std::find(pos.begin(), pos.end(), q) == pos.end()) pos.push_back(q);
      }
      for (size_t q : pos) env[10 + b * cw + q] ^= static_cast<uint8_t>(1 + rng.below(255));
      injected += 8;
    }
    env[0] ^= 0xFF;
    env[4] ^= 0x01;
    env[9] ^= 0x80;
    Bytes out;
    RecoverInfo info;
    REQUIRE(recover(env, &out, &info));
    CHECK(out == payload);
    CHECK(info.corrected == injected);
    CHECK(info.header_corrected == 3);
    CHECK(info.blocks == blocks);
  }
}

TEST_CASE("protect/recover: a burst of B*t bytes is corrected only thanks to interleaving") {
  Rng rng(402);
  Bytes payload = rng.bytes(900);  // 4 blocks with nroots 16
  for (bool il : {true, false}) {
    ProtectParams p{16, il};
    Bytes env = protect(payload, p);
    size_t burst = 4 * 8;  // 32 consecutive bytes
    size_t start = 10 + rng.below(env.size() - 10 - burst);
    for (size_t i = start; i < start + burst; ++i) env[i] ^= static_cast<uint8_t>(1 + rng.below(255));
    Bytes out;
    RecoverInfo info;
    bool ok = recover(env, &out, &info);
    if (il) {
      REQUIRE(ok);
      CHECK(out == payload);
      CHECK(info.corrected == burst);
      MESSAGE("interleaved 4x RS(242,226): 32-byte burst corrected, " << info.corrected << " symbols");
    } else {
      CHECK_FALSE(ok);  // 32 errors land in one or two blocks: beyond t=8
    }
  }
}

TEST_CASE("protect/recover: header damage beyond 3 bytes and body beyond t fail cleanly") {
  Rng rng(403);
  Bytes payload = rng.bytes(100);
  Bytes env = protect(payload);
  Bytes out;
  int header_fail = 0;
  for (int iter = 0; iter < 100; ++iter) {
    Bytes e = env;
    std::vector<size_t> pos;
    while (pos.size() < 4) {
      size_t q = rng.below(10);
      if (std::find(pos.begin(), pos.end(), q) == pos.end()) pos.push_back(q);
    }
    for (size_t q : pos) e[q] ^= static_cast<uint8_t>(1 + rng.below(255));
    header_fail += !recover(e, &out);
    if (recover(e, &out)) CHECK(out == payload);  // never a wrong payload
  }
  CHECK(header_fail >= 95);
  // 9 errors in the single body block
  for (int iter = 0; iter < 100; ++iter) {
    Bytes e = env;
    std::vector<size_t> pos;
    while (pos.size() < 9) {
      size_t q = 10 + rng.below(env.size() - 10);
      if (std::find(pos.begin(), pos.end(), q) == pos.end()) pos.push_back(q);
    }
    for (size_t q : pos) e[q] ^= static_cast<uint8_t>(1 + rng.below(255));
    CHECK_FALSE(recover(e, &out));
  }
}

TEST_CASE("protect/recover fuzz: garbage and heavy corruption are never accepted") {
  Rng rng(404);
  Bytes out;
  size_t total = 0;
  for (int iter = 0; iter < 3000; ++iter) {
    Bytes g = rng.bytes(rng.below(400));
    CHECK_FALSE(recover(g, &out));
    peek_header(g, &total);
  }
  // forged headers (valid RS) with inconsistent fields
  for (uint8_t ver : {uint8_t(0x00), uint8_t(0x20), uint8_t(0x13)}) {
    Bytes env = protect(Bytes{1, 2, 3});
    env[0] = ver;  // breaks the header codeword -> RS repairs it back (1 error) -> still fine
    CHECK(recover(env, &out));
  }
  Bytes payload = rng.bytes(300);
  Bytes env = protect(payload);
  int wrong = 0;
  for (int iter = 0; iter < 500; ++iter) {
    Bytes e = env;
    size_t nerr = 20 + rng.below(100);
    for (size_t k = 0; k < nerr; ++k) e[rng.below(e.size())] = rng.byte();
    if (recover(e, &out) && out != payload) ++wrong;
  }
  CHECK(wrong == 0);
}

}  // TEST_SUITE
