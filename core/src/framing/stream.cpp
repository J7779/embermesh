#include "framing/stream.h"

#include "ember/crc.h"

namespace ember::stream {

bool encode_append(ByteView payload, Bytes& out, uint8_t type) {
  if (payload.size() > kMaxPayload) return false;
  size_t start = out.size();
  out.reserve(start + payload.size() + kOverhead);
  out.push_back(SYNC0);
  out.push_back(SYNC1);
  out.push_back(type);
  out.push_back(static_cast<uint8_t>(payload.size() >> 8));
  out.push_back(static_cast<uint8_t>(payload.size()));
  out.push_back(crc8(ByteView(out.data() + start + 2, 3)));
  out.insert(out.end(), payload.begin(), payload.end());
  uint32_t c = crc32(ByteView(out.data() + start + 2, 4 + payload.size()));
  out.push_back(static_cast<uint8_t>(c >> 24));
  out.push_back(static_cast<uint8_t>(c >> 16));
  out.push_back(static_cast<uint8_t>(c >> 8));
  out.push_back(static_cast<uint8_t>(c));
  return true;
}

Bytes encode(ByteView payload, uint8_t type) {
  Bytes out;
  encode_append(payload, out, type);
  return out;
}

Decoder::Decoder(size_t max_payload) : max_(max_payload > kMaxPayload ? kMaxPayload : max_payload) {
  credit_cap_ = 4 * kWorkPerByte * (max_ + kOverhead);
  credit_ = credit_cap_;
}

void Decoder::reset() {
  credit_ = credit_cap_;
  buf_.clear();
  head_ = 0;
  consumed_frame_ = 0;
  payload_ = ByteView();
}

void Decoder::drop(size_t n) {
  head_ += n;
  if (head_ >= buf_.size()) {
    buf_.clear();
    head_ = 0;
  }
}

bool Decoder::try_parse(bool* frame_ready) {
  for (;;) {
    size_t avail = buf_.size() - head_;
    const uint8_t* p = buf_.data() + head_;
    size_t skip = 0;
    while (skip < avail && !(p[skip] == SYNC0 && (skip + 1 == avail || p[skip + 1] == SYNC1))) ++skip;
    if (skip) {
      stats_.discarded_bytes += skip;
      drop(skip);
      continue;
    }
    if (avail < kHeaderSize) return false;
    if (crc8(ByteView(p + 2, 3)) != p[5]) {
      ++stats_.bad_header;
      ++stats_.discarded_bytes;
      drop(1);
      continue;
    }
    size_t len = static_cast<size_t>(p[3]) << 8 | p[4];
    if (len > max_) {
      ++stats_.oversize;
      ++stats_.discarded_bytes;
      drop(1);
      continue;
    }
    size_t total = kOverhead + len;
    if (avail < total) return false;
    if (credit_ < len + 4) {
      ++stats_.budget_skipped;
      ++stats_.discarded_bytes;
      drop(1);
      continue;
    }
    credit_ -= len + 4;
    uint32_t want = static_cast<uint32_t>(p[6 + len]) << 24 | static_cast<uint32_t>(p[7 + len]) << 16 |
                    static_cast<uint32_t>(p[8 + len]) << 8 | p[9 + len];
    if (crc32(ByteView(p + 2, 4 + len)) != want) {
      ++stats_.bad_crc;
      ++stats_.discarded_bytes;
      drop(1);
      continue;
    }
    type_ = p[2];
    payload_ = ByteView(p + kHeaderSize, len);
    consumed_frame_ = total;
    ++stats_.frames;
    *frame_ready = true;
    return true;
  }
}

// How many more bytes the parser needs before it can make progress, given the buffered bytes.
// Only called when try_parse() returned false, i.e. the buffer starts with a (partial) candidate.
static size_t bytes_needed(const Bytes& buf, size_t head, size_t max) {
  size_t avail = buf.size() - head;
  if (avail < kHeaderSize) return kHeaderSize - avail;
  const uint8_t* p = buf.data() + head;
  size_t len = static_cast<size_t>(p[3]) << 8 | p[4];
  if (len > max) return 1;  // cannot happen (rejected by try_parse), be safe
  return kOverhead + len - avail;
}

size_t Decoder::feed_some(ByteView in, bool* frame_ready) {
  *frame_ready = false;
  if (consumed_frame_) {
    drop(consumed_frame_);
    consumed_frame_ = 0;
    payload_ = ByteView();
  }
  size_t used = 0;
  for (;;) {
    if (try_parse(frame_ready)) return used;
    if (used == in.size()) return used;
    size_t need = bytes_needed(buf_, head_, max_);
    if (need == 0) need = 1;
    size_t take = in.size() - used;
    if (take > need) take = need;
    // Compact before growing so memory stays bounded by roughly 2 * (max_payload + overhead).
    if (head_ > 0 && head_ >= buf_.size() - head_) {
      buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(head_));
      head_ = 0;
    }
    buf_.insert(buf_.end(), in.data() + used, in.data() + used + take);
    used += take;
    credit_ += take * kWorkPerByte;
    if (credit_ > credit_cap_) credit_ = credit_cap_;
  }
}

}  // namespace ember::stream
