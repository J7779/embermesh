---
id: radio/wp2-digital-selective-calling
title: Digital selective calling
category: radio
tags: [digital selective calling, dsc, radio]
summary: Digital selective calling (DSC) is a standard for transmitting predefined digital messages via the medium-frequency (MF), high-frequency (HF) and very-high-frequency (VHF) maritime radio systems.
source: https://en.wikipedia.org/w/index.php?title=Digital_selective_calling&oldid=1368512122
source_title: Wikipedia: Digital selective calling
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Digital selective calling", Wikipedia, The Free Encyclopedia (revision 1368512122), https://en.wikipedia.org/w/index.php?title=Digital_selective_calling&oldid=1368512122 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Digital selective calling

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Digital selective calling** (**DSC**) is a standard for transmitting predefined digital messages via the medium-frequency (MF), high-frequency (HF) and very-high-frequency (VHF) maritime radio systems. It is a core part of the Global Maritime Distress Safety System (GMDSS).

## Workings

DSC was developed to replace a voice call in older procedures. Because a DSC signal uses a stable signal with a narrow bandwidth and the receiver has no squelch, it has a slightly longer range than analog signals, with up to twenty-five percent longer range and significantly faster. DSC senders are programmed with the ship's Maritime Mobile Service Identity (MMSI) and may be connected to the ship's Global Positioning System (GPS), which allows the apparatus to know who it is, what time it is and where it is. This allows a distress signal to be sent very quickly.

Often, ships use separate VHF DSC and MF/HF DSC controllers. For VHF, DSC has its own dedicated receiver for monitoring Channel 70, but uses the main VHF transceiver for transmission. However, for the user, the controller is often a single unit. MF/HF DSC devices monitor multiple bands for distress, urgency and safety *sécurité* transmissions in the 2, 4, 6, 8, 12 and 16 MHz marine bands. At minimum, controllers will monitor 2187.5 kHz and 8414.5 kHz and one more band. However for automated monitoring a second, receive-only antenna is often needed (especially on non-commercial leisure boats) since a separate tuner is used apart from the main one; this is separate from programming radios to monitor user-defined DSC frequencies (which would use the main antenna).

## Distress

When sending a distress signal, the DSC device will at minimum include the ship's MMSI number. It may also include the coordinates if radio is connected to GPS system and, if necessary, the channel for the following radiotelephony or radiotelex messages. The distress can be sent either as a single-frequency or multi-frequency attempt. In the former, a distress signal is sent on one band and the system will wait up to four minutes for a DSC acknowledgment from a coast station. If none is received, it will repeat the distress alert up to five times. In a multi-frequency attempt, the distress signal is sent on the MF and all the HF distress frequencies in turn. As this requires retuning the antenna for each sending, without waiting for an acknowledgment, a multi-frequency attempt should only be done if there are only a few minutes until the ship's batteries are under water. As the distress message can only be sent on one of the bands, many ships and coast stations may be listening to a band without the message, and will after five minutes relay the distress signal to a coast station.

Distress calls can be both non designated and designated. The latter allows one of ten predefined designations to be sent along with the distress signal. These are "abandoning ship", "fire or explosion", "flooding", "collision", "grounding", "listing", "sinking", "disabled and adrift", "piracy or attack" and "man overboard". To avoid false distress alerts, distress buttons normally have protective covers, often with a spring-loaded cover so two hands need to be used simultaneously. Alternatively, some devices have two-button systems. Operators are required to cancel falsely sent distress alerts with a transmission on the channel designated by the distress signal.

A coast station which receives a DSC distress alert will wait 2.75 minutes prior to sending an acknowledgment to allow other vessels in the nearby area to receive the alert. This helps shore stations narrow down where a vessel without GPS is located. The sending device will then both stop repeating the alert, and tune to the designated channel for the distress message to be sent. Ships receiving a distress alert who are outside coast station range or do not receive an acknowledgment, are required to relay the distress alert by any means to land.

## Other priorities

The priorities for communication are, Distress, Urgency, Safety and Routine. A Distress DSC call is called an Alert. Urgency, Safety and Routine are called Announcements.

Class A VHFs, used on commercial ships, have the ability to send distress, distress relay, all ships urgency, all ships safety, individual, group, geographic area and telephone alerts/announcements on DSC channel 70 (Digital channel reserved for DSC only.). Class D VHFs, used for most leisure vessels, can send distress, all ships urgency, all ships safety and individual alerts/announcements on VHF channels on DSC channel 70. On both class A and D you can be directed to Ch 06, 08, 72, 77 or other simplex channel for the follow-up RT (radio telephony [voice]) call. Class D has only one antenna and thus can only watch Channel 70 when not transmitting. For routine alerts, which are used to establish communication with another station on a working channel, the receiver acknowledges to confirm that communication can be done on the appropriate channel.

While there are reserved frequencies for distress HF DSC calls, there is no prohibition against broadcasting non-distress, "routine" calls on other DSC-designated frequencies, which are defined in ITU M.541 as:

- 2177, 2189.5 kHz
- 4208, 4208.5, 4209 kHz
- 6312.5, 6313, 6313.5 kHz
- 8415, 8415.5, 8416 kHz
- 12577.5, 12578, 12578.5 kHz
- 16805, 16805.5, 16806 kHz
- 18898.5, 18899, 18899.5 kHz
- 22374.5, 22375, 22375.5 kHz
- 25208.5, 25209, 25209.5 kHz

There is a general consensus for routine calls to use 2177.0, 4208.0, 6312.5, 8415.0, 12577.5, and 16805.0 kHz (the first frequency listed above in each band).

## Technical details

The DSC is a synchronous system using characters composed from a ten-bit error detecting code. The bits are encoded using frequency-shift keying. For High Frequency and Medium Frequency two tones 170 Hz apart either side of the allocated frequency with 100 baud symbol rate are used. For VHF the two tones used are 1300 and 2100 Hz with a symbol rate of 1200 baud. Each character is transmitted twice with a time delay. The detailed specification is published in the International Telecommunication Union recommendation ITU-R M.493, revision 16 published in 2023 being the most recent.
