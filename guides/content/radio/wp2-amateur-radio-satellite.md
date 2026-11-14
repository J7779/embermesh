---
id: radio/wp2-amateur-radio-satellite
title: Amateur radio satellite
category: radio
tags: [amateur radio satellite, oscar, radio]
summary: An amateur radio satellite is an artificial satellite built and used by amateur radio operators.
source: https://en.wikipedia.org/w/index.php?title=Amateur_radio_satellite&oldid=1378695231
source_title: Wikipedia: Amateur radio satellite
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Amateur radio satellite", Wikipedia, The Free Encyclopedia (revision 1378695231), https://en.wikipedia.org/w/index.php?title=Amateur_radio_satellite&oldid=1378695231 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Amateur radio satellite

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **amateur radio satellite** is an artificial satellite built and used by amateur radio operators. It forms part of the Amateur-satellite service. These satellites use amateur radio frequency allocations to facilitate communication between amateur radio stations.

Many amateur satellites receive an **OSCAR** designation, which is an acronym for **Orbiting Satellite Carrying Amateur Radio**. The designation is assigned by AMSAT, an organization which promotes the development and launch of amateur radio satellites. Because of the prevalence of this designation, amateur radio satellites are often referred to as OSCARs.

These satellites can be used free of charge by licensed amateur radio operators for voice (FM, SSB) and data (AX.25, packet radio, APRS) communications. Currently, over 18 fully operational amateur radio satellites are in orbit. They may be designed to act as repeaters, as linear transponders, and as store and forward digital relays.

Amateur radio satellites have helped advance the science of satellite communications. Contributions include the launch of the first satellite voice transponder (OSCAR 3) and the development of highly advanced digital "store-and-forward" messaging transponder techniques.

The Amateur Radio Satellite community is very active in building satellites and in finding launch opportunities. Lists of functioning satellites need updating regularly, as new satellites are launched and older ones fail. Current information is published by AMSAT. AMSAT has not been actively involved in the launch and operation of most amateur satellites in the last two decades beyond allocating an OSCAR number.

## Hardware

The first amateur satellites contained telemetry beacons. Since 1965, most OSCARs carry a linear transponder for two-way communications in real time. Some satellites have a bulletin board for store-and-forward digital communications, or a digipeater for direct packet radio connections.

## Orbits

Amateur satellites have been launched into low Earth orbits and into highly elliptical orbits.

## Operations

### Satellite communications

Currently, amateur satellites support many different types of operation, including FM voice and SSB voice, as well as digital communications of AX.25 FSK (Packet radio) and PSK-31.

#### Mode designators

Uplink and downlink designations use sets of paired letters following the structure **X/Y** where **X** is the uplink band and **Y** is the downlink band. Occasionally, the downlink letter is rendered in lower case (i.e., **X/y**). With a few exceptions, the letters correspond to IEEE's standard for radar frequency letter bands:

|**H**
|**A**
|**V**
|**U**
|**L**
|**S**
|**S2**
|**C**
|**X**
|**K**
|**R**
|15 m
|10 m
|2 m
|70 cm
|23 cm
|13 cm
|9 cm
|5 cm
|3 cm
|1.2 cm
|6 mm
(General)
|21 MHz
|29 MHz
|145 MHz
|435 MHz
|1.2 GHz
|2.4 GHz
|3.4 GHz
|5 GHz
|10 GHz
|24 GHz
|47 GHz

Prior to the launch of OSCAR 40, operating modes were designated using single letters to indicate both uplink and downlink bands. While deprecated, these older mode designations are still widely used in casual conversation.

 - Mode A: 2 m uplink / 10 m downlink
 - Mode B: 70 cm uplink / 2 m downlink
 - Mode J: 2 m uplink / 70 cm downlink

#### Doppler shift

Due to the high orbital speed of the amateur satellites, the uplink and downlink frequencies will vary during the course of a satellite pass. This phenomenon is known as the Doppler effect. While the satellite is moving towards the ground station, the downlink frequency will appear to be *higher* than normal. Hence, the receiver frequency at the ground station must be adjusted *higher* to continue receiving the satellite. The satellite in turn, will be receiving the uplink signal at a *higher* frequency than normal so the ground station's transmitted uplink frequency must be *lower* to be received by the satellite. After the satellite passes overhead and begins to move away, this process is reversed. The downlink frequency will appear *lower* and the uplink frequency will need to be adjusted *higher*. The following mathematical formulas relate the Doppler shift to the velocity of the satellite.

| Where: | | |
|---|---|---|
| f_d | = | doppler corrected downlink frequency |
| f_u | = | doppler corrected uplink frequency |
| f | = | original frequency |
| v | = | velocity of the satellite relative to ground station in m/s.; Positive when moving towards, negative when moving away. |
| c | = | the speed of light in a vacuum (3×10^8 m/s). |

| Change in frequency | Downlink Correction | Uplink Correction |
|---|---|---|
| Δ f=f×v/c | f_d=f(1+v/c) | f_u=f(1-v/c) |

Due to the complexity of finding the relative velocity of the satellite and the speed with which these corrections must be made, these calculations are normally accomplished using satellite tracking software. Many modern transceivers include a computer interface that allows for automatic doppler effect correction. Manual frequency-shift correction is possible, but it is difficult to remain precisely near the frequency. Frequency modulation is more tolerant of doppler shifts than single-sideband, and therefore FM is much easier to tune manually.

### FM satellites

A number of low Earth orbit (LEO) OSCAR satellites use frequency modulation (FM). These are also commonly referred to as "FM LEOs" or the "FM Birds". Such satellites act as FM amateur radio repeaters that can be communicated through using commonly available amateur radio equipment. Communication can be achieved with handheld transceivers using manual doppler correction. Satellite passes are typically less than 15 minutes long.

## Facts

### Multinational effort

Currently, 30 countries have constructed a launched OSCAR satellite. These countries, in chronological order by date of launch, include:

_(Later sections of the original article were left out of this offline copy for length: Launches, International regulation.)_
