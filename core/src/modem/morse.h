// EmberMesh core / modem — International Morse code table (ITU-R M.1677-1) plus common prosigns.
//
// Patterns are strings of '.' and '-'. Prosigns are written in text as <XX> (e.g. "<SK>", "<AR>").
// Encoding: letters are case-insensitive; prosigns that share a pattern with a character
// (<AR> = '+', <BT> = '=', <KN> = '(', <AS> = '&') encode to that pattern. Decoding prefers the
// character for shared patterns and returns the prosign name only for patterns that have no
// character: <SK> <VE>(=<SN>) <KA>(=<CT>) <HH> (error, 8 dots) <SOS> <BK> <CL> <DO>.
#pragma once

#include <string>
#include <string_view>

namespace ember {
namespace modem {
namespace morse {

// Pattern for a single character (nullptr if none). Accepts lowercase.
const char* pattern_for(char c);
// Pattern for a prosign name without brackets ("SK", "AR", ...), nullptr if unknown.
const char* prosign_pattern(std::string_view name);
// Text for a received pattern: a single character, a "<XX>" prosign, or "" if unknown.
std::string decode(std::string_view pattern);
// Converts text to a sequence of tokens: each element is a pattern, " " for a word gap.
// Unknown characters are skipped. "<XX>" sequences are prosigns. Returns false if nothing usable.
bool encode(std::string_view text, std::string* out_tokens_joined_by_bar);

}  // namespace morse
}  // namespace modem
}  // namespace ember
