// EmberMesh node — RPC result type and strict parameter validation helpers.
//
// Every handler returns an RpcResult. Params validates one field at a time; on the first failure
// it records an error (bad_params, or too_large for over-long strings) and every later getter
// returns false, so a handler can chain checks and finish with `return p.fail();`.
// `null` is treated exactly like an absent field (optional fields keep their defaults).
#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "base/json.h"

namespace ember::node {

struct RpcResult {
  bool ok = true;
  Json result;
  std::string code;
  std::string message;
};

inline RpcResult rpc_ok(Json r = Json::object()) {
  RpcResult x;
  x.result = std::move(r);
  if (!x.result.is_object()) x.result = Json::object();
  return x;
}
inline RpcResult rpc_err(const char* code, std::string message) {
  RpcResult x;
  x.ok = false;
  x.code = code;
  x.message = std::move(message);
  return x;
}

class Params {
 public:
  explicit Params(const Json& j) : j_(j) {}

  const Json& raw() const { return j_; }
  bool ok() const { return code_.empty(); }
  RpcResult fail() const { return rpc_err(code_.empty() ? "bad_params" : code_.c_str(), msg_); }
  bool has(std::string_view key) const {
    const Json* v = j_.find(key);
    return v && !v->is_null();
  }
  bool error(std::string msg, const char* code = "bad_params") {
    if (code_.empty()) {
      code_ = code;
      msg_ = std::move(msg);
    }
    return false;
  }

  // String field. `required` → missing is an error. `allow_empty` false → "" is an error.
  bool str(std::string_view key, std::string* out, size_t max_len, bool required, bool allow_empty = true) {
    if (!ok()) return false;
    const Json* v = j_.find(key);
    if (!v || v->is_null()) return required ? error(k(key) + " is required") : true;
    if (!v->is_string()) return error(k(key) + " must be a string");
    if (v->as_string().size() > max_len)
      return error(k(key) + " is too long (max " + std::to_string(max_len) + " bytes)", "too_large");
    if (!allow_empty && v->as_string().empty()) return error(k(key) + " must not be empty");
    *out = v->as_string();
    return true;
  }
  bool boolean(std::string_view key, bool* out, bool* present = nullptr) {
    if (present) *present = false;
    if (!ok()) return false;
    const Json* v = j_.find(key);
    if (!v || v->is_null()) return true;
    if (!v->is_bool()) return error(k(key) + " must be true or false");
    *out = v->as_bool();
    if (present) *present = true;
    return true;
  }
  bool integer(std::string_view key, int64_t* out, int64_t min, int64_t max, bool required, bool* present = nullptr) {
    if (present) *present = false;
    if (!ok()) return false;
    const Json* v = j_.find(key);
    if (!v || v->is_null()) return required ? error(k(key) + " is required") : true;
    int64_t x = 0;
    if (!v->is_number() || !v->get_int_if(&x)) return error(k(key) + " must be an integer");
    if (x < min || x > max)
      return error(k(key) + " must be between " + std::to_string(min) + " and " + std::to_string(max));
    *out = x;
    if (present) *present = true;
    return true;
  }
  bool number(std::string_view key, double* out, double min, double max, bool required, bool* present = nullptr) {
    if (present) *present = false;
    if (!ok()) return false;
    const Json* v = j_.find(key);
    if (!v || v->is_null()) return required ? error(k(key) + " is required") : true;
    if (!v->is_number()) return error(k(key) + " must be a number");
    double x = v->as_double();
    if (!std::isfinite(x) || x < min || x > max) return error(k(key) + " is out of range");
    *out = x;
    if (present) *present = true;
    return true;
  }
  // Object / array presence + type check; *out points into the params (valid while they live).
  bool object(std::string_view key, const Json** out, bool required) {
    *out = nullptr;
    if (!ok()) return false;
    const Json* v = j_.find(key);
    if (!v || v->is_null()) return required ? error(k(key) + " is required") : true;
    if (!v->is_object()) return error(k(key) + " must be an object");
    *out = v;
    return true;
  }
  bool array(std::string_view key, const Json** out, bool required) {
    *out = nullptr;
    if (!ok()) return false;
    const Json* v = j_.find(key);
    if (!v || v->is_null()) return required ? error(k(key) + " is required") : true;
    if (!v->is_array()) return error(k(key) + " must be an array");
    *out = v;
    return true;
  }

 private:
  static std::string k(std::string_view key) { return std::string(key); }
  const Json& j_;
  std::string code_, msg_;
};

}  // namespace ember::node
