---
id: security/diceware
title: Diceware
category: security
tags: [security, passphrases, diceware]
summary: "Diceware: generating strong passphrases with ordinary dice."
source: "https://en.wikipedia.org/w/index.php?title=Diceware&oldid=1376007704"
source_title: Diceware (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Diceware' (rev 1376007704), Wikipedia, https://en.wikipedia.org/wiki/Diceware. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# Diceware

Diceware is a method for creating passphrases, passwords, and other cryptographic variables using ordinary dice as a hardware random number generator. For each word in the passphrase, five rolls of a six-sided die are required. The numbers from 1 to 6 that come up in the rolls are assembled as a five-digit number, e.g. *43146*. That number is then used to look up a word in a cryptographic word list. In the original Diceware list *43146* corresponds to *munch*. Thus, by generating several words in sequence, a lengthy passphrase can be constructed randomly.

A Diceware word list is any list of 6^5 = 7776 unique words, preferably ones the user will find easy to spell and to remember. The contents of the word list do not have to be protected or concealed in any way, as the security of a Diceware passphrase is in the number of words selected, and the number of words each selected word could be taken from. Lists have been compiled for over two dozen languages.

The level of unpredictability of a Diceware passphrase can be easily calculated: each word adds 12.9 bits of entropy to the passphrase (that is, `log_2(6^5)` bits). Originally, in 1995, Diceware creator Arnold Reinhold considered five words (64.6 bits) the minimal length needed by average users. However, in 2014 Reinhold started recommending that at least six words (77.5 bits) be used.

This level of unpredictability assumes that potential attackers know three things: that Diceware has been used to generate the passphrase, the particular word list used, and exactly how many words make up the passphrase. If the attacker has less information, the entropy can be greater than 12.9 bits/word.

The above calculations of the Diceware algorithm's entropy assume that, as recommended by Diceware's author, each word is separated by a space (or any other non-letter character). If, instead, words are simply concatenated, the calculated entropy is slightly reduced due to redundancy; for example, the three-word Diceware phrases "in put clammy" and "input clam my" become identical if the spaces are removed.

## EFF wordlists

The Electronic Frontier Foundation published three alternative English diceware word lists in 2016, further emphasizing ease-of-memorization with a bias against obscure, abstract or otherwise problematic words; one tradeoff is that typical EFF-style passphrases require typing a larger number of characters.

## Snippet

The original diceware word list consists of a line for each of the 7,776 possible five-die combinations. One excerpt:

```
...
43136	mulct
43141	mule
43142	mull
43143	multi
43144	mum
43145	mummy
43146	munch
43151	mung
...
```

## Examples

Diceware wordlist passphrase examples:

- dobbs bella bump flash begin ansi
- easel venom aver flung jon call

EFF wordlist passphrase examples:

- conjoined sterling securely chitchat spinout pelvis
- rice immorally worrisome shopping traverse recharger

The *XKCD* #936 strip shows a password similar to a Diceware generated one, even if the used wordlist is shorter than the regular 7,776-words list used for Diceware.
