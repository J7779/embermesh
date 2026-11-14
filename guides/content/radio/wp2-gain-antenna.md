---
id: radio/wp2-gain-antenna
title: Gain (antenna)
category: radio
tags: [gain, radio]
summary: In electromagnetics, an antenna's gain is a key performance parameter which combines the antenna's directivity and radiation efficiency.
source: https://en.wikipedia.org/w/index.php?title=Gain_(antenna)&oldid=1385588929
source_title: Wikipedia: Gain (antenna)
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Gain (antenna)", Wikipedia, The Free Encyclopedia (revision 1385588929), https://en.wikipedia.org/w/index.php?title=Gain_(antenna)&oldid=1385588929 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Gain (antenna)

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electromagnetics, an antenna's **gain** is a key performance parameter which combines the antenna's directivity and radiation efficiency. The term *power gain* has been deprecated by IEEE. In a transmitting antenna, the gain describes how well the antenna converts input power into radio waves headed in a specified direction. In a receiving antenna, the gain describes how well the antenna converts radio waves arriving from a specified direction into electrical power.

When no direction is specified, gain is understood to refer to the peak value of the gain, the gain in the direction of the antenna's main lobe. A plot of the gain as a function of direction is called the antenna pattern or radiation pattern. It is not to be confused with directivity, which does not take an antenna's radiation efficiency into account.

Gain or 'absolute gain' is defined as "The ratio of the radiation intensity in a given direction to the radiation intensity that would be produced if the power accepted by the antenna were isotropically radiated". Usually this ratio is expressed in decibels with respect to an isotropic radiator (dBi). An alternative definition compares the received power to the power received by a lossless half-wave dipole antenna, in which case the units are written as "dBd". Since a lossless dipole antenna has a gain of 2.15 dBi, the relation between these units is Gain(dBd) ≈ Gain(dBi) − 2.15. For a given frequency, the antenna's effective area is proportional to the gain. An antenna's effective length is proportional to the *square root* of the antenna's gain for a particular frequency and radiation resistance. Due to reciprocity, the gain of any antenna when receiving is equal to its gain when transmitting.

## Gain

**Gain** is a unitless measure that multiplies an antenna's radiation efficiency η and directivity D:
G = η D

## Radiation efficiency

The radiation efficiency η of an antenna is "The ratio of the total power radiated by an antenna to the net power accepted by the antenna from the connected transmitter."
η = P_R over P_O

A transmitting antenna is supplied with power by a transmission line connecting the antenna to a radio transmitter. The **power accepted by the antenna** P_O is the power supplied to the antenna's terminals. Losses prior to the antenna terminals are accounted for by separate impedance mismatch factors which are therefore not included in the calculation of radiation efficiency.

## Gain in decibels

Published numbers for antenna gain are almost always expressed in decibels (dB), a logarithmic scale. From the gain factor G, one finds the gain in decibels as:
G_dBi = 10 log_10(G).

Therefore, an antenna with a peak power gain of 5 would be said to have a gain of 7 dBi. 'dBi' is used rather than just 'dB' to emphasize that this is the gain according to the basic definition, in which the antenna is compared to an isotropic radiator.

When actual measurements of an antenna's gain are made by a laboratory, the field strength of the test antenna is measured when supplied with, say, 1 watt of transmitter power, at a certain distance. That field strength is compared to the field strength found using a so-called *reference antenna* at the same distance receiving the same power in order to determine the gain of the antenna under test. That ratio would be equal to G if the reference antenna were an isotropic radiator ("irad").

However a true isotropic radiator cannot be built, so in practice a different antenna is used. This will often be a half-wave dipole, a very well understood and repeatable antenna that can be easily built for any frequency. The directive gain of a half-wave dipole with respect to the isotropic radiator is known to be 1.64 and it can be made nearly 100% efficient. Since the gain has been measured with respect to this reference antenna, the difference in the gain of the test antenna is often compared to that of the dipole. The gain relative to a dipole is thus often quoted and is denoted using 'dBd' instead of 'dBi' to avoid confusion. Therefore, in terms of the true gain (relative to an isotropic radiator) G, this figure for the gain is given by:
G_dBd ≈ 10 log_10(G/1.64).

For instance, the above antenna with a gain G = 5 would have a gain with respect to a dipole of 5/1.64 ≈ 3.05, or in decibels one would call this 10 log(3.05) ≈ 4.84 dBd. In general:
G_dBd ≈ G_dBi - 2.15 dB

Both dBi and dBd are in common use. When an antenna's maximum gain is specified in decibels (for instance, by a manufacturer) one must be certain as to whether this means the gain relative to an isotropic radiator or with respect to a dipole. If it specifies dBi or dBd then there is no ambiguity, but if only dB is specified then the fine print must be consulted. Either figure can be easily converted into the other using the above relationship.

When considering an antenna's directional pattern, gain with respect to a dipole does *not* imply a comparison of that antenna's gain in each direction to a dipole's gain in that direction. Rather, it is a comparison between the antenna's gain in each direction to the *peak* gain of the dipole (1.64). In any direction, therefore, such numbers are 2.15 dB smaller than the gain expressed in dBi.

## Partial gain

**Partial gain** is calculated as power gain, but for a particular polarization. It is defined as the part of the radiation intensity U corresponding to a given polarization, divided by the total radiation intensity of an isotropic antenna.

The partial gains in the θ and φ components are expressed as
G_θ = 4π((U_θ)/(P_in))
and
G_φ = 4π((U_φ)/(P_in)),

where U_θ and U_φ represent the radiation intensity in a given direction contained in their respective E field component.

As a result of this definition, we can conclude that the total gain of an antenna is the sum of partial gains for any two orthogonal polarizations.
G = G_θ + G_φ

## Realized gain

**Realized gain** differs from gain in that it is "reduced by its impedance mismatch factor". This mismatch induces losses above the dissipative losses described above; therefore, realized gain will always be less than gain. Gain may be expressed as **absolute gain** if further clarification is required to differentiate it from realized gain.

## Total radiated power

**Total radiated power** (TRP) is the sum of all RF power radiated by the antenna when the source power is included in the measurement. TRP is expressed in watts or the corresponding logarithmic expressions, often dBm or dBW.

When testing mobile devices, TRP can be measured while in close proximity of power-absorbing losses such as the body and hand of the user.

The TRP can be used to determine body loss (BoL). The body loss is considered as the ratio of TRP measured in the presence of losses and TRP measured while in free space.

_(Later sections of the original article were left out of this offline copy for length: Examples.)_
