---
id: security/wp2-hamming-code
title: Hamming code
category: security
tags: [hamming code, hamming codes, secded, security]
summary: In computer science and telecommunications, Hamming codes are a family of linear error-correcting codes.
source: https://en.wikipedia.org/w/index.php?title=Hamming_code&oldid=1382647289
source_title: Wikipedia: Hamming code
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Hamming code", Wikipedia, The Free Encyclopedia (revision 1382647289), https://en.wikipedia.org/w/index.php?title=Hamming_code&oldid=1382647289 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Hamming code

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In computer science and telecommunications, **Hamming codes** are a family of linear error-correcting codes. Hamming codes can detect one-bit and two-bit errors, or correct one-bit errors without detection of uncorrected errors. By contrast, the simple parity code cannot correct errors, and can detect only an odd number of bits in error. Hamming codes are perfect codes, that is, they achieve the highest possible rate for codes with their block length and minimum distance of three.
Richard W. Hamming invented Hamming codes in 1950 as a way of automatically correcting errors introduced by punched card readers. In his original paper, Hamming elaborated his general idea, but specifically focused on the Hamming(7,4) code which adds three parity bits to four bits of data.

In mathematical terms, Hamming codes are a class of binary linear codes. For each integer *r* ≥ 2 there is a code-word with block length *n* = 2^*r* − 1 and message length *k* = 2^*r* − *r* − 1. Hence the rate of Hamming codes is *R* = *k* / *n* = 1 − *r* / (2^*r* − 1), which is the highest possible for codes with minimum distance of three (i.e., the minimal number of bit changes needed to go from any code word to any other code word is three) and block length 2^*r* − 1. The parity-check matrix of a Hamming code is constructed by listing all columns of length *r* that are non-zero, which means that the dual code of the Hamming code is the shortened Hadamard code, also known as a Simplex code. The parity-check matrix has the property that any two columns are pairwise linearly independent.

Due to the limited redundancy that Hamming codes add to the data, they can only detect and correct errors when the error rate is low. This is the case in computer memory (usually RAM), where bit errors are extremely rare and Hamming codes are widely used. Memory with this correction system is known as ECC memory. In this context, an extended Hamming code having one extra parity bit is often used. Extended Hamming codes achieve a Hamming distance of four, which allows the decoder to distinguish between when at most one one-bit error occurs and when any two-bit errors occur. In this sense, extended Hamming codes are single-error correcting and double-error detecting, abbreviated as **SECDED**.

## Description

If more error-correcting bits are included with a message, and if those bits can be arranged such that different incorrect bits produce different error results, then bad bits could be identified. In a seven-bit message, there are seven possible single bit errors, so three error control bits could potentially specify not only that an error occurred but also which bit caused the error.

Hamming studied the existing coding schemes, including two-of-five, and generalized their concepts. To start with, he developed a nomenclature to describe the system, including the number of data bits and error-correction bits in a block. For instance, parity includes a single bit for any data word, so assuming ASCII words with seven bits, Hamming described this as an *(8,7)* code, with eight bits in total, of which seven are data. The repetition example would be *(3,1)*, following the same logic. The code rate is the second number divided by the first, for our repetition example, 1/3.

Hamming also noticed the problems with flipping two or more bits, and described this as the "distance" (it is now called the *Hamming distance*, after him). Parity has a distance of 2, so one bit flip can be detected but not corrected, and any two bit flips will be invisible. The (3,1) repetition has a distance of 3, as three bits need to be flipped in the same triple to obtain another code word with no visible errors. It can correct one-bit errors or it can detect - but not correct - two-bit errors. A (4,1) repetition (each bit is repeated four times) has a distance of 4, so flipping three bits can be detected, but not corrected. When three bits flip in the same group there can be situations where attempting to correct will produce the wrong code word. In general, a code with distance *k* can detect but not correct *k* − 1 errors.

Hamming was interested in two problems at once: increasing the distance as much as possible, while at the same time increasing the code rate as much as possible. During the 1940s he developed several encoding schemes that were dramatic improvements on existing codes. The key to all of his systems was to have the parity bits overlap, such that they managed to check each other as well as the data.

### General algorithm

The following general algorithm generates a single-error correcting (SEC) code for any number of bits. The main idea is to choose the error-correcting bits such that the index-XOR (the XOR of all the bit positions containing a 1) is 0. We use positions 1, 10, 100, etc. (in binary) as the error-correcting bits, which guarantees it is possible to set the error-correcting bits so that the index-XOR of the whole message is 0. If the receiver receives a string with index-XOR 0, they can conclude there were no corruptions, and otherwise, the index-XOR indicates the index of the corrupted bit.

An algorithm can be deduced from the following description:

1. Number the bits starting from 1: bit 1, 2, 3, 4, 5, 6, 7, etc.
1. Write the bit numbers in binary: 1, 10, 11, 100, 101, 110, 111, etc.
1. All bit positions that are powers of two (have a single 1 bit in the binary form of their position) are parity bits: 1, 2, 4, 8, etc. (1, 10, 100, 1000)
1. All other bit positions, with two or more 1 bits in the binary form of their position, are data bits.
1. Each data bit is included in a unique set of 2 or more parity bits, as determined by the binary form of its bit position.
 1. Parity bit 1 covers all bit positions which have the **least** significant bit set: bit 1 (the parity bit itself), 3, 5, 7, 9, etc.
 1. Parity bit 2 covers all bit positions which have the **second** least significant bit set: bits 2–3, 6–7, 10–11, etc.
 1. Parity bit 4 covers all bit positions which have the **third** least significant bit set: bits 4–7, 12–15, 20–23, etc.
 1. Parity bit 8 covers all bit positions which have the **fourth** least significant bit set: bits 8–15, 24–31, 40–47, etc.
 1. In general each parity bit covers all bits where the bitwise AND of the parity position and the bit position is non-zero.

If a byte of data to be encoded is 10011010, then the data word (using _ to represent the parity bits) would be __1_001_1010, and the code word is 011100101010.

The choice of the parity, even or odd, is irrelevant but the same choice must be used for both encoding and decoding.

This general rule can be shown visually:

|rowspan="7"| ...
bit
coverage

| || || || || || || || || || || || || || || || || || || ||
| || || || || || || || || || || || || || || || || || || ||
| || || || || || || || || || || || || || || || || || || ||
| || || || || || || || || || || || || || || || || || || ||
| || || || || || || || || || || || || || || || || || || ||

Shown are only 20 encoded bits (5 parity, 15 data) but the pattern continues indefinitely. The key thing about Hamming codes that can be seen from visual inspection is that any given bit is included in a unique set of parity bits. To check for errors, check all of the parity bits. The pattern of errors, called the error syndrome, identifies the bit in error. If all parity bits are correct, there is no error. Otherwise, the sum of the positions of the erroneous parity bits identifies the erroneous bit. For example, if the parity bits in positions 1, 2 and 8 indicate an error, then bit 1+2+8=11 is in error. If only one parity bit indicates an error, the parity bit itself is in error.

With m parity bits, bits from 1 up to 2^m-1 can be covered. After discounting the parity bits, 2^m-m-1 bits remain for use as data. As m varies, we get all the possible Hamming codes:

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Description, Hamming codes with additional parity (SECDED), [7,4] Hamming code.)_
