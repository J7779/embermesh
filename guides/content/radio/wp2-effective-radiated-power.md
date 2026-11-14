---
id: radio/wp2-effective-radiated-power
title: Effective radiated power
category: radio
tags: [effective radiated power, erp, equivalent radiated power, effective isotropic radiated power, eirp, radio]
summary: Effective radiated power (ERP), synonymous with equivalent radiated power, is an IEEE standardized definition of directional radio frequency (RF) power, such as that emitted by a radio transmitter.
source: https://en.wikipedia.org/w/index.php?title=Effective_radiated_power&oldid=1385588769
source_title: Wikipedia: Effective radiated power
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Effective radiated power", Wikipedia, The Free Encyclopedia (revision 1385588769), https://en.wikipedia.org/w/index.php?title=Effective_radiated_power&oldid=1385588769 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Effective radiated power

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Effective radiated power** (**ERP**), synonymous with **equivalent radiated power**, is an IEEE standardized definition of directional radio frequency (RF) power, such as that emitted by a radio transmitter. It is used in electronics and telecommunications, particularly in broadcasting to quantify the apparent power of a broadcasting station experienced by listeners in its reception area.

ERP is the total power that would have to be radiated by a half-wave dipole antenna to give the same radiation intensity (signal strength, or power flux density, expressed as power per area) as the actual source antenna, measured at a distant receiver located in the direction of the antenna's strongest beam (main lobe). ERP measures the combination of the power emitted by the transmitter and the ability of the antenna to direct that power in a given direction. It is equal to the input power to the antenna multiplied by the gain of the antenna.

An alternate parameter that measures the same thing is **effective isotropic radiated power** (**EIRP**). Effective isotropic radiated power is the hypothetical power that would have to be radiated by an isotropic antenna to give the same ("equivalent") signal strength as the actual source antenna in the direction of the antenna's strongest beam. The difference between EIRP and ERP is that ERP compares the actual antenna to a half-wave dipole antenna, while EIRP compares it to a theoretical isotropic antenna. Since a half-wave dipole antenna has a gain of about 1.64 (or about 2.15 dB) compared to an isotropic radiator, if ERP and EIRP are expressed as power their relation is
EIRP_(W) ≈ 1.64 × ERP_(W)\
If they are expressed in decibels
EIRP_(dBm) ≈ ERP_(dBm) + 2.15 dB\

## Definitions

Effective radiated power and effective isotropic radiated power both measure the power density a radio transmitter and antenna (or other source of electromagnetic waves) radiate in a specific direction: in the direction of maximum signal strength (the "main lobe") of its radiation pattern.
This apparent power is dependent on two factors: the total power output and the radiation pattern of the antenna – how much of that power is radiated in the direction of maximal intensity. The latter factor is quantified by the antenna gain, which is the ratio of the signal strength radiated by an antenna in its direction of maximum radiation to that radiated by some *necessarily named* standard antenna. (Without specifying a standard for comparison there can be no ratio.) For example, a 1,000 watt transmitter feeding an antenna with a gain of 4× compared to a theoretical isotropic antenna, or about 6 dBi, will radiate the same power in the direction of its main lobe, and thus the same EIRP, as a 4,000 watt transmitter feeding the theoretical isotropic antenna radiates in all directions equally. So ERP and EIRP are measures of radiated power that can compare different combinations of transmitters and antennas on an equal basis.

In spite of the names, ERP and EIRP do not measure transmitter power or total power radiated by the antenna; they are just measures of signal strength along the main lobe. They give no information about power radiated in other directions or total power. ERP and EIRP are always greater than the actual total power radiated by the antenna.

The difference between ERP and EIRP is that antenna gain has traditionally been measured in two different units, comparing the antenna to two different standard antennas, both theoretical; an isotropic antenna and a (perfect, lossless) half-wave dipole:

- *Isotropic gain* is the ratio of the power density (signal strength, in power per area) received at a point far from the antenna (i.e. in its far field) in the direction of its maximum radiation (i.e. its main lobe), S_max\, to the power density received at the same point from a hypothetical lossless isotropic antenna, which radiates equally in all directions, S_max,iso\: G_i = ( S_max )/( S_max,iso )\ Gain is often expressed in logarithmic units of decibels (dB). The gain relative to an isotropic antenna and expressed in decibels, dBi, is given by
G_(dB_i) = 10 log_10( ( S_max )/( S_max,iso ) )\

- *Dipole gain* is the ratio of the power density (signal strength, in power per area) received at a point far from the antenna (i.e. in its far field) in the direction of its maximum radiation (i.e. its main lobe), S_max\, to the power density received at the same point from a hypothetical lossless half-wave dipole antenna, S_max,dipole:G_d = ( S_max )/( S_max,dipole )\ The gain relative to a half-wave dipole antenna and expressed in decibels, dBd, is given by G_(dB_d) = 10 log_10( ( S_max )/( S_max,dipole ) )\
In contrast to an isotropic antenna, the dipole has a "donut-shaped" radiation pattern; its radiated power is maximum in directions perpendicular to the antenna, declining to zero on the antenna axis. Since the radiation of the dipole is concentrated in horizontal directions (assuming the antenna axis is vertical), the gain of a half-wave dipole is greater than that of an isotropic antenna. The isotropic gain of a half-wave dipole is about 1.64, or, in decibels, 10 log_10(1.64) ≈ 2.15 dB, so
G_i ≈ 1.64 G_d.
In decibels
G_(dB_i) ≈ G_(dB_d) + 2.15 dB.

The two measures EIRP and ERP are based on the two different standard antennas above:

- EIRP is defined as the RMS power input required to a theoretical lossless isotropic antenna to radiate the same maximum power density far from the antenna as the actual transmitter and antenna do in the direction of greatest power (i.e. the main lobe). It is equal to the power input to the transmitter's antenna multiplied by the antenna gain relative to isotropic EIRP = G_i P_in. The ERP and EIRP are also often expressed in decibels (dB). The input power in decibels is usually calculated with comparison to a reference level of one watt (W): P_in (dB_W) = 10 log_10 P_in. Since multiplication of two factors is equivalent to addition of their decibel values EIRP_(dB_W) = G_(dB_i) + P_in (dB_W)\

- ERP is defined as the RMS power input required to a theoretical lossless half-wave dipole to give the same maximum power density far from the antenna as the actual transmitter. It is equal to the power input to the transmitter's antenna multiplied by the antenna gain relative to a (theoretical, perfect, lossless) half-wave dipole: ERP = G_d P_in. In decibels ERP_(dB_W) = G_(dB_d) + P_in (dB_W).

Since the two definitions of gain only differ by a constant factor, so do ERP and EIRP
EIRP_(W) ≈ 1.64 × ERP_(W).
In decibels
EIRP_(dB_W) ≈ ERP_(dB_W) + 2.15 dB.

## Relation to transmitter output power

The transmitter is usually connected to the antenna through a number of different parts such as a filter, transmission line and impedance matching network. Since these components may have significant losses L, the power applied to the antenna is usually less than the output power of the transmitter P_TX. The relation of ERP and EIRP to transmitter output power is
EIRP_(dB_W) = P_TX (dB_W) - L_(dB) + G_(dB_i),
ERP_(dB_W) = P_TX (dB_W) - L_(dB) + G_(dB_i) - 2.15 dB.
Losses in the antenna itself are included in the gain.

## Relation to signal strength

If the signal path is in free space (line-of-sight propagation with no multipath) the signal strength (power flux density in watts per square meter) S\ of the radio signal on the main lobe axis at any particular distance r from the antenna can be calculated from the EIRP or ERP. Since an isotropic antenna radiates equal power flux density over a sphere centered on the antenna, and the area of a sphere with radius r\ is A = 4π r^2\ then
S(r) = ( EIRP )/( 4π r^2 ).
Since EIRP = ERP × 1.64,
S(r) = ( 0.410 × ERP )/( π r^2 ).
After dividing out the factor of π, we get:
S(r) = ( 0.131 × ERP )/( r^2 ).

However, if the radio waves travel by ground wave as is typical for medium or longwave broadcasting, skywave, or indirect paths play a part in transmission, the waves will suffer additional attenuation which depends on the terrain between the antennas, so these formulas are not valid.

_(Later sections of the original article were left out of this offline copy for length: Dipole vs. isotropic radiators, Polarization, FM example, Microwave band issues, Lower-frequency issues, HAAT.)_
