---
id: radio/dipole-antenna-part2
title: Dipole antenna (part 2 of 3)
category: radio
tags: [radio, antenna, dipole, half-wave, HF]
summary: "The dipole antenna: half-wave dipole lengths, feed impedance, radiation pattern and variants. (continued, part 2)"
source: "https://en.wikipedia.org/w/index.php?title=Dipole_antenna&oldid=1382744782"
source_title: Dipole antenna (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Dipole antenna' (rev 1382744782), Wikipedia, https://en.wikipedia.org/wiki/Dipole_antenna. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
part_of: radio/dipole-antenna
order: 2
---

# Dipole antenna (part 2 of 3)

## Dipole characteristics

### Impedance of dipoles of various lengths

The feedpoint impedance of a dipole antenna is sensitive to its electrical length and feedpoint position. Therefore, a dipole will generally only perform optimally over a rather narrow bandwidth, beyond which its impedance will become a poor match for the transmitter or receiver (and transmission line). The real (resistive) and imaginary (reactive) components of that impedance, as a function of electrical length, are shown in the accompanying graph. The detailed calculation of these numbers are described below. Note that the value of the reactance is highly dependent on the diameter of the conductors; this plot is for conductors with a diameter of 0.001 wavelengths.

Dipoles that are much smaller than one half the wavelength of the signal are called *short dipoles*. These have a very low radiation resistance (and a high capacitive reactance) making them inefficient antennas. More of a transmitter's current is dissipated as heat due to the finite resistance of the conductors which is greater than the radiation resistance. However they can nevertheless be practical receiving antennas for longer wavelengths.

Dipoles whose length is approximately half the wavelength of the signal are called *half-wave dipoles* and are widely used as such or as the basis for derivative antenna designs. These have a radiation resistance which is much greater, closer to the characteristic impedances of available transmission lines, and normally much larger than the resistance of the conductors, so that their efficiency approaches 100%. In general radio engineering, the term *dipole*, if not further qualified, is taken to mean a center-fed half-wave dipole.

A true half-wave dipole is one half of the wavelength λ in length, where *λ* = *c*/ *f*  in free space. Such a dipole has a feedpoint impedance consisting of 73 Ω resistance and +43 Ω reactance, thus presenting a slightly inductive reactance. To cancel that reactance, and present a pure resistance to the feedline, the element is shortened by the factor k for a net length `ℓ` of:

`ℓ = 1/2 kλ = 1/2 k c/f`

where λ is the free-space wavelength, c is the speed of light in free space, and f is the frequency. The adjustment factor k which causes feedpoint reactance to be eliminated, depends on the diameter of the conductor, as is plotted in the accompanying graph. The relative scale-size k ranges from about 0.98 for thin wires (diameter, 0.00001 wave) to about 0.94 for thick conductors (diameter, 0.008 wave). This is because the effect of antenna length on reactance (upper graph) is much greater for thinner conductors so that a smaller deviation from the exact half wavelength is required in order to cancel the 43 Ω inductive reactance it has when exactly 1/ 2  λ. For the same reason, antennas with thicker conductors have a wider operating bandwidth over which they attain a practical standing wave ratio which is degraded by any remaining reactance.

For a typical k of about 0.95, the above formula for the corrected antenna length can be written, for a length in meters as 143/ f , or a length in feet as 468/ f  where f is the frequency in megahertz.

Dipole antennas of lengths approximately equal to any *odd* multiple of 1/ 2 λ are also resonant, presenting little or no reactance (which can be removed by making a small length adjustment). However, these are rarely used. One size that is a much more efficient radiator both in terms of Watts out and in direction radiated is a dipole with a length of 5/ 4  wave. Not being close to 3/ 2  wave, this antenna's impedance has a large (negative) reactance and can only be used with an inductive impedance matching network (a tapped loading coil or a so-called antenna tuner). It is a desirable length because such an antenna has the highest gain for any dipole which isn't a great deal longer.

### Radiation pattern and gain

A dipole is omnidirectional in the plane perpendicular to the wire axis, with the radiation falling to zero on the axis (off the ends of the antenna). In a half-wave dipole, the radiation is maximum perpendicular to the antenna, declining as `(sinθ)^2` to zero on the axis. Its radiation pattern in three dimensions (see figure) would be plotted approximately as a toroid (doughnut shape) symmetric about the conductor. When mounted vertically this results in maximum radiation in horizontal directions. When mounted horizontally, the radiation peaks at right angles (90°) to the conductor, with nulls in the direction of the dipole.

Neglecting electrical inefficiency, the antenna gain is equal to the directive gain, which is 1.50 (1.76 dBi or -0.39 dBd) for a short dipole, increasing to 1.64 (2.15 dBi or 0 dBd) for a half-wave dipole. For a 5/ 4  wave dipole the gain further increases to about 5.2 dBi, making this length desirable for that reason even though the antenna is then off-resonance. Longer dipoles than that have radiation patterns that are multi-lobed, with poorer gain (unless they are *much* longer) even along the strongest lobe. Other enhancements to the dipole (such as including a corner reflector or an array of dipoles) can be considered when more substantial directivity is desired. Such antenna designs, although based on the half-wave dipole, generally acquire their own names.

### Feeding a dipole antenna

Ideally, a half-wave dipole should be fed using a balanced transmission line matching its typical 65–70 Ω input impedance. Twin lead with a similar impedance is available but seldom used and does not match the balanced antenna terminals of most radio and television receivers. Much more common is the use of common 300 Ω twin lead in conjunction with a *folded dipole*. The driving point impedance of a half-wave folded dipole is 4 times that of a simple half-wave dipole, thus closely matching that 300 Ω characteristic impedance. Most FM broadcast band tuners and older analog televisions include balanced 300 Ω antenna input terminals. However twin lead has the drawback that it is electrically disturbed by any other nearby conductor (including earth); when used for transmitting, care must be taken not to place it near other conductors.

Many types of coaxial cable (or coax) have a characteristic impedance of 75 Ω, which would otherwise be a good match for a half-wave dipole. However, coax is a single-ended line whereas a center-fed dipole expects a balanced line (such as twin lead). By symmetry, one can see that the dipole's terminals have an equal but opposite voltage, whereas coax has one conductor grounded. Using coax regardless results in an unbalanced line, in which the currents along the two conductors of the transmission line are no longer equal and opposite. Since you then have a *net current* along the transmission line, the transmission line becomes an antenna itself, with unpredictable results (since it depends on the path of the transmission line). This will generally alter the antenna's intended radiation pattern, and change the impedance seen at the transmitter or receiver.

A balun is required to use coaxial cable with a dipole antenna. The balun transfers power between the single-ended coax and the balanced antenna, sometimes with an additional change in impedance. A balun can be implemented as a transformer which also allows for an impedance transformation. This is usually wound on a ferrite toroidal core. The toroid core material must be suitable for the frequency of use, and in a transmitting antenna it must be of sufficient size to avoid saturation. Other balun designs are mentioned below.

#### Current balun

A current balun uses a transformer wound on a toroid or rod of magnetic material such as ferrite. All of the current seen at the input goes into one terminal of the balanced antenna. It forms a balun by choking common-mode current. The material isn't critical for 1:1 because there is no transformer action applied to the desired differential current. A related design involves *two* transformers and includes a 1:4 impedance transformation.

#### Coax balun

A coax balun is a cost-effective method of eliminating feeder radiation but is limited to a narrow set of operating frequencies.

One easy way to make a balun is to use a length of coaxial cable equal to half a wavelength. The inner core of the cable is linked at each end to one of the balanced connections for a feeder or dipole. One of these terminals should be connected to the inner core of the coaxial feeder. All three braids should be connected together. This then forms a 4:1 balun, which works correctly at only a narrow band of frequencies.

#### Sleeve balun

At VHF frequencies, a sleeve balun can also be built to remove feeder radiation.

Another narrow-band design is to use a 1/ 4  λ length of metal pipe. The coaxial cable is placed inside the pipe; at one end the braid is wired to the pipe while at the other end no connection is made to the pipe. The balanced end of this balun is at the end where no connection is made to the pipe. The 1/ 4  λ conductor acts as a transformer, converting the zero impedance at the short to the braid into an infinite impedance at the open end. This infinite impedance at the open end of the pipe prevents current flowing into the outer coax formed by the outside of the inner coax shield and the pipe, forcing the current to remain in the inside coax. This balun design is impractical for low frequencies because of the long length of pipe that will be needed.

## Common applications

### "Rabbit ears" TV antenna

One of the most common applications of the dipole antenna is the *rabbit ears* or *bunny ears* television antenna, found atop broadcast television receivers. It is used to receive the VHF terrestrial television bands, consisting in the US of 54–88 MHz (band I) and 174–216 MHz (band III), with wavelengths of 5.5–1.4 meters (18 feet to 4 feet 8 inches). Since this frequency range is much wider than a single fixed dipole antenna can cover, it is made with several degrees of adjustment. It is constructed of two telescoping rods that can each be extended out to about 1 meters (3 feet) length (one-quarter wavelength at 75 MHz). With control over the segments' length, angle with respect to vertical, and compass angle, one has much more flexibility in optimizing reception than available with a rooftop antenna even if equipped with an antenna rotor.

### FM-broadcast-receiving antennas

In contrast to the wide television frequency bands, the FM broadcast band (88-108 MHz) is narrow enough that a dipole antenna can cover it. For fixed use in homes, hi-fi tuners are typically supplied with simple folded dipoles resonant near the center of that band. The feedpoint impedance of a folded dipole, which is quadruple the impedance of a simple dipole, is a good match for 300 Ω twin lead, so that is usually used for the transmission line to the tuner. A common construction is to make the arms of the folded dipole out of twin lead also, shorted at their ends. This flexible antenna can be conveniently taped or nailed to walls, following the contours of moldings.

### Shortwave antenna

Horizontal wire dipole antennas are popular for use on the HF shortwave bands, both for transmitting and shortwave listening. They are usually constructed of two lengths of wire joined by a strain insulator in the center, which is the feedpoint. The ends can be attached to existing buildings, structures, or trees, taking advantage of their heights. If used for transmitting, it is essential that the ends of the antenna be attached to supports through strain insulators with a sufficiently high flashover voltage, since the antenna's high-voltage antinodes occur there. Being a balanced antenna, they are best fed with a balun between the (coax) transmission line and the feedpoint.

These are simple to put up for temporary or field use. But they are also widely used by radio amateurs and short wave listeners in fixed locations due to their simple (and inexpensive) construction, while still realizing a resonant antenna at frequencies where resonant antenna elements need to be of quite some size. They are an attractive solution for these frequencies, when significant directionality is not desired, and the cost of several such resonant antennas for different frequency bands, built at home, may still be much less than a single commercially produced antenna.

### Dipole towers

Antennas for MF and LF radio stations are usually constructed as mast radiators, in which the vertical mast itself forms the antenna. Although mast radiators are most commonly monopoles, some are dipoles. The metal structure of the mast is divided at its midpoint into two insulated sections to make a vertical dipole, which is driven at the midpoint.

### Dipole arrays

Many types of array antennas are constructed using multiple dipoles, usually half-wave dipoles. The purpose of using multiple dipoles is to increase the directional gain of the antenna over the gain of a single dipole; the radiation of the separate dipoles interferes to enhance power radiated in desired directions. In arrays with multiple dipole driven elements, the feedline is split using an electrical network in order to provide power to the elements, with careful attention paid to the relative phase delays due to transmission between the common point and each element.

In order to increase antenna gain in horizontal directions (at the expense of radiation towards the sky or towards the ground) one can stack antennas in the vertical direction in a broadside array where the antennas are fed in phase. Doing so with horizontal dipole antennas retains those dipoles' directionality and null in the direction of their elements. However, if each dipole is vertically oriented, in a so-called collinear antenna array (see graphic), that null direction becomes vertical and the array acquires an omnidirectional radiation pattern (in the horizontal plane) as is typically desired. Vertical collinear arrays are used in the VHF and UHF frequency bands at which wavelengths the size of the elements are small enough to practically stack several on a mast. They are a higher-gain alternative to quarter-wave ground plane antennas used in fixed base stations for mobile two-way radios, such as police, fire, and taxi dispatchers.

On the other hand, for a rotating antenna (or one used only towards a particular direction) one may desire increased gain and directivity in a particular horizontal direction. If the broadside array discussed above (whether collinear or not) is turned horizontal, then the one obtains a greater gain in the horizontal direction perpendicular to the antennas, at the expense of most other directions. Unfortunately, that also means that the direction *opposite* the desired direction also has a high gain, whereas high gain is usually desired in one single direction. The power that is wasted in the reverse direction, however, can be redirected, for instance by using a large planar reflector, as is accomplished in the reflective array antenna, increasing the gain in the desired direction by another 3 dB

An alternative realization of a uni-directional antenna is the *end-fire array*. In this case the dipoles are again side by side (but not collinear), but fed in progressing phases, arranged so that their waves add coherently in one direction but cancel in the opposite direction. So now, rather than being perpendicular to the array direction as in a broadside array, the directivity is *in* the array direction (i.e. the direction of the line connecting their feedpoints) but with one of the opposite directions suppressed.

### Yagi antennas

The above-described antennas with multiple driven elements require a complex feed system of signal splitting, phasing, distribution to the elements, and impedance matching. A different sort of end-fire array which is much more often used is based on the use of so-called *parasitic elements*. In the popular high-gain Yagi antenna, only one of the dipoles is actually connected electrically, but the others receive and reradiate power supplied by the driven element. This time, the phasing is accomplished by careful choice of the lengths as well as positions of the parasitic elements, in order to concentrate gain in one direction and largely cancel radiation in the opposite direction (as well as all other directions). Although the realized gain is less than a driven array with the same number of elements, the simplicity of the electrical connections makes the Yagi more practical for consumer applications.

### Dipole as a reference standard

Antenna gain is frequently measured as decibels relative to a half-wave dipole. One reason is that practical antenna measurements need a reference strength to compare the field strength of an antenna under test at a particular distance to. While there is no such thing as an isotropic radiator, the half-wave dipole is well understood and behaved, and can be constructed to be nearly 100% efficient. It is also a fairer comparison, since the gain obtained by the dipole itself is essentially "free," given that almost no antenna design has a smaller directive gain.

For a gain measured relative to a dipole, one says the antenna has a gain of "x dBd" (see Decibel). More often, gains are expressed relative to an isotropic radiator, making the gain seem higher. In consideration of the known gain of a half-wave dipole, 0 dBd is defined as 2.15 dBi; all gains in "dBi" are shifted 2.15 higher than gains in "dBd".

## Hertzian dipole

The *Hertzian dipole* or *elementary doublet* refers to a theoretical construction, rather than a physical antenna design: It is an idealized tiny segment of conductor carrying a RF current with constant amplitude and direction along its entire (short) length; a real antenna can be modeled as the combination of many Hertzian dipoles laid end-to-end.

The Hertzian dipole may be defined as a finite oscillating current (in a specified direction) of `Ie^(iωt)` over a tiny or infinitesimal length `δℓ` at a specified position. The solution of the fields from a Hertzian dipole can be used as the basis for analytical or numerical calculation of the radiation from more complex antenna geometries (such as practical dipoles) by forming the superposition of fields from a large number of Hertzian dipoles comprising the current pattern of the actual antenna. As a function of position, taking the elementary current elements `I (𝐫),` multiplied by infinitesimal lengths `δℓ,` the resulting field pattern then reduces to an integral over the path of an antenna conductor (modeled as a thin wire).

For the following derivation, we shall take the current to be in the `z` direction, centered at the origin where `x = y = z = 0,` with the sinusoidal time dependence `e^(iωt)` for all quantities being understood. The simplest approach is to use the calculation of the vector potential `𝐀 (𝐫)` using the formula for the retarded potential. Although the value of `𝐀` is not unique, we shall constrain it by adopting the Lorenz gauge, and assuming sinusoidal current at radian frequency `ω` the retardation of the field is converted just into a phase factor `e^(− ikr),` where the wave number `k = ω/c` in free space and `r` is the linear distance between the point being considered to the origin (where we assumed the current source to be), so `r ≡ |𝐫|.` This results in a vector potential `𝐀` at position `𝐫` due to that current element only, which we find is purely in the `z` direction (the direction of the current):

`𝐀 (𝐫) = Iδℓ (μ_0)/(4πr) e^(− ikr)𝐳 ̂`

where `μ_0` is the permeability of free space. Then using

`μ𝐇 = 𝐁 = ∇ × 𝐀`

we can solve for the magnetic field `𝐇,` and from that (dependent on us having chosen the Lorenz gauge) the electric field `𝐄` using

`𝐄 = (∇ × 𝐇)/(iωϵ)`

In spherical coordinates we find that the magnetic field `𝐇` has only a component in the `ϕ` direction:

`𝐇 = H_ϕ𝝓 ̂`

where

`H_ϕ = i (Iδℓ)/(4π) (k/r − i/(r^2))e^(− ikr)sinθ,`

while the electric field has components both in the `θ` and `r` directions:

`𝐄 = E_θ𝜽 ̂ + E_r𝐫 ̂`

where

`E_θ = i (ζ_0Iδℓ)/(4π) (k/r − i/(r^2) − 1/(kr^3))e^(− ikr)sinθ,; E_r = (ζ_0Iδℓ)/(2π) (1/(r^2) − i/(kr^3))e^(− ikr)cosθ,`

with `ζ_0 ≡ sqrt((μ_0)/(ϵ_0))` is the impedance of free space.

This solution includes near field terms that are very strong near the source but which are not radiated. As seen in the accompanying animation, the `𝐄` and `𝐇` fields very close to the source are almost 90° out of phase, thus contributing very little to the Poynting vector by which radiated flux is computed. The near field solution for an antenna element (from the integral using this formula over the length of that element) is the field that can be used to compute the mutual impedance between it and another nearby element.

For computation of the far field radiation pattern, the above equations are simplified as only the `1/r` terms remain significant:

`H_ϕ = i (Iδℓk)/(4πr) e^(− ikr)sinθ,; E_θ = i (ζ_0Iδℓk)/(4πr) e^(− ikr)sinθ.`

The far-field pattern is thus seen to consist of a transverse electromagnetic (TEM) wave, with electric and magnetic fields at right angles to each other and at right angles to the direction of propagation (the direction of `𝐫`, as we assumed the source to be at the origin). The electric polarization, in the `θ` direction, is coplanar with the source current (in the `z` direction), while the magnetic field is at right angles to that, in the `ϕ` direction. It can be seen from these equations, and also in the animation, that the fields at these distances are exactly *in phase*. Both fields fall according to `1/r,` with the power thus falling according to `1/(r^2)` as dictated by the inverse square law.

### Radiation resistance

If one knows the far field radiation pattern due to a given antenna current, then it is possible to compute the radiation resistance directly. For the above fields due to the Hertzian dipole, we can compute the power flux according to the Poynting vector, resulting in a power (as averaged over one cycle) of:

`⟨𝐒⟩ = 1/2 ℛ_ℯ{𝐄 × 𝐇^∗} = ⟨S_𝗋⟩𝐫 ̂ + ⟨S_ϕ⟩𝝓 ̂.`

With increasing `r` the `⟨S_ϕ⟩` becomes insignificantly small compared to the `⟨S_𝗋⟩` component. Although not required, it is easiest to only work with the asymptotic value that `⟨S_𝗋⟩` approaches at a large `r` using the simpler far-field expressions for `𝐄` and `𝐇.` Consider a large sphere surrounding the source with a radius `r.` We find the power per unit area crossing the surface of that sphere in the `𝐫 ̂` direction is:

`⟨S_𝗋⟩ = (ζ_0)/2 (((|I|kδℓ)/(4πr) sinθ))^2`

Integration of this flux over the complete sphere results in:

`P_(𝗇𝖾𝗍) = ∫_0^(2π) ∫_0^π|⟨S_𝗋⟩|r^2sinθdϕdθ; = 1/2 ζ_0(((|I|kδℓ)/(4π)))^2∫_0^(2π) ∫_0^π(sin)^3θdϕdθ; = (ζ_0)/(12π) (|I|kδℓ)^2 = π/3 ζ_0((|I| (δℓ)/λ))^2`

where `λ = 2π/k` is the free space wavelength corresponding to the radian frequency `ω.` By definition, the radiation resistance `R_(rad)` times the average of the square of the current `1/2 (|I|)^2` is the net power radiated due to that current, so equating the above to `1/2 (|I|)^2R_(rad)` we find:

`R_(rad) = (2π)/3 ζ_0(((δℓ)/λ))^2.`

This method can be used to compute the radiation resistance for any antenna whose far-field radiation pattern has been found in terms of a specific antenna current. If ohmic losses in the conductors are neglected, the radiation resistance (considered relative to the feedpoint) is identical to the resistive (real) component of the feedpoint impedance. Unfortunately, this exercise tells us nothing about the reactive (imaginary) component of feedpoint impedance, whose calculation is considered below.

### Directive gain

Using the above expression for the radiated flux given by the Poynting vector, it is also possible to compute the directive gain of the Hertzian dipole. Dividing the total power computed above by `4πr^2` we can find the flux averaged over all directions `P_(avg)` as

`P_(avg) = (P_(net))/(4πr^2) = (ζ_0)/(48π^2r^2) k^2(|I|)^2(δℓ)^2.`

Dividing the flux radiated in a *particular* direction by `P_(avg)` we obtain the directive gain `𝖦(θ):`

`𝖦(θ) = (⟨𝐒_𝗋⟩)/(P_(avg)) = 3/2 (sin)^2θ`

The commonly quoted antenna "gain", meaning the peak value of the gain pattern (radiation pattern), is found to be 1.5~1.76 dBi, lower than practically any other antenna configuration.

### Comparison with the short dipole

The Hertzian dipole is *similar to* but differs from the short dipole, discussed above. In both cases the conductor is very short compared to a wavelength, so the standing wave pattern present on a half-wave dipole (for instance) is absent. However, with the Hertzian dipole we specified that the current along that conductor is *constant* over its short length. This makes the Hertzian dipole useful for analysis of more complex antenna configurations, where every infinitesimal section of that *real* antenna's conductor can be modeled as a Hertzian dipole with the current found to be flowing in that real antenna.

However a short conductor fed with a RF voltage will not have a uniform current even along that short range. Rather, a short dipole in real life has a current equal to the feedpoint current at the feedpoint but falling linearly to zero over the length of that short conductor. By placing a *capacitive hat*, such as a metallic ball, at the end of the conductor, it is possible for its self capacitance to absorb the current from the conductor and better approximate the constant current assumed for the Hertzian dipole. But again, the Hertzian dipole is meant only as a theoretical construct for antenna analysis.

The short dipole, with a feedpoint current of `I_0,` has an *average* current over each conductor of only `1/2 I_0.` The above field equations for the Hertzian dipole of length `δℓ` would then predict the *actual* fields for a short dipole using that effective current `I = 1/2 I_0.` This would result in a power measured in the far field of *one quarter* that given by the above equation for the magnitude of the Poynting vector `⟨S_𝗋⟩` if we had assumed an element current of `I_0.` Consequently, it can be seen that the radiation resistance computed for the short dipole is one quarter of that computed above for the Hertzian dipole. But their radiation patterns (and gains) are otherwise identical.
