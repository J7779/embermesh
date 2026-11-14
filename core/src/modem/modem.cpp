#include "modem/modem.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "dsp/dsp.h"
#include "modem/afsk.h"
#include "modem/cw.h"
#include "modem/dpsk.h"
#include "modem/dtmf.h"
#include "modem/psk31.h"
#include "modem/robust.h"
#include "modem/rtty.h"

namespace ember {
namespace modem {

// ---- parameter parsing --------------------------------------------------------------------------

bool parse_number(std::string_view s, double* out) {
  // Validate the grammar ourselves (strtod accepts hex, inf, nan, leading spaces, locale commas).
  size_t i = 0, n = s.size();
  if (n == 0 || n > 64) return false;
  if (s[i] == '+' || s[i] == '-') ++i;
  size_t digits = 0;
  while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; ++digits; }
  if (i < n && s[i] == '.') {
    ++i;
    while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; ++digits; }
  }
  if (!digits) return false;
  if (i < n && (s[i] == 'e' || s[i] == 'E')) {
    ++i;
    if (i < n && (s[i] == '+' || s[i] == '-')) ++i;
    size_t ed = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') { ++i; ++ed; }
    if (!ed) return false;
  }
  if (i != n) return false;
  char buf[72];
  std::memcpy(buf, s.data(), n);
  buf[n] = 0;
  double v = std::strtod(buf, nullptr);
  if (!std::isfinite(v)) return false;
  *out = v;
  return true;
}

static void set_err(std::string* e, const std::string& m) {
  if (e) *e = m;
}

bool param_double(const KeyValues& p, const char* key, double def, double lo, double hi, double* out,
                  std::string* error) {
  auto it = p.find(key);
  if (it == p.end()) { *out = def; return true; }
  double v;
  if (!parse_number(it->second, &v)) { set_err(error, std::string("bad number for ") + key); return false; }
  if (v < lo || v > hi) { set_err(error, std::string(key) + " out of range"); return false; }
  *out = v;
  return true;
}

bool param_bool(const KeyValues& p, const char* key, bool def, bool* out, std::string* error) {
  auto it = p.find(key);
  if (it == p.end()) { *out = def; return true; }
  const std::string& s = it->second;
  if (s == "1" || s == "true" || s == "on" || s == "yes") { *out = true; return true; }
  if (s == "0" || s == "false" || s == "off" || s == "no") { *out = false; return true; }
  set_err(error, std::string("bad boolean for ") + key);
  return false;
}

bool check_keys(const KeyValues& p, const char* const* allowed, std::string* error) {
  for (const auto& kv : p) {
    bool ok = false;
    for (const char* const* a = allowed; a && *a; ++a) if (kv.first == *a) { ok = true; break; }
    if (!ok) { set_err(error, "unknown parameter: " + kv.first); return false; }
  }
  return true;
}

bool parse_options(const KeyValues& kv, ModemOptions* out, std::string* error) {
  ModemOptions o;
  for (const auto& e : kv) {
    double v = 0;
    auto num = [&](double lo, double hi) {
      if (!parse_number(e.second, &v) || v < lo || v > hi) {
        set_err(error, "bad value for " + e.first);
        return false;
      }
      return true;
    };
    if (e.first == "txdelay_ms") { if (!num(0, 5000)) return false; o.txdelay_ms = static_cast<uint32_t>(v); }
    else if (e.first == "txtail_ms") { if (!num(0, 2000)) return false; o.txtail_ms = static_cast<uint32_t>(v); }
    else if (e.first == "tx_level") { if (!num(0.001, 1)) return false; o.tx_level = static_cast<float>(v); }
    else if (e.first == "max_frame") { if (!num(0, 65535)) return false; o.max_frame = static_cast<uint16_t>(v); }
    else if (e.first == "min_frame") { if (!num(1, 65535)) return false; o.min_frame = static_cast<uint16_t>(v); }
    else if (e.first == "max_frames_per_tx") { if (!num(1, 64)) return false; o.max_frames_per_tx = static_cast<uint8_t>(v); }
    else if (e.first == "rx_queue_max") { if (!num(1, 100000)) return false; o.rx_queue_max = static_cast<size_t>(v); }
    else if (e.first == "tx_queue_max") { if (!num(1, 100000)) return false; o.tx_queue_max = static_cast<size_t>(v); }
    else o.params[e.first] = e.second;
  }
  *out = o;
  return true;
}

// ---- Modem base ---------------------------------------------------------------------------------

Modem::Modem(uint32_t device_rate, uint32_t internal_rate, const ModemOptions& opt)
    : opt_(opt), rate_(device_rate), irate_(internal_rate) {
  if (opt_.txdelay_ms > 5000) opt_.txdelay_ms = 5000;
  if (opt_.txtail_ms > 2000) opt_.txtail_ms = 2000;
  if (!(opt_.tx_level > 0)) opt_.tx_level = 0.5f;
  if (opt_.tx_level > 1) opt_.tx_level = 1;
  if (opt_.min_frame < 1) opt_.min_frame = 1;
  if (opt_.max_frames_per_tx < 1) opt_.max_frames_per_tx = 1;
  if (opt_.max_frames_per_tx > 64) opt_.max_frames_per_tx = 64;
  if (opt_.rx_queue_max < 1) opt_.rx_queue_max = 1;
  if (opt_.tx_queue_max < 1) opt_.tx_queue_max = 1;
}

bool Modem::init_base() {
  if (rate_ < 8000 || rate_ > 192000 || irate_ < 4000) return false;
  if (!rx_rs_.init(rate_, irate_) || !tx_rs_.init(irate_, rate_)) return false;
  meter_.init(rate_, 0.3, 1.0);
  dc_a_ = 1.0 - std::exp(-1.0 / (0.1 * rate_));   // DC tracker, 100 ms
  ms_a_ = 1.0 - std::exp(-1.0 / (0.02 * rate_));  // level, 20 ms
  if (info_.frames) {
    if (opt_.max_frame == 0 || opt_.max_frame > info_.max_frame) opt_.max_frame = info_.max_frame;
    info_.max_frame = opt_.max_frame;
  }
  if (opt_.min_frame > opt_.max_frame && info_.frames) opt_.min_frame = opt_.max_frame;
  info_.internal_rate = irate_;
  return true;
}

void Modem::rx(const float* x, size_t n) {
  if (!x || !n) return;
  stats_.rx_samples += n;
  const size_t kChunk = 1024;
  while (n) {
    size_t k = n < kChunk ? n : kChunk;
    scratch_.resize(k);
    for (size_t i = 0; i < k; ++i) {
      float v = x[i];
      if (!(v == v)) v = 0;             // NaN
      if (v > 4) v = 4;                 // also +Inf
      if (v < -4) v = -4;
      scratch_[i] = v;
      dc_ += dc_a_ * (v - dc_);
      double ac = v - dc_;
      ac_ms_ += ms_a_ * (ac * ac - ac_ms_);
      if (ac_ms_ < 1e-30) ac_ms_ = 0;  // no denormals
    }
    meter_.push(scratch_.data(), k);
    if (rx_rs_.passthrough()) {
      demod(scratch_.data(), k);
      rx_internal_samples_ += k;
    } else {
      rs_out_.clear();
      rx_rs_.process(scratch_.data(), k, rs_out_);
      if (!rs_out_.empty()) demod(rs_out_.data(), rs_out_.size());
      rx_internal_samples_ += rs_out_.size();
    }
    x += k;
    n -= k;
  }
  flush_text();
}

void Modem::rx_pcm16(const int16_t* x, size_t n) {
  if (!x || !n) return;
  float buf[512];
  while (n) {
    size_t k = n < 512 ? n : 512;
    for (size_t i = 0; i < k; ++i) buf[i] = dsp::pcm16_to_float(x[i]);
    rx(buf, k);
    x += k;
    n -= k;
  }
}

bool Modem::pop(RxItem* out) {
  if (rxq_.empty() || !out) return false;
  *out = std::move(rxq_.front());
  rxq_.pop_front();
  return true;
}

void Modem::push_item(RxItem&& it) {
  if (cb_) { cb_(it); return; }
  while (rxq_.size() >= opt_.rx_queue_max) {
    rxq_.pop_front();
    ++stats_.rx_dropped;
  }
  rxq_.push_back(std::move(it));
}

void Modem::emit_frame(ByteView data, RxQuality q) {
  flush_text();
  q.level_dbfs = level_dbfs();
  RxItem it;
  it.kind = RxKind::Frame;
  it.data = data.to_bytes();
  it.q = q;
  ++stats_.rx_frames;
  push_item(std::move(it));
}

void Modem::emit_text(std::string_view text, RxQuality q) {
  if (text.empty()) return;
  if (pending_text_.size() + text.size() > 4096) flush_text();
  pending_text_.append(text.data(), text.size());
  q.level_dbfs = level_dbfs();
  pending_q_ = q;
  stats_.rx_chars += static_cast<uint32_t>(text.size());
}

void Modem::flush_text() {
  if (pending_text_.empty()) return;
  RxItem it;
  it.kind = RxKind::Text;
  it.text.swap(pending_text_);
  it.q = pending_q_;
  pending_text_.clear();
  push_item(std::move(it));
}

bool Modem::send_frame(ByteView frame) {
  if (!info_.frames || frame.empty() || frame.size() > opt_.max_frame) return false;
  if (txq_.size() >= opt_.tx_queue_max) return false;
  TxItem it;
  it.data = frame.to_bytes();
  txq_.push_back(std::move(it));
  return true;
}

bool Modem::send_text(std::string_view text) {
  if (text.empty() || !accept_text(text)) return false;
  if (txq_.size() >= opt_.tx_queue_max) return false;
  if (info_.frames && !info_.text) return send_frame(ByteView(text));
  if (text.size() > 65536) return false;
  TxItem it;
  it.is_text = true;
  it.data = ByteView(text).to_bytes();
  txq_.push_back(std::move(it));
  return true;
}

bool Modem::take_tx(TxItem* out) {
  if (txq_.empty()) return false;
  *out = std::move(txq_.front());
  txq_.pop_front();
  ++stats_.tx_frames;
  return true;
}

void Modem::tx_abort() {
  txq_.clear();
  txbuf_.clear();
  txbuf_pos_ = 0;
  active_ = false;
  tx_rs_.reset();
  tx_reset();
}

size_t Modem::pull(float* out, size_t max) {
  if (!out) return 0;
  size_t n = 0;
  const size_t kGen = 256;
  const float lvl = opt_.tx_level;
  while (n < max) {
    if (txbuf_pos_ < txbuf_.size()) {
      size_t k = txbuf_.size() - txbuf_pos_;
      if (k > max - n) k = max - n;
      std::memcpy(out + n, txbuf_.data() + txbuf_pos_, k * sizeof(float));
      n += k;
      txbuf_pos_ += k;
      continue;
    }
    txbuf_.clear();
    txbuf_pos_ = 0;
    if (!active_) {
      if (txq_.empty()) break;
      if (!tx_begin()) break;
      active_ = true;
      ++stats_.tx_transmissions;
      tx_rs_.reset();
    }
    gen_.resize(kGen);
    size_t g = tx_generate(gen_.data(), kGen);
    if (g > kGen) g = kGen;
    for (size_t i = 0; i < g; ++i) {
      float v = gen_[i] * lvl;
      gen_[i] = v > 1 ? 1 : (v < -1 ? -1 : v);
    }
    if (tx_rs_.passthrough()) {
      txbuf_.assign(gen_.begin(), gen_.begin() + static_cast<std::ptrdiff_t>(g));
    } else {
      tx_rs_.process(gen_.data(), g, txbuf_);
    }
    if (g < kGen) {
      if (!tx_rs_.passthrough()) tx_rs_.flush(txbuf_);
      active_ = false;
      if (txbuf_.empty()) break;
    }
  }
  return n;
}

size_t Modem::pull_pcm16(int16_t* out, size_t max) {
  if (!out) return 0;
  float buf[512];
  size_t n = 0;
  while (n < max) {
    size_t want = max - n < 512 ? max - n : 512;
    size_t got = pull(buf, want);
    for (size_t i = 0; i < got; ++i) out[n + i] = dsp::float_to_pcm16(buf[i]);
    n += got;
    if (got < want) break;
  }
  return n;
}

// ---- factory ------------------------------------------------------------------------------------

namespace {
struct Entry {
  const char* name;
  std::unique_ptr<Modem> (*make)(const std::string&, uint32_t, const ModemOptions&, std::string*);
};
const Entry kEntries[] = {
    {"afsk1200", create_afsk}, {"afsk300", create_afsk},     {"psk2400", create_dpsk},
    {"psk4800", create_dpsk},  {"robust31", create_robust},  {"robust62", create_robust},
    {"robust125", create_robust}, {"robust250", create_robust}, {"dtmf", create_dtmf},
    {"cw", create_cw},         {"psk31", create_psk31},      {"rtty", create_rtty},
};
}  // namespace

std::unique_ptr<Modem> create_modem(const std::string& name, uint32_t sample_rate, const ModemOptions& opt,
                                    std::string* error) {
  if (sample_rate < 8000 || sample_rate > 192000) {
    set_err(error, "sample rate must be 8000..192000");
    return nullptr;
  }
  std::string n = name;
  if (n == "robust") n = "robust62";
  if (n == "bell202" || n == "packet") n = "afsk1200";
  for (const Entry& e : kEntries) {
    if (n == e.name) {
      std::unique_ptr<Modem> m = e.make(n, sample_rate, opt, error);
      return m;
    }
  }
  set_err(error, "unknown modem: " + name);
  return nullptr;
}

std::vector<std::string> modem_names() {
  std::vector<std::string> v;
  for (const Entry& e : kEntries) v.push_back(e.name);
  return v;
}

std::vector<ModemInfo> modem_catalog() {
  std::vector<ModemInfo> v;
  for (const Entry& e : kEntries) {
    std::string err;
    auto m = e.make(e.name, 8000, ModemOptions(), &err);
    if (m) v.push_back(m->info());
  }
  return v;
}

}  // namespace modem
}  // namespace ember
