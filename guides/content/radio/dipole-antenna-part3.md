---
id: radio/dipole-antenna-part3
title: Dipole antenna (part 3 of 3)
category: radio
tags: [radio, antenna, dipole, half-wave, HF]
summary: "The dipole antenna: half-wave dipole lengths, feed impedance, radiation pattern and variants. (continued, part 3)"
source: "https://en.wikipedia.org/w/index.php?title=Dipole_antenna&oldid=1382744782"
source_title: Dipole antenna (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Dipole antenna' (rev 1382744782), Wikipedia, https://en.wikipedia.org/wiki/Dipole_antenna. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
part_of: radio/dipole-antenna
order: 3
---

# Dipole antenna (part 3 of 3)

## Detailed calculation of dipole feedpoint impedance

The impedance seen at the feedpoint of a dipole of various lengths has been plotted above, in terms of the real (resistive) component Rdipole and the imaginary (reactive) component j Xdipole of that impedance. For the case of an antenna with perfect conductors (no Ohmic loss), Rdipole is identical to the radiation resistance, which can more easily be computed from the total power in the far-field radiation pattern for a given applied current as we showed for the short dipole. The calculation of Xdipole is more difficult.

### Induced EMF method

Using the *induced EMF method* closed form expressions are obtained for both components of the feedpoint impedance; such results are plotted above. The solution depends on an assumption for the form of the current distribution along the antenna conductors. For wavelength-to-element diameter ratios greater than about 60, the current distribution along each antenna element of length 1/ 2  L is very well approximated as having the form of the sine function at points along the antenna z, with the current reaching zero at the elements' ends, where *z* = ±+1/2 *L*, as follows:

`I(z) = Asin(k(1/2 L − |z|)),`

where k is the wavenumber given by *k* = 2 *π*/ *λ*  = 2 *π f*/ *c* , and the amplitude A is set to match a specified driving point current at *z* = 0.

In cases where an approximately sinusoidal current distribution can be assumed, this method solves for the driving point impedance in closed form using the cosine and sine integral functions Si(*x*) and Ci(*x*). For a dipole of total length L, the resistive and reactive components of the driving point impedance can be expressed as:

`R_(𝖽𝗂𝗉𝗈𝗅𝖾) = (ζ_0)/(2π(sin)^2(1/2 kL)) {γ_e + ln(kL) − Ci(kL) + 1/2 sin(kL) [ + Si(2kL) − 2Si(kL)]; + 1/2 cos(kL) [ + Ci(2kL) − 2Ci(kL) + γ_e + ln(1/2 kL)]},; X_(𝖽𝗂𝗉𝗈𝗅𝖾) = (ζ_0)/(2π(sin)^2(1/2 kL)) { + Si(kL) + 1/2 cos(kL) [ − Si(2kL) + 2Si(kL)]; + 1/2 sin(kL) [ + Ci(2kL) − 2Ci(kL) + Ci((2ka^2)/L)]},`

where a is the radius of the conductors, k is again the wavenumber as defined above, ζ0 is the impedance of empty space, which very nearly the same as impedance of air: ζ0 ≈ 377 Ω, and `γ_e = 0.57721566…` is Euler's constant. There is an equivalent alternate form favored by some authors that uses a different function, Cin.

### Integral methods

The induced EMF method is dependent on the assumption of a sinusoidal current distribution, delivering an accuracy better than about 10% as long as the wavelength-to-element diameter ratio is greater than about 60. However, for yet larger conductors numerical solutions are required which solve for the conductor's current distribution (rather than *assuming* a sinusoidal pattern). This can be based on approximating solutions for either *Pocklington's integrodifferential equation* or the *Hallén integral equation*. These approaches also have greater generality, not being limited to linear conductors.

Numerical solution of either is performed using the *moment method solution* which requires expansion of that current into a set of basis functions; one simple (but not the best) choice, for instance, is to break up the conductor into N segments with a constant current assumed along each. After setting an appropriate weighting function the cost may be minimized through the inversion of a *N*×*N* matrix. Determination of each matrix element requires at least one double integration involving the weighting functions, which may become computationally intensive. These are simplified if the weighting functions are simply delta functions, which corresponds to fitting the boundary conditions for the current along the conductor at only N discrete points. Then the *N*×*N* matrix must be inverted, which is also computationally intensive as N increases. In one simple example, Balanis (2011) performs this computation to find the antenna impedance with different N using Pocklington's method, and finds that with *N* > 60 the solutions approach their limiting values to within a few percent.

## Sources for elementary, short, and half-wave dipoles

- *Feynman, Richard. Leighton; Sands (eds.). *Feynman's Lectures on Physics*. Addison-Wesley.*

- *Panofsky, W.; Phillips, M. *Classical Electricity and Magnetism*. Addison-Wesley.*

- *Orfanidis, Sophocles J. *Electromagnetic Waves and Antennas* (online textbook manuscript). Rutgers University. Archived from the original on 20 December 2008. Retrieved 25 December 2006 – via ece.rutgers.edu.*

- *N0HR. "Wire antenna resources for ham radio". *n0hr.com*.* — Wire antenna web links including off center fed dipole (OCFD), dipole calculators and construction sites

- *"Linear antennas" (PDF). *stevens-tech.edu*. Archived from the original (PDF) on 7 September 2006.*

- *"asc antenna slides" (PDF). *nt.hs-bremen.de*. Archived from the original (PDF) on 26 September 2007.*

- *Fitzpatrick, Richard (2 February 2006). "The Hertzian dipole". Electromagnetic radiation. *farside.ph.utexas.edu* (notes for an intermediate level physics course). PHY 352K – *Classical Electromagnetism*. Austin, TX: University of Texas.*

- *Buchwald, J.Z. (c. 2002). Reflections on Hertz and the Hertzian Dipole (Report). MIT and Dibner Inst. Archived from the original on 26 May 2005. Retrieved 13 March 2011.*
