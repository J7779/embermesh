---
id: electronics/wp2-voltage-divider
title: Voltage divider
category: electronics
tags: [voltage divider, potential divider, voltage division, electronics]
summary: In electronics, a voltage divider (also known as a potential divider) is a passive linear circuit that produces an output voltage (V_out) that is a fraction of its input voltage (V_in).
source: https://en.wikipedia.org/w/index.php?title=Voltage_divider&oldid=1383599655
source_title: Wikipedia: Voltage divider
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Voltage divider", Wikipedia, The Free Encyclopedia (revision 1383599655), https://en.wikipedia.org/w/index.php?title=Voltage_divider&oldid=1383599655 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Voltage divider

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electronics, a **voltage divider** (also known as a **potential divider**) is a passive linear circuit that produces an output voltage (*V*_out) that is a fraction of its input voltage (*V*_in). **Voltage division** is the result of distributing the input voltage among the components of the divider. A simple example of a voltage divider is two resistors connected in series, with the input voltage applied across the resistor pair and the output voltage emerging from the connection between them.

Resistor voltage dividers are commonly used to create reference voltages, or to reduce the magnitude of a voltage so it can be measured, and may also be used as signal attenuators at low alternating current frequencies. For direct current and relatively low alternating current frequencies, a voltage divider may be sufficiently accurate if made only of resistors; where frequency response over a wide range is required (such as in an oscilloscope probe), a voltage divider may have capacitive elements added to compensate load capacitance. In electric power transmission, a capacitive voltage divider is used for measurement of high voltage.

## General case

A voltage divider referenced to ground is created by connecting two electrical impedances in series, as shown in Figure 1. The input voltage is applied across the series impedances *Z*₁ and *Z*₂ and the output is the voltage across *Z*₂.
*Z*₁ and *Z*₂ may be composed of any combination of elements such as resistors, inductors and capacitors.

If the current in the output wire is zero (I_out=0) then the relationship between the input voltage, *V*_in, and the output voltage, *V*_out, is:
V_out = (Z_2)/(Z_1+Z_2) · V_in

Proof (using Ohm's law):

V_in = I·(Z_1+Z_2)
V_out = I· Z_2
I = (V_in)/(Z_1+Z_2)
V_out = V_in ·(Z_2)/(Z_1+Z_2)

The transfer function (also known as the divider's **voltage ratio**) of this circuit is:
H = (V_out)/(V_in) = (Z_2)/(Z_1+Z_2)

In general this transfer function is a complex, rational function of frequency.

## Examples

### Resistive divider

A resistive divider is the case where both impedances, *Z*₁ and *Z*₂, are purely resistive (Figure 2).

Substituting *Z*₁ = *R*₁ and *Z*₂ = R₂ into the previous expression gives:
V_out = (R_2)/(R_1+R_2) · V_in

If *R*₁ = *R*₂ then
V_out = 1/2 · V_in

If *V*_out = 6 V and *V*_in = 9 V (both commonly used voltages), then:
(V_out)/(V_in) = (R_2)/(R_1+R_2) = 6/9 = 2/3
and by solving using algebra, *R*₂ must be twice the value of *R*₁.

To solve for *R*₁:
R_1 = (R_2 · V_in)/(V_out) - R_2 = R_2 · ((V_in)/(V_out)-1)

To solve for *R*₂:
R_2 = R_1 · frac1 ((V_in)/(V_out)-1)

Any ratio *V*_out / *V*_in greater than 1 is not possible. That is, using resistors alone it is not possible to either invert the voltage or increase *V*_out above *V*_in.

### Low-pass RC filter

Consider a divider consisting of a resistor and capacitor as shown in Figure 3.

Comparing with the general case, we see *Z*₁ = *R* and *Z*₂ is the impedance of the capacitor, given by

Z_2 = -jX_C =(1)/(j ω C),

where *X*_C is the reactance of the capacitor, *C* is the capacitance of the capacitor, *j* is the imaginary unit, and *ω* (omega) is the radian frequency of the input voltage.

This divider will then have the voltage ratio:
(V_out)/(V_in) = (Z_2)/(Z_1 + Z_2) = ((1)/(j ω C))/((1)/(j ω C) + R) = (1)/(1 + j ω R C).

The product *τ* (tau) = *RC* is called the *time constant* of the circuit.

The ratio then depends on frequency, in this case decreasing as frequency increases. This circuit is, in fact, a basic (first-order) low-pass filter. The ratio contains an imaginary number, and actually contains both the amplitude and phase shift information of the filter. To extract just the amplitude ratio, calculate the magnitude of the ratio, that is:

| (V_out)/(V_in) | = (1)/(√(1 + (ω R C)^2)).

### Inductive divider

Inductive dividers split AC input according to inductance:

V_out = (L_2)/(L_1 + L_2) · V_in

(with components in the same positions as Figure 2.)

The above equation is for non-interacting inductors; mutual inductance (as in an autotransformer) will alter the results.

Inductive dividers split AC input according to the reactance of the elements as for the resistive divider above.

### Capacitive divider

Capacitive dividers do not pass DC input.

For an AC input a simple capacitive equation is:

V_out = (Xc_2)/(Xc_1 + Xc_2) · V_in = (1/C_2)/(1/C_1 + 1/C_2) · V_in = (C_1)/(C_1 + C_2) · V_in

(with components in the same positions as Figure 2.)

Any leakage current in the capactive elements requires use of the generalized expression with two impedances. By selection of parallel *R* and *C* elements in the proper proportions, the same division ratio can be maintained over a useful range of frequencies. This is the principle applied in compensated oscilloscope probes to increase measurement bandwidth.

## Loading effect

The output voltage of a voltage divider will vary according to the electric current it is supplying to its external electrical load. The effective source impedance coming from a divider of *Z*₁ and *Z*₂, as above, will be *Z*₁ in parallel with *Z*₂ (sometimes written *Z*₁ || *Z*₂), that is: (*Z*₁ *Z*₂) / (*Z*₁ + *Z*₂) = *HZ*₁.

To obtain a sufficiently stable output voltage, the output current must either be stable (and so be made part of the calculation of the potential divider values) or limited to an appropriately small percentage of the divider's input current. Load sensitivity can be decreased by reducing the impedance of both halves of the divider, though this increases the divider's quiescent input current and results in higher power consumption (and wasted heat) in the divider. Voltage regulators are often used in lieu of passive voltage dividers when it is necessary to accommodate high or fluctuating load currents.

_(Later sections of the original article were left out of this offline copy for length: Applications.)_
