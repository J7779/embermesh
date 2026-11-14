---
id: radio/dipole-antenna-part1
title: Dipole antenna (part 1 of 3)
category: radio
tags: [radio, antenna, dipole, half-wave, HF]
summary: "The dipole antenna: half-wave dipole lengths, feed impedance, radiation pattern and variants."
source: "https://en.wikipedia.org/w/index.php?title=Dipole_antenna&oldid=1382744782"
source_title: Dipole antenna (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Dipole antenna' (rev 1382744782), Wikipedia, https://en.wikipedia.org/wiki/Dipole_antenna. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
part_of: radio/dipole-antenna
order: 1
---

# Dipole antenna (part 1 of 3)

In radio and telecommunications, a dipole antenna or doublet is one of the two simplest and most widely used types of antenna; the other is the monopole. The dipole is any one of a class of antennas producing a radiation pattern approximating that of an elementary electric dipole with a radiating structure supporting a line current so energized that the current has only one node at each far end. A dipole antenna commonly consists of two identical conductive elements such as metal wires or rods. The driving current from the transmitter is applied, or for receiving antennas the output signal to the receiver is taken, between the two halves of the antenna. Each side of the feedline to the transmitter or receiver is connected to one of the conductors. This contrasts with a monopole antenna, which consists of a single rod or conductor with one side of the feedline connected to it, and the other side connected to some type of ground. A common example of a dipole is the rabbit ears television antenna found on broadcast television sets. All dipoles are electrically equivalent to two monopoles mounted end-to-end and fed with opposite phases, with the ground plane between them made *virtual* by the opposing monopole.

The dipole is the simplest type of antenna from a theoretical point of view. Most commonly it consists of two conductors of equal length oriented end-to-end with the feedline connected between them. Dipoles are frequently used as resonant antennas. If the feedpoint of such an antenna is shorted, then it will be able to resonate at a particular frequency, just like a guitar string that is plucked. Using the antenna at around that frequency is advantageous in terms of feedpoint impedance (and thus standing wave ratio), so its length is determined by the intended wavelength (or frequency) of operation. The most commonly used is the center-fed half-wave dipole which is just under a half-wavelength long. The radiation pattern of the half-wave dipole is maximum perpendicular to the conductor, falling to zero in the axial direction, thus implementing an omnidirectional antenna if installed vertically, or (more commonly) a weakly directional antenna if horizontal.

Although they may be used as standalone low-gain antennas, dipoles are also employed as driven elements in more complex antenna designs such as the Yagi antenna and driven arrays. Dipole antennas (or such designs derived from them, including the monopole) are used to feed more elaborate directional antennas such as a horn antenna, parabolic reflector, or corner reflector. Engineers analyze vertical (or other monopole) antennas on the basis of dipole antennas of which they are one half.

## History

German physicist Heinrich Hertz first demonstrated the existence of radio waves in 1887 using what we now know as a dipole antenna (with capacitative end-loading). On the other hand, Guglielmo Marconi empirically found that he could just ground the transmitter (or one side of a transmission line, if used) dispensing with one half of the antenna, thus realizing the *vertical* or monopole antenna. For the low frequencies Marconi employed to achieve long-distance communications, this form was more practical; when radio moved to higher frequencies (especially VHF transmissions for FM radio and TV) it was advantageous for these much smaller antennas to be entirely atop a tower thus requiring a dipole antenna or one of its variations.

In the early days of radio, the thus-named Marconi antenna (monopole) and the doublet (dipole) were seen as distinct inventions. Now, however, the monopole antenna is understood as a special case of a dipole which has a virtual element underground.

## Dipole variations

### Short dipole

A short dipole is a dipole formed by two conductors with a total length ℓ substantially less than a half wavelength (1/ 2  λ). Short dipoles are sometimes used in applications where a full half-wave dipole would be too large. They can be analyzed easily using the results obtained below for the Hertzian dipole, a fictitious entity. Being shorter than a resonant antenna (half a wavelength long) its feedpoint impedance includes a large capacitive reactance requiring a loading coil or other matching network in order to be practical, especially as a transmitting antenna.

To find the far-field electric and magnetic fields generated by a short dipole we use the result shown below for the Hertzian dipole (an infinitesimal current element) at a distance r from the current and at an angle θ to the direction of the current, as being:

`H_ϕ = j (I_hℓk)/(4πr) e^(j(ωt − kr))sin(θ); E_θ = ζ_oH_ϕ = j (ζ_oI_hℓk)/(4πr) e^(j(ωt − kr))sin(θ).`

where the radiator consists of a current of `I_he^(jωt)` over a short length ℓ and `j^2 ≡ − 1` in electronics replaces the customary mathematical symbol i for the square root of −1. ω is the angular (radian) frequency (`ω ≡ 2πf`) and k is the wavenumber (`k ≡ 2π/λ`). ζ0 is the impedance of free space (`ζ_o ≈ 376.7Ω`), which is the ratio of a free space plane wave's electric to magnetic field strength.

The feedpoint is usually at the center of the dipole as shown in the diagram. The current along dipole arms are approximately described as proportional to `sin(kz)` where z is the distance to the nearest end of the arm. In the case of a short dipole, that is essentially a linear drop from `I_o` at the feedpoint to zero at the end. Therefore, this is comparable to a Hertzian dipole with an *effective* current Ih equal to the average current over the conductor, so `I_h = 1/2 I_o.` With that substitution, the above equations closely approximate the fields generated by a short dipole fed by current `I_o.`

From the fields calculated above, one can find the radiated *flux* (power per unit area) at any point as the magnitude of the real part of the Poynting vector, S, which is given by `1/2 𝐄 × 𝐇^∗.` Because E and H are at right angles and in phase, there is no imaginary part and the cross product is equal to `1/2 E_θH_ϕ^∗;` the phase factors (the exponentials) cancel out, leaving:

`S = 1/2 E_θH_ϕ^∗ = 1/2 (ζ_oI_h^2ℓ^2k^2)/((4πr)^2) (sin)^2 (θ) = (ζ_o)/32 I_o^2(ℓ/λ)^2 1/(r^2) (sin)^2 (θ).`

We have now expressed the flux in terms of the feedpoint current Io and the ratio of the short dipole's length ℓ to the wavelength of radiation λ. The radiation pattern given by `(sin)^2 (θ)` is seen to be similar to and only slightly less directional than that of the half-wave dipole.

Using the above expression for the radiation in the far field for a given feedpoint current, we can integrate over all solid angle to obtain the total radiated power.

`P_(total) = π/12 ζ_oI_o^2(ℓ/λ)^2.`

From that, it is possible to infer the radiation resistance, equal to the resistive (real) part of the feedpoint impedance, neglecting a component due to ohmic losses (presumed smaller). By setting Ptotal to the power supplied at the feedpoint `1/2 I_o^2R_(radiation)` we find:

`R_(radiation) = π/6 ζ_o(ℓ/λ)^2 ≈ (ℓ/λ)^2(197Ω).`

Again, these approximations become quite accurate for ℓ ≪ 1/ 2 λ. Setting *ℓ* = 1/ 2 *λ* despite its use not quite being valid for so large a fraction of the wavelength, the formula would predict a radiation resistance of 49 Ω, instead of the actual value of 73 Ω produced by a half-wave dipole, when more correct quarter-wave sinusoidal currents are used.

### Dipole antennas of various lengths

The fundamental resonance of a thin linear conductor occurs at a frequency whose free-space wavelength is *twice* the wire's length; i.e. where the conductor is 1/2 wavelength long. Dipole antennas are frequently used at around that frequency and thus termed *half-wave dipole* antennas. This important case is dealt with in the next section.

Thin linear conductors of length `ℓ` are in fact resonant at any integer multiple of a half-wavelength:

`ℓ = n × λ/2`

where n is an integer, `λ = c/f` is the wavelength, and c is the reduced speed of radio waves in the radiating conductor (c ≈ 97%×co, the speed of light). For a center-fed dipole, however, there is a great dissimilarity between n being odd or being even. Dipoles which are an *odd* number of half-wavelengths in length have reasonably low driving point impedances (which are purely resistive at that resonant frequency). However ones which are an *even* number of half-wavelengths in length, that is, an integer number of wavelengths in length, have a *high* driving point impedance (albeit purely resistive at that resonant frequency).

For instance, a full-wave dipole antenna can be made with two half-wavelength conductors placed end to end for a total length of approximately `ℓ ≈ λ.` This results in an additional gain over a half-wave dipole of about 2 dB. Full wave dipoles can be used in short wave broadcasting only by making the effective diameter very large and feeding from a high impedance balanced line. Cage dipoles are often used to get the large diameter.

A 5/4-wave dipole antenna has a much lower but not purely resistive feedpoint impedance, which requires a matching network to the impedance of the transmission line. Its gain is about 3 dB greater than a half-wave dipole, the highest gain of any dipole of any similar length.

**Gain of dipole antennas**

| Length, `ℓ` in wavelengths | Directive gain (dBi) | Notes |
|---|---|---|
| ≪ 0.5 | 1.76 | Poor efficiency |
| 0.5 | 2.15 | Most common |
| 1.0 | 4.0 | Only with fat dipoles |
| 1.25 | 5.2 | Greatest gain |
| 1.5 | 3.5 | Third harmonic |
| 2.0 | 4.3 | Not used |

Other reasonable lengths of dipole do not offer advantages and are seldom used. However the overtone resonances of a half-wave dipole antenna at odd multiples of its fundamental frequency are sometimes exploited. For instance, amateur radio antennas designed as half-wave dipoles at 7 MHz can also be used as 3/2-wave dipoles at 21 MHz; likewise VHF television antennas resonant at the low VHF television band (centered around 65 MHz) are also resonant at the high VHF television band (around 195 MHz).

### Half-wave dipole

A half-wave dipole antenna consists of two quarter-wavelength conductors placed end to end for a total length of approximately ℓ = 1/2 *λ*. The current distribution is that of a standing wave, approximately sinusoidal along the length of the dipole, with a node at each end and an antinode (peak current) at the center (feedpoint):

`I(z) = I_0cos(ωt)cos(kz),`

where *k* = 2*π*/*λ* and z runs from −+1/2ℓ to ++1/2ℓ.

In the far field, this produces a radiation pattern whose electric field is given by

`E_θ = (ζ_0I_0)/(2πr) (cos(π/2 cosθ))/(sinθ) sin(ωt − kr).`

The directional factor `(cos[ π/2 cosθ])/(sinθ)` is very nearly the same as sin *θ* applying to the short dipole, resulting in a very similar radiation pattern as noted above.

A numerical integration of the radiated power `(|E_θ|^2)/(2ζ_0)` over all solid angle, as we did for the short dipole, obtains a value for the total power Ptotal radiated by the dipole with a current having a peak value of I0 as in the form specified above. Dividing Ptotal by `4πr^2` supplies the flux at a large distance, *averaged* over all directions. Dividing the flux in the *θ* = π/2 direction (where it is at its peak) at that large distance by the average flux, we find the directive gain to be 1.64. This can also be directly computed using the cosine integral:

`G = 4/(Cin(2π)) ≈ 1.64` (2.15 dBi)

(The Cin(*x*) form of the cosine integral is not the same as the Ci(*x*) form; they differ by a logarithm. Both MATLAB and Mathematica have inbuilt functions which compute Ci(*x*), but not Cin(*x*). See the Wikipedia page on cosine integral for the relationship between these functions.)

We can now also find the radiation resistance as we did for the short dipole by solving:

`P_(𝗍𝗈𝗍𝖺𝗅) = 1/2 I_0^2R_(𝗋𝖺𝖽𝗂𝖺𝗍𝗂𝗈𝗇)`

to obtain:

`R_(𝗋𝖺𝖽𝗂𝖺𝗍𝗂𝗈𝗇) ≈ 73.1Ω.`

Using the induced EMF method, the real part of the driving point impedance can also be written in terms of the cosine integral, obtaining the same result:

`R_(𝗋𝖺𝖽𝗂𝖺𝗍𝗂𝗈𝗇) = (ζ_0)/(4π) Cin(2π) = (ζ_0)/(4π) ∫_0^(2π) (1 − cosθ)/θ dθ ≈ 73.1Ω.`

If a half-wave dipole is driven at a point other the center, then the feed point resistance will be higher. The radiation resistance is *usually* expressed relative to the maximum current present along an antenna element, which for the half-wave dipole (and most other antennas) is also the current at the feedpoint. However, if the dipole is fed at a different point at a distance x from a current maximum (the center in the case of a half-wave dipole), then the current there is not I0 but only *I*0 cos(*k x*).

In order to supply the same power, the voltage at the feedpoint has to be similarly *increased* by the factor sec(*k x*). Consequently, the resistive part of the feedpoint impedance `ℛ_ℯ[ V/I ]` is increased by the factor sec^2(*k x*):

`R_(𝖿𝖾𝖾𝖽𝗉𝗈𝗂𝗇𝗍) = (R_(𝗋𝖺𝖽𝗂𝖺𝗍𝗂𝗈𝗇))/((cos)^2(kx)) ≈ (73.1Ω)/((cos)^2(kx))`

This equation can also be used for dipole antennas of any length, provided that Rradiation has been computed relative to the current maximum, which is *not* generally the same as the feedpoint current for dipoles longer than half-wave. Note that this equation breaks down when feeding an antenna near a current node, where cos(*k x*) approaches zero. The driving point impedance does indeed rise greatly, but is nevertheless limited due to higher order components of the elements' not-quite-exactly-sinusoidal current, which have been ignored above in the model for the current distribution.

### Folded dipole

A folded dipole is a half-wave dipole with an additional parallel wire connecting its two ends. If the additional wire has the same diameter and cross-section as the dipole, two nearly identical radiating currents are generated. The resulting far-field emission pattern is nearly identical to the one for the single-wire dipole described above, but at resonance its feedpoint impedance `R_(𝖿.𝖽.)` is four times the feedpoint impedance of a single-wire dipole.

A folded dipole is, technically, a folded full-wave loop antenna, where the loop has been bent at opposing ends and squashed into two parallel wires in a flat line. Although the broad bandwidth, high feedpoint impedance, and high efficiency are characteristics more similar to a full loop antenna, the folded dipole's radiation pattern is more like an ordinary dipole. Since the operation of a single halfwave dipole is easier to understand, both full loops and folded dipoles are often described as two halfwave dipoles in parallel, connected at the ends.

The high feedpoint impedance `R_(𝖿.𝖽.)` at resonance is because for a fixed amount of power, the total radiating current `I_0` is equal to twice the current in each wire separately and thus equal to twice the current at the feed point. We equate the average radiated power to the average power delivered at the feedpoint, we may write

`1/2 R_(𝗁.𝗐.)I_0^2 = 1/2 R_(𝖿.𝖽.)(1/2 I_0)^2,`

where `R_(𝗁.𝗐.)` is the lower feedpoint impedance of the resonant halfwave dipole. It follows that

`R_(𝖿.𝖽.) = 4R_(𝗁.𝗐.) ≈ 292Ω.`

Half-wave folded dipoles are often used for FM radio antennas; versions made with twin lead which can be hung on an inside wall often come with FM tuners. They are also widely used as driven elements for rooftop Yagi television antennas. The T²FD antenna is a folded dipole with a resistor added on the second wire, opposite the feedpoint.

The folded dipole is therefore well matched to 300 Ω balanced transmission lines, such as twin-feed ribbon cable. The folded dipole has a wider bandwidth than a single dipole. They can be used for transforming the value of input impedance of the dipole over a broad range of step-up ratios by changing the thicknesses of the wire conductors for the fed- and folded-sides.

Instead of altering thickness or spacing, one can add a third parallel wire to increase the feedpoint impedance to 9 times that of a single-wire dipole, raising the impedance to 658 Ω, making a good match for open wire feed cable, and further broadening the resonant frequency band of the antenna. More extra parallel wires can be added: Any number of extra parallel wires can be joined onto the antenna, with the feedpoint impedance given by

`R_(𝗋𝖺𝖽) ≈ n^2 × 73Ω,`

where `n` is the number of parallel halfwave-long wires laid side-by-side in the antenna, and connected at their ends. It is also possible to modify the so-called flattened-loop design, and get nearly as good performance, by making each of the parallel wires too short by the same amount, but connecting a single capacitive loading wire (going off in nearly any direction, most often dangling) on each of the antenna ends. The loading wire length is equal to the single missing length of one of the parallel wires.

### Other variants

There are numerous modifications to the shape of a dipole antenna which are useful in one way or another but result in similar radiation characteristics (low gain). This is not to mention the many directional antennas which include one or more dipole elements in their design as driven elements, many of which are linked to in the information box at the bottom of this page.

- The *bow-tie antenna* is a dipole with flaring, triangular shaped arms. The shape gives it a much wider bandwidth than an ordinary dipole. It is widely used in UHF television antennas.

- The *cage dipole* is a similar modification in which the bandwidth is increased by using fat cylindrical dipole elements made of a cage of wires (see photo). These are used in a few broadband array antennas in the medium wave and shortwave bands for applications such as over-the-horizon radar and radio telescopes.
- A *halo antenna* is a half-wave dipole bent into a circle for a nearly uniform radiation pattern in the plane of the circle. When the halo's circle is horizontal, it produces horizontally polarized radiation in a nearly omnidirectional pattern with only a little power wasted toward the zenith, compared to a straight horizontal dipole. In practice, it is categorized either as a bent dipole or as a loop antenna, depending on author preference.
- A *turnstile antenna* comprises two dipoles crossed at a right angle and feed system which introduces a quarter-wave phase difference between the currents along the two. With that geometry, the two dipoles do not interact electrically but their fields add in the far-field producing a net radiation pattern that is rather close to isotropic, with horizontal polarization in the plane of the elements and circular or elliptical polarization at other angles. Turnstile antennas can be stacked and fed in phase to realize an omnidirectional broadside array or phased for an end-fire array with circular polarization.
- The *batwing antenna* is a turnstile antenna with its linear elements widened as in a bow-tie antenna, again for the purpose of widening its resonant frequency and thus usable over a larger bandwidth, without re-tuning. When stacked to form an array the radiation is omnidirectional, horizontally polarized, and with increased gain at low elevations, making it ideal for television broadcasting.
- A V antenna is a dipole with a bend in the middle so its arms are at an angle instead of co-linear.
- A *quadrant* antenna is a 'V' antenna with an unusual overall length of a *full* wavelength, with two half-wave horizontal elements meeting at a right angle where it is fed. Quadrant antennas produce mostly horizontal polarization at low to intermediate elevation angles and have nearly omnidirectional radiation patterns.

One implementation uses cage elements (see above); the thickness of the resulting elements lowers the high driving point impedance of a full-wave dipole to a value that accommodates a reasonable match to open wire lines and increases the bandwidth (in terms of SWR) to a full octave. They are used for HF band transmissions.

- The *G5RV antenna* is a dipole antenna fed indirectly, through a carefully chosen length of 300 Ω or 450 Ω twin lead, which acts as an impedance matching network to connect (through a balun) to a standard 50 Ω coaxial transmission line.
- The *sloper antenna* is a slanted vertical dipole antenna attached to the top of a single tower. The element can be center-fed or can be end-fed as an unbalanced monopole antenna from a transmission line at the top of the tower, in which case the monopole's ground connection can better be viewed as a second element comprising the tower or transmission line shield.
- The *inverted 'V' antenna* is likewise supported using a single tower but is a balanced antenna with two symmetric elements angled toward the ground. It is thus a half-wave dipole with a bend in the middle. Like the sloper, this has the practical advantage of elevating the antenna but requiring only a *single* tower.
- The *AS-2259 antenna* is an inverted-‘V’ dipole antenna used for local communications via Near Vertical Incidence Skywave (NVIS).

### Vertical (monopole) antennas

The *vertical*, *Marconi*, or monopole antenna is a single-element antenna usually fed at the bottom (with the shield side of its unbalanced transmission line connected to ground). It behaves essentially the same as half of a dipole antenna. The ground (or ground plane) is considered to be a conductive surface that works as a reflector (see effect of ground). Vertical currents in the reflected image have the same direction (thus are *not* reflected about the ground) and phase as the current in the real antenna. The conductor and its image together act as a dipole in the upper half of space. Like a dipole, in order to achieve resonance (resistive feedpoint impedance) the conductor must be close to a quarter wavelength in height (like each conductor in a half-wave dipole).

In this upper side of space, the emitted field has the same amplitude of the field radiated by a similar dipole fed with the same current. Therefore, the total emitted power is half the emitted power of a dipole fed with the same current. As the current is the same, the radiation resistance (real part of series impedance) will be half of the series impedance of the comparable dipole. A quarter-wave monopole, then, has an impedance of `(73 + j43)/2 = 36 + j21Ω.` Another way of seeing this is that a true dipole receiving a current I has voltages on its terminals of +V and −V, for an impedance across the terminals of 2+*V*/ *I* , whereas the comparable vertical antenna has the current I but an applied voltage of only V.

Since the fields above ground are the same as for the dipole, but only half the power is applied, the gain is doubled to 5.14 dBi. This is not an actual performance advantage *per se*, since in practice a dipole also reflects half of its power off the ground which (depending on the antenna height and sky angle) can augment (or cancel!) the direct signal. The vertical polarization of the monopole (as for a vertically oriented dipole) is advantageous at low elevation angles where the ground reflection combines with the direct wave approximately in phase.

The earth acts as a ground plane, but it can be a poor conductor leading to losses. Its conductivity can be improved (at cost) by laying a copper mesh. When an actual ground is not available (such as in a vehicle) other metallic surfaces can serve as a ground plane (typically the vehicle's roof). Alternatively, radial wires placed at the base of the antenna can form a ground plane. For VHF and UHF bands, the radiating and ground plane elements can be constructed from rigid rods or tubes. Using such an artificial ground plane allows for the entire antenna and ground to be mounted at an arbitrary height. One common modification has the radials forming the ground plane sloped down, which has the effect of raising the feedpoint impedance to around 50 Ω, matching common coaxial cable. No longer being a true ground, a balun (such as a simple choke balun) is then recommended.
