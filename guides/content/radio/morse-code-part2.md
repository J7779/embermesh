---
id: radio/morse-code-part2
title: Morse code (part 2 of 2)
category: radio
tags: [radio, morse code, CW, signalling]
summary: "International Morse code: the code table, timing, and how to learn and use it. (continued, part 2)"
source: "https://en.wikipedia.org/w/index.php?title=Morse_code&oldid=1384998743"
source_title: Morse code (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Morse code' (rev 1384998743), Wikipedia, https://en.wikipedia.org/wiki/Morse_code. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
part_of: radio/morse-code
order: 2
---

# Morse code (part 2 of 2)

## Representation, timing, and speeds

International Morse code is composed of five elements:

1. short mark, dot or *dit* (▄): "dit duration" is one time unit long
2. long mark, dash or *dah* (▄▄▄): three time units long
3. inter-element gap between the *dits* or *dahs* within a single character: one dit duration silence, one unit long
4. short gap (between letters): one dah duration silence, three time units long
5. medium gap (between words): a long silence, duration the same as two (silent) dahs sent with a normal one dit gap, seven time units (formerly five)

Note that the two mark signals, the *dit* and *dah*, are an odd number of *dits* long (1 or 3), and the gaps or spaces that always follow them are also odd numbers of *dits* (1, 3, or 7). Because of that, any code in ITU International Morse always comes out to an even number of *dits* when both the mark and space timing of the whole symbol are counted. That makes ITU Morse have a two-*dit*-long staccato beat, which helps telegraphers keep their transmit rate steady.

### Transmission

Morse code can be transmitted in a number of ways: Originally as electrical pulses along a telegraph wire, but later extended to an audio tone, a radio signal with short and long tones, or high and low tones, or as a mechanical, audible, or visual signal (e.g. a flashing light) using devices like an Aldis lamp or a heliograph, a common flashlight, or even a car horn. Some mine rescues have used pulling on a rope - a short pull for a dot and a long pull for a *dah*. Ground forces send messages to aircraft with panel signalling, where a horizontal panel is a dah and a vertical panel a dit.

Morse messages are generally transmitted by a hand-operated device such as a telegraph key, so there are variations introduced by the skill of the sender and receiver — more experienced operators can send and receive at faster speeds. In addition, individual operators differ slightly, for example, using slightly longer or shorter *dahs* or gaps, perhaps only for particular characters. This is called their "fist", and experienced operators can recognize specific individuals by it alone. A good operator who sends clearly and is easy to copy is said to have a "good fist". A "poor fist" is a characteristic of sloppy or hard to copy Morse code.

#### Digital storage

Morse code is transmitted using just two states (on and off). Morse code may be represented as a binary code, and that is what telegraph operators do when transmitting messages. Working from the above ITU definition and further defining a bit as a dot time, a Morse code sequence may be crudely represented a combination of the following five bit-strings:

1. short mark, dot or *dit* (▄): '1'b
2. longer mark, dash or *dah* (▄▄▄): '111'b
3. intra-character gap (between the *dits* and *dahs* within a character): 0
4. short gap (between letters): '000'b
5. medium gap (between words): '0000000'b

The marks and gaps alternate: *Dits* and *dahs* are always separated by one of the gaps, and that the gaps are always separated by a *dit* or a *dah*.

A more efficient binary encoding uses only two-bits for each *dit* or *dah* element, with the 1 *dit*-length pause that must follow after each automatically included for every 2 bit code. One possible coding is by number value for the length of signal tone sent one could use '01'b for a *dit* and the automatic single-dit pause after it, and '11'b for a *dah* and the automatic single-*dit* following pause, and '00'b for the *extra* pause between letters (in effect, an end-of-letter mark). That leaves the code '10'b available for some other purpose, such as an escape character, or to more compactly represent the *extra* space between words (an end-of-word mark) instead of '00 00 00'b (only 6 *dit* lengths, since the 7th is automatically inserted as part of the prior *dit* or *dah*).

Although the *dit* and inter-letter pauses work out to be the same, for any letter containing a *dah*, the two-bit encoding uses digital memory more compactly than the direct-conversion bit strings mentioned above. Including the letter-separating spaces, all International Morse letter codes pack into 12 bits or less (5 symbols), and most fit into 10 bits or less (4 symbols); most of the procedural signs fit into 14 bits, with a few only needing 12 bits (5 symbols); and all digits require exactly 12 bits.

For example, Morse g (▄▄▄ ▄▄▄ ▄ + 2 *extra* empty dits for "end of letter") would binary-encode as '11'b, '11'b, '01'b, '00'b; when packed it is '1111 0100'b = 'F4'x, which stores into only one byte (two nibbles) (as does every three-element code). The bit encoding for the longer method mentioned earlier the same letter would encode as '1110'b, '1110'b, '1000'b = '1110 1110 1000'b = 'EE8'x, or one-and-a-half bytes (three nibbles). The space saving allows small devices, like portable memory keyers, to have more and longer International Morse code sequences in small, conventional device-driver microprocessors' RAM chips.

### Cable code

The very long time constants of 19th and early 20th century submarine communications cables required a different form of Morse signalling. Instead of keying a voltage on and off for varying times, the dits and dahs were represented by two polarities of voltage impressed on the cable, for a uniform time.

### Timing

Below is an illustration of timing conventions. The phrase MORSE CODE, in Morse code format, would normally be written something like this, where ▄▄▄ represents *dahs* and ▄ represents *dits*:

| M | O | R | S | E |   | C | O | D | E |
|---|---|---|---|---|---|---|---|---|---|
| ▄▄▄ ▄▄▄ | ▄▄▄ ▄▄▄ ▄▄▄ | ▄ ▄▄▄ ▄ | ▄ ▄ ▄ | ▄ |   | ▄▄▄ ▄ ▄▄▄ ▄ | ▄▄▄ ▄▄▄ ▄▄▄ | ▄▄▄ ▄ ▄ | ▄ |

Next is the exact conventional timing for this phrase, with `▓` representing "signal on", and `˽` representing "signal off", each for the time length of exactly one dit:

| M |   | O |   | R |   | S |   | E |   | C |   | O |   | D |   | E |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ▓▓▓˽▓▓▓ | ˽˽˽ | ▓▓▓˽▓▓▓˽▓▓▓ | ˽˽˽ | ▓˽▓▓▓˽▓ | ˽˽˽ | ▓˽▓˽▓ | ˽˽˽ | ▓ | ˽˽˽˽˽˽˽ | ▓▓▓˽▓˽▓▓▓˽▓ | ˽˽˽ | ▓▓▓˽▓▓▓˽▓▓▓ | ˽˽˽ | ▓▓▓˽▓˽▓ | ˽˽˽ | ▓ |
| ↑ |   | ↑ |   | ↑ | ↑ |   |   |   | ↑ |   |   |   |   |   |   |   |
| symbol |   | dah |   | dit | letter |   | word |   |   |   |   |   |   |   |   |   |
| ^space |   |   |   |   | ^space |   | ^space |   |   |   |   |   |   |   |   |   |

### Spoken representation

Morse code is often spoken or written with *dah* for dashes, *dit* for dots located at the end of a character, and *di* for dots located at the beginning or internally within the character. Thus, the following Morse code sequence:

| ▄▄▄ ▄▄▄ | ▄▄▄ ▄▄▄ ▄▄▄ | ▄ ▄▄▄ ▄ | ▄ ▄ ▄ | ▄ |   | ▄▄▄ ▄ ▄▄▄ ▄ | ▄▄▄ ▄▄▄ ▄▄▄ | ▄▄▄ ▄ ▄ | ▄ |
|---|---|---|---|---|---|---|---|---|---|
| M | O | R | S | E |   | C | O | D | E |

is spoken (or sung):

| *dah dah* | *dah dah dah* | *di dah dit* | *di di dit* | *dit* |   | *dah di dah dit* | *dah dah dah* | *dah di dit* | *dit* |
|---|---|---|---|---|---|---|---|---|---|
| ▄▄▄ ▄▄▄ | ▄▄▄ ▄▄▄ ▄▄▄ | ▄ ▄▄▄ ▄ | ▄ ▄ ▄ | ▄ |   | ▄▄▄ ▄ ▄▄▄ ▄ | ▄▄▄ ▄▄▄ ▄▄▄ | ▄▄▄ ▄ ▄ | ▄ |
| M | O | R | S | E |   | C | O | D | E |

For use on radio, there is little point in learning to read Morse *written* in dashes and dots, as above; rather, the *sounds* of all of the letters and symbols need to be learned, for both sending and receiving.

### Speed in words per minute

All Morse code elements depend on the dot / *dit* length. A *dah* is the length of 3 dits (with no gaps between), and spacings are specified in number of *dit* lengths. An unambiguous method of specifying the transmission speed is to specify the *dit* duration as, for example, 50 milliseconds.

Specifying the *dit* duration is, however, not the common practice. Morse code transmission rate is typically specified in *groups per minute*, commonly referred to as *words per minute*. The time needed to transmit the word paris is typically the standard "word" for calculating the "word per minute" rate. Standards using other "words" such as codex are also seen occasionally, so a reported rate may be ambiguous if this is not specified.

PARIS mimics a word rate that is typical of natural language words and reflects the benefits of Morse code's shorter code durations for common characters such as E and T. CODEX offers a word rate that is typical of 5 letter code groups (sequences of random letters). Using the word PARIS as a standard, the number of *dit* units is 50 and a simple calculation shows that the *dit* length at 20 WPM is 60 milliseconds. Using the word CODEX with 60 dit units, the *dit* length at 20 WPM is 50 milliseconds.

Because Morse code is usually sent by hand, it is unlikely that an operator could be that precise with the *dit* length, and the individual characteristics and preferences of the operators usually override the standards.

For commercial radiotelegraph licenses in the United States, the Federal Communications Commission specifies tests for Morse code proficiency in words per minute and in code groups per minute. The FCC specifies that a "word" is 5 characters long. The Commission specifies Morse code test elements at 16 code groups per minute, 20 words per minute, 20 code groups per minute, and 25 words per minute. The word per minute rate would be close to the PARIS standard, and the code groups per minute would be close to the CODEX standard.

While the Federal Communications Commission no longer requires Morse code for amateur radio licenses, the old requirements were similar to the requirements for commercial radiotelegraph licenses.

A difference between amateur radio licenses and commercial radiotelegraph licenses is that commercial operators must be able to receive code groups of random characters along with plain language text. For each class of license, the code group speed requirement is slower than the plain language text requirement. For example, for the Radiotelegraph Operator License, the examinee must pass a 20 word per minute plain text test and a 16 word per minute code group test.

Based upon a 50 dit duration standard word such as PARIS, the time for one *dit* duration or one unit can be computed by the formula:

*T* =   1200  / *W*

where:  T  is the unit time, or *dit* duration in milliseconds, and  W  is the speed in WPM.

High-speed telegraphy contests are held; according to the *Guinness Book of Records* in June 2005 at the International Amateur Radio Union's 6th World Championship in High Speed Telegraphy in Primorsko, Bulgaria, Andrei Bindasov of Belarus transmitted 230 Morse code marks of mixed text in one minute.

### Farnsworth speed

Sometimes, especially while teaching Morse code, the timing rules above are changed so two different speeds are used: A character speed and a text speed. The character speed is how fast each individual letter is sent. The text speed is how fast the entire message is sent. For example, individual characters may be sent at a 13 words-per-minute rate, but the intercharacter and interword gaps may be lengthened so the word rate is only 5 WPM.

Using different character and text speeds is, in fact, a common practice, and is used in the Farnsworth method of learning Morse code.

### Alternative display of common characters in International Morse code

Some methods of teaching Morse code use a dichotomic search table.

## Learning methods

People learning Morse code using the *Farnsworth method* are taught to send and receive letters and other symbols at their full target speed, that is with normal relative timing of the *dits*, *dahs*, and spaces within each symbol for that speed. The Farnsworth method is named for Donald R. "Russ" Farnsworth, also known by his call sign, W6TTB. However, initially exaggerated spaces between symbols and words are used, to give "thinking time" to make the sound "shape" of the letters and symbols easier to learn. The spacing can then be reduced with practice and familiarity.

Another popular teaching method is the Koch method, published in 1936 by the German engineer and former Nazi stormtrooper Ludwig Koch, which uses the full target speed from the outset but begins with just two characters. Once strings containing those two characters can be copied with 90% accuracy, an additional character is added, and so on until the full character set is mastered.

In North America, many thousands of individuals have increased their code recognition speed (after initial memorization of the characters) by listening to the regularly scheduled code practice transmissions broadcast by W1AW, the station at the American Radio Relay League's headquarters or by listening to the archived recordings available at its website. As of 2015, the United States military taught Morse code as an 81 day self-paced course, having phased out more traditional classes.

### Mnemonics

Visual mnemonic charts have been devised over the ages. Baden-Powell included one in the Girl Guides handbook in 1918.

In the United Kingdom, many people learned the Morse code by means of a series of words or phrases that have the same rhythm as a Morse character. For instance, Q in Morse is *dah dah di dah*, which can be memorized by the phrase "God Save the Queen", and the Morse for F is *di di dah dit*, which can be memorized as "Did she like it?"

A well-known Morse code rhythm from the Second World War period derives from Beethoven's Fifth Symphony, the opening phrase of which was regularly played at the beginning of BBC broadcasts. The timing of the notes corresponds to the Morse for V, *di di di dah* (▄ ▄ ▄ ▄▄▄), promoted by prime minister Churchill as *"'V' for Victory"*.

## Characters and prosigns

Nonstandard punctuation, characters with diacritics, and non-Latin letters are not present in either ITU-R M.1172 or ITU-R M.1677-1, except for Ę, ę.

| Category | Character | Code |
|---|---|---|
| Letters | A, a |   |
| Letters | B, b |   |
| Letters | C, c |   |
| Letters | D, d |   |
| Letters | E, e |   |
| Letters | F, f |   |
| Letters | G, g |   |
| Letters | H, h |   |
| Letters | I, i |   |
| Letters | J, j |   |
| Letters | K, k Prosign for general *invitation to transmit* |   |
| Letters | L, l |   |
| Letters | M, m |   |
| Letters | N, n |   |
| Letters | O, o |   |
| Letters | P, p |   |
| Letters | Q, q |   |
| Letters | R, r |   |
| Letters | S, s |   |
| Letters | T, t |   |
| Letters | U, u |   |
| Letters | V, v |   |
| Letters | W, w |   |
| Letters | X, x |   |
| Letters | Y, y |   |
| Letters | Z, z |   |
| Numbers | 0 |   |
| Numbers | 1 |   |
| Numbers | 2 |   |
| Numbers | 3 |   |
| Numbers | 4 |   |
| Numbers | 5 |   |
| Numbers | 6 |   |
| Numbers | 7 |   |
| Numbers | 8 |   |
| Numbers | 9 |   |
| Punctuation | Period [ . ] RK digraph |   |
| Punctuation | Comma [ , ] GW digraph |   |
| Punctuation | Question mark [ ? ] UD digraph |   |
| Punctuation | Apostrophe [ ' ], also used for minutes of arc and doubled for seconds of arc ('') WG digraph |   |
| Punctuation | Slash or fraction bar [ */* ] DN digraph |   |
| Punctuation | Open parenthesis [ ( ] KN digraph; unofficial prosign for *exclusive invitation to transmit* |   |
| Punctuation | Close parenthesis [ ) ] KK digraph |   |
| Punctuation | Colon [ : ] OS digraph |   |
| Punctuation | Double dash [ = ] BT digraph; prosign for *new section* or *new paragraph* |   |
| Punctuation | Plus sign [ + ] RN digraph; prosign for *new message* or *new page* |   |
| Punctuation | Hyphen or Minus sign [ − ] DU |   |
| Punctuation | Quotation mark [ " ] RR |   |
| Punctuation | At sign [ @ ] AC digraph |   |
| Nonstandard punctuation | Exclamation mark, [ ! ] KW digraph, alt. MN | alt. |
| Nonstandard punctuation | Ampersand [ & ] AS digraph; prosign for *wait* |   |
| Nonstandard punctuation | Semicolon [ ; ] KR |   |
| Nonstandard punctuation | Underscore [ _ ] UK |   |
| Nonstandard punctuation | Dollar sign [ $ ] SX digraph |   |
| Prosigns | *End of work* SK digraph |   |
| Prosigns | *Error* HH digraph |   |
| Prosigns | *General invitation to transmit* Also used for letter 'K' |   |
| Prosigns | *Transmission start signal* CT digraph |   |
| Prosigns | *Next message follows* RN digraph; message or page separator. Sometimes represented as + in a single-line decoding display. |   |
| Prosigns | *Verified* SN digraph; also used for letter Ŝ |   |
| Prosigns | *Wait* AS digraph; proposed for use as ampersand [ & ] |   |
| Diacritics and non-Latin extensions | À, à Code shared with Å |   |
| Diacritics and non-Latin extensions | Ä, ä Code shared with Æ, Ą |   |
| Diacritics and non-Latin extensions | Å, å Code shared with À |   |
| Diacritics and non-Latin extensions | Ą, ą Code shared with Ä, Æ |   |
| Diacritics and non-Latin extensions | Æ, æ Code shared with Ä, Ą |   |
| Diacritics and non-Latin extensions | Ć, ć Code shared with Ĉ, Ç |   |
| Diacritics and non-Latin extensions | Ĉ, ĉ Code shared with Ć, Ç |   |
| Diacritics and non-Latin extensions | Ç, ç Code shared with Ć, Ĉ |   |
| Diacritics and non-Latin extensions | CH, ch Code shared with Ĥ, Š |   |
| Diacritics and non-Latin extensions | Đ, đ (D with stroke) Code shared with É, Ę; distinct from *eth* (Ð, ð) |   |
| Diacritics and non-Latin extensions | Ð, ð (*Edh* or *eth*) Distinct from D with stroke (Đ, đ) |   |
| Diacritics and non-Latin extensions | É, é Code shared with Đ,Ę |   |
| Diacritics and non-Latin extensions | È, è Code shared with Ł |   |
| Diacritics and non-Latin extensions | Ę, ę Code shared with Đ, É |   |
| Diacritics and non-Latin extensions | Ĝ, ĝ |   |
| Diacritics and non-Latin extensions | Ĥ, ĥ Code shared with CH, Š |   |
| Diacritics and non-Latin extensions | Ĵ, ĵ |   |
| Diacritics and non-Latin extensions | Ł, ł Code shared with È |   |
| Diacritics and non-Latin extensions | Ń, ń Code shared with Ñ |   |
| Diacritics and non-Latin extensions | Ñ, ñ Code shared with Ń |   |
| Diacritics and non-Latin extensions | Ó, ó Code shared with Ö, Ø, [ ! ] |   |
| Diacritics and non-Latin extensions | Ö, ö Code shared with Ó, Ø, [ ! ] |   |
| Diacritics and non-Latin extensions | Ø, ø Code shared with Ó, Ö, [ ! ] |   |
| Diacritics and non-Latin extensions | Ś, ś |   |
| Diacritics and non-Latin extensions | Ŝ, ŝ Prosign for *verified* |   |
| Diacritics and non-Latin extensions | Š, š Code shared with CH, Ĥ |   |
| Diacritics and non-Latin extensions | Þ, þ |   |
| Diacritics and non-Latin extensions | Ü, ü Code shared with Ŭ |   |
| Diacritics and non-Latin extensions | Ŭ, ŭ Code shared with Ü |   |
| Diacritics and non-Latin extensions | Ź, ź |   |
| Diacritics and non-Latin extensions | Ż, ż |   |

### Cut numbers

Since the 1870s, most numbers have an unofficial short-form, given in the table below. They are only used when both the sender and the receiver understand that numbers, and not letters, are intended; for example, one often sees the most common R-S-T signal report rendered as 5 N N   instead of 5 9 9.

Cut numbers are made by reducing multiple *dahs* in the standard Morse number codes to only one *dah*, but keeping all the *dits* as-is. This leaves the codes for 4 and 6 unchanged.

**Cut (abbreviated) numbers**

| Intended digit | "Cut" number code | Same as code in Intl. Morse | Normal code (long form) |
|---|---|---|---|
| 0 | ▄▄▄ | T | ▄▄▄ ▄▄▄ ▄▄▄ ▄▄▄ ▄▄▄ |
| 1 | ▄ ▄▄▄ | A | ▄ ▄▄▄ ▄▄▄ ▄▄▄ ▄▄▄ |
| 2 | ▄ ▄ ▄▄▄ | U | ▄ ▄ ▄▄▄ ▄▄▄ ▄▄▄ |
| 3 | ▄ ▄ ▄ ▄▄▄ | V | ▄ ▄ ▄ ▄▄▄ ▄▄▄ |
| 4 | ▄ ▄ ▄ ▄ ▄▄▄ | 4 | ▄ ▄ ▄ ▄ ▄▄▄ |
| 5 | ▄ or ▄ ▄ ▄ ▄ ▄ | e or 5 | ▄ ▄ ▄ ▄ ▄ |
| 6 | ▄▄▄ ▄ ▄ ▄ ▄ | 6 | ▄▄▄ ▄ ▄ ▄ ▄ |
| 7 | ▄▄▄ ▄ ▄ ▄ | B | ▄▄▄ ▄▄▄ ▄ ▄ ▄ |
| 8 | ▄▄▄ ▄ ▄ | D | ▄▄▄ ▄▄▄ ▄▄▄ ▄ ▄ |
| 9 | ▄▄▄ ▄ | N | ▄▄▄ ▄▄▄ ▄▄▄ ▄▄▄ ▄ |
| . | ▄▄▄ ▄ ▄▄▄ | K | ▄ ▄▄▄ ▄ ▄▄▄ ▄ ▄▄▄ |

### Prosigns

Prosigns for Morse code are special (usually) unwritten procedural signals or symbols that are used to indicate changes in communications protocol status or white space text formatting actions. Almost all prosigns are also used to encode special characters, such as `+`, `=`, or `;`; the receiving telegrapher must distinguish the character-meaning from the prosign-meaning by context.

### Symbol representations

The symbols `!`, `$`, and `&` are not defined inside the official ITU-R *International Morse Code Recommendation*, but informal conventions for them exist. (The `@` symbol was formally added in 2004. The `%` and `‰` symbols both have recommended multi-character encodings (see below).)

**Exclamation mark**

There is no standard representation for the exclamation mark `!`, although the KW digraph (▄▄▄ ▄ ▄▄▄ ▄ ▄▄▄ ▄▄▄) was proposed in the 1980s by the Heathkit Company. While Morse code translation software prefers the Heathkit version, on-air use is not yet universal, as some amateur radio operators in North America and the Caribbean continue to use the older MN digraph (▄▄▄ ▄▄▄ ▄▄▄ ▄) copied over from American Morse landline code. MN or OE, ▄▄▄ ▄▄▄ ▄▄▄ ▄, which some telegraphers unofficially use for an exclamation mark `!`, is shared with unofficial letters Ö, Ó, and Ø used in some non-Latin alphabets.

**Currency symbols**

The ITU has never formally codified any currency symbols into Morse code: The unambiguous ISO 4217 currency codes are preferred for transmission (e.g. CNY, EUR, GBP, JPY, KRW, USD, etc.). The dollar sign ($) was represented in the Phillips Code collection of abbreviations as two characters "SX". Eventually operators dropped the intervening space and merged the two letter code or abbreviation into the single unofficial punctuation encoding SX (▄ ▄ ▄ ▄▄▄ ▄ ▄ ▄▄▄).

**Ampersand ( & )**

The suggested unofficial encoding of the ampersand `&` sign listed above, often shown as AS, is also the official Morse prosign for *wait*. In addition, the American Morse encoding for an ampersand (▄ ▄ ▄ ▄) was similar to ES (▄ ▄ ▄ ▄), adapted from the code for ampersand in Railroad Morse. Hams have nearly universally carried over this use as an abbreviation for "and" (e.g. WX HR COLD ES RAINY "the weather here is cold and rainy"). Since ES is well established and slightly quicker than AS, there is no motive for replacing it.

**Keyboard "at" sign ( @ )**

On 24 May 2004—the 160th anniversary of the first public Morse telegraph transmission—the Radiocommunication Bureau of the International Telecommunication Union (ITU-R) formally added the `@` ("commercial at" or "commat") character to the official Morse character set, using the sequence denoted by the AC digraph: ▄ ▄▄▄ ▄▄▄ ▄ ▄▄▄ ▄.

This sequence was reported to have been chosen to represent "A[t] C[ommercial]", or a letter 'a' inside a swirl represented by a letter 'C'. The new character facilitates sending e‑mail addresses by Morse code, and is notable since it is the first official addition to the Morse set of characters since World War I.

**Percent ( % ) and permille ( ‰ ) signs**

Percent and permille signs should be encoded with zeroes separated by a slash, joined to the preceding number by a dash; so e.g. "4%" would be sent as "`4-0/0`", and "5‰" as "`5-0/00`", and "6.7%" as "`6.7-0/0`".

**Underscore**

The underscore code is unofficial. Typically, when a whole word is meant to be underlined, the word begins with an underscore mark, followed by a letter space and the first letter of the word, and ends with the last letter of the word, followed by a letter space and another underscore mark. For example, read _MORSE_ as "Morse". Telegraphers may improvise other forms of use as needed (for example, for an underscore character embedded in a computer software keyword).

### Diacritics and non-Latin extensions

É é is the only accented character that is encoded in the ITU-R Morse code standard.

The typical tactic for creating Morse codes for other Latin characters with diacritics and non-Latin alphabetic scripts has been to begin by simply re-using the International Morse codes already used for letters whose sound matches the sound of the local alphabet. Gerke code (the predecessor to International Morse) was in official use in central Europe and included four characters not included in the International Morse standard: Ä, Ö, Ü, and CH. These four have served as a starting point for other languages that use an alphabetic script, but require codes for letters not accommodated by International Morse.

The usual method has been to first transliterate the sounds represented by the International code and the four unique Gerke codes into the local alphabet, hence Greek, Hebrew, Russian, and Ukrainian Morse codes. If more codes are needed, one can either invent a new code or convert an otherwise unused code from either code set to the non-Latin letter. For example:

- Spanish letter Ñ in Spanish Morse is ▄▄▄ ▄▄▄ ▄ ▄▄▄ ▄▄▄; it is a unique code specific to the Spanish language (although also used elsewhere for equivalent Ń), and is not used in either International Morse nor in Gerke Morse.
- For the Greek letter Ψ, Greek Morse code uses the International Morse code for Q, ▄▄▄ ▄▄▄ ▄ ▄▄▄, which has no corresponding letter in modern Greek. Except for the code being otherwise unused, the choice is arbitrary: Ψ and Q have no historical, phonetic, or shape relationship.

For Russian and Bulgarian, Russian Morse code maps the Cyrillic characters to four-element codes. Many of those characters are encoded the same as their Latin alphabet look-alikes or sound-alikes (A, O, E, I, T, M, N, R, K, etc.). The Bulgarian alphabet contains 30 characters, which exactly matches the number of all possible permutations of 1, 2, 3, and 4 *dits* and *dahs* (Russian Ы is used as Bulgarian Ь, Russian Ь is used as Bulgarian Ъ). Russian requires two more codes, for the letters Э and Ъ which are each encoded with 5 elements.

Non-alphabetic scripts require more radical adaptation. Japanese Morse code (Wabun code) has a separate encoding for kana script; although many of the codes are used for International Morse, the sounds they represent are mostly unrelated. The Japanese / Wabun code includes special prosigns for switching back-and-forth from International Morse: ▄▄▄ ▄ ▄ ▄▄▄ ▄▄▄ ▄▄▄ signals a switch from International Morse to Wabun, and ▄ ▄ ▄ ▄▄▄ ▄ to return from Wabun to International Morse.

For Chinese, Chinese telegraph code is used to map Chinese characters to four-digit codes and send these digits out using standard Morse code. Korean Morse code uses the SKATS mapping, originally developed to allow Korean to be typed on western typewriters. SKATS maps hangul characters to arbitrary letters of the Latin script and has no relationship to pronunciation in Korean.

### Unusual variants

During early World War I (1914–1916), Germany briefly experimented with 'dotty' and 'dashy' Morse, in essence adding a dot or a dash at the end of each Morse symbol. Each one was quickly broken by Allied SIGINT, and standard Morse was resumed by Spring 1916. Only a small percentage of Western Front (North Atlantic and Mediterranean Sea) traffic was in 'dotty' or 'dashy' Morse during the entire war. In popular culture, this is mostly remembered in the book *The Codebreakers* by David Kahn and in the national archives of the UK and Australia (whose SIGINT operators copied most of this Morse variant). Kahn's cited sources come from the popular press and wireless magazines of the time.

Other variations include forms of "fractional Morse" or "fractionated Morse", which recombine the characters of the Morse code–encoded message and then encrypt them using a cipher in order to disguise the text.
