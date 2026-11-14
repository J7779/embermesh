---
id: radio/wp2-intermodulation
title: Intermodulation
category: radio
tags: [intermodulation, intermodulation distortion, imd, radio]
summary: Intermodulation (IM) or intermodulation distortion (IMD) is the amplitude modulation of signals containing two or more different frequencies, caused by nonlinearities or time variance in a system.
source: https://en.wikipedia.org/w/index.php?title=Intermodulation&oldid=1350162449
source_title: Wikipedia: Intermodulation
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Intermodulation", Wikipedia, The Free Encyclopedia (revision 1350162449), https://en.wikipedia.org/w/index.php?title=Intermodulation&oldid=1350162449 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Intermodulation

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Intermodulation** (**IM**) or **intermodulation distortion** (**IMD**) is the amplitude modulation of signals containing two or more different frequencies, caused by nonlinearities or time variance in a system. The intermodulation between frequency components will form additional components at frequencies that are not just at harmonic frequencies (integer multiples) of either, like harmonic distortion, but also at the sum and difference frequencies of the original frequencies and at sums and differences of multiples of those frequencies.

Intermodulation is caused by non-linear behaviour of the signal processing (physical equipment or even algorithms) being used. The theoretical outcome of these non-linearities can be calculated by generating a Volterra series of the characteristic, or more approximately by a Taylor series.

Practically all audio equipment has some non-linearity, so it will exhibit some amount of IMD, which however may be low enough to be imperceptible by humans. Due to the characteristics of the human auditory system, the same percentage of IMD is perceived as more bothersome when compared to the same amount of harmonic distortion.

Intermodulation is also usually undesirable in radio, as it creates unwanted spurious emissions, often in the form of sidebands. For radio transmissions this increases the occupied bandwidth, leading to adjacent channel interference, which can reduce audio clarity or increase spectrum usage.

IMD is only distinct from harmonic distortion in that the stimulus signal is different. The same nonlinear system will produce both total harmonic distortion (with a solitary sine wave input) and IMD (with more complex tones). In music, for instance, IMD is intentionally applied to electric guitars using overdriven amplifiers or effects pedals to produce new tones at *sub*harmonics of the tones being played on the instrument. See Power chord.

IMD is also distinct from intentional modulation (such as a frequency mixer in superheterodyne receivers) where signals to be modulated are presented to an intentional nonlinear element (multiplied). See non-linear mixers such as mixer diodes and even single-transistor oscillator-mixer circuits. However, while the intermodulation products of the received signal with the local oscillator signal are intended, superheterodyne mixers can, at the same time, also produce unwanted intermodulation effects from strong signals near in frequency to the desired signal that fall within the passband of the receiver.

## Causes of intermodulation

A linear time-invariant system cannot produce intermodulation. If the input of a linear time-invariant system is a signal of a single frequency, then the output is a signal of the same frequency; only the amplitude and phase can differ from the input signal.

Non-linear systems generate harmonics in response to sinusoidal input, meaning that if the input of a non-linear system is a signal of a single frequency, ~f_a, then the output is a signal which includes a number of integer multiples of the input frequency signal; (i.e. some of f_a, 2f_a, 3f_a, 4f_a, …).

Intermodulation occurs when the input to a non-linear system is composed of two or more frequencies. Consider an input signal that contains three frequency components at~f_a, f_b, and ~f_c; which may be expressed as

x(t) = M_a sin(2 π f_a t + φ_a) + M_b sin(2 π f_b t + φ_b) + M_c sin(2 π f_c t + φ_c)

where the M and φ are the amplitudes and phases of the three components, respectively.

We obtain our output signal, y(t), by passing our input through a non-linear function G:
y(t) = G(x(t))

y(t) will contain the three frequencies of the input signal, ~f_a, f_b, and ~f_c (which are known as the *fundamental* frequencies), as well as a number of linear combinations of the fundamental frequencies, each in the form

k_af_a + k_bf_b + k_cf_c

where ~k_a, k_b, and ~k_c are arbitrary integers which can assume positive or negative values. These are the **intermodulation products** (or **IMPs**).

In general, each of these frequency components will have a different amplitude and phase, which depends on the specific non-linear function being used, and also on the amplitudes and phases of the original input components.

More generally, given an input signal containing an arbitrary number N of frequency components f_a, f_b, …, f_N, the output signal will contain a number of frequency components, each of which may be described by

k_a f_a + k_b f_b + … + k_N f_N,

where the coefficients k_a, k_b, …, k_N are arbitrary integer values.

### Intermodulation order

The *order* O of a given intermodulation product is the sum of the absolute values of the coefficients,

O = |k_a| + |k_b| + … + |k_N|,

For example, in our original example above, third-order intermodulation products (IMPs) occur where |k_a|+|k_b|+|k_c| = 3:

- f_a + f_b + f_c
- f_a + f_b - f_c
- f_a + f_c - f_b
- f_b + f_c - f_a
- 2f_a - f_b
- 2f_a - f_c
- 2f_b - f_a
- 2f_b - f_c
- 2f_c - f_a
- 2f_c - f_b

In many radio and audio applications, odd-order IMPs are of most interest, as they fall within the vicinity of the original frequency components, and may therefore interfere with the desired behaviour. For example, intermodulation distortion from the third order (**IMD3**) of a circuit can be seen by looking at a signal that is made up of two sine waves, one at f_1 and one at f_2. When you cube the sum of these sine waves you will get sine waves at various frequencies including 2× f_2-f_1 and 2× f_1-f_2. If f_1 and f_2 are large but very close together then 2× f_2-f_1 and 2× f_1-f_2 will be very close to f_1 and f_2.

## Intermodulation in electronic circuits

Slew-induced distortion (SID) can produce intermodulation distortion (IMD) when the first signal is slewing (changing voltage) at the limit of the amplifier's power bandwidth product. This induces an effective reduction in gain, partially amplitude-modulating the second signal. If SID only occurs for a portion of the signal, it is called "transient" intermodulation distortion.

## Measurement

Intermodulation distortion in audio is usually specified as the root mean square (RMS) value of the various sum-and-difference signals as a percentage of the original signal's root mean square voltage, although it may be specified in terms of individual component strengths, in decibels, as is common with radio frequency work. Audio system measurements (Audio IMD) include SMPTE standard RP120-1994 where two signals (at 60 Hz and 7 kHz, with 4:1 amplitude ratios) are used for the test; many other standards (such as DIN, CCIF) use other frequencies and amplitude ratios. Opinion varies over the ideal ratio of test frequencies (e.g. 3:4, or almost — but not exactly — 3:1 for example).

After feeding the equipment under test with low distortion input sinewaves, the output distortion can be measured by using an electronic filter to remove the original frequencies, or spectral analysis may be made using Fourier transformations in software or a dedicated spectrum analyzer, or when determining intermodulation effects in communications equipment, may be made using the receiver under test itself.

In radio applications, intermodulation may be measured as adjacent channel power ratio. Hard to test are intermodulation signals in the GHz-range generated from passive devices (PIM: passive intermodulation). Manufacturers of these scalar PIM-instruments are Summitek and Rosenberger. The newest developments are PIM-instruments to measure also the distance to the PIM-source. Anritsu offers a radar-based solution with low accuracy and Heuermann offers a frequency converting vector network analyzer solution with high accuracy.

_(Later sections of the original article were left out of this offline copy for length: Passive intermodulation (PIM).)_
