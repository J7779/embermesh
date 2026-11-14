---
id: radio/wp2-multiple-frequency-shift-keying
title: Multiple frequency-shift keying
category: radio
tags: [multiple frequency-shift keying, mfsk, radio]
summary: Multiple frequency-shift keying (MFSK) is a variation of frequency-shift keying (FSK) that uses more than two frequencies.
source: https://en.wikipedia.org/w/index.php?title=Multiple_frequency-shift_keying&oldid=1377161964
source_title: Wikipedia: Multiple frequency-shift keying
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Multiple frequency-shift keying", Wikipedia, The Free Encyclopedia (revision 1377161964), https://en.wikipedia.org/w/index.php?title=Multiple_frequency-shift_keying&oldid=1377161964 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Multiple frequency-shift keying

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Multiple frequency-shift keying** (**MFSK**) is a variation of frequency-shift keying (FSK) that uses more than two frequencies. MFSK is a form of M-ary orthogonal modulation, where each symbol consists of one element from an alphabet of orthogonal waveforms. M, the size of the alphabet, is usually a power of two so that each symbol represents log₂ M bits.

## Fundamentals

In a M-ary signaling system like MFSK, an "alphabet" of M tones is established and the transmitter selects one tone at a time from the alphabet for transmission. M is often a power of 2, so each tone transmission from the alphabet represents log₂ M data bits.

MFSK is classed as an M-ary **orthogonal** signaling scheme because each of the M tone detection filters at the receiver responds only to its tone and not at all to the others; this independence provides the orthogonality.

Like other M-ary orthogonal schemes, the required E_b/N₀ ratio
for a given probability of error decreases as M increases without the need for multisymbol coherent detection. In fact, as M approaches infinity the required E_b/N₀ ratio decreases asymptotically to the Shannon limit of −1.6 dB. However this decrease is slow with increasing M, and large values are impractical because of the exponential increase in required bandwidth. Typical values in practice range from 4 to 64, and MFSK is combined with another forward error correction scheme to provide additional (systematic) coding gain.

Spectral efficiency of MFSK modulation schemes decreases with increasing of modulation order *M*:

ρ = (2log_2 M)/(M)

Like any other form of angle modulation that transmits a single RF tone that varies only in phase or frequency, MFSK produces a constant envelope. This significantly relaxes the design of the RF power amplifier, allowing it to achieve greater conversion efficiencies than linear amplifiers.

### 2-tone MFSK

It is possible to combine two MFSK systems to increase the throughput of the link. Perhaps the most widely used 2-tone MFSK system is dual-tone multi-frequency (DTMF), better known by its AT&T trademark of "Touch Tone". Another is the Multi-frequency (MF) scheme used during the 20th century for in-band signalling on trunks between telephone exchanges. Both are examples of in-band signaling schemes, i.e., they share the user's communication channel.

Symbols in the DTMF and MF alphabets are sent as tone pairs; DTMF selects one tone from a "high" group and one from a "low" group, while MF selects its two tones from a common set. DTMF and MF use different tone frequencies largely to keep end users from interfering with inter-office signaling. In the 1970s, MF began to be replaced by digital out-of-band signaling, a conversion motivated in part by the widespread fraudulent use of MF signals by end users known as phone phreaks.

These signals are distinctive when received aurally as a rapid succession of tone pairs with almost musical quality.

The simultaneous transmission of two tones directly at RF loses the constant-envelope property of the single tone system. Two simultaneous RF tones is in fact the classic "stress test" of an RF power amplifier for measuring linearity and intermodulation distortion. However, two audio tones can be sent simultaneously on a conventional, constant-envelope FM RF carrier, but the noncoherent detection of the FM signal at the receiver would destroy any signal-to-noise ratio advantage the multitone scheme might have.

### MFSK in HF communications

Skywave propagation on the high frequency bands introduces random distortions that generally vary with both time and frequency.

#### Delay spread and coherence bandwidth

When several separate paths from transmitter to receiver exist, a condition known as multipath, they almost never have exactly the same length so they almost never exhibit the same propagation delay. Small delay differences, or delay spread, smear adjacent modulation symbols together and cause unwanted intersymbol interference.

Delay spread is inversely proportional to its frequency-domain counterpart, coherence bandwidth. This is the frequency range over which the channel gain is relatively constant. This is because summing two or more paths with different delays creates a comb filter even when the individual paths have a flat frequency response.

#### Coherence time and Doppler spread

Fading is a (usually random and undesired) change in path gain with time. The maximum fade rate is limited by the physics of the channel, such as the rate at which free electrons form and are recombined in the ionosphere and charged particle cloud velocities within the ionosphere. The maximum interval over which the channel gain does not appreciably change is the coherence time.

A fading channel effectively imposes an unwanted random amplitude modulation on the signal. Just as the bandwidth of intentional AM increases with the modulation rate, fading spreads a signal over a frequency range that increases with the fading rate. This is Doppler spreading, the frequency domain counterpart of coherence time. The shorter the coherence time, the greater the Doppler spread and vice versa.

#### Designing MFSK for HF

With appropriate parameter selection, MFSK can tolerate significant Doppler or delay spreads, especially when augmented with forward error correction. (Mitigating large amounts of Doppler *and* delay spread is significantly more challenging, but it is still possible). A long delay spread with little Doppler spreading can be mitigated with a relatively long MFSK symbol period to allow the channel to "settle down" quickly at the start of each new symbol. Because a long symbol contains more energy than a short one for a given transmitter power, the detector can more easily attain a sufficiently high signal-to-noise ratio (SNR). The resultant throughput reduction can be partly compensated with a large tone set so that each symbol represents several data bits; a long symbol interval allows these tones to be packed more closely in frequency while maintaining orthogonality. This is limited by the exponential growth of tone set size with the number of data bits/symbol.

Conversely, if the Doppler spread is large while the delay spread is small, then a shorter symbol period may permit coherent tone detection and the tones must be spaced more widely to maintain orthogonality.

The most challenging case is when the delay and Doppler spreads are both large, i.e., the coherence bandwidth and coherence time are both small. This is more common on auroral and EME channels than on HF, but it can occur. A short coherence time limits the symbol time, or more precisely, the maximum coherent detection interval at the receiver. If the symbol energy is too small for an adequate per-symbol detection SNR, then one alternative is transmit a symbol longer than the coherence time but to detect it with a filter much
wider than one matched to the transmitted symbol. (The filter should instead be matched to the tone spectrum expected at the receiver). This will capture much of the symbol energy despite Doppler spreading, but it will necessarily do so inefficiently. A wider tone spacing, i.e., a wider channel, is also required. Forward error correction is especially helpful in this case.

#### MFSK schemes for HF

Because of the wide variety of conditions found on HF, a wide variety of MFSK schemes, some of them experimental, have been developed for HF. Some of them are:

- MFSK8
- MFSK16
- Olivia MFSK
- Coquelet
- Piccolo
- ALE (MIL-STD 188-141)
- DominoF
- DominoEX
- THROB
- CIS-36 MFSK or CROWD-36
- XPA, XPA2

Piccolo was the original MFSK mode, developed for British government communications by Harold Robin, Donald Bailey and Denis Ralphs of the Diplomatic Wireless Service (DWS), a branch of the Foreign and Commonwealth Office. It was first used in 1962 and presented to the IEE in 1963. The current specification "Piccolo Mark IV" was still in limited use by the UK government, mainly for point-to-point military radio communications, up to the late 1990s.

Coquelet is a similar modulation system developed by the French government for similar applications.

MFSK8 and MFSK16 were developed by Murray Greenman, ZL1BPU for amateur radio communications on HF. Olivia MFSK is also an amateur radio mode. Greenman has also developed DominoF and DominoEX for NVIS radio communications on the upper MF and lower HF frequencies (1.8–7.3 MHz).

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Fundamentals.)_
