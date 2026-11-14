---
id: security/wp2-caesar-cipher
title: Caesar cipher
category: security
tags: [caesar cipher, security]
summary: A Caesar cipher is one of the simplest and most widely known encryption techniques used in cryptography.
source: https://en.wikipedia.org/w/index.php?title=Caesar_cipher&oldid=1384766789
source_title: Wikipedia: Caesar cipher
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Caesar cipher", Wikipedia, The Free Encyclopedia (revision 1384766789), https://en.wikipedia.org/w/index.php?title=Caesar_cipher&oldid=1384766789 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Caesar cipher

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **Caesar cipher** is one of the simplest and most widely known encryption techniques used in cryptography. It is a type of substitution cipher in which each letter in the plaintext is replaced by a letter some fixed number of positions along the alphabet. For example, with a left shift of 3, `D` would be replaced by `A`, `E` would become `B`, and so on. The method is named after Julius Caesar, who used it in his private correspondence.

The encryption step performed by a Caesar cipher is often incorporated as part of more complex schemes, such as the Vigenère cipher, and still has modern application in the ROT13 system. As with all single-alphabet substitution ciphers, the Caesar cipher is easily broken and in modern practice offers essentially no communications security.

## Example

The transformation can be represented by aligning two alphabets; the cipher is the plain alphabet shifted left or right by a certain number of positions. For instance, here is a Caesar cipher using a left shift of 3 places, equivalent to a right shift of 23 (the shift parameter is used as the key):

When encrypting, a person looks up each letter of the message in the "plain" line and writes down the corresponding letter in the "cipher" line.

 Plaintext: THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG
 Ciphertext: QEB NRFZH YOLTK CLU GRJMP LSBO QEB IXWV ALD

Deciphering is done in reverse, with a right shift of 3, equivalent to a left shift of 23.

The encryption can also be represented using modular arithmetic by first transforming the letters into numbers, according to the scheme, A → 0, B → 1, ..., Z → 25. Encryption of a letter *x* by a shift *n* can be described mathematically as:

E_n(x) = (x + n) mod 26.

Decryption is performed similarly:

D_n(x) = (x - n) mod 26.

The modulo operation (mod) may need to be performed twice during decryption depending upon the environment, as x - n is negative if n > x:

D_n(x) = (((x - n) mod 26) + 26) mod 26.

A negative number mod a positive number has no standard definition, and programming languages differ in how it is defined. As an example, the operation -12 mod 5 returns 3 when performed in Python but -2 when performed in JavaScript.

The replacement remains the same throughout the message, so the cipher is classed as a type of *monoalphabetic substitution*, as opposed to *polyalphabetic substitution*.

## History and usage

The Caesar cipher is named for Julius Caesar, who, according to the Roman historian Suetonius, used it with a shift of three (`A` becoming `D` when encrypting, and vice versa when decrypting) to protect messages of military significance. While Caesar's was the first recorded use of this scheme, other substitution ciphers are known to have existed earlier. So, Suetonius writes that his nephew, Augustus, used the cipher with a right shift of one, but it did not wrap around to the beginning of the Latin alphabet, instead replacing `Z` with `AA`. Evidence exists that Caesar also used more complicated systems. The grammarian Aulus Gellius refers to a (now lost) treatise on his ciphers:

> "There is even a rather ingeniously written treatise by the grammarian Probus concerning the secret meaning of letters in the composition of Caesar's epistles."

It is unknown how effective the Caesar cipher was at the time: there is no record of contemporary techniques for the solution of simple substitution ciphers. The earliest surviving records date to the 9th-century works of Al-Kindi in the Arab world with the discovery of frequency analysis.

A piece of text encrypted in a Hebrew version of the Caesar cipher (not to be confused with Atbash) is sometimes found on the back of Jewish mezuzah scrolls. When each letter is replaced with the letter before it in the Hebrew alphabet, the text reads "YHWH, our God, YHWH", a quotation from the scroll.

The Vigenère cipher uses a Caesar cipher with a different shift at each position in the text; the value of the shift is defined using a repeating keyword. Repeating keywords (e.g., "Complete Victory" used by the Confederacy during the American Civil War) introduce a cyclic pattern that might be detected with statistically advanced frequency analysis. (*See e.g. Coincidence counting.*) If the keyword is as long as the message, is chosen at random, never becomes known to anyone else, and is never reused, it is a one-time pad cipher, impossible to break cryptographically. However, the problems involved in distributing such a key make the one-time pad difficult to use in practice.

In the 19th century, the personal advertisements section in newspapers would sometimes be used to exchange messages encrypted using simple cipher schemes. David Kahn (1967) describes instances of lovers engaging in secret communications enciphered using the Caesar cipher in *The Times*. As late as 1915 during World War I, the Caesar cipher was used by the Russian army as a replacement for more complicated ciphers which had proven difficult for their troops to master; German and Austrian cryptanalysts had little difficulty in decrypting their messages.

In April 2006, fugitive Mafia boss Bernardo Provenzano was captured in Sicily partly because some of his messages, clumsily written in a variation of the Caesar cipher, were broken. Provenzano's cipher used numbers, so that "A" would be written as "4", "B" as "5", and so on.

In 2011, British Airways employee Rajib Karim was convicted of "terrorism offences" after using a Caesar cipher to discuss with Bangladeshi jihadi activists plots to bomb the airline's planes or disrupt its IT systems. Although the parties had access to far better encryption techniques (Karim himself used PGP for data storage), they chose to use their own scheme implemented in Microsoft Excel, rejecting a more sophisticated code program called Mujahedeen Secrets "because 'kaffirs', or non-believers, know about it, so it must be less secure".

Caesar ciphers can be found today in children's toys such as secret decoder rings. A Caesar shift of thirteen is also performed in the ROT13 cipher, a simple method of obfuscating text widely found on Usenet and used to obscure text (such as joke punchlines and story spoilers), but not seriously used as a method of encryption.

_(Later sections of the original article were left out of this offline copy for length: Breaking the cipher.)_
