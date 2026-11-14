---
id: radio/wp2-signal-modulation
title: Signal modulation
category: radio
tags: [signal modulation, modulator, demodulator, mod, dem, radio]
summary: Signal modulation is the process of varying one or more properties of a periodic waveform in electronics and telecommunication for the purpose of transmitting information.
source: https://en.wikipedia.org/w/index.php?title=Signal_modulation&oldid=1381143769
source_title: Wikipedia: Signal modulation
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Signal modulation", Wikipedia, The Free Encyclopedia (revision 1381143769), https://en.wikipedia.org/w/index.php?title=Signal_modulation&oldid=1381143769 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Signal modulation

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Signal modulation** is the process of varying one or more properties of a periodic waveform in electronics and telecommunication for the purpose of transmitting information.

The process encodes information in the form of a message signal modulated onto a carrier signal to be transmitted. For example, the message signal might be an audio signal representing sound from a microphone, a video signal representing moving images from a video camera, or a digital signal representing a sequence of binary digits, a bitstream from a computer.

Carrier waves are necessary when the frequency of the message is too low to practically transmit. Generally, receiving a radio wave requires a radio antenna with a length that is one-fourth of the wavelength of the transmitted wave. For low-frequency radio waves, wavelength is on the scale of kilometers and building such a large antenna is not practical.

Another purpose of modulation is to transmit multiple channels of information through a single communication medium, using frequency-division multiplexing (FDM). For example, in cable television (which uses FDM), many carrier signals, each modulated with a different television channel, are transported through a single cable to customers. Since each carrier occupies a different frequency, the channels do not interfere with each other. At the destination end, the carrier signal is demodulated to extract the information-bearing modulation signal.

A **modulator** is a device or circuit that performs modulation. A **demodulator** (sometimes *detector*) is a circuit that performs demodulation, the inverse of modulation. A modem (from **mod**ulator–**dem**odulator), used in bidirectional communication, can perform both operations. The lower frequency band occupied by the modulation signal is called the *baseband*, while the higher frequency band occupied by the modulated carrier is called the *passband*.

Signal modulation techniques are fundamental methods used in wireless communication to encode information onto a carrier wave by varying its amplitude, frequency, or phase. Key techniques and their typical applications

**Types of Signal Modulation**

| Category | Modulation type | Key features | Example uses |
|---|---|---|---|
| **Analog modulation** | AM | Varies amplitude of carrier | AM radio |
| | FM | Varies frequency of carrier | FM and two-way radio |
| | PM | Varies phase of carrier | Analog TV and satellite |
| **Digital modulation** | ASK | Amplitude represents binary data | RFID, Optical Comm |
| | FSK | Frequency shift encodes data | Modems, Bluetooth |
| | PSK | Phase of carrier encodes bits | Wi-Fi, Satellite |
| | QPSK | 4 phase states: two bits per symbol | DVB, LTE |
| | QAM | Both amplitude and phase vary; can transmit multiple bits | Cable TV, Wi-Fi |
| | OFDM | Multiple carriers, each modulated separately | 4G/5G, Wi-Fi |
| **Pulse modulation** | PWM | Pulse width represents amplitude | Motor Control, Audio |
| | PPM | Pulse position within a time slot represents data | Optical Comm., Radar |
| **Spread spectrum** | DSSS | Signal bandwidth spread using code sequence | CDMA, GPS |
| | FHSS | Carrier hops between different frequencies | Bluetooth, Military |

- Amplitude Shift Keying (ASK): Varies the amplitude of the carrier signal to represent data. Simple and energy efficient, but vulnerable to noise. Used in RFID and sensor networks.

- Frequency Shift Keying (FSK): Changes the frequency of the carrier signal to encode information. Resistant to noise, simple in implementation, often used in telemetry and paging systems.
- Phase Shift Keying (PSK): Modifies the phase of the carrier signal based on data. Common forms include Binary PSK (BPSK) and Quadrature PSK (QPSK), used in Wi-Fi, Bluetooth, and cellular networks. Offers good spectral efficiency and robustness against interference.
- Quadrature Amplitude Modulation (QAM): Simultaneously varies both amplitude and phase to transmit multiple bits per symbol, increasing data rates. Used extensively in Wi-Fi, cable television, and LTE systems.
- Orthogonal Frequency Division Multiplexing (OFDM): Splits the data across multiple, closely spaced sub-carriers, each modulated separately (often with QAM or PSK). Provides high spectral efficiency and robustness in multipath environments and is widely used in WLAN, LTE, and WiMAX.
Other advanced techniques:

- Amplitude Phase Shift Keying (APSK): Combines features of PSK and QAM, mainly used in satellite communications for improved power efficiency.
- Spread Spectrum (e.g., DSSS): Spreads the signal energy across a wide band for robust, low probability of intercept transmission.
In **analog modulation**, an analog modulation signal is "impressed" on the carrier. Examples are amplitude modulation (AM) in which the amplitude (strength) of the carrier wave is varied by the modulation signal, and frequency modulation (FM) in which the frequency of the carrier wave is varied by the modulation signal. These were the earliest types of modulation, and are used to transmit an audio signal representing sound in AM and FM radio broadcasting. More recent systems use **digital modulation**, which impresses a digital signal consisting of a sequence of binary digits (bits), a bitstream, on the carrier, by means of mapping bits to elements from a discrete alphabet to be transmitted. This alphabet can consist of a set of real or complex numbers, or sequences, like oscillations of different frequencies, so-called frequency-shift keying (FSK) modulation. A more complicated digital modulation method that employs multiple carriers, orthogonal frequency-division multiplexing (OFDM), is used in WiFi networks, digital radio stations and digital cable television transmission.

## Analog modulation methods

In analog modulation, the modulation is applied continuously in response to the analog information signal. Common analog modulation techniques include:

- Amplitude modulation (AM) (here the amplitude of the carrier signal is varied in accordance with the instantaneous amplitude of the modulating signal)
 - Double-sideband modulation (DSB)
 - Double-sideband modulation with carrier (DSB-WC) (used on the AM radio broadcasting band)
 - Double-sideband suppressed-carrier transmission (DSB-SC)
 - Double-sideband reduced-carrier transmission (DSB-RC)
 - Single-sideband modulation (SSB, or SSB-AM)
 - Single-sideband modulation with carrier (SSB-WC)
 - Single-sideband modulation suppressed carrier modulation (SSB-SC)
 - Vestigial-sideband modulation (VSB, or VSB-AM)
 - Quadrature amplitude modulation (QAM)
- Angle modulation, which is approximately constant envelope
 - Frequency modulation (FM) (here the frequency of the carrier signal is varied in accordance with the instantaneous amplitude of the modulating signal)
 - Phase modulation (PM) (here the phase shift of the carrier signal is varied in accordance with the instantaneous amplitude of the modulating signal)
 - Transpositional Modulation (TM), in which the waveform inflection is modified, resulting in a signal where each quarter cycle is transposed in the modulation process. TM is a pseudo-analog modulation (AM). Where an AM carrier also carries a phase variable phase f(ǿ). TM is f(AM,ǿ)

## Pulse modulation methods

Pulse modulation schemes aim at transferring a narrowband analog signal over an analog baseband channel as a two-level signal by modulating a pulse wave. Some pulse modulation schemes also allow the narrowband analog signal to be transferred as a digital signal (i.e., as a quantized discrete-time signal) with a fixed bit rate, which can be transferred over an underlying digital transmission system, for example, some line code. These are not modulation schemes in the conventional sense since they are not channel coding schemes, but should be considered as source coding schemes, and in some cases, analog-to-digital conversion techniques.

**Analog-over-analog methods**
- Pulse-amplitude modulation (PAM)
- Pulse-width modulation (PWM) and pulse-depth modulation (PDM)
- Pulse-frequency modulation (PFM)
- Pulse-position modulation (PPM)

**Analog-over-digital methods**
- Pulse-code modulation (PCM)
 - Differential PCM (DPCM)
 - Adaptive DPCM (ADPCM)
- Delta modulation (DM or Δ-modulation)
 - Delta-sigma modulation (ΣΔ)
 - Continuously variable slope delta modulation (CVSDM), also called adaptive delta modulation (ADM)
- Pulse-density modulation (PDM)

_(Later sections of the original article were left out of this offline copy for length: Digital modulation methods, Miscellaneous modulation techniques.)_
