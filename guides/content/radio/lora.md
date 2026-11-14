---
id: radio/lora
title: LoRa
category: radio
tags: [radio, LoRa, LPWAN, mesh, ISM band]
summary: "LoRa chirp spread-spectrum radio: long range at low data rates on licence-free ISM bands."
source: "https://en.wikipedia.org/w/index.php?title=LoRa&oldid=1385168620"
source_title: LoRa (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'LoRa' (rev 1385168620), Wikipedia, https://en.wikipedia.org/wiki/LoRa. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# LoRa

LoRa (from "long range") is a physical proprietary radio communication technique based on spread spectrum modulation. LoRa can be thought of as a radio signal technology, similar to Wi-Fi or cellular. It was developed by Cycleo and acquired by Semtech in 2012.

The technology is primarily used for applications where small amounts of data need to be transmitted infrequently from hard-to-reach locations.

LoRa uses license-free sub-gigahertz radio frequency bands EU433 (433.050-434.790 MHz) or EU868 (863–870/873 MHz) in Europe; AU915/AS923-1 (915–928 MHz) in South America; US915 (902–928 MHz) in North America; IN865 (865–867 MHz) in India; and AS923 (915–928 MHz) in Asia; LoRa enables long-range transmissions with low power consumption. The technology covers the physical layer, while other technologies and protocols such as LoRaWAN cover the upper layers. It can achieve data rates between 0.3 kbit/s and 27 kbit/s, depending upon the spreading factor.

## Description

LoRa uses a proprietary spread spectrum modulation that is similar to and a derivative of chirp spread spectrum (CSS) modulation. Each symbol is represented by a cyclic shifted chirp over the bandwidth centered around the base frequency.

The spreading factor (SF) is a selectable radio parameter from 5 to 12 and represents the number of bits sent per symbol and in addition determines how much the information is spread over time. There are `M = 2^(SF)` different initial frequencies of the cyclic shifted chirp across the bandwidth around the center frequency.

The symbol rate is determined by `R_s = B/M`. LoRa can tradeoff data rate for sensitivity (assuming a fixed channel bandwidth `B`) by selecting the SF, i.e. the amount of spread used. A lower SF corresponds to a higher data rate but a worse sensitivity, a higher SF implies a better sensitivity but a lower data rate. Compared to lower SF, sending the same amount of data with higher SF needs more transmission time, known as time-on-air. More time-on-air means that the modem is transmitting for a longer time and consuming more energy.

Typical LoRa modems support transmit powers up to +22 dBm. However, the regulations of the respective country may additionally limit the allowed transmit power. Higher transmit power results in higher signal power at the receiver and hence a higher link budget, but at the cost of consuming more energy. There are measurement studies of LoRa performance with regard to energy consumption, communication distances, and medium access efficiency. According to the LoRa Development Portal, the range provided by LoRa can be up to 3 miles (4.8 km) in urban areas, and up to 10 miles (16 km) or more in rural areas (line of sight).

In addition, LoRa uses forward error correction coding to improve resilience against interference. LoRa's high range is characterized by high wireless link budgets of around 155 dB to 170 dB.

### Frame structure

A LoRa frame begins with a preamble of identical, unmodulated upchirps. The receiver uses the preamble to detect the frame and to synchronize coarsely. Two modulated chirps follow. They carry the sync word, which acts as a network identifier. Unlike data symbols, the sync word symbols are not Gray coded, whitened or protected by forward error correction. A start frame delimiter (SFD) of 2.25 downchirps comes next. The end of the SFD marks the first data symbol and gives the receiver symbol-level timing. The data symbols carry an optional header, the payload and an optional payload CRC. In explicit header mode, the header states the payload length, the coding rate and whether a CRC is present. In implicit header mode, both ends fix these values in advance and no header is sent.

### Channel coding

LoRa applies whitening, a modified Hamming code, interleaving and Gray mapping to the data before modulation. The code adds one to four parity bits to each four-bit nibble, which gives coding rates from 4/5 to 4/8. The header is always sent at coding rate 4/8 in a reduced-rate mode that carries `SF − 2` bits per symbol. A low data rate optimization mode applies the same reduction to the payload. Semtech datasheets mandate it when the symbol duration exceeds 16 ms.

### Demodulation

A receiver demodulates each symbol by dechirping. It multiplies the received chirp by a downchirp, the complex conjugate of the base upchirp. The product is a tone whose frequency equals the symbol's cyclic shift. At a sample rate equal to the bandwidth, a fast Fourier transform of `2^(SF)` points maps this tone to a single bin. The index of the strongest bin is the transmitted symbol value. Dechirping concentrates the symbol energy, which was spread across the full bandwidth, into that bin. Noise stays spread across all bins. This processing gain lets commercial receivers demodulate frames at a signal-to-noise ratio of about −20 dB. Commercial transceivers also provide channel activity detection (CAD). CAD confirms whether a LoRa frame is present on a channel without demodulating its payload.

## LoRa transceivers

Semtech makes several families of LoRa transceivers, grouped into four generations. The first generation appeared in 2013 and the fourth in 2025. A host microcontroller configures each of them over SPI. Each generation adds frequency bands, modulations or features, and newer chips remain compatible with earlier LoRa devices.

**Semtech LoRa transceiver families**

| Family | Generation | Frequency bands | Modulations | Extra features | Firmware can be changed |
|---|---|---|---|---|---|
| SX127x | 1 | Sub-GHz (137–1020 MHz) | LoRa, (G)FSK, (G)MSK, OOK |   | No |
| SX126x, LLCC68 | 2 | Sub-GHz (150–960 MHz) | LoRa, (G)FSK, (G)MSK, LR-FHSS |   | No |
| SX128x | 2 | 2.4 GHz | LoRa, FLRC, (G)FSK, BLE PHY | Time-of-flight ranging | No |
| LR11xx | 3 | Sub-GHz; 2.4 GHz and S-band on newer models | LoRa, (G)FSK, LR-FHSS | GNSS and Wi-Fi scanning on LR1110 and LR1120 | Yes |
| LR20xx | 4 | Sub-GHz, 2.4 GHz and S-band | LoRa, FLRC, (G)FSK, OOK, O-QPSK, LR-FHSS |   | Yes |

Earlier generations run fixed firmware. In the third and fourth generations, the radio firmware can be changed. Semtech publishes firmware for the LR11xx chips in a public repository, together with a reference update tool. This allows chips already deployed in the field to receive updates. For example, LoRa Basics Modem-E firmware turns an LR1110 or LR1121 into a complete LoRaWAN modem. On the LR1121, it provides a certified LoRaWAN 1.0.4 stack with relay and carrier-sense multiple access (CSMA).

Some third-generation chips also support geolocation. The LR1110 and LR1120 can scan signals from GPS and BeiDou satellites as well as nearby Wi-Fi access points. A LoRa device can therefore be located without a separate GNSS receiver.

## Open-source implementations

Several research groups have reverse engineered the LoRa physical layer and implemented it in open-source software. In 2016, researcher Matt Knight analysed LoRa signals captured with a software-defined radio. He documented the modulation and the coding chain, including whitening, interleaving, Gray mapping and the Hamming code. He released an open-source GNU Radio implementation named gr-lora. He presented the work at the GNU Radio Conference in September 2016 and at the 33rd Chaos Communication Congress (33C3) in December 2016. A detailed account appeared in the hacker journal PoC||GTFO.

Pieter Robyns and colleagues at Hasselt University in Belgium published a detailed description of the PHY in 2018. Their decoder, a separate project also named gr-lora, decodes several channels at once without frequency offset correction. In their tests it decoded every configuration of the RN2483 and SX1272. LoRa Tools is a browser-based tool that generates LoRa I/Q data and decodes recordings step by step.

## Applications

LoRa applications:

- Meshtastic — an open source mesh network protocol that uses LoRa flood messaging
- MeshCore — open source mesh network protocol that uses LoRa with more structured routing than Meshtastic
- LoRaWAN — a low-power, wide-area network (LPWAN) protocol that wirelessly connects battery-operated devices to the Internet. Uses LoRa.
  - Helium Network — LoRaWAN protocol paired with blockchain technology
- ExpressLRS — open source UAV remote control protocol that uses LoRa, widely used in FPV drones
- Amazon Sidewalk — a mesh wireless network developed by Amazon. Uses LoRa for long range
- LoRa APRS — a variation of Amateur Radio Automatic Packet Reporting System for reporting realtime location, using LoRa
- Reticulum (Network Stack) — Reticulum is an encrypted mesh network with routing. It supports LoRa as one of several transport layers. In addition to a chat protocol, it includes a Gopher (protocol)-like text browser protocol called NomadNet and a real-time voice message protocol that can use Opus (audio format).
