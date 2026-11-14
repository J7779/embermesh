---
id: radio/wp2-olivia-mfsk
title: Olivia MFSK
category: radio
tags: [olivia mfsk, olivia x / y, olivia y / x, radio]
summary: Olivia MFSK is an amateur radioteletype protocol, using multiple frequency-shift keying (MFSK) and designed to work in difficult (low signal-to-noise ratio plus multipath propagation) conditions on shortwave bands.
source: https://en.wikipedia.org/w/index.php?title=Olivia_MFSK&oldid=1324522392
source_title: Wikipedia: Olivia MFSK
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Olivia MFSK", Wikipedia, The Free Encyclopedia (revision 1324522392), https://en.wikipedia.org/w/index.php?title=Olivia_MFSK&oldid=1324522392 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Olivia MFSK

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Olivia MFSK** is an amateur radioteletype protocol, using multiple frequency-shift keying (MFSK) and designed to work in difficult (low signal-to-noise ratio plus multipath propagation) conditions on shortwave bands. The signal can be accurately received even if the surrounding noise is 10 dB stronger. It is commonly used by amateur radio operators to reliably transmit ASCII characters over noisy channels using the high frequency (3–30 MHz) spectrum. The effective data rate of the Olivia MFSK protocol is 150 characters/minute.

Olivia modes are commonly referred to as **Olivia *X* / *Y*** (or, alternatively, **Olivia *Y* / *X***), where ***X*** refers to the number of different audio tones transmitted and ***Y*** refers to the bandwidth in hertz over which these signals are spread. Examples of common Olivia modes are 16/500, 32/1000 and 8/250.

## Voluntary channelization

Since Olivia signals can be decoded even when received signals are extremely weak, (signal-to-noise ratio of −14 dB), signals strong enough to be decoded are sometimes below the noise floor and therefore impossible to search for manually. As a result, amateur radio operators have voluntarily decided upon channelization for this mode. This channelization allows even imperceptibly weak signals to be properly tuned for reception and decoding. By common convention amateur stations initiate contacts utilizing the 8/250 mode and then switch to other wider-bandwidth modes to continue the conversation. The following table lists the common center frequencies used in the amateur radio bands, and is based on community collaboration after the introduction of new amateur radio digital modes and the resulting overlapping with the Olivia calling frequencies on some amateur radio bands.

**Olivia suggested calling frequencies (move after establishing QSO)**

| Band | Center frequency (MHz) | Dial frequency | Number of tones/bandwidth (initial) | Notes |
|---|---|---|---|---|
| 160 meters | 1.8270 MHz | 1.8255 MHz | 8/250 | (ITU Region 2; Secondary) |
| 160 meters | 1.8390 MHz | 1.8375 MHz | 8/250 | (ITU Regions 1, 3; Primary, International) |
| 80 meters | 3.5830 MHz | 3.5815 MHz | 8/250 | |
| 40 meters | 7.0400 MHz | 7.0385 MHz | 8/250 | (International/DX) |
| 40 meters | 7.0730 MHz | 7.0715 MHz | 8/250 | (Primary DX) |
| 30 meters | 10.1430 MHz | 10.1415 MHz | 8/250 | |
| 30 meters | 10.1440 MHz | 10.1425 MHz | 16/1000 | (Potential - be mindful of other stations and modes) |
| 20 meters | 14.0730 MHz | 14.0715 MHz | 8/250 | |
| 20 meters | 14.1075 MHz | 14.1060 MHz | 16/1000, 32/1000 | |
| 17 meters | 18.0990 MHz | 18.0975 MHz | 8/250 | |
| 15 meters | 21.0730 MHz | 21.0715 MHz | 8/250 | |
| 12 meters | 24.9230 MHz | 24.9215 MHz | 8/250 | |
| 10 meters | 28.1230 MHz | 28.1215 MHz | 8/250 | |

## Tones and bandwidth combinations

Conversations using Olivia are by convention initiated using either Olivia 16/500 (16 tones over a 500 Hz bandwidth) or Olivia 32/1000 (32 tones over a 1000 Hz bandwidth). Once communications have been established, the communicating parties mutually decide if another mode would better suit the current propagation conditions. The possible number of tones that can be chosen are 2, 4, 8, 16, 32, 64, 128, and 256 with higher numbers of tones giving more data redundancy but slower throughput and lower numbers of tones giving faster throughput at the cost of less redundancy. Available bandwidths for Olivia are 125 Hz, 256 Hz, 512 Hz, 500 Hz, 1000 Hz, and 2000 Hz with wider bandwidths giving faster throughput and narrower bandwidths giving slower throughput. The most commonly used combinations are 4/125, 8/250, 8/500, 16/500, 16/1000, and 32/1000.

## Technical details

Being a digital protocol, Olivia transmits a stream of ASCII (7-bit) characters. The characters are sent in blocks of 5. Each block takes 2 seconds to transmit, thus the effective data rate is 2.5 character/second or 150 characters/minute. The most common transmission bandwidth is 1000 Hz and the baud rate is 31.25 MFSK tones/second. To accommodate for different conditions and for the purpose of experimentation the bandwidth and the baud rate can be changed.

The Olivia transmission system is constructed of two layers: the lower, modulation and forward error correcting (FEC) code layer is a classical multiple frequency-shift keying (MFSK) while the higher layer is a forward error correcting code based on Walsh functions.

Both layers are of similar nature: they constitute a "1-out-of-N" FEC code. For the first layer the orthogonal functions are (co)sine functions, with 32 different frequencies (tones). At a given time only one of those 32 tones is being sent. The demodulator measures the amplitudes of all the 32 possible tones (using a Fourier transform) and (knowing that only **one** of those 32 could have been sent) picks up the tone with the highest amplitude.

For the second FEC layer: every ASCII character is encoded as one of 64 possible Walsh functions (or vectors of a Hadamard matrix). The receiver again measures the amplitudes for all 64 vectors (here comes the Hadamard Transform) and chooses the greatest.

For optimal performance the actual demodulators work with soft decisions and the final (hard) decision to decode a character is taken only at the second layer. Thus the first layer demodulator actually produces soft decisions for each of the 5 bits associated to an MFSK tone instead of simply picking up the highest tone to produce hard decisions for those 5 bits.

In order to avoid simple transmitted patterns (like a constant tone) and to minimize the chance for a false lock at the synchronizer the characters encoded into the Walsh function pass through a scrambler and interleaver. This stage simply shifts and XORs bits with predefined scrambling vectors and so it does not improve the performance where the white (uncorrelated) noise is concerned, but the resulting pattern gains certain distinct characteristics which are of great help to the synchronizer.

The receiver synchronizes automatically by searching through possible time and frequency offsets for a matching pattern. The frequency search range is normally ±100 Hz but can be as high as ±500 Hz if the user wishes so.

### The MFSK layer

The default mode sends 32 tones within the 1000 Hz audio bandwidth and the tones are spaced by 1000 Hz/32 = 31.25 Hz. The tones are shaped to minimize the amount of energy sent outside the nominal bandwidth.

The exact shape formula is:

+1.0000000000 +1.1913785723 cos(x) -0.0793018558 cos(2x) -0.2171442026 cos(3x) -0.0014526076 cos(4x)

where *x* ranges from −π to π.

The coefficients represent the symbol shape in the frequency domain and were calculated by a minimization procedure which sought to make the smallest crosstalk and the smallest frequency spillover.

The tones are sent at 31.25 baud or every 32 milliseconds. The phase is not preserved from one tone to the next: instead a random shift of ±90 degrees is introduced in order not to transmit a pure tone when the same symbol is repeatedly sent. Because the symbols are smoothly shaped there is no need to keep the phase constant, which normally is the case when no (e.g., square) shaping is used.

The modulator uses the Gray code to encode 5-bit symbols into the tone numbers.

The waveform generator is based on the 8000 Hz sampling rate. The tones are spaced by 256 samples in time and the window that shapes them is 512 samples long. The demodulator is based on the FFT with the size of 512 points. The tone spacing in frequency is 8000 Hz/256 = 31.25 Hz and the demodulator FFT has the resolution of 8000 Hz/512 = 15.625 Hz thus half of the tone separation.

To adapt the system to different propagation conditions, the number of tones and the bandwidth can be changed and the time and frequency parameters are proportionally scaled. The number of tones can be 2, 4, 8, 16, 32, 64, 128 or 256. The bandwidth can be 125, 250, 500, 1000 or 2000 Hz.

### The Walsh functions FEC layer

The modulation layer of the Olivia transmission system in the default mode sends one of 32 tones at a time. Each tone thus constitutes a symbol that carries 5 bits of information. For the FEC code, 64 symbols are taken to form a block. Within each block one bit out of every symbol is taken and it forms a 64-bit vector coded as a Walsh function. Every 64-bit vector represents a 7-bit ASCII character, thus each block represents 5 ASCII characters.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Technical details, Samples, Contestia.)_
