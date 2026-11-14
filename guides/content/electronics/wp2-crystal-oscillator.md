---
id: electronics/wp2-crystal-oscillator
title: Crystal oscillator
category: electronics
tags: [crystal oscillator, electronics]
summary: A crystal oscillator is an electronic oscillator circuit that uses a piezoelectric crystal as a frequency-selective element.
source: https://en.wikipedia.org/w/index.php?title=Crystal_oscillator&oldid=1384673722
source_title: Wikipedia: Crystal oscillator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Crystal oscillator", Wikipedia, The Free Encyclopedia (revision 1384673722), https://en.wikipedia.org/w/index.php?title=Crystal_oscillator&oldid=1384673722 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Crystal oscillator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **crystal oscillator** is an electronic oscillator circuit that uses a piezoelectric crystal as a frequency-selective element. The oscillator frequency is often used to keep track of time, as in quartz wristwatches, to provide a stable clock signal for digital integrated circuits, and to stabilize frequencies for radio transmitters and receivers. The most common type of piezoelectric resonator used is a quartz crystal, so oscillator circuits incorporating them became known as crystal oscillators. However, other piezoelectric materials including polycrystalline ceramics are used in similar circuits.

A crystal oscillator relies on the slight change in shape of a quartz crystal under an electric field, a property known as inverse piezoelectricity. A voltage applied to the electrodes on the crystal causes it to change shape; when the voltage is removed, the crystal generates a small voltage as it elastically returns to its original shape. The quartz oscillates at a stable resonant frequency (relative to other low-priced oscillators) with frequency accuracy measured in parts per million (ppm). It behaves like an RLC circuit, but with a much higher Q factor (lower energy loss on each cycle of oscillation and higher frequency selectivity) than can be reliably achieved with discrete capacitors (C) and inductors (L), which suffer from parasitic resistance (R). Once a quartz crystal is adjusted to a particular frequency (which is affected by the mass of electrodes attached to the crystal, the orientation of the crystal, temperature and other factors), it maintains that frequency with high stability.

Quartz crystals are manufactured for frequencies from a few tens of kilohertz to hundreds of megahertz. As of 2003, around two billion crystals were manufactured annually. Most are used for consumer devices such as wristwatches, clocks, radios, computers, and cellphones. However, in applications where small size and weight is needed crystals can be replaced by thin-film bulk acoustic resonators, specifically if ultra-high frequency (more than roughly 1.5 GHz) resonance is needed. Quartz crystals are also found inside test and measurement equipment, such as counters, signal generators, and oscilloscopes.

## Principle

A crystal is a solid in which the constituent atoms, molecules, or ions are packed in a regularly ordered, repeating pattern extending in all three spatial dimensions.

Almost any object made of an elastic material could be used like a crystal, with appropriate transducers, since all objects have natural resonant frequencies of vibration. For example, steel is very elastic and has a high speed of sound. It was often used in mechanical filters before quartz. The resonant frequency depends on size, shape, elasticity, and the speed of sound in the material. High-frequency crystals are typically cut in the shape of a simple rectangle or circular disk. Low-frequency crystals, such as those used in digital watches, are typically cut in the shape of a tuning fork. For applications not needing very precise timing, a low-cost ceramic resonator is often used in place of a quartz crystal.

When a crystal of quartz is properly cut and mounted, it can be made to distort in an electric field by applying a voltage to an electrode near or on the crystal. This property is known as inverse piezoelectricity. When the field is removed, the quartz generates an electric field as it returns to its previous shape, and this can generate a voltage. The result is that a quartz crystal behaves like an RLC circuit, composed of an inductor, capacitor and resistor, with a precise resonant frequency.

Quartz has the further advantage that its elastic constants and its size change in such a way that the frequency dependence on temperature can be very low. The specific characteristics depend on the mode of vibration and the angle at which the quartz is cut (relative to its crystallographic axes). Therefore, the resonant frequency of the plate, which depends on its size, does not change much. This means that a quartz clock, filter or oscillator remains accurate. For critical applications the quartz oscillator is mounted in a temperature-controlled container, called a crystal oven, and can also be mounted on shock absorbers to prevent perturbation by external mechanical vibrations.

## Modeling

### Electrical model

A quartz crystal can be modeled as an electrical network with low-impedance (series) and high-impedance (parallel) resonance points spaced closely together. Mathematically, using the Laplace transform, the impedance of this network can be written as:

Z(s) = ( (1)/(s· C_1)+s· L_1+R_1 ) \| ( (1)/(s· C_0) ).,

or

Z(s) = fracs^2 + s(R_1)/(L_1) + ω_s^2(s · C_0)[s^2 + s(R_1)/(L_1) + ω_p^2]; [2pt] Rightarrow ω_s = (1)/(√(L_1 · C_1)), ω_p = √((C_1 + C_0)/(L_1 · C_1 · C_0)) = ω_s √(1 + (C_1)/(C_0)) ≈ ω_s (1 + (C_1)/(2 C_0)) (C_0 gg C_1)

where s is the complex frequency (s=jω), ω_s is the series resonant angular frequency, and ω_p is the parallel resonant angular frequency.

Adding capacitance across a crystal causes the (parallel) resonant frequency to decrease. Adding inductance across a crystal causes the (parallel) resonant frequency to increase. These effects can be used to adjust the frequency at which a crystal oscillates. Crystal manufacturers normally cut and trim their crystals to have a specified resonant frequency with a known "load" capacitance added to the crystal. For example, a crystal intended for a 6 pF load has its specified parallel resonant frequency when a 6.0 pF capacitor is placed across it. Without the load capacitance, the resonant frequency is higher.

### Resonance modes

A quartz crystal provides both series and parallel resonance. The series resonance is a few kilohertz lower than the parallel one. Crystals below 30 MHz are generally operated between series and parallel resonance, which means that the crystal appears as an inductive reactance in operation, this inductance forming a parallel resonant circuit with externally connected parallel capacitance.

Any small additional capacitance in parallel with the crystal pulls the frequency lower. Moreover, the effective inductive reactance of the crystal can be reduced by adding a capacitor in series with the crystal. This latter technique can provide a useful method of trimming the oscillatory frequency within a narrow range; in this case inserting a capacitor in series with the crystal raises the frequency of oscillation. For a crystal to operate at its specified frequency, the electronic circuit has to be exactly that specified by the crystal manufacturer. Note that these points imply a subtlety concerning crystal oscillators in this frequency range: the crystal does not usually oscillate at precisely either of its resonant frequencies.

Crystals above 30 MHz (up to >200 MHz) are generally operated at series resonance where the impedance appears at its minimum and equal to the series resistance. For these crystals the series resistance is specified (<100 Ω) instead of the parallel capacitance. To reach higher frequencies, a crystal can be made to vibrate at one of its overtone modes, which occur near multiples of the fundamental resonant frequency. Only odd numbered overtones are used. Such a crystal is referred to as a 3rd, 5th, or even 7th overtone crystal. To accomplish this, the oscillator circuit usually includes additional LC circuits to select the desired overtone.

### Temperature effects

A crystal's frequency characteristic depends on the shape or "cut" of the crystal. A tuning-fork crystal is usually cut such that its frequency dependence on temperature is quadratic with the curve's peak around 25 °C; known as an AT cut. This means that such a tuning-fork crystal oscillator resonates closest to its target frequency at room temperature, and this temperature is where the crystal's frequency is least sensitive to (most stable against) changes in temperature. However, the crystal's frequency slows when the temperature either increases or decreases from room temperature. A common parabolic coefficient for a 32 kHz tuning-fork crystal is −0.04 ppm/°C²:

f = f_0[1 - 0.04~ppm/^circC^2 · (T - T_0)^2].

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Modeling, Crystal oscillator circuits, Commonly used crystal frequencies, Crystal structures and materials, Stability, Aging, Crystal cuts, Circuit notations and abbreviations.)_
