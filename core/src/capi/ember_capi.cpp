// EmberMesh core — the C API (ember/ember.h): one ember::node::Node per ember_node, one lock per
// node, NULL / garbage-argument safety on every entry point. Exception-free.
#include "ember/ember.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>

#include "base/json.h"
#include "node/node.h"

#ifndef EMBER_VERSION
#define EMBER_VERSION "0.0.0"
#endif

struct ember_node {
  std::mutex mu;
  std::unique_ptr<ember::node::Node> node;
};

namespace {

constexpr size_t kMaxMethodLen = 256;
constexpr size_t kMaxParamsLen = size_t(24) << 20;

char* dup_cstr(const std::string& s) {
  char* out = static_cast<char*>(std::malloc(s.size() + 1));
  if (!out) return nullptr;
  std::memcpy(out, s.c_str(), s.size() + 1);
  return out;
}

char* error_envelope(const char* code, const char* message) {
  std::string s = "{\"ok\":false,\"error\":{\"code\":";
  ember::Json::quote(code, s);
  s += ",\"message\":";
  ember::Json::quote(message, s);
  s += "}}";
  return dup_cstr(s);
}

// Bounded strlen: never reads past `max` bytes (returns max + 1 when longer).
size_t bounded_len(const char* s, size_t max) {
  size_t n = 0;
  while (n <= max && s[n]) ++n;
  return n;
}

}  // namespace

extern "C" {

const char* ember_version(void) { return EMBER_VERSION; }

ember_node* ember_create(const char* config_json, const uint8_t* entropy, size_t entropy_len) {
  if (!entropy || entropy_len < 32) return nullptr;
  ember::Json cfg = ember::Json::object();
  if (config_json) {
    size_t n = bounded_len(config_json, 1u << 20);
    if (n <= (1u << 20)) {
      std::optional<ember::Json> j = ember::Json::parse(std::string_view(config_json, n));
      if (j && j->is_object()) cfg = std::move(*j);
    }
  }
  ember_node* n = new (std::nothrow) ember_node();
  if (!n) return nullptr;
  n->node.reset(new (std::nothrow) ember::node::Node(ember::node::NodeConfig::from_json(cfg),
                                                     ember::ByteView(entropy, entropy_len)));
  if (!n->node) {
    delete n;
    return nullptr;
  }
  return n;
}

void ember_destroy(ember_node* node) {
  if (!node) return;
  {
    std::lock_guard<std::mutex> lock(node->mu);
    node->node.reset();
  }
  delete node;
}

void ember_add_entropy(ember_node* node, const uint8_t* data, size_t len) {
  if (!node || !data || len == 0) return;
  std::lock_guard<std::mutex> lock(node->mu);
  node->node->add_entropy(ember::ByteView(data, len));
}

char* ember_call(ember_node* node, const char* method, const char* params_json) {
  if (!node) return error_envelope("bad_params", "node is NULL");
  if (!method) return error_envelope("bad_params", "method is NULL");
  size_t ml = bounded_len(method, kMaxMethodLen);
  if (ml > kMaxMethodLen) return error_envelope("bad_params", "method name too long");
  size_t pl = 0;
  if (params_json) {
    pl = bounded_len(params_json, kMaxParamsLen);
    if (pl > kMaxParamsLen) return error_envelope("too_large", "params exceed 24 MiB");
  }
  std::lock_guard<std::mutex> lock(node->mu);
  std::string env = node->node->call(std::string_view(method, ml),
                                     params_json ? std::string_view(params_json, pl) : std::string_view());
  return dup_cstr(env);
}

void ember_free(void* ptr) { std::free(ptr); }

void ember_tick(ember_node* node, uint64_t now_ms) {
  if (!node) return;
  std::lock_guard<std::mutex> lock(node->mu);
  node->node->tick(now_ms);
}

uint32_t ember_next_wakeup_ms(ember_node* node) {
  if (!node) return 60000;
  std::lock_guard<std::mutex> lock(node->mu);
  uint32_t w = node->node->next_wakeup_ms();
  return w > 60000 ? 60000 : w;
}

void ember_link_rx(ember_node* node, uint32_t link_id, const uint8_t* data, size_t len) {
  if (!node || !data || len == 0) return;
  std::lock_guard<std::mutex> lock(node->mu);
  node->node->link_rx(link_id, ember::ByteView(data, len));
}

void ember_audio_rx(ember_node* node, uint32_t link_id, const int16_t* samples, size_t count, uint32_t sample_rate) {
  if (!node || !samples || count == 0) return;
  std::lock_guard<std::mutex> lock(node->mu);
  node->node->audio_rx(link_id, samples, count, sample_rate);
}

size_t ember_audio_tx(ember_node* node, uint32_t link_id, int16_t* out, size_t max, uint32_t sample_rate) {
  if (!node || !out || max == 0) return 0;
  std::lock_guard<std::mutex> lock(node->mu);
  return node->node->audio_tx(link_id, out, max, sample_rate);
}

int ember_poll(ember_node* node, ember_out* out) {
  if (!out) return 0;
  out->kind = EMBER_OUT_NONE;
  out->link_id = 0;
  out->data = nullptr;
  out->len = 0;
  if (!node) return 0;
  std::lock_guard<std::mutex> lock(node->mu);
  const ember::node::OutItem* it = nullptr;
  if (!node->node->poll(&it)) return 0;
  out->kind = it->kind == ember::node::kOutEvent ? EMBER_OUT_EVENT : EMBER_OUT_LINK_TX;
  out->link_id = it->link_id;
  out->data = reinterpret_cast<const uint8_t*>(it->data.c_str());
  out->len = it->data.size();
  return 1;
}

int ember_guides_load(ember_node* node, const uint8_t* pack, size_t len, int copy) {
  if (!node) return -1;
  std::lock_guard<std::mutex> lock(node->mu);
  return node->node->guides_load(pack, len, copy != 0);
}

}  // extern "C"
