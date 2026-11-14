---
id: radio/wp2-wspr-amateur-radio-software
title: WSPR (amateur radio software)
category: radio
tags: [wspr, weak signal propagation reporter, radio]
summary: WSPR (pronounced "whisper") is an acronym for Weak Signal Propagation Reporter.
source: https://en.wikipedia.org/w/index.php?title=WSPR_(amateur_radio_software)&oldid=1373359324
source_title: Wikipedia: WSPR (amateur radio software)
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "WSPR (amateur radio software)", Wikipedia, The Free Encyclopedia (revision 1373359324), https://en.wikipedia.org/w/index.php?title=WSPR_(amateur_radio_software)&oldid=1373359324 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# WSPR (amateur radio software)

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**WSPR** (pronounced "whisper") is an acronym for **Weak Signal Propagation Reporter**. It is a protocol, implemented in a computer program, used for weak-signal radio communication between amateur radio operators. The protocol was designed, and a program written initially, by Joe Taylor, K1JT. The software code is now open source and is developed by a small team. The program is designed for sending and receiving low-power transmissions to test propagation paths on the MF and HF bands.

WSPR implements a protocol designed for probing potential propagation paths with low-power transmissions. Transmissions carry a station's callsign, Maidenhead grid locator, and transmitter power in dBm. The program can decode signals with a signal-to-noise ratio as low as −28 dB in a 2.5 kHz bandwidth. Stations with internet access can automatically upload their reception reports to a central database called WSPRnet, which includes a mapping facility.

## The WSPR protocol

The type of radio emission is "F1D", frequency-shift keying. A message contains a station's callsign, Maidenhead grid locator, and transmitter power in dBm. The WSPR protocol compresses the information in the message into 50 bits (binary digits). These are encoded using a convolutional code with constraint length *K* = 32 and a rate of *r* = 1/2. The long constraint length makes undetected decoding errors less probable, at the cost that the highly efficient Viterbi algorithm must be replaced by a simple sequential algorithm for the decoding process.

### Protocol specification

The standard message is + +; for example “K1ABC FN20 37” is a signal from station K1ABC in Maidenhead grid cell “FN20”, sending 37 dBm, or about 5.0 W (legal limit for 630 m).
Messages with a compound callsign and/or 6 digit locator use a two-transmission sequence. The first transmission carries compound callsign and power level, or standard callsign, 4 digit locator, and power level; the second transmission carries a hashed callsign, 6 digit locator, and power level. Add-on prefixes can be up to three alphanumeric characters; add-on suffixes can be a single letter or one or two digits.

- Fields of a standard message:
28 bits for callsign,
15 bits for locator,
5 bits for power level,
2 bits for message type,
total: 50 bits.

- Forward error correction (FEC):
non-recursive convolutional code with constraint length *K* = 32, rate *r* = 1/2.

- Number of binary channel symbols:
nsym = (50 + K − 1) × 2 = 162.

- Keying rate is 12000/8192 = 1.4648 baud.
- Modulation is continuous phase 4 FSK, with 1.4648 Hz tone separation.

- Occupied bandwidth is about 6 Hz
- Synchronization is via a 162 bit pseudo-random sync vector.
- Each channel symbol conveys one sync bit (LSB) and one data bit (MSB).
- Duration of transmission is 162 × 8192/12000 = 110.6 s.
- Transmissions nominally start one second into an even UTC minute: e.g., at hh:00:01, hh:02:01, etc.
- Minimum S/N for reception is around –34 dB on the WSJT scale (2500 Hz reference bandwidth).

## Applications

The protocol was designed to test propagation paths on the LF, MF and HF bands.
Also used experimentally at VHF and higher frequencies.

Other applications include antenna testing, frequency stability and frequency accuracy checking.

Usually a WSPR station contains a computer and a transceiver, but it is also possible to build very simple beacon transmitters with little effort.

For example a simple WSPR beacon can be built using the Si 570, or Si 5351. The Raspberry Pi can also be used as WSPR beacon.

An accurate clock is essential both for transmission and decoding of received signals.

### MH370 hypothesis

In May 2021, banking IT professional Richard Godfrey suggested examining historical WSPR data as a way to define the flight path of Malaysia Airlines Flight 370 on 8 March 2014. In November 2021, Godfrey stated his belief that his analysis indicated the aircraft had flown in circles for approximately 20 minutes in an area 150 nmi from the coast of Sumatra before vanishing, later proposing a search area centered around. However, two searches conducted in 2025 and 2026 by Ocean Infinity in the region suggested by Godfrey failed to find any evidence of wreckage from flight MH370.
