---
id: security/wp2-error-correction-code
title: Error correction code
category: security
tags: [error correction code, forward error correction, fec, channel coding, error correcting code, security]
summary: In computing, telecommunication, information theory, and coding theory, forward error correction (FEC) or channel coding is a technique used for controlling errors in data transmission over unreliable or noisy communication channels.
source: https://en.wikipedia.org/w/index.php?title=Error_correction_code&oldid=1381879873
source_title: Wikipedia: Error correction code
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Error correction code", Wikipedia, The Free Encyclopedia (revision 1381879873), https://en.wikipedia.org/w/index.php?title=Error_correction_code&oldid=1381879873 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Error correction code

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In computing, telecommunication, information theory, and coding theory, **forward error correction** (**FEC**) or **channel coding** is a technique used for controlling errors in data transmission over unreliable or noisy communication channels.

The central idea is that the sender encodes the message in a redundant way, most often by using an **error correction code**, or **error correcting code** (**ECC**). The redundancy allows the receiver not only to detect errors that may occur anywhere in the message, but often to correct a limited number of errors. Therefore a reverse channel to request re-transmission may not be needed. The cost is a fixed, higher forward channel bandwidth.

The American mathematician Richard Hamming pioneered this field in the 1940s and invented the first error-correcting code in 1950: the Hamming (7,4) code.

FEC can be applied in situations where re-transmissions are costly or impossible, such as one-way communication links or when transmitting to multiple receivers in multicast.

Long-latency connections also benefit; in the case of satellites orbiting distant planets, retransmission due to errors would create a delay of several hours. FEC is also widely used in modems and in cellular networks.

FEC processing in a receiver may be applied to a digital bit stream or in the demodulation of a digitally modulated carrier. For the latter, FEC is an integral part of the initial analog-to-digital conversion in the receiver. The Viterbi decoder implements a soft-decision algorithm to demodulate digital data from an analog signal corrupted by noise. Many FEC decoders can also generate a bit-error rate (BER) signal which can be used as feedback to fine-tune the analog receiving electronics.

FEC information is added to mass storage (magnetic, optical and solid state/flash based) devices to enable recovery of corrupted data, and is used as ECC computer memory on systems that require special provisions for reliability.

The maximum proportion of errors or missing bits that can be corrected is determined by the design of the ECC, so different forward error correcting codes are suitable for different conditions. In general, a stronger code induces more redundancy that needs to be transmitted using the available bandwidth, which reduces the effective bit-rate while improving the received effective signal-to-noise ratio. The noisy-channel coding theorem of Claude Shannon can be used to compute the maximum achievable communication bandwidth for a given maximum acceptable error probability. This establishes bounds on the theoretical maximum information transfer rate of a channel with some given base noise level. However, the proof is not constructive, and hence gives no insight of how to build a capacity achieving code. After years of research, some advanced FEC systems like polar code come very close to the theoretical maximum (Shannon limit) given by the Shannon channel capacity under the hypothesis of an infinite length frame.

## Method

ECC is accomplished by adding redundancy to the transmitted information using an algorithm. A redundant bit may be a complicated function of many original information bits. The original information may or may not appear literally in the encoded output; codes that include the unmodified input in the output are **systematic**, while those that do not are **non-systematic**.

A simplistic example of ECC is to transmit each data bit three times, which is known as a (3,1) repetition code. Through a noisy channel, a receiver might see eight versions of the output; see the table below.

| Triplet received | Interpreted as |
|---|---|
| 000 | 0 (error-free) |
| 001 | 0 |
| 010 | 0 |
| 100 | 0 |
| 111 | 1 (error-free) |
| 110 | 1 |
| 101 | 1 |
| 011 | 1 |

This allows an error in any one of the three samples to be corrected by "majority vote" or "democratic voting". The correcting ability of this ECC is:

- up to one bit of triplet in error, or
- up to two bits of triplet omitted (cases not shown in table).
Though simple to implement and widely used, this triple modular redundancy is a relatively inefficient ECC. Better ECC codes typically examine the last several tens or even the last several hundreds of previously received bits to determine how to decode the current small handful of bits (typically in groups of two to eight bits).

### Simplified formalism

Formally, an error-correcting code is given by its (injective) encoding function f: Sigma^* hookrightarrow Δ^* which assigns to each word u in Sigma^* stackreldef= bigcup_n in mathbb N Sigma^n of a finite alphabet Sigma a unique word w in Δ^* stackreldef= bigcup_n in N Δ^n (a concatenation of letters) from the alphabet Δ.

Most commonly, f is a homomorphism in the sense that if x_1 * x_2 is the concatenation of x_1 and x_2, then we have the following:f(u_1 * u_2) = f(u_1) * f(u_2)This implies that it is enough to define f(σ) for single-letter words σ. The range Im(f) of the function f is the set of *code-words.* The capabilities of the code to detect and correct errors can then be understood from the *distance* d = min_w_1, w_2 in Im(f); w_1 ne w_2 d_H(w_1, w_2) of the code, which is the minimum Hamming distance separating any two distinct code words. A code with distance d can detect errors on k bits as long as k < d, and among those detected errors, the code can correct l-bit errors whenever 2l < d.

### Averaging noise to reduce errors

ECC could be said to work by "averaging noise"; since each data bit affects many transmitted symbols, the corruption of some symbols by noise usually allows the original user data to be extracted from the other, uncorrupted received symbols that also depend on the same user data.

- Because of this "risk-pooling" effect, digital communication systems that use ECC tend to work well above a certain minimum signal-to-noise ratio and not at all below it.
- This *all-or-nothing tendency* – the cliff effect – becomes more pronounced as stronger codes are used that more closely approach the theoretical Shannon limit.
- Interleaving ECC coded data can reduce the all or nothing properties of transmitted ECC codes when the channel errors tend to occur in bursts. However, this method has limits; it is best used on narrowband data.

Most telecommunication systems use a fixed channel code designed to tolerate the expected worst-case bit error rate, and then fail to work at all if the bit error rate is ever worse.
However, some systems adapt to the given channel error conditions: some instances of hybrid automatic repeat-request use a fixed ECC method as long as the ECC can handle the error rate, then switch to ARQ when the error rate gets too high;
adaptive modulation and coding uses a variety of ECC rates, adding more error-correction bits per packet when there are higher error rates in the channel, or taking them out when they are not needed.

## Local decoding and testing of codes

Sometimes it is only necessary to decode single bits of the message, or to check whether a given signal is a codeword, and do so without looking at the entire signal. This can make sense in a streaming setting, where codewords are too large to be classically decoded fast enough and where only a few bits of the message are of interest for now. Also such codes have become an important tool in computational complexity theory, e.g., for the design of probabilistically checkable proofs.

Locally decodable codes are error-correcting codes for which single bits of the message can be probabilistically recovered by only looking at a small (say constant) number of positions of a codeword, even after the codeword has been corrupted at some constant fraction of positions. Locally testable codes are error-correcting codes for which it can be checked probabilistically whether a signal is close to a codeword by only looking at a small number of positions of the signal.

Not all locally decodable codes (LDCs) are locally testable codes (LTCs) neither locally correctable codes (LCCs), q-query LCCs are bounded exponentially while LDCs can have subexponential lengths.

_(Later sections of the original article were left out of this offline copy for length: Types, Describing the performance of an ECC, Improving performance, Software for error-correcting codes.)_
