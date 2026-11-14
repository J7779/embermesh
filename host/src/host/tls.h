// SPDX-License-Identifier: MIT
// TLS client for the host's WebSocket client (wss:// relay links, CONNECTIVITY §4.11), on the
// vendored mbedTLS 3.6 LTS (host/third_party/mbedtls, Apache-2.0). Built only with EMBER_HOST_TLS.
//
// Transport-agnostic: ciphertext goes out through `send_raw` and comes in through feed(), so it
// sits on top of the existing non-blocking TcpStream. Certificate validation is mandatory and
// cannot be turned off: the chain must verify against the platform's trust anchors (macOS:
// Security framework anchors; Windows: the ROOT system store; Linux: the distribution CA bundle;
// on every platform SSL_CERT_FILE, when set and readable, replaces the platform store) and the certificate must match the host name (also sent as SNI). An IP literal
// URL is verified against the certificate's IP subjectAltName only if mbedTLS supports it; there is
// no "insecure" switch. TLS 1.2 with ECDHE and AEAD ciphers only.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace ember_host {

class TlsClient {
 public:
  using SendFn = std::function<void(const uint8_t* data, size_t len)>;
  explicit TlsClient(SendFn send_raw);
  ~TlsClient();
  TlsClient(const TlsClient&) = delete;
  TlsClient& operator=(const TlsClient&) = delete;

  // Starts the handshake for `host` (SNI + certificate name). `ca_file_for_tests`: a PEM file used
  // INSTEAD of the system trust anchors (only the host's own tests pass this; no product path
  // sets it). Returns false with *err on setup failure.
  bool start(const std::string& host, const std::string& ca_file_for_tests, std::string* err);
  // Ciphertext from the transport. Appends decrypted application data to *plain. Returns false on
  // a fatal error (handshake or certificate failure, bad record) with *err; the connection must
  // then be closed.
  bool feed(const uint8_t* data, size_t len, std::string* plain, std::string* err);
  bool handshake_done() const { return done_; }
  // Application data to send; buffered until the handshake is done. False on a fatal error.
  bool write(const uint8_t* data, size_t len, std::string* err);
  void close_notify();
  // Number of trust anchors the platform store provided (0 = none found: every wss:// fails).
  static size_t system_anchor_count();

 private:
  struct Impl;
  bool pump(std::string* plain, std::string* err);
  std::unique_ptr<Impl> d_;
  SendFn send_;
  bool done_ = false;
  std::string pending_out_;
};

}  // namespace ember_host
