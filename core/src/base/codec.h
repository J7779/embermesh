// EmberMesh core — text codecs for binary data: base64 (RFC 4648 std + url-safe) and Crockford
// base32 (for things humans read aloud or type: case-insensitive, I/L->1, O->0, '-' and spaces
// ignored on decode). All decoders are strict about alphabet and trailing bits and never throw.
#pragma once

#include <string>
#include <string_view>

#include "ember/bytes.h"

namespace ember {

// Standard alphabet (+/) with '=' padding.
std::string base64_encode(ByteView data);
// URL-safe alphabet (-_) without padding.
std::string base64url_encode(ByteView data);

// Accepts the standard alphabet, padded or unpadded. Padding is deliberately optional (review L9):
// callers only use the decoded bytes, and those are canonical (trailing bits must be zero). Rejects other characters (including
// whitespace), non-canonical trailing bits and bad padding. Clears `out` on failure.
bool base64_decode(std::string_view text, Bytes& out);
// Accepts the URL-safe alphabet, padded or unpadded.
bool base64url_decode(std::string_view text, Bytes& out);
// Accepts either alphabet, ignores ASCII whitespace (pasted text, wrapped lines).
bool base64_decode_lenient(std::string_view text, Bytes& out);

// Crockford base32, uppercase, no padding, no check symbol.
std::string base32_encode(ByteView data);
// Case-insensitive; maps I/L -> 1 and O -> 0; ignores '-' and ' '. Rejects U and other characters
// and non-zero trailing bits.
bool base32_decode(std::string_view text, Bytes& out);

}  // namespace ember
