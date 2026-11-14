// One-time-pad tools (category "crypto"). Randomness must come from the caller (`random_hex`),
// e.g. the UI fills it from crypto.getRandomValues() or dice rolls converted to hex.
#include <cstdio>

#include "ember/bytes.h"
#include "tools/otp.h"
#include "tools/tool_impl.h"

namespace ember {
namespace tools {
namespace detail {
namespace {

const char* kLegal =
    "Encrypted traffic is prohibited on amateur bands and some other services: only use pads where encryption is lawful "
    "(or when life is in immediate danger).";

ToolDef d_sheet() {
  return {"otp_sheet", "One-time pad sheet", "crypto",
          "Generates a letter one-time pad (groups of 5) from random bytes you supply. Print two copies, one per party.",
          {txt("random_hex", "Random bytes (hex)", "textarea").req().maxlen(20000)
               .help("At least ~1.1 bytes per letter. Must be truly random (OS RNG, dice); never reuse."),
           num("groups", "Groups of 5 letters", "").def(50).range(1, 2000).whole(),
           txt("sheet_id", "Sheet identifier", "text").def_s("A01").maxlen(12).help("Sent in clear so the receiver picks the same sheet.")}};
}
void r_sheet(Ctx& c) {
  std::string hex, id;
  int g;
  if (!c.text("random_hex", &hex) || !c.integer("groups", &g) || !c.text("sheet_id", &id)) return;
  std::string clean;
  for (char ch : hex)
    if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') clean += ch;
  Bytes rnd;
  if (!from_hex(clean, rnd)) { c.fail("Random bytes must be hexadecimal (pairs of 0-9 a-f)"); return; }
  std::string pad;
  size_t used = 0, need = static_cast<size_t>(g) * 5;
  if (!otp::pad_letters(ByteView(rnd), need, &pad, &used)) {
    c.fail("Not enough random bytes: " + std::to_string(rnd.size()) + " given, about " +
           std::to_string(need * 256 / 234 + 8) + " needed for " + std::to_string(need) + " letters");
    return;
  }
  std::string text = "ONE-TIME PAD  sheet " + id + "\n\n";
  std::string body = otp::groups(pad, 5, 10);
  size_t line = 1;
  text += "01  ";
  for (char ch : body) {
    if (ch == '\n') {
      char b[8];
      std::snprintf(b, sizeof b, "\n%02zu  ", ++line);
      text += b;
    } else {
      text += ch;
    }
  }
  text += "\n\nEncrypt: cipher = plain + key (mod 26, A=0). Decrypt: plain = cipher - key.\nDESTROY AFTER USE.\n";
  c.r.text = text;
  c.out_s("pad", "Pad letters", pad);
  c.out("letters", "Letters", static_cast<double>(pad.size()), "", 0);
  c.out("bytes_used", "Random bytes consumed", static_cast<double>(used), "", 0);
  c.note("Each sheet must be used once only and destroyed afterwards; reusing a pad breaks it completely.");
  c.note(kLegal);
}

ToolDef d_enc() {
  return {"otp_encode", "One-time pad encrypt", "crypto",
          "Encrypts a short message with a letter pad (A-Z, digits spelled out) for reading over voice radio.",
          {txt("plaintext", "Message", "textarea").req().maxlen(4000),
           txt("pad", "Pad letters (from the sheet)", "textarea").req().maxlen(20000)}};
}
void r_enc(Ctx& c) {
  std::string pt, pad;
  if (!c.text("plaintext", &pt) || !c.text("pad", &pad)) return;
  std::string p = otp::normalize_plaintext(pt), k = otp::letters_only(pad), out;
  if (p.empty()) { c.fail("Message has no letters or digits"); return; }
  if (!otp::encrypt(p, k, &out)) {
    c.fail("Pad too short: message needs " + std::to_string(p.size()) + " letters, pad has " + std::to_string(k.size()));
    return;
  }
  std::string g = otp::groups(out, 5, 10);
  c.out_s("ciphertext", "Ciphertext", otp::groups(out, 5));
  c.out("letters", "Pad letters used", static_cast<double>(p.size()), "", 0);
  c.out_s("normalized", "Message as encrypted", p);
  c.r.text = g;
  c.note("Cross out the pad letters you used. Read groups using the phonetic alphabet.");
  c.note(kLegal);
}

ToolDef d_dec() {
  return {"otp_decode", "One-time pad decrypt", "crypto", "Decrypts letter groups with the matching pad sheet.",
          {txt("ciphertext", "Ciphertext groups", "textarea").req().maxlen(8000),
           txt("pad", "Pad letters (from the sheet)", "textarea").req().maxlen(20000)}};
}
void r_dec(Ctx& c) {
  std::string ct, pad;
  if (!c.text("ciphertext", &ct) || !c.text("pad", &pad)) return;
  std::string x = otp::letters_only(ct), k = otp::letters_only(pad), out;
  if (x.empty()) { c.fail("Ciphertext has no letters"); return; }
  if (!otp::decrypt(x, k, &out)) {
    c.fail("Pad too short: ciphertext has " + std::to_string(x.size()) + " letters, pad has " + std::to_string(k.size()));
    return;
  }
  c.out_s("plaintext", "Plaintext", out);
  c.r.text = out;
  c.note("Spaces are not transmitted: read the result aloud to find word breaks. Digits arrive spelled out (SEVEN).");
}

const Entry kEntries[] = {{d_sheet, r_sheet}, {d_enc, r_enc}, {d_dec, r_dec}};

}  // namespace

const Entry* crypto_tools(size_t* n) {
  *n = sizeof(kEntries) / sizeof(kEntries[0]);
  return kEntries;
}

}  // namespace detail
}  // namespace tools
}  // namespace ember
