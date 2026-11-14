---
id: electronics/wp2-resistor
title: Resistor
category: electronics
tags: [resistor, electronics]
summary: A resistor is a passive two-terminal electronic component that implements electrical resistance as a circuit element.
source: https://en.wikipedia.org/w/index.php?title=Resistor&oldid=1384632088
source_title: Wikipedia: Resistor
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Resistor", Wikipedia, The Free Encyclopedia (revision 1384632088), https://en.wikipedia.org/w/index.php?title=Resistor&oldid=1384632088 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Resistor

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **resistor** is a passive two-terminal electronic component that implements electrical resistance as a circuit element. In electronic circuits, resistors are used to reduce current flow, adjust signal levels, to divide voltages, bias active elements, and terminate transmission lines, among other uses. High-power resistors that can dissipate many watts of electrical power as heat may be used as part of motor controls, in power distribution systems, or as test loads for generators.
Fixed resistors have resistances that only change slightly with temperature, time or operating voltage. Variable resistors can be used to adjust circuit elements (such as a volume control or a lamp dimmer), or as sensing devices for heat, light, humidity, force, or chemical activity.

Resistors are common elements of electrical networks and electronic circuits and are ubiquitous in electronic equipment. Practical resistors as discrete components can be composed of various compounds and forms. Resistors are also implemented within integrated circuits.

The electrical function of a resistor is specified by its resistance: common commercial resistors are manufactured over a range of more than nine orders of magnitude. The nominal value of the resistance falls within the manufacturing tolerance, indicated on the component.

## Electronic symbols and notation

Two typical schematic diagram symbols are as follows:

The notation to state a resistor's value in a circuit diagram varies.

One common scheme is the RKM code following IEC 60062. Rather than using a decimal separator, this notation uses a letter loosely associated with SI prefixes corresponding with the part's resistance. For example, *8K2* as part marking code, in a circuit diagram or in a bill of materials (BOM) indicates a resistor value of 8.2 kΩ. Additional zeros imply a tighter tolerance, for example *15M0* for three significant digits. When the value can be expressed without the need for a prefix (that is, multiplicator 1), an "R" is used instead of the decimal separator. For example, *1R2* indicates 1.2 Ω, and *18R* indicates 18 Ω.

## Theory of operation

### Ohm's law

An *ideal resistor* (i.e. a resistance without reactance) obeys Ohm's law:

V=I · R.

Ohm's law states that the voltage (V) across a resistor is proportional to the current (I) passing through it, where the constant of proportionality is the resistance (R). For example, if a 300-ohm resistor is attached across the terminals of a 12-volt battery, then a current of 12 / 300 = 0.04 amperes flows through that resistor.

The ohm (symbol: Ω) is the SI unit of electrical resistance, named after Georg Simon Ohm. An ohm is equivalent to a volt per ampere. Since resistors are specified and manufactured over a very large range of values, the derived units of milliohm (1 mΩ = 10⁻³ Ω), kilohm (1 kΩ = 10³ Ω), and megohm (1 MΩ = 10⁶ Ω) are also in common usage.

### Series and parallel resistors

The total resistance of resistors connected in series is the sum of their individual resistance values.

R_eq = Σ_i=1^n R_i = R_1 + R_2 + … + R_n.
Equivalently, the total conductance of resistors connected in series is the reciprocal of the sum of the reciprocals of the individual conductances.
G_eq = (Σ_i=1^n(1)/(G_i))^-1 = (1over G_1 + 1over G_2 + 1over G_3 + … + 1over G_n)^-1

The total resistance of resistors connected in parallel is the reciprocal of the sum of the reciprocals of the individual resistances.

R_eq = (Σ_i=1^n(1)/(R_i))^-1 = (1over R_1 + 1over R_2 + 1over R_3 + … + 1over R_n)^-1
Equivalently, the total conductance of resistors connected in parallel is the sum of the individual conductances.
G_eq = Σ_i=1^n G_i = G_1 + G_2 + … + G_n.

For example, a 10 ohm resistor connected in parallel with a 5 ohm resistor and a 15 ohm resistor produces 1/1/10 + 1/5 + 1/15 ohms of resistance, or 30/11 = 2.727 ohms.

A resistor network that is a combination of parallel and series connections can be broken up into smaller parts that are either one or the other. Some complex networks of resistors cannot be resolved in this manner, requiring more sophisticated circuit analysis. Generally, the Y-Δ transform, or matrix methods can be used to solve such problems.

### Power dissipation

At any instant, the power *P* (watts) consumed by a resistor of resistance *R* (ohms) is calculated as:
P = I V = I^2 R = (V^2)/(R) = G V^2
where *V* (volts) is the voltage across the resistor and *I* (amps) is the current flowing through it. Using Ohm's law, the two other forms can be derived. This power is converted into heat which must be dissipated by the resistor's package before its temperature rises excessively.

Resistors are rated according to their maximum power dissipation. Discrete resistors in solid-state electronic systems are typically rated as 1/10, 1/8, or 1/4 watt. They usually absorb much less than a watt of electrical power and require little attention to their power rating.

Power resistors are required to dissipate substantial amounts of power and are typically used in power supplies, power conversion circuits, and power amplifiers; this designation is loosely applied to resistors with power ratings of 1 watt or greater. Power resistors are physically larger and may not use the preferred values, color codes, and external packages described below.

If the average power dissipated by a resistor is more than its power rating, damage to the resistor may occur, permanently altering its resistance; this is distinct from the reversible change in resistance due to its temperature coefficient when it warms. Excessive power dissipation may raise the temperature of the resistor to a point where it can burn the circuit board or adjacent components, or even cause a fire. There are flameproof resistors that will not produce flames with any overload of any duration.

Resistors may be specified with higher rated dissipation than is experienced in service to account for poor air circulation, high altitude, or high operating temperature.

All resistors have a maximum voltage rating; this may limit the power dissipation for higher resistance values. For instance, among 1/4 watt resistors (a very common sort of leaded resistor) one is listed with a resistance of 100 MΩ and a maximum rated voltage of 750 V. However even placing 750 V across a 100 MΩ resistor continuously would only result in a power dissipation of less than 6 mW, making the nominal 1/4 watt rating meaningless.

## Nonideal properties

Practical resistors have a series inductance and a small parallel capacitance; these specifications can be important in high-frequency applications. And while even an ideal resistor inherently has Johnson noise, some resistors have worse noise characteristics and so may be an issue for low-noise amplifiers or other sensitive electronics.

In some precision applications, the temperature coefficient of the resistance may also be of concern.

The unwanted inductance, excess noise, and temperature coefficient are mainly dependent on the technology used in manufacturing the resistor. They are not normally specified individually for a particular family of resistors manufactured using a particular technology. A family of discrete resistors may also be characterized according to its form factor, that is, the size of the device and the position of its leads (or terminals). This is relevant in the practical manufacturing of circuits that may use them.

Practical resistors are also specified as having a maximum power rating which must exceed the anticipated power dissipation of that resistor in a particular circuit: this is mainly of concern in power electronics applications.
Resistors with higher power ratings are physically larger and may require heat sinks. In a high-voltage circuit, attention must sometimes be paid to the rated maximum working voltage of the resistor. While there is no minimum working voltage for a given resistor, failure to account for a resistor's maximum rating may cause the resistor to incinerate when current is run through it.

_(Later sections of the original article were left out of this offline copy for length: Fixed resistors, Variable resistors, Measurement, Resistor marking, Common usage patterns, Electrical and thermal noise, Failure modes.)_
