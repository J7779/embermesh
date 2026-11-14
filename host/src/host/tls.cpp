// SPDX-License-Identifier: MIT
// TLS client on the vendored mbedTLS (see tls.h). Compiled only with EMBER_HOST_TLS.
#include "host/tls.h"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

#include "host/util.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <Security/Security.h>
#elif defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#endif

namespace ember_host {

namespace {

std::string mbed_err(int rc) {
  char buf[160] = {0};
  mbedtls_strerror(rc, buf, sizeof buf);
  return format("%s (-0x%04x)", buf, unsigned(-rc));
}

int entropy_cb(void*, unsigned char* out, size_t len) { return random_bytes(out, len) ? 0 : -1; }

// The platform's trust anchors, loaded once per process and shared read-only by every session.
std::once_flag g_roots_once;
mbedtls_x509_crt g_roots;
size_t g_roots_count = 0;

bool load_pem_file(const std::string& f) {
  std::string pem;
  if (!read_file(f, &pem, size_t(16) << 20)) return false;
  pem.push_back('\0');  // mbedTLS wants the terminating NUL counted for PEM input
  int rc = mbedtls_x509_crt_parse(&g_roots, reinterpret_cast<const unsigned char*>(pem.data()), pem.size());
  if (rc < 0) return false;
  for (const mbedtls_x509_crt* c = &g_roots; c && c->raw.len; c = c->next) ++g_roots_count;
  return g_roots_count > 0;
}

void load_system_roots() {
  mbedtls_x509_crt_init(&g_roots);
  // SSL_CERT_FILE (the usual OpenSSL convention) replaces the platform store on every platform:
  // for a private CA, and for end-to-end tests with a CA generated at test time.
  if (const char* e = std::getenv("SSL_CERT_FILE")) {
    if (*e && load_pem_file(e)) return;
    if (*e) EH_LOGW("tls: SSL_CERT_FILE could not be read: using the system trust store");
  }
#if defined(__APPLE__) && TARGET_OS_OSX
  CFArrayRef anchors = nullptr;
  if (SecTrustCopyAnchorCertificates(&anchors) == errSecSuccess && anchors) {
    CFIndex n = CFArrayGetCount(anchors);
    for (CFIndex i = 0; i < n; ++i) {
      SecCertificateRef c = static_cast<SecCertificateRef>(const_cast<void*>(CFArrayGetValueAtIndex(anchors, i)));
      CFDataRef der = SecCertificateCopyData(c);
      if (!der) continue;
      if (mbedtls_x509_crt_parse_der(&g_roots, CFDataGetBytePtr(der), size_t(CFDataGetLength(der))) == 0) ++g_roots_count;
      CFRelease(der);
    }
    CFRelease(anchors);
  }
#elif defined(_WIN32)
  HCERTSTORE store = CertOpenSystemStoreW(0, L"ROOT");
  if (store) {
    PCCERT_CONTEXT c = nullptr;
    while ((c = CertEnumCertificatesInStore(store, c)) != nullptr)
      if (c->dwCertEncodingType == X509_ASN_ENCODING &&
          mbedtls_x509_crt_parse_der(&g_roots, c->pbCertEncoded, size_t(c->cbCertEncoded)) == 0)
        ++g_roots_count;
    CertCloseStore(store, 0);
  }
#else
  // Linux and other Unix systems: the distribution's CA bundle (the first one found).
  for (const char* f : {"/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt",
                        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem", "/etc/ssl/ca-bundle.pem", "/etc/ssl/cert.pem",
                        "/usr/local/etc/ssl/cert.pem"})
    if (load_pem_file(f)) break;
#endif
  if (!g_roots_count) EH_LOGW("tls: no system trust anchors found: wss:// connections will fail");
}

}  // namespace

struct TlsClient::Impl {
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_ctr_drbg_context drbg;
  mbedtls_x509_crt test_ca;  // only with ca_file_for_tests
  bool own_ca = false;
  std::string in;            // ciphertext received, not yet consumed
  size_t in_off = 0;
  TlsClient* self = nullptr;
  bool failed = false;
  Impl() {
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&drbg);
    mbedtls_x509_crt_init(&test_ca);
  }
  ~Impl() {
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&drbg);
    mbedtls_x509_crt_free(&test_ca);
  }
  static int send_cb(void* ctx, const unsigned char* buf, size_t len) {
    auto* d = static_cast<Impl*>(ctx);
    d->self->send_(buf, len);  // the transport buffers: everything is "sent"
    return int(len);
  }
  static int recv_cb(void* ctx, unsigned char* buf, size_t len) {
    auto* d = static_cast<Impl*>(ctx);
    size_t avail = d->in.size() - d->in_off;
    if (avail == 0) return MBEDTLS_ERR_SSL_WANT_READ;
    size_t n = len < avail ? len : avail;
    std::memcpy(buf, d->in.data() + d->in_off, n);
    d->in_off += n;
    if (d->in_off == d->in.size()) {
      d->in.clear();
      d->in_off = 0;
    }
    return int(n);
  }
};

TlsClient::TlsClient(SendFn send_raw) : d_(new Impl()), send_(std::move(send_raw)) { d_->self = this; }
TlsClient::~TlsClient() = default;

size_t TlsClient::system_anchor_count() {
  std::call_once(g_roots_once, load_system_roots);
  return g_roots_count;
}

bool TlsClient::start(const std::string& host, const std::string& ca_file_for_tests, std::string* err) {
  static const char pers[] = "ember-tls-client";
  int rc = mbedtls_ctr_drbg_seed(&d_->drbg, entropy_cb, nullptr, reinterpret_cast<const unsigned char*>(pers), sizeof pers - 1);
  if (rc != 0) {
    *err = "tls: random generator: " + mbed_err(rc);
    return false;
  }
  rc = mbedtls_ssl_config_defaults(&d_->conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  if (rc != 0) {
    *err = "tls: config: " + mbed_err(rc);
    return false;
  }
  mbedtls_ssl_conf_authmode(&d_->conf, MBEDTLS_SSL_VERIFY_REQUIRED);  // never optional, never none
  mbedtls_ssl_conf_rng(&d_->conf, mbedtls_ctr_drbg_random, &d_->drbg);
  mbedtls_ssl_conf_min_tls_version(&d_->conf, MBEDTLS_SSL_VERSION_TLS1_2);
  if (!ca_file_for_tests.empty()) {
    std::string pem;
    if (!read_file(ca_file_for_tests, &pem, size_t(1) << 20)) {
      *err = "tls: cannot read " + ca_file_for_tests;
      return false;
    }
    pem.push_back('\0');
    rc = mbedtls_x509_crt_parse(&d_->test_ca, reinterpret_cast<const unsigned char*>(pem.data()), pem.size());
    if (rc != 0) {
      *err = "tls: bad CA file: " + mbed_err(rc);
      return false;
    }
    mbedtls_ssl_conf_ca_chain(&d_->conf, &d_->test_ca, nullptr);
  } else {
    std::call_once(g_roots_once, load_system_roots);
    mbedtls_ssl_conf_ca_chain(&d_->conf, &g_roots, nullptr);
  }
  rc = mbedtls_ssl_setup(&d_->ssl, &d_->conf);
  if (rc != 0) {
    *err = "tls: setup: " + mbed_err(rc);
    return false;
  }
  if (host.empty() || mbedtls_ssl_set_hostname(&d_->ssl, host.c_str()) != 0) {  // SNI + name check
    *err = "tls: bad host name";
    return false;
  }
  mbedtls_ssl_set_bio(&d_->ssl, d_.get(), &Impl::send_cb, &Impl::recv_cb, nullptr);
  std::string plain;
  return pump(&plain, err);  // sends the ClientHello
}

bool TlsClient::pump(std::string* plain, std::string* err) {
  if (d_->failed) {
    *err = "tls: connection failed earlier";
    return false;
  }
  if (!done_) {
    int rc = mbedtls_ssl_handshake(&d_->ssl);
    if (rc == MBEDTLS_ERR_SSL_WANT_READ || rc == MBEDTLS_ERR_SSL_WANT_WRITE) return true;
    if (rc != 0) {
      d_->failed = true;
      uint32_t flags = mbedtls_ssl_get_verify_result(&d_->ssl);
      if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && flags) {
        char info[512] = {0};
        mbedtls_x509_crt_verify_info(info, sizeof info, "", flags);
        std::string why = info;
        for (auto& c : why)
          if (c == '\n') c = ';';
        *err = "tls: certificate not accepted: " + why;
      } else {
        *err = "tls: handshake failed: " + mbed_err(rc);
      }
      return false;
    }
    done_ = true;
    if (!pending_out_.empty()) {
      std::string p;
      p.swap(pending_out_);
      if (!write(reinterpret_cast<const uint8_t*>(p.data()), p.size(), err)) return false;
    }
  }
  unsigned char buf[16384];
  while (true) {
    int n = mbedtls_ssl_read(&d_->ssl, buf, sizeof buf);
    if (n > 0) {
      plain->append(reinterpret_cast<const char*>(buf), size_t(n));
      continue;
    }
    if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) return true;
    if (n == 0 || n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
      *err = "tls: closed by peer";
      return false;
    }
    d_->failed = true;
    *err = "tls: " + mbed_err(n);
    return false;
  }
}

bool TlsClient::feed(const uint8_t* data, size_t len, std::string* plain, std::string* err) {
  if (len) {
    if (d_->in.size() - d_->in_off + len > (size_t(1) << 20)) {  // bounded: a record is <= 18 KiB
      *err = "tls: receive buffer overflow";
      return false;
    }
    d_->in.append(reinterpret_cast<const char*>(data), len);
  }
  return pump(plain, err);
}

bool TlsClient::write(const uint8_t* data, size_t len, std::string* err) {
  if (!done_) {
    if (pending_out_.size() + len > (size_t(1) << 20)) {
      *err = "tls: send buffer overflow";
      return false;
    }
    pending_out_.append(reinterpret_cast<const char*>(data), len);
    return true;
  }
  size_t off = 0;
  for (int guard = 0; off < len && guard < 100000; ++guard) {
    int n = mbedtls_ssl_write(&d_->ssl, data + off, len - off);
    if (n > 0) {
      off += size_t(n);
      continue;
    }
    if (n == MBEDTLS_ERR_SSL_WANT_WRITE || n == MBEDTLS_ERR_SSL_WANT_READ) continue;  // our send never blocks
    d_->failed = true;
    *err = "tls: " + mbed_err(n);
    return false;
  }
  if (off < len) {
    d_->failed = true;
    *err = "tls: write did not progress";
    return false;
  }
  return true;
}

void TlsClient::close_notify() {
  if (done_ && !d_->failed) mbedtls_ssl_close_notify(&d_->ssl);
}

}  // namespace ember_host
