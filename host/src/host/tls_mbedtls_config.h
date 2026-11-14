// SPDX-License-Identifier: MIT
// mbedTLS build configuration for the host's TLS client (MBEDTLS_CONFIG_FILE). Client only,
// TLS 1.2, ECDHE key exchange (X25519, P-256, P-384) with ECDSA or RSA certificates, AEAD ciphers
// (AES-GCM, ChaCha20-Poly1305), X.509 chain verification with validity dates. No server, no
// renegotiation, no PSK, no static RSA key exchange, no SHA-1/MD5 signatures, no file system or
// network code inside mbedTLS (the host provides the transport and the entropy).
#pragma once

// --- system ---
#define MBEDTLS_HAVE_ASM
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_HAVE_TIME_DATE  // certificate validity (expired / not yet valid)
#define MBEDTLS_NO_PLATFORM_ENTROPY
#if defined(_WIN32)
#define MBEDTLS_PLATFORM_C  // required on Windows (snprintf shims); nothing else of platform.c is used
#endif

// --- TLS ---
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_PROTO_TLS1_2
#define MBEDTLS_SSL_SERVER_NAME_INDICATION
#define MBEDTLS_SSL_EXTENDED_MASTER_SECRET
#define MBEDTLS_SSL_ENCRYPT_THEN_MAC
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED
#define MBEDTLS_KEY_EXCHANGE_ECDHE_RSA_ENABLED

// --- X.509 ---
#define MBEDTLS_X509_USE_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C

// --- public key ---
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_RSA_C
#define MBEDTLS_PKCS1_V15
#define MBEDTLS_PKCS1_V21
#define MBEDTLS_ECP_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_DP_SECP384R1_ENABLED
#define MBEDTLS_ECP_DP_CURVE25519_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM

// --- symmetric / hashes ---
#define MBEDTLS_CIPHER_C
#define MBEDTLS_AES_C
#define MBEDTLS_GCM_C
#define MBEDTLS_CHACHA20_C
#define MBEDTLS_POLY1305_C
#define MBEDTLS_CHACHAPOLY_C
#define MBEDTLS_MD_C
#define MBEDTLS_SHA224_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA384_C
#define MBEDTLS_SHA512_C
#define MBEDTLS_CTR_DRBG_C

// --- diagnostics ---
#define MBEDTLS_ERROR_C
