---
id: radio/wp2-earthmoonearth-communication
title: Earth–Moon–Earth communication
category: radio
tags: [earth–moon–earth communication, eme, moon bounce, radio]
summary: Earth–Moon–Earth communication (EME), also known as Moon bounce, is a radio communications technique that relies on the propagation of radio waves from an Earth-based transmitter directed via reflection from the surface of the Moon back to an Earth-based receiver.
source: https://en.wikipedia.org/w/index.php?title=Earth%E2%80%93Moon%E2%80%93Earth_communication&oldid=1378191492
source_title: Wikipedia: Earth–Moon–Earth communication
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Earth–Moon–Earth communication", Wikipedia, The Free Encyclopedia (revision 1378191492), https://en.wikipedia.org/w/index.php?title=Earth%E2%80%93Moon%E2%80%93Earth_communication&oldid=1378191492 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Earth–Moon–Earth communication

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Earth–Moon–Earth communication** (**EME**), also known as **Moon bounce**, is a radio communications technique that relies on the propagation of radio waves from an Earth-based transmitter directed via reflection from the surface of the Moon back to an Earth-based receiver.

## Current EME communications

Amateur radio (ham) operators utilize EME for two-way communications. EME presents significant challenges to amateur operators interested in weak signal communication. EME provides the longest communications path that any two stations on Earth can use.

Amateur frequency bands from 50 MHz (6 meter wavelength) to 47 GHz (6.38 mm) have been used successfully, but most EME communications are on the 2 meter, 70-centimeter, or 23-centimeter bands. Common modulation modes are continuous wave with Morse code, digital (JT65, Q65, FT8) and when the link budgets allow, voice.

Recent advances in digital signal processing have allowed EME contacts, admittedly with low data rate, to take place with powers in the order of 100 watts and a single Yagi–Uda antenna.

World Moon Bounce Day, June 29, 2009, was created by Echoes of Apollo and celebrated worldwide as an event preceding the 40th anniversary of the Apollo 11 Moon landing. A highlight of the celebrations was an interview via the Moon with Apollo 8 astronaut Bill Anders, who was also part of the backup crew for Apollo 11. The University of Tasmania in Australia with their 26-meter (85') dish were able to bounce a data signal off the surface of the Moon which was received by a large dish in the Netherlands, Dwingeloo Radio Observatory. The data signal was successfully resolved back to data setting a world record for the lowest power data signal returned from the Moon with a transmit power of 3 milliwatts, about 1,000th of the power of a flashlight lamp. The second World Moon Bounce Day was April 17, 2010, coinciding with the 40th anniversary of the conclusion of the Apollo 13 mission.

In October 2009 media artist Daniela de Paulis proposed to the CAMRAS radio amateur association based at the Dwingeloo Radio Observatory to use Moon bounce for a live image transmission performance. As a result of her proposal, in December 2009 CAMRAS radio operator Jan van Muijlwijk and radio operator Daniel Gautchi made the first image transmission via the Moon using the open source software MMSSTV. De Paulis called the innovative technology "Visual Moonbounce" and since 2010 she has been using it in several of her art projects, including the live performance called OPTICKS, during which digital images are sent to the Moon and back in real time and projected live.

## Echo delay and time spread

Radio waves propagate in vacuum at the speed of light *c*, exactly 299,792,458 m/s. Propagation time to the Moon and back ranges from 2.4 to 2.7 seconds, with an average of 2.56 seconds (the average distance from Earth to the Moon is 384,400 km).

The Moon is nearly spherical, and its radius corresponds to about 5.8 milliseconds of wave travel time. The trailing parts of an echo, reflected from irregular surface features near the edge of the lunar disk, are delayed from the leading edge by as much as twice this value.

Most of the Moon's surface appears relatively smooth at the typical microwave wavelengths used for amateur EME. Most amateurs do EME contacts below 6 GHz, and differences in the moon's reflectivity are somewhat hard to discern above 1 GHz.

Lunar reflections are by nature quasi-specular (like those from a shiny ball bearing). The power useful for communication is mostly reflected from a small region near the center of the disk. The effective time spread of an echo amounts to no more than 0.1 ms.

Antenna polarization for EME stations must consider that reflection from a smooth surface preserves linear polarization but reverses the sense of circular polarizations.

At shorter wavelengths the lunar surface appears increasingly rough, so reflections at 10 GHz and above contain a significant diffuse component as well as a quasi-specular component. The diffuse component is depolarized, and can be viewed as a source of low level system noise. Significant portions of the diffused component arise from regions farther out toward the lunar rim. The median time spread can then be as much as several milliseconds. In all practical cases, however, time spreading is small enough that it does not cause significant smearing of CW keying or intersymbol interference in the slowly keyed modulations commonly used for digital EME. The diffused component may appear as significant noise at higher message data rates.

EME time spreading does have one very significant effect. Signal components reflected from different parts of the lunar surface travel different distances and arrive at Earth with random phase relationships. As the relative geometry of the transmitting station, receiving station and reflecting lunar surface changes, signal components sometimes add and sometimes cancel, depending on their phase relationship, creating large amplitude fluctuations in the received signal. These "libration fading" amplitude variations are well correlated over the coherence bandwidth (typically a few kHz). The libration fading components are related to the time spread of reflected signals.

## Modulation types and frequencies for EME

VHF

- CW
- JT65A, JT65B, Q65

UHF

- CW
- JT65, Q65
- SSB

Microwave

- CW
- SSB
- Q65

## Other factors influencing EME communications

The Doppler shift at 144 MHz is 300 Hz at moonrise and moonset. The Doppler offset reduces to around zero when the Moon is overhead. At other frequencies other Doppler offsets exist. At moonrise, returned 144 MHz signals are shifted approximately 300 Hz higher. As the Moon traverses the sky to a point due south or due north, the Doppler effect approaches zero. By moonset, it is shifted 300 Hz lower. The Doppler effect causes many problems when tuning into, and locking onto, signals from the Moon.

Polarization effects can reduce the strength of received signals. One component is the geometrical alignment of the transmitting and receiving antennas. Many antennas produce a preferred plane of polarization. Transmitting and receiving station antennas may not be aligned from the perspective of an observer on the moon. This component is fixed by the alignment of the antennas and stations may include a facility to rotate antennas to adjust polarization. Another component is Faraday rotation on the Earth–Moon–Earth path. The plane of polarization of radio waves rotates as they pass through ionized layers of the Earth's atmosphere. This effect is more pronounced at lower VHF frequencies and becomes less significant at and above. Some of the polarization mismatch loss can be reduced by using a larger antenna array (more Yagi elements or a larger dish).
