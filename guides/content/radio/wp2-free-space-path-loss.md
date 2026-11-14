---
id: radio/wp2-free-space-path-loss
title: Free-space path loss
category: radio
tags: [free-space path loss, fspl, radio]
summary: In telecommunications, the free-space path loss (FSPL) (also known as free-space loss, FSL) is the decrease in signal strength of a signal traveling between two antennas on a line-of-sight path through free space, which occurs because the signal spreads out as it propagates.
source: https://en.wikipedia.org/w/index.php?title=Free-space_path_loss&oldid=1365112863
source_title: Wikipedia: Free-space path loss
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Free-space path loss", Wikipedia, The Free Encyclopedia (revision 1365112863), https://en.wikipedia.org/w/index.php?title=Free-space_path_loss&oldid=1365112863 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Free-space path loss

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In telecommunications, the **free-space path loss** (**FSPL**) (also known as free-space loss, FSL) is the decrease in signal strength of a signal traveling between two antennas on a line-of-sight path through free space, which occurs because the signal spreads out as it propagates. The "Standard Definitions of Terms for Antennas", IEEE Std 145-1993, defines free-space loss as "The loss between two isotropic radiators in free space, expressed as a power ratio."

Free-space path loss increases with the square of the distance between the antennas because radio waves spread out following an inverse square law. It decreases with the square of the wavelength of the radio waves, and does not include any power loss in the antennas themselves due to imperfections such as resistance or losses due to interaction with the environment such as atmospheric absorption.

The FSPL is rarely used standalone, but rather as a part of the Friis transmission formula, which includes the gain of antennas. It is a major factor used in power link budgets to analyze radio communication systems, to ensure that sufficient radio power reaches the receiver so that the received signal is intelligible.

## Free-space path loss formula

The free-space path loss (FSPL) formula derives from the Friis transmission formula. This states that in a radio system consisting of a transmitting antenna transmitting radio waves to a receiving antenna, the ratio of radio wave power received P_r to the power transmitted P_t is:
(P_r)/(P_t) = D_t D_r ( (λ)/(4 π d) )^2
where

- D_t is the directivity of the transmitting antenna
- D_r is the directivity of the receiving antenna
- λ is the signal wavelength
- d is the distance between the antennas
The distance between the antennas d must be large enough that the antennas are in the far field of each other dggλ.
The free-space path loss is the loss factor in this equation that is due to distance and wavelength, or in other words, the ratio of power transmitted to power received assuming the antennas are isotropic and have no directivity (D_t = D_r = 1):
FSPL = ( frac4π d λ )^2

Since the frequency of a radio wave f is equal to the speed of light c divided by the wavelength, the path loss can also be written in terms of frequency:
FSPL = (4π df over c)^2

Beside the assumption that the antennas are lossless, this formula assumes that the polarization of the antennas is the same, that there are no multipath effects, and that the radio wave path is sufficiently far away from obstructions that it acts as if it is in free space. This last restriction requires an ellipsoidal area around the line of sight out to 0.6 of the Fresnel zone be clear of obstructions. The Fresnel zone increases in diameter with the wavelength of the radio waves. The concept of free space path loss is often applied to radio systems that don't completely meet these requirements, but these imperfections can be accounted for by small constant power loss factors that can be included in the link budget.

## Influence of distance and frequency

The free-space loss increases with the distance between the antennas and decreases with the wavelength of the radio waves due to these factors:

- **Intensity** (I) – the power density of the radio waves decreases with the square of distance from the transmitting antenna due to spreading of the electromagnetic energy in space according to the inverse square law
- **Antenna capture area** (A_eff) – the amount of power the receiving antenna captures from the radiation field is proportional to a factor called the *antenna aperture* or antenna capture area, which increases with the square of wavelength. Since this factor is not related to the radio wave path but comes from the receiving antenna, the term "free-space path loss" is a little misleading.
- **Directivity of receiving antenna**- while the above formulas are correct, the presence of Directivities Dt and Dr builds the wrong intuition in the FSPL Friis transmission formula. The formula seems to say that "free space path loss" increases with frequency, which is misleading. The frequency dependence of path loss does not come from free space propagation, but rather from receiving antenna capture area frequency dependence. As frequency increases, the directivity of an antenna of a given physical size will increase. In order to keep receiver antenna directivity constant in the formula, the antenna size must be reduced, and a smaller size antenna results in less power being received as it is able to capture less power with a smaller area. In other words, the path loss increases with frequency because the antenna size is reduced to keep directivity constant in the formula, and has nothing to do with propagation in vacuum.
- **Directivity of transmitting antenna** - the directivity of transmitting antenna does not have the same role as directivity of receiving antenna. The difference is that the receiving antenna is receiving the power from free space, and hence captures less power as it becomes smaller. The transmitting antenna does not transmit less power as it becomes smaller (for example half wave dipole), because it is receiving its RF power from a generator or source, and if the source is 1 Watt or Pt, the antenna will transmit all of it (assuming ideal efficiency and VSWR for simplicity).
- **System Loss Factor (L) :** Miscellaneous losses or system losses (L=>1) are usually due to transmission line attenuation, filter losses, and antenna losses in communication system. A value of L = 1 indicates no loss in the system hardware.

## Derivation

The radio waves from the transmitting antenna spread out in a spherical wavefront. The amount of power passing through any sphere centered on the transmitting antenna is equal. The surface area of a sphere of radius d is 4π d^2. Thus the intensity or power density of the radiation in any particular direction from the antenna is inversely proportional to the square of distance
I ∝ P_t over 4π d^2
(The term 4π d^2 means the surface of a sphere, which has a radius d. Please remember, that d here has a meaning of 'distance' between the two antennas, and does not mean the diameter of the sphere (as notation usually used in mathematics).)
For an isotropic antenna which radiates equal power in all directions, the power density is evenly distributed over the surface of a sphere centered on the antenna
I = P_t over 4π d^2 (1)
The amount of power the receiving antenna receives from this radiation field is
P_r = A_effI (2)
The factor A_eff, called the *effective area* or *aperture* of the receiving antenna, which has the units of area, can be thought of as the amount of area perpendicular to the direction of the radio waves from which the receiving antenna captures energy. Since the linear dimensions of an antenna scale with the wavelength λ, the cross sectional area of an antenna and thus the aperture scales with the square of wavelength λ^2. The effective area of an isotropic antenna (for a derivation of this see antenna aperture article) is
A_eff = λ^2 over 4π
Combining the above (1) and (2), for isotropic antennas
P_r = Big(P_t over 4π d^2Big)Big(λ^2 over 4πBig)
FSPL = P_t over P_r = Big(4π d over λBig)^2

## Free-space path loss in decibels

A convenient way to express FSPL is in terms of decibels (dB):
FSPL(dB) = 10log_10(((4π d f)/(c))^2); = 20log_10((4π d f)/(c)); = 20log_10(d) + 20log_10(f) + 20log_10((4π)/(c)); = 20log_10(d) + 20log_10(f) -147.55,
using SI units of meters for d, hertz (s⁻¹) for f, and meters per second (m⋅s⁻¹) for c, (where c=299 792 458 m/s in vacuum, ≈ 300 000 km/s)

For typical radio applications, it is common to find d measured in kilometers and f in gigahertz, in which case the FSPL equation becomes
FSPL(dB) = 20log_10(d_km) + 20log_10(f_GHz) + 92.45,
an increase of 240 dB, because the units increase by factors of and respectively, so:
20log_10(10^3) + 20log_10(10^9) = 240.

(The constants differ in the second decimal digit when the speed of light is approximated by 300 000 km/s. Whether one uses 92.4, 92.44 or 92.45 dB, the result will be OK as the average measurement instruments cannot provide more accurate results anyway. A logarithmic scale is introduced to see the important differences (i.e. order of magnitudes), so in engineering practice dB results are rounded)
