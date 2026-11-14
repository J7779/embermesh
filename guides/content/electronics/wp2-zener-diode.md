---
id: electronics/wp2-zener-diode
title: Zener diode
category: electronics
tags: [zener diode, electronics]
summary: A Zener diode is a type of diode designed to exploit the Zener effect to affect electric current to flow against the normal direction from anode to cathode, when the voltage across its terminals exceeds a certain characteristic threshold, the Zener voltage.
source: https://en.wikipedia.org/w/index.php?title=Zener_diode&oldid=1373728185
source_title: Wikipedia: Zener diode
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Zener diode", Wikipedia, The Free Encyclopedia (revision 1373728185), https://en.wikipedia.org/w/index.php?title=Zener_diode&oldid=1373728185 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Zener diode

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **Zener diode** is a type of diode designed to exploit the Zener effect to affect electric current to flow against the normal direction from anode to cathode, when the voltage across its terminals exceeds a certain characteristic threshold, the *Zener voltage*.

Zener diodes are manufactured with a variety of Zener voltages, including variable devices. Some types have an abrupt, heavily doped p–n junction with a low Zener voltage, in which case the reverse conduction occurs due to electron quantum tunnelling in the short distance between p and n regions. Diodes with a higher Zener voltage have more lightly doped junctions, causing their mode of operation to involve avalanche breakdown. Both breakdown types are present in Zener diodes with the Zener effect predominating at lower voltages and avalanche breakdown at higher voltages.

Zener diodes are used to generate low-power stabilized supply rails from higher voltages and to provide reference voltages for circuits, especially stabilized power supplies. They are also used to protect circuits from overvoltage, especially electrostatic discharge.

## Operation

A conventional solid-state diode allows significant current if it is reverse biased above its reverse-breakdown voltage. When the reverse-bias breakdown voltage is exceeded, a conventional diode will conduct a high current due to avalanche breakdown. Unless this current is limited by external circuits, the diode may be permanently damaged due to overheating at the small (localized) areas of the semiconductor junction where avalanche breakdown conduction is occurring. A Zener diode exhibits almost the same properties, except the device is specially designed so as to have a reduced breakdown voltage, the *Zener voltage*. By contrast with the conventional device, a reverse-biased Zener diode exhibits a controlled breakdown and allows the current to keep the voltage across the Zener diode close to the Zener breakdown voltage. For example, a diode with a Zener breakdown voltage of 3.2 V exhibits a voltage drop of very nearly 3.2 V across a wide range of reverse currents. The Zener diode is therefore well suited for applications such as the generation of a reference voltage (e.g. for an amplifier stage), or as a voltage stabilizer for low-current applications.

Another mechanism that produces a similar effect is the avalanche effect as in the avalanche diode. The two types of diode are in fact constructed in similar ways and both effects are present in diodes of this type. In silicon diodes up to about 5.6 volts, the Zener effect is the predominant effect and shows a marked negative temperature coefficient. Above 5.6 volts, the avalanche effect dominates and exhibits a positive temperature coefficient.

In a 5.6 V diode, the two effects occur together, and their temperature coefficients nearly cancel each other out, thus the 5.6 V diode is useful in temperature-critical applications. An alternative, which is used for voltage references that need to be highly stable over long periods of time, is to use a Zener diode with a temperature coefficient (TC) of +2 mV/°C (breakdown voltage 6.2–6.3 V) connected in series with a forward-biased silicon diode (or a transistor B–E junction) manufactured on the same chip. The forward-biased diode has a temperature coefficient of −2 mV/°C, causing the TCs to cancel out for a near-zero net temperature coefficient.

It is also worth noting that the temperature coefficient of a 4.7 V Zener diode is close to that of the emitter-base junction of a silicon transistor at around −2 mV/°C, so in a simple regulating circuit where the 4.7 V diode sets the voltage at the base of an NPN transistor (i.e. their coefficients are acting in parallel), the emitter will be at around 4 V and quite stable with temperature.

Modern designs have produced devices with voltages lower than 5.6 V with negligible temperature coefficients,. Higher-voltage devices have temperature coefficients that are approximately proportional to the amount by which the breakdown voltage exceeds 5 V. Thus a 75 V diode has about ten times the coefficient of a 12 V diode.

Zener and avalanche diodes, regardless of breakdown voltage, are usually marketed under the umbrella term of "Zener diode".

Under 5.6 V, where the Zener effect dominates, the IV curve near breakdown is much more rounded, which calls for more care in choosing its biasing conditions. The IV curve for Zeners above 5.6 V (being dominated by avalanche), is much more precise at breakdown.

## Construction

The Zener diode's operation depends on the heavy doping of its p–n junction. The depletion region formed in the diode is very thin (< 1 μm) and the electric field is consequently very high (about 500 kV/m) even for a small reverse bias voltage of about 5 V, allowing electrons to tunnel from the valence band of the p-type material to the conduction band of the n-type material.

At the atomic scale, this tunneling corresponds to the transport of valence-band electrons into the empty conduction-band states, as a result of the reduced barrier between these bands and high electric fields that are induced due to the high levels of doping on both sides. The breakdown voltage can be controlled quite accurately by the doping process. Adding impurities, or doping, changes the behaviour of the semiconductor material in the diode. In the case of Zener diodes, this heavy doping creates a situation where the diode can operate in the breakdown region. While tolerances within 0.07% are available, commonly available tolerances are 5% and 10%. Breakdown voltage for commonly available Zener diodes can vary from 1.2 V to 200 V.

For diodes that are lightly doped, the breakdown is dominated by the avalanche effect rather than the Zener effect. Consequently, the breakdown voltage is higher (over 5.6 V) for these devices.

### Surface Zeners

The emitter–base junction of a bipolar NPN transistor behaves as a Zener diode, with breakdown voltage at about 6.8 V for common bipolar processes and about 10 V for lightly doped base regions in BiCMOS processes. Older processes with poor control of doping characteristics had the variation of Zener voltage up to ±1 V, newer processes using ion implantation can achieve no more than ±0.25 V. The NPN transistor structure can be employed as a *surface Zener diode*, with collector and emitter connected together as its cathode and base region as anode. In this approach the base doping profile usually narrows towards the surface, creating a region with intensified electric field where the avalanche breakdown occurs. Hot carriers produced by acceleration in the intense field can inject into the oxide layer above the junction and become trapped there. The accumulation of trapped charges can then cause 'Zener walkout', a corresponding change of the Zener voltage of the junction. The same effect can be achieved by radiation damage.

The emitter–base Zener diodes can handle only low currents as the energy is dissipated in the base depletion region which is very small. Higher amounts of dissipated energy (higher current for longer time, or a short very high current spike) causes thermal damage to the junction and/or its contacts. Partial damage of the junction can shift its Zener voltage. Total destruction of the Zener junction by overheating it and causing migration of metallization across the junction ("spiking") can be used intentionally as a 'Zener zap' antifuse.

### Subsurface Zeners

A subsurface Zener diode, also called a *buried Zener*, is a device similar to the surface Zener, but the doping and design is such that the avalanche region is located deeper in the structure, typically several micrometers below the oxide. Hot carriers then lose energy by collisions with the semiconductor lattice before reaching the oxide layer and cannot be trapped there. The Zener walkout phenomenon therefore does not occur here, and the buried Zeners have stable voltage over their entire lifetime. Most buried Zeners have breakdown voltage of 5–7 volts. Several different junction structures are used.

_(Later sections of the original article were left out of this offline copy for length: Uses.)_
