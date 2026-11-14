---
id: radio/wp2-phase-shift-keying
title: Phase-shift keying
category: radio
tags: [phase-shift keying, psk, differential phase-shift keying (dpsk), radio]
summary: Phase-shift keying (PSK) is a digital modulation process which conveys data by changing (modulating) the phase of a constant frequency carrier wave.
source: https://en.wikipedia.org/w/index.php?title=Phase-shift_keying&oldid=1383935022
source_title: Wikipedia: Phase-shift keying
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Phase-shift keying", Wikipedia, The Free Encyclopedia (revision 1383935022), https://en.wikipedia.org/w/index.php?title=Phase-shift_keying&oldid=1383935022 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Phase-shift keying

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Phase-shift keying** (**PSK**) is a digital modulation process which conveys data by changing (modulating) the phase of a constant frequency carrier wave. The modulation is accomplished by varying the sine and cosine inputs at a precise time. It is widely used for wireless LANs, RFID and Bluetooth communication.

Any digital modulation scheme uses a finite number of distinct signals to represent digital data. PSK uses a finite number of phases, each assigned a unique pattern of binary digits. Usually, each phase encodes an equal number of bits. Each pattern of bits forms the symbol that is represented by the particular phase. The demodulator, which is designed specifically for the symbol-set used by the modulator, determines the phase of the received signal and maps it back to the symbol it represents, thus recovering the original data. This requires the receiver to be able to compare the phase of the received signal to a reference signal – such a system is termed coherent (and referred to as CPSK).

CPSK requires a complicated demodulator, because it must extract the reference wave from the received signal and keep track of it, to compare each sample to. Alternatively, the phase shift of each symbol sent can be measured with respect to the phase of the previous symbol sent. Because the symbols are encoded in the difference in phase between successive samples, this is called **differential phase-shift keying (DPSK)**. DPSK can be significantly simpler to implement than ordinary PSK, as it is a 'non-coherent' scheme, i.e. there is no need for the demodulator to keep track of a reference wave. A trade-off is that it has more demodulation errors.

## Introduction

There are three major classes of digital modulation techniques used for transmission of digitally represented data:

- Amplitude-shift keying (ASK)
- Frequency-shift keying (FSK)
- Phase-shift keying (PSK)

All convey data by changing some aspect of a base signal, the carrier wave (usually a sinusoid), in response to a data signal. In the case of PSK, the phase is changed to represent the data signal. There are two fundamental ways of utilizing the phase of a signal in this way:

- By viewing the phase itself as conveying the information, in which case the demodulator must have a reference signal to compare the received signal's phase against; or
- By viewing the *change* in the phase as conveying information – *differential* schemes, some of which do not need a reference carrier (to a certain extent).

A convenient method to represent M-ary transmission PSK schemes is on a constellation diagram. This shows the points in the complex plane where, in this context, the real and imaginary axes are termed the in-phase and quadrature axes respectively due to their 90° separation. Such a representation on perpendicular axes lends itself to straightforward implementation. The amplitude of each point along the in-phase axis is used to modulate a cosine (or sine) wave and the amplitude along the quadrature axis to modulate a sine (or cosine) wave. By convention, in-phase modulates cosine and quadrature modulates sine.

In PSK, the constellation points chosen are usually positioned with uniform angular spacing around a circle. This gives maximum phase-separation between adjacent points and thus the best immunity to corruption. They are positioned on a circle so that they can all be transmitted with the same energy. In this way, the moduli of the complex numbers they represent will be the same and thus so will the amplitudes needed for the cosine and sine waves. Two common examples are "binary phase-shift keying" (BPSK) which uses two phases, and "quadrature phase-shift keying" (QPSK) which uses four phases, although any number of phases may be used. Since the data to be conveyed are usually binary, the PSK scheme is usually designed with the number of constellation points being a power of two.

### Definitions

For determining error-rates mathematically, some definitions will be needed:

- E_b, energy per bit
- E_s = nE_b, energy per symbol with *n* bits
- T_b, bit duration
- T_s, symbol duration
- 1/2N_0, noise power spectral density (W/Hz)
- P_b, probability of *bit-error*
- P_s, probability of symbol-error

Q(x) will give the probability that a single sample taken from a random process with zero-mean and unit-variance Gaussian probability density function will be greater or equal to x. It is a scaled form of the complementary Gaussian error function:

Q(x) = (1)/(√(2π))∫_x^∞ e^-1/2t^2 dt = 1/2 erfc((x)/(√(2))), x ≥ 0.

The error rates quoted here are those in additive white Gaussian noise (AWGN). These error rates are lower than those computed in fading channels, hence, are a good theoretical benchmark to compare with.

## Binary phase-shift keying (BPSK)

BPSK (also sometimes called PRK, phase reversal keying, or 2PSK) is the simplest form of phase shift keying (PSK). It uses two phases which are separated by 180° and so can also be termed 2-PSK. It does not particularly matter exactly where the constellation points are positioned, and in this figure they are shown on the real axis, at 0° and 180°. Therefore, it handles the highest noise level or distortion before the demodulator reaches an incorrect decision. That makes it the most robust of all the PSKs. It is, however, only able to modulate at 1 bit/symbol (as seen in the figure) and so is unsuitable for high data-rate applications.

In the presence of an arbitrary phase-shift introduced by the communications channel, the demodulator (see, e.g. Costas loop) is unable to tell which constellation point is which. As a result, the data is often differentially encoded prior to modulation.

BPSK is functionally equivalent to 2-QAM modulation.

### Implementation

The general form for BPSK follows the equation:

s_n(t) = √((2E_b)/(T_b)) cos(2 π f t + π(1-n )), n = 0,1.

This yields two phases, 0 and π.
In the specific form, binary data is often conveyed with the following signals:

s_0(t) = √((2E_b)/(T_b)) cos(2 π f t + π ) = - √((2E_b)/(T_b)) cos(2 π f t) for binary "0"
s_1(t) = √((2E_b)/(T_b)) cos(2 π f t) for binary "1"

where *f* is the frequency of the base band.

Hence, the signal space can be represented by the single basis function

φ(t) = √((2)/(T_b)) cos(2 π f t)

where 1 is represented by √(E_b) φ(t) and 0 is represented by -√(E_b) φ(t). This assignment is arbitrary.

This use of this basis function is shown at the end of the next section in a signal timing diagram. The topmost signal is a BPSK-modulated cosine wave that the BPSK modulator would produce. The bit-stream that causes this output is shown above the signal (the other parts of this figure are relevant only to QPSK). After modulation, the base band signal will be moved to the high frequency band by multiplying cos(2 π f_c t).

### Bit error rate

The bit error rate (BER) of BPSK under additive white Gaussian noise (AWGN) can be calculated as:

P_b = Q(√((2 E_b)/(N_0))) or P_e = 1/2 erfc ( √((E_b)/(N_0)))

Since there is only one bit per symbol, this is also the symbol error rate.

## Mutual information with additive white Gaussian noise

The mutual information of PSK can be evaluated in additive Gaussian noise by numerical integration of its definition. The curves of mutual information saturate to the number of bits carried by each symbol in the limit of infinite signal to noise ratio E_s/N_0. On the contrary, in the limit of small signal to noise ratios the mutual information approaches the AWGN channel capacity, which is the supremum among all possible choices of symbol statistical distributions.

At intermediate values of signal to noise ratios the mutual information (MI) is well approximated by:

MI simeq log_2(√((4π)/(e)(E_s)/(N_0))).

The mutual information of PSK over the AWGN channel is generally lower than that of QAM modulation formats.

_(Later sections of the original article were left out of this offline copy for length: Quadrature phase-shift keying (QPSK), Higher-order PSK, Differential phase-shift keying (DPSK), Applications.)_
