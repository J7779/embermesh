// Signalling tools (category "signal"): Morse, NATO phonetics, CTCSS/DCS tables, Q-codes and
// prosigns, RST, ground-to-air signals.
#include <cmath>
#include <cstdio>

#include "tools/morse.h"
#include "tools/tool_impl.h"

namespace ember {
namespace tools {
namespace detail {
namespace {

// ---- Morse ----
ToolDef d_morse() {
  return {"morse", "Morse code", "signal",
          "Encode text to Morse (with an on/off millisecond pattern for flashlight or tone) or decode dots and dashes.",
          {sel("mode", "Mode").opt("encode", "Text -> Morse").opt("decode", "Morse -> text").def_s("encode"),
           txt("text", "Text or Morse", "textarea").def_s("SOS").maxlen(2000)
               .help("Prosigns as <AR>, <SK>, <BT>. For decoding: '.' and '-', letters separated by spaces, words by '/'."),
           num("wpm", "Speed", "WPM").def(12).range(1, 60),
           num("farnsworth_wpm", "Farnsworth spacing speed (0 = off)", "WPM").def(0).range(0, 60).adv()}};
}
void r_morse(Ctx& c) {
  std::string mode, text;
  double wpm, fw;
  if (!c.choice("mode", &mode) || !c.text("text", &text) || !c.num("wpm", &wpm) || !c.num("farnsworth_wpm", &fw)) return;
  if (mode == "decode") {
    std::string out = morse::decode(text);
    if (out.empty()) { c.fail("No Morse symbols found"); return; }
    c.out_s("text", "Decoded text", out);
    c.r.text = out;
    if (out.find('?') != std::string::npos) c.note("'?' marks codes that are not in the table.");
    return;
  }
  std::string skipped;
  std::string code = morse::encode(text, &skipped);
  if (code.empty()) { c.fail("Nothing to encode (letters, digits and common punctuation only)"); return; }
  std::vector<int> p = morse::timing(text, wpm, fw);
  std::string ps;
  for (size_t i = 0; i < p.size(); ++i) {
    if (i) ps += ',';
    ps += std::to_string(p[i]);
  }
  c.out_s("code", "Morse", code);
  c.out_s("pattern_ms", "On/off pattern (ms, starts ON)", ps);
  c.out("duration_s", "Duration", morse::total_ms(p) / 1000.0, "s", 1);
  c.out("dot_ms", "Dot length", 1200.0 / wpm, "ms", 0);
  c.r.text = code;
  if (!skipped.empty()) c.note("Skipped characters not in Morse: " + skipped.substr(0, 40));
  c.note("Timing: dot = 1 unit, dash = 3, gap inside a letter = 1, between letters = 3, between words = 7 (PARIS standard).");
  c.note("Distress: SOS (... --- ...) sent continuously without letter gaps; or three of anything (flashes, whistle blasts, fires).");
}

// ---- NATO ----
ToolDef d_nato() {
  return {"nato", "NATO phonetic spelling", "signal", "Spell callsigns, names and grid references with the ITU/NATO alphabet.",
          {txt("text", "Text").def_s("EMBER 73").maxlen(500)}};
}
void r_nato(Ctx& c) {
  std::string t;
  if (!c.text("text", &t)) return;
  std::string s = morse::nato(t);
  if (s.empty()) { c.fail("Nothing to spell"); return; }
  c.out_s("spelled", "Phonetic", s);
  c.r.text = s;
  c.note("'|' marks a word break. Numbers: say each digit; 'Niner' avoids confusion with German 'nein'.");
}

// ---- CTCSS / DCS ----
const double kCtcss[50] = {67.0,  69.3,  71.9,  74.4,  77.0,  79.7,  82.5,  85.4,  88.5,  91.5,  94.8,  97.4,  100.0,
                           103.5, 107.2, 110.9, 114.8, 118.8, 123.0, 127.3, 131.8, 136.5, 141.3, 146.2, 151.4, 156.7,
                           159.8, 162.2, 165.5, 167.9, 171.3, 173.8, 177.3, 179.9, 183.5, 186.2, 189.9, 192.8, 196.6,
                           199.5, 203.5, 206.5, 210.7, 218.1, 225.7, 229.1, 233.6, 241.8, 250.3, 254.1};
// The 38 tones used as FRS/GMRS "privacy codes" 1-38 by most manufacturers.
const double kFrsCodes[38] = {67.0,  71.9,  74.4,  77.0,  79.7,  82.5,  85.4,  88.5,  91.5,  94.8,  97.4,  100.0, 103.5,
                              107.2, 110.9, 114.8, 118.8, 123.0, 127.3, 131.8, 136.5, 141.3, 146.2, 151.4, 156.7, 162.2,
                              167.9, 173.8, 179.9, 186.2, 192.8, 203.5, 210.7, 218.1, 225.7, 233.6, 241.8, 250.3};
const int kDcs[104] = {23,  25,  26,  31,  32,  36,  43,  47,  51,  53,  54,  65,  71,  72,  73,  74,  114, 115,
                       116, 122, 125, 131, 132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, 212, 223,
                       225, 226, 243, 244, 245, 246, 251, 252, 255, 261, 263, 265, 266, 271, 274, 306, 311, 315,
                       325, 331, 332, 343, 346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, 445, 446,
                       452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 523, 526, 532, 546, 565, 606, 612, 624,
                       627, 631, 632, 654, 662, 664, 703, 712, 723, 731, 732, 734, 743, 754};

ToolDef d_tones() {
  return {"ctcss_dcs", "CTCSS / DCS tone tables", "signal",
          "Standard CTCSS sub-audible tones, FRS/GMRS privacy-code numbering and DCS codes.",
          {sel("table", "Table").opt("ctcss", "CTCSS (50 EIA tones)").opt("frs", "FRS/GMRS privacy codes 1-38")
               .opt("dcs", "DCS codes (104)").def_s("ctcss")}};
}
void r_tones(Ctx& c) {
  std::string t;
  if (!c.choice("table", &t)) return;
  std::string s;
  char b[48];
  if (t == "ctcss" || t == "frs") {
    const double* v = t == "ctcss" ? kCtcss : kFrsCodes;
    int n = t == "ctcss" ? 50 : 38;
    for (int i = 0; i < n; ++i) {
      std::snprintf(b, sizeof b, "%2d: %5.1f Hz%s", i + 1, v[i], (i % 4 == 3 || i == n - 1) ? "\n" : "    ");
      s += b;
    }
    c.out("count", "Tones", n, "", 0);
    c.note(t == "frs" ? "Most FRS/GMRS brands number their 38 'privacy codes' this way; some radios add codes 39+ for DCS."
                      : "Tone squelch only hides other users; it does not make a channel private. Monitor before you talk.");
  } else {
    for (int i = 0; i < 104; ++i) {
      std::snprintf(b, sizeof b, "D%03dN%s", kDcs[i], (i % 8 == 7 || i == 103) ? "\n" : "  ");
      s += b;
    }
    c.out("count", "Codes", 104, "", 0);
    c.note("DCS codes are octal. N = normal, I = inverted polarity; both radios must match.");
  }
  c.r.text = s;
}

// ---- Q-codes / prosigns ----
struct QEntry { const char* code; const char* meaning; };
const QEntry kQ[] = {
    {"QRA", "What is the name of your station? / The name of my station is ..."},
    {"QRG", "Will you tell me my exact frequency? / Your exact frequency is ..."},
    {"QRL", "Are you busy? (Is this frequency in use?) / I am busy, please do not interfere"},
    {"QRM", "Are you being interfered with? / I am being interfered with (man-made)"},
    {"QRN", "Are you troubled by static? / I am troubled by static (natural noise)"},
    {"QRO", "Shall I increase power? / Increase power"},
    {"QRP", "Shall I decrease power? / Decrease power (also: low-power operation)"},
    {"QRQ", "Shall I send faster? / Send faster"},
    {"QRS", "Shall I send more slowly? / Send more slowly"},
    {"QRT", "Shall I stop sending? / Stop sending (closing down)"},
    {"QRU", "Have you anything for me? / I have nothing for you"},
    {"QRV", "Are you ready? / I am ready"},
    {"QRX", "When will you call me again? / Wait, I will call you again at ..."},
    {"QRZ", "Who is calling me? / You are being called by ..."},
    {"QSA", "What is the strength of my signals? (1-5)"},
    {"QSB", "Are my signals fading? / Your signals are fading"},
    {"QSL", "Can you acknowledge receipt? / I acknowledge receipt"},
    {"QSO", "Can you communicate with ... ? / I can communicate with ... (a contact)"},
    {"QSP", "Will you relay to ... ? / I will relay to ..."},
    {"QST", "General call to all stations"},
    {"QSY", "Shall I change frequency? / Change to frequency ..."},
    {"QTH", "What is your location? / My location is ..."},
    {"QTR", "What is the correct time? / The correct time (UTC) is ..."},
};
const QEntry kPro[] = {
    {"<SOS>", "Distress (... --- ...) sent as one character"},
    {"<AR>", "End of message (.-.-.)"},
    {"<AS>", "Wait (.-...)"},
    {"<BT>", "Break / new paragraph (-...-)"},
    {"<BK>", "Break-in, back to you (-...-.-)"},
    {"<KN>", "Over, only the named station may answer (-.--.)"},
    {"K", "Over, anyone may answer (-.-)"},
    {"<SK>", "End of contact (...-.-)"},
    {"<CL>", "Closing station (-.-..-..)"},
    {"<KA>", "Start of transmission (-.-.-)"},
    {"<VE>", "Understood (...-.)"},
    {"<HH>", "Error, I will repeat (........)"},
};
ToolDef d_q() {
  return {"qcodes", "Q-codes and prosigns", "signal", "Common Q-codes and Morse/voice procedure signals.",
          {txt("filter", "Search (optional)").def_s("").maxlen(40)}};
}
void r_q(Ctx& c) {
  std::string f;
  if (!c.text("filter", &f)) return;
  std::string fu;
  for (char ch : f) fu += (ch >= 'a' && ch <= 'z') ? static_cast<char>(ch - 32) : ch;
  auto match = [&](const QEntry& e) {
    if (fu.empty()) return true;
    std::string all = std::string(e.code) + " " + e.meaning;
    for (auto& ch : all) if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 32);
    return all.find(fu) != std::string::npos;
  };
  std::string s = "Q-codes (add '?' to ask, e.g. QTH?)\n";
  int n = 0;
  for (const auto& e : kQ) if (match(e)) { s += "  " + pad_right(e.code, 6) + e.meaning + "\n"; ++n; }
  s += "\nProsigns\n";
  for (const auto& e : kPro) if (match(e)) { s += "  " + pad_right(e.code, 6) + e.meaning + "\n"; ++n; }
  c.out("matches", "Entries", n, "", 0);
  c.r.text = s;
  c.note("Voice: MAYDAY x3 = distress (life in danger), PAN-PAN x3 = urgency, SECURITE x3 = safety.");
}

// ---- RST ----
const char* kR[5] = {"Unreadable", "Barely readable, occasional words distinguishable", "Readable with considerable difficulty",
                     "Readable with practically no difficulty", "Perfectly readable"};
const char* kS[9] = {"Faint, barely perceptible", "Very weak", "Weak", "Fair", "Fairly good", "Good", "Moderately strong", "Strong",
                     "Extremely strong"};
const char* kT[9] = {"Sixty-cycle AC or less, very rough and broad", "Very rough AC, very harsh and broad",
                     "Rough AC tone, rectified but not filtered", "Rough note, some trace of filtering",
                     "Filtered rectified AC but strongly ripple-modulated", "Filtered tone, definite trace of ripple modulation",
                     "Near pure tone, trace of ripple modulation", "Near perfect tone, slight trace of modulation",
                     "Perfect tone, no trace of ripple or modulation"};
ToolDef d_rst() {
  return {"rst", "RST signal report", "signal", "Readability / Strength / Tone report meanings (tone only for CW).",
          {num("r", "Readability", "1-5").def(5).range(1, 5).whole(), num("s", "Strength", "1-9").def(9).range(1, 9).whole(),
           num("t", "Tone (CW only, 0 = voice)", "0-9").def(0).range(0, 9).whole()}};
}
void r_rst(Ctx& c) {
  int r, s, t;
  if (!c.integer("r", &r) || !c.integer("s", &s) || !c.integer("t", &t)) return;
  c.out_s("report", "Report", std::to_string(r) + std::to_string(s) + (t ? std::to_string(t) : ""));
  c.out_s("readability", "Readability", kR[r - 1]);
  c.out_s("strength", "Strength", kS[s - 1]);
  if (t) c.out_s("tone", "Tone", kT[t - 1]);
  std::string tx = "R  Readability\n";
  for (int i = 0; i < 5; ++i) tx += "  " + std::to_string(i + 1) + "  " + kR[i] + "\n";
  tx += "S  Strength\n";
  for (int i = 0; i < 9; ++i) tx += "  " + std::to_string(i + 1) + "  " + kS[i] + "\n";
  tx += "T  Tone (CW)\n";
  for (int i = 0; i < 9; ++i) tx += "  " + std::to_string(i + 1) + "  " + kT[i] + "\n";
  c.r.text = tx;
}

// ---- ground-to-air ----
ToolDef d_ga() {
  return {"ground_air", "Ground-to-air emergency signals", "signal",
          "ICAO survivor ground-to-air visual signal code and other improvised distress signals.", {}};
}
void r_ga(Ctx& c) {
  c.r.text =
      "ICAO ground-to-air visual signals (survivors)\n"
      "  V   Require assistance\n"
      "  X   Require medical assistance\n"
      "  N   No / negative\n"
      "  Y   Yes / affirmative\n"
      "  ->  Proceeding in this direction (arrow)\n"
      "\n"
      "Make each symbol at least 2.5 m (8 ft) long, with strong contrast against the ground\n"
      "(logs, stones, dug trenches in snow, bright cloth). Keep lines straight: nature has few.\n"
      "\n"
      "Other distress signals understood almost everywhere\n"
      "  Three of anything: three fires in a triangle, three whistle blasts, three flashes\n"
      "  SOS  ... --- ...  by light, sound or written large on the ground\n"
      "  Signal mirror flashes towards aircraft or distant observers\n"
      "  Orange smoke / orange panel, red flares\n"
      "  Voice radio: MAYDAY MAYDAY MAYDAY, then who, where, what, how many people\n"
      "\n"
      "Aircraft acknowledgement: rocking wings (day) or flashing landing lights (night)\n"
      "usually means 'message seen and understood'.\n";
  c.out("symbols", "ICAO survivor symbols", 5, "", 0);
  c.note("Source: ICAO Annex 12 / IAMSAR Manual ground-to-air signal code (survivor signals).");
}

const Entry kEntries[] = {{d_morse, r_morse}, {d_nato, r_nato}, {d_tones, r_tones},
                          {d_q, r_q},         {d_rst, r_rst},   {d_ga, r_ga}};

}  // namespace

const Entry* signal_tools(size_t* n) {
  *n = sizeof(kEntries) / sizeof(kEntries[0]);
  return kEntries;
}

}  // namespace detail
}  // namespace tools
}  // namespace ember
