---
id: radio/wp2-frequency-shift-keying
title: Frequency-shift keying
category: radio
tags: [frequency-shift keying, fsk, binary fsk, bfsk, 2fsk, radio]
summary: Frequency-shift keying (FSK) is a frequency modulation scheme in which digital information is encoded on a carrier signal by periodically shifting the frequency of the carrier between several discrete frequencies.
source: https://en.wikipedia.org/w/index.php?title=Frequency-shift_keying&oldid=1385601255
source_title: Wikipedia: Frequency-shift keying
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Frequency-shift keying", Wikipedia, The Free Encyclopedia (revision 1385601255), https://en.wikipedia.org/w/index.php?title=Frequency-shift_keying&oldid=1385601255 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Frequency-shift keying

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Frequency-shift keying** (**FSK**) is a frequency modulation scheme in which digital information is encoded on a carrier signal by periodically shifting the frequency of the carrier between several discrete frequencies. The technology is used for communication systems such as telemetry, weather balloon radiosondes, caller ID, garage door openers, and low frequency radio transmission in the VLF and ELF bands. The simplest FSK is **binary FSK** (**BFSK**, which is also commonly referred to as **2FSK** or **2-FSK**), in which the carrier is shifted between two discrete frequencies to transmit binary (0s and 1s) information.

## Modulating and demodulating

Reference implementations of FSK modems exist and are documented in detail. The demodulation of a binary FSK signal can be done using the Goertzel algorithm very efficiently, even on low-power microcontrollers.

## Variations

### Continuous-phase frequency-shift keying

In principle FSK can be implemented by using completely independent free-running oscillators, and switching between them at the beginning of each symbol period.
In general, independent oscillators will not be at the same phase and therefore the same amplitude at the switch-over instant,
causing sudden discontinuities in the transmitted signal.

In practice, many FSK transmitters use only a single oscillator, and the process of switching to a different frequency at the beginning of each symbol period preserves the phase.
The elimination of discontinuities in the phase (and therefore elimination of sudden changes in amplitude) reduces sideband power, reducing interference with neighboring channels.

### Gaussian frequency-shift keying

Rather than directly modulating the frequency with the digital data symbols, "instantaneously" changing the frequency at the beginning of each symbol period, **Gaussian frequency-shift keying** (**GFSK**) filters the data pulses with a Gaussian filter to make the transitions smoother. This filter has the advantage of reducing sideband power, reducing interference with neighboring channels, at the cost of increasing intersymbol interference. It is used by Improved Layer 2 Protocol, DECT, Bluetooth, Cypress WirelessUSB, Nordic Semiconductor, Texas Instruments, IEEE 802.15.4, Z-Wave and Wavenis devices, and by the popular FT8 digimodes used in amateur radio as implemented by WSJT-X and PSK Reporter software. For basic data rate Bluetooth the minimum deviation is 115 kHz.

A GFSK modulator differs from a simple frequency-shift keying modulator in that before the baseband waveform (with levels −1 and +1) goes into the FSK modulator, it is passed through a Gaussian filter to make the transitions smoother to limit spectral width. Gaussian filtering is a standard way to reduce spectral width; it is called *pulse shaping* in this application.

In ordinary non-filtered FSK, at a jump from −1 to +1 or +1 to −1, the modulated waveform changes rapidly, which introduces large out-of-band spectrum. If the pulse is changed going from −1 to +1 as −1, −0.98, −0.93, ..., +0.93, +0.98, +1, and this smoother pulse is used to determine the carrier frequency, the out-of-band spectrum will be reduced.

### Minimum-shift keying

Minimum frequency-shift keying or minimum-shift keying (MSK) is a particular spectrally efficient form of coherent FSK. In MSK, the difference between the higher and lower frequency is identical to half the bit rate. Consequently, the waveforms that represent a 0 and a 1 bit differ by exactly half a carrier period. The maximum frequency deviation is δ = 0.25 *f_m*, where *f_m* is the maximum modulating frequency. As a result, the modulation index *m* is 0.5. This is the smallest FSK modulation index that can be chosen such that the waveforms for 0 and 1 are orthogonal.

### Gaussian minimum-shift keying

A variant of MSK called Gaussian minimum-shift keying (GMSK) is used in the GSM mobile phone standard.

### Audio frequency-shift keying

*Audio frequency-shift keying* (AFSK) is a modulation technique by which digital data is represented by changes in the frequency (pitch) of an audio tone, yielding an encoded signal suitable for transmission via radio or telephone. Normally, the transmitted audio alternates between two tones: one, the "mark", represents a binary one; the other, the "space", represents a binary zero.

AFSK differs from regular frequency-shift keying in performing the modulation at baseband frequencies. In radio applications, the AFSK-modulated signal normally is being used to modulate an RF carrier (using a conventional technique, such as AM or FM) for transmission.

AFSK is not always used for high-speed data communications, since it is far less efficient in both power and bandwidth than most other modulation modes. In addition to its simplicity, however, AFSK has the advantage that encoded signals will pass through AC-coupled links, including most equipment originally designed to carry music or speech.

AFSK is used in the U.S.-based Emergency Alert System to notify stations of the type of emergency, locations affected, and the time of issue without actually hearing the text of the alert.

### Multilevel frequency-shift keying

Phase 1 radios in the Project 25 system use 4-level frequency-shift keying (4FSK).

## Applications

In 1910, Reginald Fessenden invented a two-tone method of transmitting Morse code. Dots and dashes were replaced with different tones of equal length. The intent was to minimize transmission time.

Some early Continuous Wave (CW) transmitters employed an arc converter that could not be conveniently keyed. Instead of turning the arc on and off, the key slightly changed the transmitter frequency in a technique known as the *compensation-wave method*. The compensation-wave was not used at the receiver. Spark transmitters used for this method consumed a lot of bandwidth and caused interference, so it was discouraged by 1921.

Most early telephone-line modems used audio frequency-shift keying (AFSK) to send and receive data at rates up to about 1200 bits per second. The Bell 103 and Bell 202 modems used this technique. Even today, North American caller ID uses 1200 baud AFSK in the form of the Bell 202 standard. Some early microcomputers used a specific form of AFSK modulation, the Kansas City standard, to store data on audio cassettes and other magnetic tape. AFSK is still widely used in amateur radio, as it allows data transmission through unmodified voiceband equipment.

The Specific Area Message Encoding (SAME) protocol, most commonly used in The United States' Emergency Alert System, utilizes AFSK data bursts to transmit data at a rate of 520 5⁄6 bits per second. This allows SAME-enabled radio receivers to activate upon demodulation of the header information.

The CHU shortwave radio station in Ottawa, Ontario, Canada broadcasts an exclusive digital time signal encoded using AFSK modulation.

_(Later sections of the original article were left out of this offline copy for length: Caller ID and remote metering standards.)_
