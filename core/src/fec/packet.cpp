#include "fec/packet.h"

#include <algorithm>

#include "ember/crc.h"
#include "fec/interleave.h"
#include "fec/reed_solomon.h"

namespace ember::fec {

namespace {

constexpr size_t kHeaderData = 4;
constexpr size_t kHeaderRoots = kPacketHeaderSize - kHeaderData;

struct Layout {
  size_t data;    // D = L + 4
  size_t blocks;  // B
  size_t m;       // data bytes per block
  size_t cw;      // m + nroots
  size_t total;   // envelope size
};

bool layout(size_t len, uint8_t nroots, Layout* out) {
  if (len > kMaxProtectedPayload || nroots < kMinRoots || nroots > kMaxRoots) return false;
  Layout l;
  l.data = len + 4;
  size_t kmax = 255 - nroots;
  l.blocks = (l.data + kmax - 1) / kmax;
  l.m = (l.data + l.blocks - 1) / l.blocks;
  l.cw = l.m + nroots;
  l.total = kPacketHeaderSize + l.blocks * l.cw;
  *out = l;
  return true;
}

// Cheap to build (6 roots); constructed on use so there is no static state.
ReedSolomon header_rs() { return ReedSolomon(kHeaderRoots); }

// Decodes the header in a copy. Returns header bytes corrected, or -1.
int read_header(ByteView in, uint8_t hdr[kPacketHeaderSize]) {
  if (in.size() < kPacketHeaderSize) return -1;
  for (size_t i = 0; i < kPacketHeaderSize; ++i) hdr[i] = in[i];
  return header_rs().decode(hdr, kPacketHeaderSize);
}

bool parse_header(const uint8_t hdr[kPacketHeaderSize], size_t max_payload, bool* interleaved, uint8_t* nroots,
                  size_t* len, Layout* l) {
  if ((hdr[0] >> 4) != kPacketVersion || (hdr[0] & 0x0E) != 0) return false;
  *interleaved = (hdr[0] & 1) != 0;
  *nroots = hdr[1];
  *len = static_cast<size_t>(hdr[2]) << 8 | hdr[3];
  if (*len > max_payload) return false;
  return layout(*len, *nroots, l);
}

}  // namespace

size_t protected_size(size_t len, const ProtectParams& p) {
  Layout l;
  return layout(len, p.nroots, &l) ? l.total : 0;
}

Bytes protect(ByteView payload, const ProtectParams& p) {
  Layout l;
  if (!layout(payload.size(), p.nroots, &l)) return Bytes();
  const ReedSolomon rs(p.nroots);

  Bytes out(l.total, 0);
  uint8_t hdr[kHeaderData] = {static_cast<uint8_t>(kPacketVersion << 4 | (p.interleave ? 1 : 0)), p.nroots,
                              static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size())};
  for (size_t i = 0; i < kHeaderData; ++i) out[i] = hdr[i];
  header_rs().encode(ByteView(hdr, kHeaderData), out.data() + kHeaderData);

  Bytes data(l.blocks * l.m, 0);
  if (!payload.empty()) std::copy(payload.begin(), payload.end(), data.begin());
  uint32_t c = crc32(payload);
  data[payload.size()] = static_cast<uint8_t>(c >> 24);
  data[payload.size() + 1] = static_cast<uint8_t>(c >> 16);
  data[payload.size() + 2] = static_cast<uint8_t>(c >> 8);
  data[payload.size() + 3] = static_cast<uint8_t>(c);

  Bytes body(l.blocks * l.cw);
  for (size_t b = 0; b < l.blocks; ++b) {
    uint8_t* cw = body.data() + b * l.cw;
    std::copy(data.begin() + static_cast<std::ptrdiff_t>(b * l.m),
              data.begin() + static_cast<std::ptrdiff_t>((b + 1) * l.m), cw);
    rs.encode(ByteView(cw, l.m), cw + l.m);
  }
  uint8_t* dst = out.data() + kPacketHeaderSize;
  if (p.interleave && l.blocks > 1) interleave(body.data(), dst, body.size(), l.blocks);
  else std::copy(body.begin(), body.end(), dst);
  return out;
}

bool peek_header(ByteView in, size_t* total_size, size_t max_payload, uint8_t* nroots_out) {
  uint8_t hdr[kPacketHeaderSize];
  if (read_header(in, hdr) < 0) return false;
  bool il;
  uint8_t nroots;
  size_t len;
  Layout l;
  if (!parse_header(hdr, max_payload, &il, &nroots, &len, &l)) return false;
  *total_size = l.total;
  if (nroots_out) *nroots_out = nroots;
  return true;
}

bool peek_header(ByteView in, size_t* total_size, size_t max_payload) {
  return peek_header(in, total_size, max_payload, nullptr);
}

bool recover(ByteView in, Bytes* payload, RecoverInfo* info, size_t max_payload) {
  RecoverInfo ri;
  uint8_t hdr[kPacketHeaderSize];
  int hc = read_header(in, hdr);
  if (hc < 0) return false;
  bool il;
  uint8_t nroots;
  size_t len;
  Layout l;
  if (!parse_header(hdr, max_payload, &il, &nroots, &len, &l)) return false;
  if (in.size() < l.total) return false;
  ri.header_corrected = static_cast<size_t>(hc);
  ri.blocks = l.blocks;
  ri.consumed = l.total;
  ri.nroots = nroots;

  const ReedSolomon rs(nroots);
  Bytes body(l.blocks * l.cw);
  const uint8_t* src = in.data() + kPacketHeaderSize;
  if (il && l.blocks > 1) deinterleave(src, body.data(), body.size(), l.blocks);
  else std::copy(src, src + body.size(), body.begin());

  Bytes data;
  data.reserve(l.blocks * l.m);
  for (size_t b = 0; b < l.blocks; ++b) {
    uint8_t* cw = body.data() + b * l.cw;
    int n = rs.decode(cw, l.cw);
    if (n < 0) return false;
    ri.corrected += static_cast<size_t>(n);
    data.insert(data.end(), cw, cw + l.m);
  }
  uint32_t want = static_cast<uint32_t>(data[len]) << 24 | static_cast<uint32_t>(data[len + 1]) << 16 |
                  static_cast<uint32_t>(data[len + 2]) << 8 | data[len + 3];
  if (crc32(ByteView(data.data(), len)) != want) return false;
  // Padding must be zero; anything else means a mis-correction slipped through.
  for (size_t i = len + 4; i < data.size(); ++i)
    if (data[i] != 0) return false;
  data.resize(len);
  *payload = std::move(data);
  if (info) *info = ri;
  return true;
}

}  // namespace ember::fec
