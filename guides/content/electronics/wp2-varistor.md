---
id: electronics/wp2-varistor
title: Varistor
category: electronics
tags: [varistor, voltage-dependent resistor (vdr), metal-oxide varistor, mov, electronics]
summary: A varistor (a.k.a. voltage-dependent resistor (VDR)) is a surge protecting electronic component with an electrical resistance that varies with the applied voltage.
source: https://en.wikipedia.org/w/index.php?title=Varistor&oldid=1337405180
source_title: Wikipedia: Varistor
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Varistor", Wikipedia, The Free Encyclopedia (revision 1337405180), https://en.wikipedia.org/w/index.php?title=Varistor&oldid=1337405180 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Varistor

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **varistor** (a.k.a. **voltage-dependent resistor (VDR)**) is a surge protecting electronic component with an electrical resistance that varies with the applied voltage. It has a nonlinear, non-ohmic current–voltage characteristic that is similar to that of a diode. Unlike a diode however, it has the same characteristic for both directions of traversing current. Traditionally, varistors were constructed by connecting two rectifiers, such as the copper-oxide or germanium-oxide rectifier in antiparallel configuration. At low voltage the varistor has a high electrical resistance which decreases as the voltage is raised. Modern varistors are primarily based on sintered ceramic metal-oxide materials which exhibit directional behavior only on a microscopic scale. This type is commonly known as the **metal-oxide varistor** (**MOV**).

Varistors are used as control or compensation elements in circuits either to provide optimal operating conditions or to protect against excessive transient voltages. When used as protection devices, they shunt the current created by the excessive voltage away from sensitive components when triggered.

The name *varistor* is a portmanteau of *varying resistor*. The term is only used for non-ohmic varying resistors. Variable resistors, such as the potentiometer and the rheostat, have ohmic characteristics.

## Composition, properties, and operation of the metal-oxide varistor

The most common modern type of varistor is the metal-oxide varistor (MOV). This type contains a ceramic mass of zinc oxide (ZnO) grains, in a matrix of other metal oxides, such as small amounts of bismuth, cobalt, manganese oxides, sandwiched between two metal plates, which constitute the electrodes of the device. The boundary between each grain and a neighbor forms a diode junction, which allows current to flow in only one direction. The accumulation of randomly oriented grains is electrically equivalent to a network of back-to-back diode pairs, each pair in parallel with many other pairs.

When a small voltage is applied across the electrodes, only a tiny current flows, caused by reverse leakage through the diode junctions. When a large voltage is applied, the diode junction breaks down due to a combination of thermionic emission and electron tunneling, resulting in a large current flow. The result of this behavior is a nonlinear current-voltage characteristic, in which the MOV has a high resistance at low voltages and a low resistance at high voltages.

### Electrical characteristics

A varistor remains non-conductive as a shunt-mode device during normal operation when the voltage across it remains well below its "clamping voltage", thus varistors are typically used for suppressing line voltage surges. Varistors can fail for either of two reasons.

A catastrophic failure occurs from not successfully limiting a very large surge from an event like a lightning strike, where the energy involved is many orders of magnitude greater than the varistor can handle. Follow-through current resulting from a strike may melt, burn, or even vaporize the varistor. This thermal runaway is due to a lack of conformity in individual grain-boundary junctions, which leads to the failure of dominant current paths under thermal stress when the energy in a transient pulse (normally measured in joules) is too high (i.e. significantly exceeds the manufacture's "Absolute Maximum Ratings"). The probability of catastrophic failure can be reduced by increasing the rating, or using specially selected MOVs in parallel.

Cumulative degradation occurs as more surges happen. For historical reasons, many MOVs have been incorrectly specified allowing frequent swells to also degrade capacity. In this condition the varistor is not visibly damaged and outwardly appears functional (no catastrophic failure), but it no longer offers protection. Eventually, it proceeds into a shorted circuit condition as the energy discharges create a conductive channel through the oxides.

The main parameter affecting varistor life expectancy is its energy (Joule) rating. Increasing the energy rating raises the number of (defined maximum size) transient pulses that it can accommodate exponentially as well as the cumulative sum of energy from clamping lesser pulses. As these pulses occur, the "clamping voltage" it provides during each event decreases, and a varistor is typically deemed to be functionally degraded when its "clamping voltage" has changed by 10%. Manufacturer's life-expectancy charts relate current, severity, and number of transients to make failure predictions based on the total energy dissipated over the life of the part.

In consumer electronics, particularly surge protectors, the MOV varistor size employed is small enough that eventually failure is expected. Other applications, such as power transmission, use VDRs of different construction in multiple configurations engineered for long life span.

### Voltage rating

MOVs are specified according to the voltage range that they can tolerate without damage. Other important parameters are the varistor's energy rating in joules, operating voltage, response time, maximum current, and breakdown (clamping) voltage. Energy rating is often defined using standardized transients such as 8/20 microseconds or 10/1000 microseconds, where 8 microseconds is the transient's front time and 20 microseconds is the time to half value.

### Capacitance

Typical capacitance for consumer-sized (7–20 mm diameter) varistors are in the range of 100–2,500 pF. Smaller, lower-capacitance varistors are available with capacitance of ~1 pF for microelectronic protection, such as in cellular phones. These low-capacitance varistors are, however, unable to withstand large surge currents simply due to their compact PCB-mount size.

### Response time

The response time of the MOV is not standardized. The sub-nanosecond MOV response claim is based on the material's intrinsic response time, but will be slowed down by other factors such as the inductance of component leads and the mounting method. That response time is also qualified as insignificant when compared to a transient having an 8 µs rise-time, thereby allowing ample time for the device to slowly turn-on. When subjected to a very fast, <1 ns rise-time transient, response times for the MOV are in the 40–60 ns range.

## Applications

A typical surge protector power strip is built using MOVs. Low-cost versions may use only one varistor, from the hot (live, active) to the neutral conductor. A better protector contains at least three varistors; one across each of the three pairs of conductors. Some standards mandate a triple varistor scheme so that catastrophic MOV failure does not create a fire hazard.

## Limitations

A MOV inside a transient voltage surge suppressor (TVSS) does not provide complete protection for electrical equipment. In particular, it provides no protection from sustained over-voltages that may result in damage to that equipment as well as to the protector device. Other sustained and harmful over-voltages may be lower and therefore ignored by a MOV device.

A varistor provides no equipment protection from inrush current surges (during equipment startup), from overcurrent (created by a short circuit), or from voltage sags (brownouts); it neither senses nor affects such events. Susceptibility of electronic equipment to these other electric power disturbances is defined by other aspects of the system design, either inside the equipment itself or externally by means such as a UPS, a voltage regulator or a surge protector with built-in overvoltage protection (which typically consists of a voltage-sensing circuit and a relay for disconnecting the AC input when the voltage reaches a danger threshold).

## Multi-layer varistor

**Multi-layer varistor** (MLV) devices provide electrostatic discharge protection to electronic circuits from low to medium energy transients in sensitive equipment operating at 0–120 volts dc. They have peak current ratings from about 20 to 500 amperes, and peak energy ratings from 0.05 to 2.5 joules.

_(Later sections of the original article were left out of this offline copy for length: Hazards, Comparison to other transient suppressors.)_
