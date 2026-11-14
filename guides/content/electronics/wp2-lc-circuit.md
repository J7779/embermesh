---
id: electronics/wp2-lc-circuit
title: LC circuit
category: electronics
tags: [lc circuit, resonant circuit, tank circuit, tuned circuit, electronics]
summary: An LC circuit, also called a resonant circuit, tank circuit, or tuned circuit, is an electric circuit consisting of an inductor, represented by the letter L, and a capacitor, represented by the letter C, connected together.
source: https://en.wikipedia.org/w/index.php?title=LC_circuit&oldid=1350658867
source_title: Wikipedia: LC circuit
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "LC circuit", Wikipedia, The Free Encyclopedia (revision 1350658867), https://en.wikipedia.org/w/index.php?title=LC_circuit&oldid=1350658867 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# LC circuit

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **LC circuit**, also called a **resonant circuit**, **tank circuit**, or **tuned circuit**, is an electric circuit consisting of an inductor, represented by the letter L, and a capacitor, represented by the letter C, connected together. The circuit can act as an electrical resonator, an electrical analogue of a tuning fork, storing energy oscillating at the circuit's resonant frequency.

LC circuits are used either for generating signals at a particular frequency, or picking out a signal at a particular frequency from a more complex signal; this function is called a bandpass filter. They are key components in many electronic devices, particularly radio equipment, used in circuits such as oscillators, filters, tuners and frequency mixers.

An LC circuit is an idealized model since it assumes there is no dissipation of energy due to resistance. Any practical implementation of an LC circuit will always include loss resulting from small but non-zero resistance within the components and connecting wires. The purpose of an LC circuit is usually to oscillate with minimal damping, so the resistance is made as low as possible. While no practical circuit is without losses, it is nonetheless instructive to study this ideal form of the circuit to gain understanding and physical intuition. For a circuit model incorporating resistance, see RLC circuit.

## Operation

An LC circuit, oscillating at its natural resonant frequency, can store electrical energy. See the animation. A capacitor stores energy in the electric field (E) between its plates, depending on the voltage across it, and an inductor stores energy in its magnetic field (B), depending on the current through it.

If an inductor is connected across a charged capacitor, the voltage across the capacitor will drive a current through the inductor, building up a magnetic field around it. The voltage across the capacitor falls to zero as the charge is used up by the current flow. At this point, the energy stored in the coil's magnetic field induces a voltage across the coil, because inductors oppose changes in current. This induced voltage causes a current to begin to recharge the capacitor with a voltage of opposite polarity to its original charge. Due to Faraday's law, the EMF which drives the current is caused by a decrease in the magnetic field, thus the energy required to charge the capacitor is extracted from the magnetic field. When the magnetic field is completely dissipated the current will stop and the charge will again be stored in the capacitor, with the opposite polarity as before. Then the cycle will begin again, with the current flowing in the opposite direction through the inductor.

The charge flows back and forth between the plates of the capacitor, through the inductor. The energy oscillates back and forth between the capacitor and the inductor until (if not replenished from an external circuit) internal resistance makes the oscillations die out. The tuned circuit's action, known mathematically as a harmonic oscillator, is similar to a pendulum swinging back and forth, or water sloshing back and forth in a tank; for this reason the circuit is also called a *tank circuit*. The natural frequency (that is, the frequency at which it will oscillate when isolated from any other system, as described above) is determined by the capacitance and inductance values. In most applications the tuned circuit is part of a larger circuit which applies alternating current to it, driving continuous oscillations. If the frequency of the applied current is the circuit's natural resonant frequency (natural frequency f_0 below), resonance will occur, and a small driving current can excite large amplitude oscillating voltages and currents. In typical tuned circuits in electronic equipment the oscillations are very fast, from thousands to billions of times per second.

## Resonance effect

Resonance occurs when an LC circuit is driven from an external source at an angular frequency *ω*₀ at which the inductive and capacitive reactances are equal in magnitude. The frequency at which this equality holds for the particular circuit is called the resonant frequency. The resonant frequency of the LC circuit is

ω_0 = (1)/(√(LC)),

where L is the inductance in henries, and C is the capacitance in farads. The angular frequency *ω*₀ has units of radians per second.

The equivalent frequency in units of hertz is

f_0 = (ω_0)/(2 π) = (1)/(2 π √(LC)).

## Applications

The resonance effect of the LC circuit has many important applications in signal processing and communications systems.

- The most common application of tank circuits is **tuning** radio transmitters and receivers. For example, when tuning a radio to a particular station, the LC circuits are set at resonance for that particular carrier frequency.
- A series resonant circuit provides **voltage magnification**.
- A parallel resonant circuit provides **current magnification**.
- A parallel resonant circuit can be used as load impedance in output circuits of RF amplifiers. Due to high impedance, the gain of amplifier is maximum at resonant frequency.
- Both parallel and series resonant circuits are used in induction heating.

LC circuits behave as electronic resonators, which are a key component in many applications:

- Amplifiers
- Contactless cards
- Electronic article surveillance (security tags)
- Filters
- Foster–Seeley discriminator
- Graphics tablets
- Mixers
- Oscillators
- Tuners

## Time domain solution

### Kirchhoff's laws

By Kirchhoff's voltage law, the voltage V_C across the capacitor plus the voltage V_L across the inductor must equal zero:

V_C + V_L = 0.

Likewise, by Kirchhoff's current law, the current through the capacitor equals the current through the inductor:

I_C = I_L.

From the constitutive relations for the circuit elements, we also know that

V_L(t) = L (dI_L)/(dt),; I_C(t) = C (dV_C)/(dt).

### Differential equation

Rearranging and substituting gives the second order differential equation

(d^2)/(dt^2)I(t) + 1/LC I(t) = 0.

The parameter *ω*₀, the resonant angular frequency, is defined as

ω_0 = frac1√(LC).

Using this can simplify the differential equation:

(d^2)/(dt^2)I(t) + ω_0^2 I(t) = 0.

The associated Laplace transform is

s^2 + ω_0^2 = 0,

thus

s = ± j ω_0,

where j is the imaginary unit.

### Solution

Thus, the complete solution to the differential equation is

I(t) = Ae^+j ω_0 t + Be^-j ω_0 t

and can be solved for A and B by considering the initial conditions. Since the exponential is complex, the solution represents a sinusoidal alternating current. Since the electric current I is a physical quantity, it must be real-valued. As a result, it can be shown that the constants A and B must be complex conjugates:

A = B^*.

Now let

A = (I_0)/(2) e^+j φ.

Therefore,

B = (I_0)/(2) e^-j φ.

Next, we can use Euler's formula to obtain a real sinusoid with amplitude *I*₀, angular frequency *ω*₀ = 1/, and phase angle φ.

Thus, the resulting solution becomes

I(t) = I_0 cos(ω_0 t + φ ),

V_L(t) = L dI/dt = -ω_0 L I_0 sin(ω_0 t + φ ).

### Initial conditions

The initial conditions that would satisfy this result are

I(0) = I_0 cos φ,

V_L(0) = L dI/dtBigg|_t=0 = -ω_0 L I_0 sin φ.

_(Later sections of the original article were left out of this offline copy for length: Series circuit, Parallel circuit, Laplace solution.)_
