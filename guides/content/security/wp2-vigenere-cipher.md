---
id: security/wp2-vigenere-cipher
title: Vigenère cipher
category: security
tags: [vigenère cipher, security]
summary: The Vigenère cipher is a method of encrypting alphabetic text where each letter of the plaintext is encoded with a different Caesar cipher, whose increment is determined by the corresponding letter of another text, the key.
source: https://en.wikipedia.org/w/index.php?title=Vigen%C3%A8re_cipher&oldid=1375565192
source_title: Wikipedia: Vigenère cipher
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Vigenère cipher", Wikipedia, The Free Encyclopedia (revision 1375565192), https://en.wikipedia.org/w/index.php?title=Vigen%C3%A8re_cipher&oldid=1375565192 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Vigenère cipher

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **Vigenère cipher** is a method of encrypting alphabetic text where each letter of the plaintext is encoded with a different Caesar cipher, whose increment is determined by the corresponding letter of another text, the key. In a Caesar cipher, each letter of the alphabet is shifted along some number of places. In a Caesar cipher of shift 3, `a` would become `D`, `b` would become `E`, `y` would become `B` and so on. The Vigenère cipher has several Caesar ciphers in sequence with different shift values.

For example, if the plaintext is `attacking tonight` and the key is `oculorhinolaryngology`, then

- the first letter of the plaintext, `a`, is shifted by 14 positions in the alphabet (because the first letter of the key, `o`, is the 14th letter of the alphabet, counting from zero), yielding `o`;

- the second letter, `t`, is shifted by 2 (because the second letter of the key, `c`, is the 2nd letter of the alphabet, counting from zero) yielding `v`;

- the third letter, `t`, is shifted by 20 (`u`), yielding `n`, with wrap-around;
and so on.

Traditionally spaces and punctuation are removed prior to encryption and reintroduced afterwards.

- In this example the tenth letter of the plaintext `t` is shifted by 14 positions (because the tenth letter of the key `o` is the 14th letter of the alphabet, counting from zero). Therefore, the encryption yields the message `ovnlqbpvt hznzeuz`.

If the recipient of the message knows the key, they can recover the plaintext by reversing this process.

The Vigenère cipher is therefore a special case of a polyalphabetic substitution.

First described by Giovan Battista Bellaso in 1553, the cipher is easy to understand and implement, but it resisted all attempts to break it until 1863, three centuries later. This earned it the description *le chiffrage indéchiffrable* (French for 'the indecipherable cipher'). Many people have tried to implement encryption schemes that are essentially Vigenère ciphers. In 1863, Friedrich Kasiski was the first to publish a general method of deciphering Vigenère ciphers.

In the 19th century, the scheme was misattributed to Blaise de Vigenère (1523–1596) and so acquired its present name.

## Tabula recta

For a visual way to encrypt and decrypt text, a table of alphabets can be used. The *tabula recta*, *Vigenère square,* or *Vigenère table* has the alphabet written out 26 times in different rows, with each alphabet shifted cyclically to the left compared to the previous alphabet, corresponding to the 26 possible Caesar ciphers.

For example, suppose that the plaintext to be encrypted is
`helloworld`

And the example keyword is "key" and is repeated until it matches the length of the plaintext
`keykeykeyk`

In order to encrypt the first letter of the plaintext using the *tabula recta*, go to column (H) and find where it meets with row (K). "R" is the result. After doing this repeatedly for each letter, the full plaintext can be encrypted
`rijvsuyvjn`

To decrypt, go to the column for the key letter (K) and find the ciphertext letter (R) within it. The row in which (R) appears is headed by (H), which is the decrypted plaintext letter.

## Algebraic description

Vigenère can also be described algebraically. If the letters `A`–`Z` are taken to be the numbers 0–25 (A widehat= 0, B widehat= 1, etc.), and addition is performed modulo 26, Vigenère encryption E using the key K can be written as

C_i = E_K(M_i) = (M_i+K_i) bmod 26

and decryption D using the key K as

M_i = D_K(C_i) = (C_i-K_i) bmod 26,

in which M = M_1 … M_n is the message, C = C_1 … C_n is the ciphertext and K = K_1 … K_n is the key obtained by repeating the keyword lceil n / m rceil times in which m is the keyword length.

Thus, by using the previous example, to encrypt A widehat= 0 with key letter L widehat= 11 the calculation would result in 11 widehat= L.
11 = (0+11) bmod 26

Therefore, to decrypt R widehat= 17 with key letter E widehat= 4, the calculation would result in 13 widehat= N.
13 = (17-4) bmod 26

In general, if Sigma is the alphabet of length ell, and m is the length of key, Vigenère encryption and decryption can be written:

C_i = E_K(M_i) = (M_i+K_(i bmod m)) bmod ell,

M_i = D_K(C_i) = (C_i-K_(i bmod m)) bmod ell.

M_i denotes the offset of the *i*-th character of the plaintext M in the alphabet Sigma. For example, by taking the 26 English characters as the alphabet Sigma = (A,B,C,…,X,Y,Z), the offset of A is 0, the offset of B is 1 etc. C_i and K_i are similar.

## Cryptanalysis

The idea behind the Vigenère cipher, like all other polyalphabetic ciphers, is to disguise the plaintext letter frequency to interfere with a straightforward application of frequency analysis. For instance, if `P` is the most frequent letter in a ciphertext whose plaintext is in English, one might suspect that `P` corresponds to `e` since `e` is the most frequently used letter in English. However, by using the Vigenère cipher, `e` can be enciphered as different ciphertext letters at different points in the message, which defeats simple frequency analysis.

The primary weakness of the Vigenère cipher is the repeating nature of its key. If a cryptanalyst correctly guesses the key's length *n*, the cipher text can be treated as *n* interleaved Caesar ciphers, which can easily be broken individually. The key length may be discovered by brute force testing each possible value of *n*, or Kasiski examination and the Friedman test can help to determine the key length (see below: and ).

### Kasiski examination

In 1863, Friedrich Kasiski was the first to publish a successful general attack on the Vigenère cipher. Earlier attacks relied on knowledge of the plaintext or the use of a recognizable word as a key. Kasiski's method had no such dependencies. Although Kasiski was the first to publish an account of the attack, it is clear that others had been aware of it. In 1854, Charles Babbage was goaded into breaking the Vigenère cipher when John Hall Brock Thwaites submitted a "new" cipher to the *Journal of the Society of the Arts*. When Babbage showed that Thwaites' cipher was essentially just another recreation of the Vigenère cipher, Thwaites presented a challenge to Babbage: given an original text (from Shakespeare's *The Tempest*: Act 1, Scene 2) and its enciphered version, he was to find the key words that Thwaites had used to encipher the original text. Babbage soon found the key words: "two" and "combined". Babbage then enciphered the same passage from Shakespeare using different key words and challenged Thwaites to find Babbage's key words. Babbage never explained the method that he used. Studies of Babbage's notes reveal that he had used the method later published by Kasiski and suggest that he had been using the method as early as 1846.

The Kasiski examination, also called the Kasiski test, takes advantage of the fact that repeated words are, by chance, sometimes encrypted using the same key letters, leading to repeated groups in the ciphertext. For example, consider the following encryption using the keyword `ABCD`:

 Key: ***ABCDAB***CDABCDABCD***ABCDAB***CDABCD
 Plaintext: ***crypto***isshortfor***crypto***graphy
 Ciphertext: ***CSASTP***KVSIQUTGQU***CSASTP***IUAQJB

There is an easily noticed repetition in the ciphertext, and so the Kasiski test will be effective.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Cryptanalysis, Variants.)_
