---
id: radio/wp2-twin-lead
title: Twin-lead
category: radio
tags: [twin-lead, twin lead, ribbon cable, parallel wire line, radio]
summary: Twin lead cable is a two-conductor flat cable used as a balanced transmission line to carry radio frequency (RF) signals.
source: https://en.wikipedia.org/w/index.php?title=Twin-lead&oldid=1374799441
source_title: Wikipedia: Twin-lead
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Twin-lead", Wikipedia, The Free Encyclopedia (revision 1374799441), https://en.wikipedia.org/w/index.php?title=Twin-lead&oldid=1374799441 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Twin-lead

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Twin lead** cable is a two-conductor flat cable used as a balanced transmission line to carry radio frequency (RF) signals. It is constructed of two, stranded copper wires, or solid copper-clad steel wires. The wires are held a fixed distance apart by a plastic ribbon that is a good insulator at radio frequencies (usually polyethylene). It is also called (two wire) **ribbon cable**. The uniform spacing of the wires is the key to the cable's function as a transmission line: Any abrupt change in spacing would cause some of the signal to reflect back toward the source, rather than passing through. The plastic also covers and insulates the wires.

The name **twin lead** is most often used to refer specifically to 300 Ω (Ohm) ribbon cable, the most common type, but on occasion, *twin lead* is used to refer to any type of **parallel wire line**. Parallel wire line is available with several different values of characteristic impedance such as *twin lead ribbon cable* (75 or 300 Ω), *window line* (300 Ω, 350 Ω, or 450 Ω), and *open wire line* or *ladder line* (500~650 Ω).

Twin lead is mainly used as an antenna feedline at shortwave and VHF frequencies, to connect radio receivers and transmitters to their antennas. It can have significantly lower signal loss than flexible coaxial cable, the main alternative type of feedline at these frequencies; for example, type RG-58 coaxial cable loses 6.6 dB per 100 m at 30 MHz, while 300 Ω twin-lead loses only 0.55 dB. 300 Ω twin lead is widely used to connect FM radios to their antennas, and was previously used to connect television antennas to televisions until it was replaced by coaxial cable.

The reason for the fallen popularity of parallel-wire cable came about in part because of an influx of inexpensive military surplus coaxial cable, which is far easier to use: Parallel-wire cable is unshielded, so much more vulnerable to interference; proximity to metal objects will inject signals into any type of parallel wire line that would be blocked out by more convenient / more popular shielded coaxial cable. Those same metal objects will also distort the impedance of unshielded cable running near them, causing some signal power to be reflected backwards from that point along the line. Twin-lead therefore requires wide routing around rain gutters, and spacing away from metal fences, exterior wall siding, and metal roofs, and to be mounted on 1~2 foot standoff insulators when run up the sides of metal antenna masts.

## Characteristics and uses

Twin lead and other types of parallel-conductor transmission line are mainly used to connect radio transmitters and receivers to their antennas. Parallel transmission line has the advantage that its losses per unit length are an order of magnitude smaller than that of coaxial cable, the main alternative form of transmission line. Its disadvantages are that it is more vulnerable to interference, and must be kept away from metal objects which can cause power losses and impedance distortion (hence back-reflected waves). For this reason, when installed along the outside of buildings and on antenna masts, standoff insulators must be used. It is also common practice to twist the twin lead on long free standing lengths to further average out any imbalance induced on the line.

**Parallel-wire line** (the generic sense of *twin lead*) is supplied in several different sizes, with characteristic impedance values of
- 600 Ω, 500 Ω (ladder line),
- 450 Ω, 350 Ω (window line),
- 300 Ω (window line, ribbon cable), and
- 75 Ω (ribbon cable).
300 Ω twin-lead ribbon cable was the most common type in the middle-20th century, when it was widely used to connect television sets and FM radios to their receiving antennas. Since the last quarter of the middle-20th century, 300 Ω twin lead ribbon cable has been largely replaced with 75 Ω coaxial cable for television installations. Multiple forms of parallel wire line are used in amateur radio stations as feedline for balanced transmission of radio frequency signals, most often as 450 Ω window line, instead of twin lead ribbon cable.

The characteristic impedance of twin lead is a function of the insulating material and its thickness, and the wire diameter and its spacing; in the most common type, 300 Ω twin-lead ribbon cable, the wire is usually AWG 20 or 22 (0.52 or 0.33 mm²), about 7.5 mm apart.
This is well matched with the natural impedance of a folded dipole antenna, which is normally around 275 Ω. Twin lead generally has higher impedance than the other common transmission wiring, coaxial cable (coax). The widely used RG-6 coax has a characteristic impedance of 75 Ω, which requires the use of a balun to match impedance when used with common antenna types.

## How it works

**Twin lead** (in the specific sense of ribbon cable) is a form of parallel wire balanced transmission line. The separation between the two wires in twin-lead is small compared to the wavelength of the radio frequency (RF) signal carried on the wire. The RF current in one wire is equal in magnitude and opposite in direction to the RF current in the other wire. Therefore, in the far field region far from the transmission line, the radio waves radiated by one wire are equal in magnitude but opposite in phase (180° out of phase) to the waves radiated by the other wire, so the overlapping opposite waves cancel each other out. The result is that almost no net radio energy is radiated by the line.

Similarly, any interfering external radio waves will induce equal, in phase RF currents, traveling in the same direction, in the two wires, as long as they each maintain the same impedance. Since the load at the destination end is connected *across* the wires, nominally, only *differential*, oppositely-directed currents in the wires create a current in the load. Thus the interfering currents are canceled out, so twin lead does not tend to pick up radio noise.

However, if a piece of metal is located sufficiently close to a twin-lead line, within a distance comparable to the wire spacing, it will be significantly closer to one wire than the other. As a result, the RF current induced in the metal object by one wire will be greater than the opposing current induced by the other wire, so the currents will no longer cancel. Thus nearby metal objects can cause power losses in twin lead lines, through energy dissipated as heat by induced currents. Similarly, radio noise originating in cables or metal objects located near the twin-lead line can induce unbalanced currents in the wires, coupling noise into the line. Therefore, the line must be kept at a distance from metal objects such as metal roofs or siding, house wiring, gutters, and antenna masts.

In order to prevent power from being reflected from the load end of the line, causing high SWR and inefficiency, the load must have an impedance which matches the characteristic impedance of the line. This causes the load to appear electrically identical to a continuation of the line, preventing reflections. Similarly, to transfer power efficiently into the line, the source must also match the characteristic impedance. To connect a balanced transmission line to an unbalanced line like coaxial cable, a device called a balun must be used.

## Antennas

Twin-lead can be connected directly to a suitably designed antenna:

**Windom antenna A multiply-resonant antenna whose resonant impedances cluster around 300 Ω.Folded dipole Double-wire dipole whose characteristic impedance in free space is around 300 Ω.Dipole Although the center impedance at resonance is approximately 73 Ω in free space, in actual use it varies between 30 Ω, depending on height above ground, so with high-impedance feedline a T-match or Y-match feed will probably be necessary.Yagi–Uda antenna Yagi and the simpler Moxon antenna, and other directional antennas; some special impedance matching arrangement at the feedpoint is necessary for any cabling, since interference between the typically close-spacing of the parallel near-resonant antenna segments causes low feedpoint resistance as well as making the antenna more directional.**

_(Later sections of the original article were left out of this offline copy for length: Window line and ladder line, Impedance matching, Characteristic impedance.)_
