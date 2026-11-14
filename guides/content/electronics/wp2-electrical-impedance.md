---
id: electronics/wp2-electrical-impedance
title: Electrical impedance
category: electronics
tags: [electrical impedance, impedance, electronics]
summary: In electrical engineering, impedance is the opposition to alternating current presented by the combined effect of resistance and reactance in a circuit.
source: https://en.wikipedia.org/w/index.php?title=Electrical_impedance&oldid=1363009092
source_title: Wikipedia: Electrical impedance
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Electrical impedance", Wikipedia, The Free Encyclopedia (revision 1363009092), https://en.wikipedia.org/w/index.php?title=Electrical_impedance&oldid=1363009092 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Electrical impedance

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electrical engineering, **impedance** is the opposition to alternating current presented by the combined effect of resistance and reactance in a circuit.

Quantitatively, the impedance of a two-terminal circuit element is the ratio of the complex representation of the sinusoidal voltage between its terminals, to the complex representation of the current flowing through it. In general, it depends upon the frequency of the sinusoidal voltage.

Impedance extends the concept of resistance to alternating current (AC) circuits, and possesses both magnitude and phase, unlike resistance, which has only magnitude.

Impedance can be represented as a complex number, with the same units as resistance, for which the SI unit is the ohm (Ω).
Its symbol is usually *Z*, and it may be represented by writing its magnitude and phase in the polar form |*Z*|*∠θ*. However, Cartesian complex number representation is often more powerful for circuit analysis purposes.

The notion of impedance is useful for performing AC analysis of electrical networks, because it allows relating sinusoidal voltages and currents by a simple linear law.
In multiple port networks, the two-terminal definition of impedance is inadequate, but the complex voltages at the ports and the currents flowing through them are still linearly related by the impedance matrix.

The reciprocal of impedance is admittance, whose SI unit is the siemens.

Instruments used to measure the electrical impedance are called impedance analyzers.

## Introduction

In addition to resistance as seen in DC circuits, impedance in AC circuits includes the effects of the induction of voltages in conductors by the magnetic fields (inductance), and the electrostatic storage of charge induced by voltages between conductors (capacitance). The impedance caused by these two effects is collectively referred to as reactance and forms the imaginary part of complex impedance whereas resistance forms the real part.

## Complex impedance

The impedance of a two-terminal circuit element is represented as a complex quantity Z. The polar form conveniently captures both magnitude and phase characteristics as

Z = |Z| e^j arg(Z)

where the magnitude |Z| represents the ratio of the voltage difference amplitude to the amplitude of the current, while the argument arg(Z) (commonly given the symbol θ) gives the phase difference between voltage and current. In electrical engineering, the letter i is used for electric current, so the imaginary unit is instead represented by the letter j.

In Cartesian form, impedance is defined as

Z = R + jX

where the real part of impedance is the resistance R and the imaginary part is the reactance X.

Where it is needed to add or subtract impedances, the cartesian form is more convenient; but when quantities are multiplied or divided, the calculation becomes simpler if the polar form is used. A circuit calculation, such as finding the total impedance of two impedances in parallel, may require conversion between forms several times during the calculation. Conversion between the forms follows the normal conversion rules of complex numbers.

## Complex voltage and current

To simplify calculations, sinusoidal voltage and current waves are commonly represented as complex-valued functions of time denoted as V and I.

V = |V| e^j(ω t + φ_V),; I = |I| e^j(ω t + φ_I).

The impedance of a bipolar circuit is defined as the ratio of these quantities:

Z = V/I = (|V|)/(|I|)e^j(φ_V - φ_I).

Hence, denoting θ = φ_V - φ_I, we have

|V| = |I| |Z|,; φ_V = φ_I + θ.

The magnitude equation is the familiar Ohm's law applied to the voltage and current amplitudes, while the second equation defines the phase relationship.

### Validity of complex representation

This representation using complex exponentials may be justified by noting that (by Euler's formula):

cos(ω t + φ) = 1/2 [ e^j(ω t + φ) + e^-j(ω t + φ)]

The real-valued sinusoidal function representing either voltage or current may be broken into two complex-valued functions. By the principle of superposition, we may analyse the behaviour of the sinusoid on the left-hand side by analysing the behaviour of the two complex terms on the right-hand side. Given the symmetry, we only need to perform the analysis for one right-hand term. The results are identical for the other. At the end of any calculation, we may return to real-valued sinusoids by further noting that

cos(ω t + φ) = Re \ e^j(ω t + φ) \

### Ohm's law

The meaning of electrical impedance can be understood by substituting it into Ohm's law. Assuming a two-terminal circuit element with impedance Z is driven by a sinusoidal voltage or current as above, there holds

V = I Z = I |Z| e^j arg (Z)

The magnitude of the impedance |Z| acts just like resistance, giving the drop in voltage amplitude across an impedance Z for a given current I. The phase factor tells us that the current lags the voltage by a phase θ = arg(Z) (i.e., in the time domain, the current signal is shifted (θ)/(2 π) T later with respect to the voltage signal).

Just as impedance extends Ohm's law to cover AC circuits, other results from DC circuit analysis, such as voltage division, current division, Thévenin's theorem and Norton's theorem, can also be extended to AC circuits by replacing resistance with impedance.

### Phasors

A phasor is represented by a constant complex number, usually expressed in exponential form, representing the complex amplitude (magnitude and phase) of a sinusoidal function of time. Phasors are used by electrical engineers to simplify computations involving sinusoids (such as in AC circuits), where they can often reduce a differential equation problem to an algebraic one.

The impedance of a circuit element can be defined as the ratio of the phasor voltage across the element to the phasor current through the element, as determined by the relative amplitudes and phases of the voltage and current. This is identical to the definition from Ohm's law given above, recognising that the factors of e^jω t cancel.

## Combining impedances

The total impedance of many simple networks of components can be calculated using the rules for combining impedances in series and parallel. The rules are identical to those for combining resistances, except that the numbers in general are complex numbers. The general case, however, requires equivalent impedance transforms in addition to series and parallel.

### Series combination

For components connected in series, the current through each circuit element is the same; the total impedance is the sum of the component impedances.

Z_eq = Z_1 + Z_2 + … + Z_n

Or explicitly in real and imaginary terms:
Z_eq = R + jX = (R_1 + R_2 + … + R_n) + j(X_1 + X_2 + … + X_n)

### Parallel combination

For components connected in parallel, the voltage across each circuit element is the same; the ratio of currents through any two elements is the inverse ratio of their impedances.

Hence the inverse total impedance is the sum of the inverses of the component impedances:
frac1Z_eq = (1)/(Z_1) + (1)/(Z_2) + … + (1)/(Z_n)

or, when n = 2:
frac1Z_eq = (1)/(Z_1) + (1)/(Z_2) = (Z_1 + Z_2)/(Z_1 Z_2)
Z_eq = (Z_1 Z_2)/(Z_1 + Z_2)

The equivalent impedance Z_eq can be calculated in terms of the equivalent series resistance R_eq and reactance X_eq.

Z_eq = R_eq + j X_eq; R_eq = ((X_1 R_2 + X_2 R_1) (X_1 + X_2) + (R_1 R_2 - X_1 X_2) (R_1 + R_2))/((R_1 + R_2)^2 + (X_1 + X_2)^2); X_eq = ((X_1 R_2 + X_2 R_1) (R_1 + R_2) - (R_1 R_2 - X_1 X_2) (X_1 + X_2))/((R_1 + R_2)^2 + (X_1 + X_2)^2)

## Variable impedance

In general, neither impedance nor admittance can vary with time, since they are defined for complex exponentials in which. If the complex exponential voltage to current ratio changes over time or amplitude, the circuit element cannot be described using the frequency domain. However, many components and systems (e.g., varicaps that are used in radio tuners) may exhibit non-linear or time-varying voltage to current ratios that seem to be linear time-invariant (LTI) for small signals and over small observation windows, so they can be roughly described as if they had a time-varying impedance. This description is an approximation: Over large signal swings or wide observation windows, the voltage to current relationship will not be LTI and cannot be described by impedance.

_(Later sections of the original article were left out of this offline copy for length: Device examples, Generalised s-plane impedance, Resistance vs reactance, Measurement.)_
