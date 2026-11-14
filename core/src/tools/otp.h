// EmberMesh core — one-time pad helpers for voice-radio use (letters A-Z, mod 26).
// Randomness is supplied by the caller (never read from the OS inside the core).
#pragma once

#include <string>

#include "ember/bytes.h"

namespace ember {
namespace otp {

// Converts random bytes to uniform letters A-Z by rejection sampling (bytes >= 234 are discarded).
// Returns false if `random` does not yield `letters` letters.
bool pad_letters(ByteView random, size_t letters, std::string* out, size_t* bytes_used = nullptr);

// Normalises plaintext for the pad: A-Z kept (upper-cased), digits spelled out (7 → SEVEN),
// everything else dropped.
std::string normalize_plaintext(const std::string& text);
// Keeps only letters A-Z (upper-cased) — used for pads and ciphertext with spaces/numbering.
std::string letters_only(const std::string& text);

// c = (p + k) mod 26. False if the pad is shorter than the (normalised) message.
bool encrypt(const std::string& plain_letters, const std::string& pad_letters, std::string* out);
// p = (c - k) mod 26.
bool decrypt(const std::string& cipher_letters, const std::string& pad_letters, std::string* out);

// "ABCDE FGHIJ …" with optional line breaks every `per_line` groups (0 = none).
std::string groups(const std::string& letters, size_t group = 5, size_t per_line = 0);

}  // namespace otp
}  // namespace ember
