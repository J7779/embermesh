---
id: radio/wp2-signal-to-noise-ratio
title: Signal-to-noise ratio
category: radio
tags: [signal-to-noise ratio, snr, s/n, radio]
summary: Signal-to-noise ratio (SNR or S/N) is a measure used in science and engineering that compares the level of a desired signal to the level of background noise.
source: https://en.wikipedia.org/w/index.php?title=Signal-to-noise_ratio&oldid=1383589748
source_title: Wikipedia: Signal-to-noise ratio
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Signal-to-noise ratio", Wikipedia, The Free Encyclopedia (revision 1383589748), https://en.wikipedia.org/w/index.php?title=Signal-to-noise_ratio&oldid=1383589748 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Signal-to-noise ratio

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Signal-to-noise ratio** (**SNR** or **S/N**) is a measure used in science and engineering that compares the level of a desired signal to the level of background noise. SNR is defined as the ratio of signal power to noise power, often expressed in decibels. A ratio higher than 1:1 (greater than 0 dB) indicates more signal than noise.

SNR is an important parameter that affects the performance and quality of systems that process or transmit signals, such as communication systems, audio equipment, radar systems, imaging systems, and data acquisition systems. A high SNR means that the signal is clear and easy to detect or interpret, while a low SNR means that the signal is corrupted or obscured by noise and may be difficult to distinguish or recover. SNR can be improved by various methods, such as increasing the signal strength, reducing the noise level, filtering out unwanted noise, or using error correction techniques.

SNR also determines the maximum possible amount of data that can be transmitted reliably over a given channel, which depends on its bandwidth and SNR. This relationship is described by the Shannon–Hartley theorem, which is a fundamental law of information theory.

SNR can be calculated using different formulas depending on how the signal and noise are measured and defined. The most common way to express SNR is in decibels, which is a logarithmic scale that makes it easier to compare large or small values. Other definitions of SNR may use different factors or bases for the logarithm, depending on the context and application.

## Definition

One definition of signal-to-noise ratio is the ratio of the power of a signal (meaningful input) to the power of background noise (meaningless or unwanted input):
SNR = (P_signal)/(P_noise),
where P is average power. Both signal and noise power must be measured at the same or equivalent points in a system, and within the same system bandwidth.

The signal-to-noise ratio of a random variable (S) to random noise N is:

SNR = (E[S^2])/(E[N^2]),

where E refers to the expected value, which in this case is the mean square of N.

If the signal is simply a constant value of *s*, this equation simplifies to:

SNR = (s^2)/(E[N^2]).

If the noise has expected value of zero, as is common, the denominator is its variance, the square of its standard deviation *σ*_N.

The signal and the noise must be measured the same way, for example, as voltages across the same impedance. Their root mean squares can alternatively be used according to:
SNR = (P_signal)/(P_noise) = ( (A_signal)/(A_noise ) )^2,
where A is root mean square (RMS) amplitude (for example, RMS voltage).

### Decibels

Because many signals have a very wide dynamic range, signals are often expressed using the logarithmic decibel scale. Based upon the definition of decibel, signal and noise may be expressed in decibels (dB) as

P_signal,dB = 10 log_10 ( P_signal )

and

P_noise,dB = 10 log_10 ( P_noise ).

In a similar manner, SNR may be expressed in decibels as

mathrmSNR_dB = 10 log_10 ( SNR ).

Using the definition of SNR

mathrmSNR_dB = 10 log_10 ( (P_signal)/(P_noise) ).

Using the quotient rule for logarithms

10 log_10 ( (P_signal)/(P_noise) ) = 10 log_10 ( P_signal ) - 10 log_10 ( P_noise ).

Substituting the definitions of SNR, signal, and noise in decibels into the above equation results in an important formula for calculating the signal-to-noise ratio in decibels, when the signal and noise are also in decibels:
mathrmSNR_dB = P_signal,dB - P_noise,dB.

In the above formula, P is measured in units of power, such as watts (W) or milliwatts (mW), and the signal-to-noise ratio is a pure number.

However, when the signal and noise are measured in volts (V) or amperes (A), which are measures of amplitude, they must first be squared to obtain a quantity proportional to power, as shown below:

mathrmSNR_dB = 10 log_10 [ ( (A_signal)/(A_noise) )^2 ] = 20 log_10 ( (A_signal)/(A_noise) ) = A_signal,dB - A_noise,dB.

### Dynamic range

The concepts of signal-to-noise ratio and dynamic range are closely related. Dynamic range measures the ratio between the strongest un-distorted signal on a channel and the minimum discernible signal, which, for most purposes, is the noise level. SNR measures the ratio between an arbitrary signal level (not necessarily the most powerful signal possible) and noise. Measuring signal-to-noise ratios requires the selection of a representative or *reference* signal. In audio engineering, the reference signal is usually a sine wave at a standardized nominal or alignment level, such as 1 kHz at +4 dBu (1.228 V_RMS).

SNR is usually taken to indicate an *average* signal-to-noise ratio, as it is possible that instantaneous signal-to-noise ratios will be considerably different. The concept can be understood as normalizing the noise level to 1 (0 dB) and measuring how far the signal 'stands out'.

### Difference from conventional power

In physics, the average power of an AC signal is defined as the average value of voltage times current; for resistive (non-reactive) circuits, where voltage and current are in phase, this is equivalent to the product of the rms voltage and current:
P = V_rmsI_rms

P= fracV_rms^2R = I_rms^2 R
But in signal processing and communication, one usually assumes that R=1 Ω so that factor is usually not included while measuring power or energy of a signal. This may cause some confusion among readers, but the resistance factor is not significant for typical operations performed in signal processing or for computing power ratios. For most cases, the power of a signal would be considered to be simply
P= V_rms^2

## Alternative definition

An alternative definition of SNR is as the reciprocal of the coefficient of variation, i.e., the ratio of mean to standard deviation of a signal or measurement:
SNR = (µ)/(σ)
where µ is the signal mean or expected value and σ is the standard deviation of the noise, or an estimate thereof. Notice that such an alternative definition is only useful for variables that are always non-negative (such as photon counts and luminance), and it is only an approximation since E[X^2 ] = σ^2 + µ^2. It is commonly used in image processing, where the SNR of an image is usually calculated as the ratio of the mean pixel value to the standard deviation of the pixel values over a given neighborhood.

Sometimes SNR is defined as the square of the alternative definition above, in which case it is equivalent to the more common definition:
SNR = (µ^2)/(σ^2)

This definition is closely related to the sensitivity index or *d'*, when assuming that the signal has two states separated by signal amplitude µ, and the noise standard deviation σ does not change between the two states.

The *Rose criterion* (named after Albert Rose) states that an SNR of at least 5 is needed to be able to distinguish image features with certainty. An SNR less than 5 means less than 100% certainty in identifying image details.

Yet another alternative, very specific, and distinct definition of SNR is employed to characterize sensitivity of imaging systems; see Signal-to-noise ratio (imaging).

Related measures are the "contrast ratio" and the "contrast-to-noise ratio".

## Modulation system measurements

### Amplitude modulation

Channel signal-to-noise ratio is given by
mathrm(SNR)_C,AM = (A_C^2 (1 + k_a^2 P))/(2 W N_0)
where W is the bandwidth and k_a is modulation index

Output signal-to-noise ratio (of AM receiver) is given by
mathrm(SNR)_O,AM = (A_c^2 k_a^2 P)/(2 W N_0)

### Frequency modulation

Channel signal-to-noise ratio is given by
mathrm(SNR)_C,FM = (A_c^2)/(2 W N_0)

Output signal-to-noise ratio is given by
mathrm(SNR)_O,FM = (A_c^2 k_f^2 P)/(2 N_0 W^3)

## Types and abbreviations

Signal to noise ratio may be abbreviated as SNR and less commonly as S/N. PSNR stands for peak signal-to-noise ratio. GSNR stands for geometric signal-to-noise ratio. SINR is the signal-to-interference-plus-noise ratio.

_(Later sections of the original article were left out of this offline copy for length: Noise reduction, Digital signals, Optical signals, Other uses.)_
