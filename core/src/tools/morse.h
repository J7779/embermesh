// EmberMesh core — Morse code text encode/decode, timing patterns for flashlight/tone signalling,
// and NATO phonetic spelling.
#pragma once

#include <string>
#include <vector>

namespace ember {
namespace morse {

// Encodes text to dots/dashes: letters separated by ' ', words by " / ". Prosigns can be written
// as <SK>, <AR>, <BT>, <SOS> … (sent without inter-letter gaps). Unknown characters are skipped
// and reported via *skipped (if non-null).
std::string encode(const std::string& text, std::string* skipped = nullptr);
// Decodes ". ... / -.-." style input ('.', '-', also '_' and the unicode-free '·'-less forms).
// Letters separated by spaces, words by '/' or 3+ spaces. Unknown codes become '?'.
std::string decode(const std::string& code);

// On/off timing in milliseconds, starting with ON: [on, off, on, off, …] (RPC host.signal.*).
// wpm: character speed (PARIS: dot = 1200 / wpm ms). farnsworth_wpm (≤ wpm, 0 = off) stretches
// the gaps between characters and words for learners.
std::vector<int> timing(const std::string& text, double wpm, double farnsworth_wpm = 0);
// Total duration in ms of a timing pattern.
long total_ms(const std::vector<int>& pattern);

// "Alfa Bravo …" (ITU/NATO). Digits use the ITU pronunciations (e.g. "Niner").
std::string nato(const std::string& text);

}  // namespace morse
}  // namespace ember
