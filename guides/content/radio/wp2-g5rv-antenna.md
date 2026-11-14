---
id: radio/wp2-g5rv-antenna
title: G5RV antenna
category: radio
tags: [g5rv antenna, radio]
summary: The G5RV antenna is a dipole with a symmetric resonant feeder line, which serves as impedance matcher for a 50 Ω coax cable to the transceiver.
source: https://en.wikipedia.org/w/index.php?title=G5RV_antenna&oldid=1342464324
source_title: Wikipedia: G5RV antenna
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "G5RV antenna", Wikipedia, The Free Encyclopedia (revision 1342464324), https://en.wikipedia.org/w/index.php?title=G5RV_antenna&oldid=1342464324 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# G5RV antenna

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **G5RV antenna** is a dipole with a symmetric resonant feeder line, which serves as impedance matcher for a 50 Ω coax cable to the transceiver.

## Impedance

The dipole elements are 15.55 m and the impedance-matching symmetric feedline (ladder-line or twin-lead) can be either 300 Ω (8.84 m) or 450 Ω (10.36 m). As is in general the case for all electric antennas, the height of the G5RV above the ground should be at least half of the longest wavelength to be used.

The ends of the symmetric feedline can be soldered directly onto a 50 Ω coax cable to the transceiver; however, this is not good practice and should be avoided: It can result in high current flow on the outer surface of the coax braid, causing RF interference and degrading the polarization and gain of the antenna. In his article, Varney recommended placing a balanced matching network at the end of the ladder line, instead of a direct cable connection. With no matching unit, Varney specified 75 Ω cable be used at the junction of the ladder line and coax (*not* 50 Ω); the higher 75 Ω impedance makes a closer match the end of the ladder line. An earth-grounded 4:1 voltage balun *may* be used to connect the coax to the ladder line, and 1:1 current balun *should* be used between the coax and the transmitter. Several sources point out that a current balun not only prevents RF interference, but also reduces receive noise. A length of at least 20 m of 75 Ω (50 Ω) cable is recommended for operation without a balun.

An antenna tuner is not required to use this antenna near its nominal design frequency of 14 MHz, and judicious length adjustments can sometimes include one other frequency band. All other frequencies require a transmatch. There are many variants of the G5RV antenna. Two variations of the G5RV design, called ZS6BKW and W0BTU, can match several more amateur bands between 3.5–28 MHz without a transmatch.
