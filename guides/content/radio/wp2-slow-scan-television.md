---
id: radio/wp2-slow-scan-television
title: Slow-scan television
category: radio
tags: [slow-scan television, sstv, radio]
summary: Slow-scan television (SSTV) is a picture transmission method, used mainly by amateur radio operators, to transmit and receive static pictures via radio in monochrome or color.
source: https://en.wikipedia.org/w/index.php?title=Slow-scan_television&oldid=1383150106
source_title: Wikipedia: Slow-scan television
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Slow-scan television", Wikipedia, The Free Encyclopedia (revision 1383150106), https://en.wikipedia.org/w/index.php?title=Slow-scan_television&oldid=1383150106 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Slow-scan television

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Slow-scan television** (**SSTV**) is a picture transmission method, used mainly by amateur radio operators, to transmit and receive static pictures via radio in monochrome or color.

A literal term for SSTV is narrowband television. Analog broadcast television requires at least 6 MHz wide channels, because it transmits 25 or 30 picture frames per second (see ITU analog broadcast standards), but SSTV usually only takes up to a maximum of 3 kHz of bandwidth. It is a much slower method of still picture transmission, usually taking from about eight seconds to a couple of minutes, depending on the mode used, to transmit one image frame.

Since SSTV systems operate on voice frequencies, amateurs use it on shortwave (also known as HF by amateur radio operators), VHF and UHF radio.

## Current systems

A modern system, having gained ground since the early 1990s, uses a personal computer and special software in place of much of the custom equipment. The sound card of a PC, with special processing software, acts as a modem. The computer screen provides the output. A small digital camera or digital photos provides the input.

### Modulation

Like the similar radiofax mode, SSTV is an analog signal. SSTV uses frequency modulation, in which every different value of brightness in the image gets a different audio frequency. In other words, the signal frequency shifts up or down to designate brighter or darker pixels, respectively. Color is achieved by sending the brightness of each color component (usually red, green and blue) separately. This signal can be fed into an SSB transmitter, which in part modulates the carrier signal.

There are a number of different modes of transmission, but the most common ones are *Martin M1* (popular in Europe) and *Scottie S1* (used mostly in the USA). Using one of these, an image transfer takes 114 (M1) or 110 (S1) seconds. Some black and white modes take only 8 seconds to transfer an image.

### Header

A calibration header is sent before the image. It consists of a 300-millisecond leader tone at 1,900 Hz, a 10 ms break at 1,200 Hz, another 300-millisecond leader tone at 1,900 Hz, followed by a digital VIS (vertical interval signaling) code, identifying the transmission mode used. The VIS consists of bits of 30 milliseconds in length. The code starts with a start bit at 1,200 Hz, followed by 7 data bits (LSB first; 1,100 Hz for 1, 1,300 Hz for 0). An even parity bit follows, then a stop bit at 1,200 Hz. For example, the bits corresponding the decimal numbers 44 or 32 imply that the mode is Martin M1, whereas the number 60 represents Scottie S1.

### Scanlines

A transmission consists of horizontal lines, scanned from left to right. The color components are sent separately one line after another. The color encoding and order of transmission can vary between modes. Most modes use an RGB color model; some modes are black-and-white, with only one channel being sent; other modes use a YC color model, which consists of luminance (Y) and chrominance (R–Y and B–Y). The modulating frequency changes between 1,500 and 2,300 Hz, corresponding to the intensity (brightness) of the color component. The modulation is analog, so even though the horizontal resolution is often defined as 256 or 320 pixels, they can be sampled using any rate. The image aspect ratio is conventionally 4:3. Lines usually end in a 1,200 Hz horizontal synchronization pulse of 5 milliseconds (after all color components of the line have been sent); in some modes, the synchronization pulse lies in the middle of the line.

### Modes

Below is a table of some of the most common SSTV modes and their differences. These modes share many properties, such as synchronization and/or frequencies and grey/color level correspondence. Their main difference is the image quality, which is proportional to the time taken to transfer the image and in the case of the AVT modes, related to synchronous data transmission methods and noise resistance conferred by the use of interlace.

| Family | Developer | Name | Color | Time | Lines |
|---|---|---|---|---|---|
| AVT | Ben Blish-Williams, AA7AS / AEA | 8 | BW or 1 of R, G, or B | 8 s | 128×128 |
| AVT | Ben Blish-Williams, AA7AS / AEA | 16w | BW or 1 of R, G, or B | 16 s | 256×128 |
| AVT | Ben Blish-Williams, AA7AS / AEA | 16h | BW or 1 of R, G, or B | 16 s | 128×256 |
| AVT | Ben Blish-Williams, AA7AS / AEA | 32 | BW or 1 of R, G, or B | 32 s | 256×256 |
| AVT | Ben Blish-Williams, AA7AS / AEA | 24 | RGB | 24 s | 128×128 |
| AVT | Ben Blish-Williams, AA7AS / AEA | 48w | RGB | 48 s | 256×128 |
| AVT | Ben Blish-Williams, AA7AS / AEA | 48h | RGB | 48 s | 128×256 |
| AVT | Ben Blish-Williams, AA7AS / AEA | 104 | RGB | 96 s | 256×256 |
| Martin | Martin Emmerson - G3OQD | M1 | RGB | 114 s | 240¹ |
| Martin | Martin Emmerson - G3OQD | M2 | RGB | 58 s | 240¹ |
| Robot | Robot SSTV | 8 | BW or 1 of R, G or B | 8 s | 120 |
| Robot | Robot SSTV | 12 | YUV | 12 s | 128 luma, 32/32 chroma × 120 |
| Robot | Robot SSTV | 24 | YUV | 24 s | 128 luma, 64/64 chroma × 120 |
| Robot | Robot SSTV | 32 | BW or 1 of R, G or B | 32 s | 256 × 240 |
| Robot | Robot SSTV | 36 | YUV | 36 s | 256 luma, 64/64 chroma × 240 |
| Robot | Robot SSTV | 72 | YUV | 72 s | 256 luma, 128/128 chroma × 240 |
| Scottie | Eddie Murphy - GM3SBC | S1 | RGB | 110 s | 240¹ |
| Scottie | Eddie Murphy - GM3SBC | S2 | RGB | 71 s | 240¹ |
| Scottie | Eddie Murphy - GM3SBC | DX | RGB | 269 s | 320 x 256 |

¹ Martin and Scottie modes actually send 256 scanlines, but the first 16 are usually grayscale.

The mode family called AVT (for *Amiga Video Transceiver*) was originally designed by Ben Blish-Williams (N4EJI, then AA7AS) for a custom modem attached to an Amiga computer, which was eventually marketed by AEA corporation.

The Scottie and Martin modes were originally implemented as ROM enhancements for the Robot Research Corporation SSTV unit. The exact line timings for the Martin M1 mode are given in this reference.

The Robot SSTV modes were designed by Robot Research Corporation for their own SSTV units.

All four sets of SSTV modes are now available in various PC-resident SSTV systems and no longer depend upon the original hardware.

#### AVT

AVT is an abbreviation of "Amiga Video Transceiver", software and hardware modem originally developed by "Black Belt Systems" (USA) around 1990 for the Amiga home computer popular all over the world before the IBM PC family gained sufficient audio quality with the help of special sound cards. These AVT modes differ radically from the other modes mentioned above, in that they are synchronous, that is, they have no per-line horizontal synchronization pulse but instead use the standard VIS vertical signal to identify the mode, followed by a frame-leading digital pulse train which pre-aligns the frame timing by counting first one way and then the other, allowing the pulse train to be locked in time at any single point out of 32 where it can be resolved or demodulated successfully, after which they send the actual image data, in a fully synchronous and typically interlaced mode.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Current systems.)_
