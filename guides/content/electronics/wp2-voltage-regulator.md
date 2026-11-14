---
id: electronics/wp2-voltage-regulator
title: Voltage regulator
category: electronics
tags: [voltage regulator, electronics]
summary: A voltage regulator is a system designed to automatically maintain a constant voltage.
source: https://en.wikipedia.org/w/index.php?title=Voltage_regulator&oldid=1385267568
source_title: Wikipedia: Voltage regulator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Voltage regulator", Wikipedia, The Free Encyclopedia (revision 1385267568), https://en.wikipedia.org/w/index.php?title=Voltage_regulator&oldid=1385267568 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Voltage regulator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **voltage regulator** is a system designed to automatically maintain a constant voltage. It may use a simple feed-forward design or may include negative feedback. It may use an electromechanical mechanism or electronic components. Depending on the design, it may be used to regulate one or more AC or DC voltages.

Electronic voltage regulators are found in devices such as computer power supplies, where they stabilize the DC voltages used by the processor and other elements. In automobile alternators and central power station generator plants, voltage regulators control the output of the plant. In an electric power distribution system, voltage regulators may be installed at a substation or along distribution lines so that all customers receive steady voltage independent of how much power is drawn from the line.

## Electronic voltage regulators

A simple voltage regulator can be made from a resistor in series with a forward-biased diode (or series of diodes), forming a voltage divider of sorts. Due to the logarithmic shape of diode V-I curves, the voltage across the diode changes only slightly due to changes in current drawn or changes in the input. When precise voltage control and efficiency are not important, this design may be adequate. Since the forward voltage of a diode is small, this kind of voltage regulator is only suitable for low-voltage regulated output. When higher-voltage output is needed, a reverse-biased zener diode or a series of zener diodes may be employed. Zener diode regulators make use of the zener diode's fixed reverse voltage, which can be quite large.

Feedback voltage regulators operate by comparing the actual output voltage to some fixed reference voltage. Any difference is amplified and used to control the regulation element in such a way as to reduce the voltage error. This forms a negative feedback control loop; increasing the open-loop gain tends to increase regulation accuracy but reduce stability. Stability is the avoidance of oscillation, or ringing, during step changes in load or input voltage. There will also be a trade-off between stability and the speed of the response to changes. If the output voltage is too low, the regulation element is commanded, *up to a point*, to produce a higher output voltage–by dropping less of the input voltage (for linear series regulators and buck switching regulators), or to draw input current for longer periods (boost-type switching regulators); if the output voltage is too high, the regulation element will normally be commanded to produce a lower voltage. However, many regulators have over-current protection, so that they will entirely stop sourcing current (or limit the current in some way) if the output current is too high, and some regulators may also shut down if the input voltage is outside a given range (see also: Crowbar (circuit)).

## Electromechanical regulators

In electromechanical regulators, voltage regulation is accomplished by coiling a sensing wire to make an electromagnet. The magnetic field produced by the current in the wire attracts a moving ferrous core held back under spring tension or gravitational pull. As voltage increases, so does the current, strengthening the magnetic field produced by the coil and pulling the core towards the field. The magnet is physically connected to an electromechanical switch, which opens as the magnet moves into the field and is set up to cause a downward adjustment in the voltage output. As voltage decreases, so does the current, releasing spring tension or the weight of the core and causing it to retract. This closes the switch and produces a downward adjustment in the voltage output.

In a refinement, the motion of the solenoid core can be used to move a selector switch across a range of resistances or transformer windings or to rotate the position of a moving-coil AC regulator to gradually step the output voltage up or down.

Early automobile generators and alternators had a mechanical voltage regulator using one, two, or three relays and various resistors to stabilize the generator's output at slightly more than either 6.7 or 13.4 V to maintain the electrical system voltage as independently of the engine's RPM or the varying load on the vehicle's electrical system as possible. The relay(s) modulate the width of a current pulse to control the average field current in the rotating machine, which determines the strength of the magnetic field produced, which determines the unloaded output voltage at a given RPM. Both alternators and generators produce a rotating magnetic field that induces an alternating current in the coils in the stator. A generator uses a mechanical commutator, graphite brushes running on copper segments, to convert the AC produced into DC by switching the external connections at the shaft angle when the voltage would reverse. An alternator accomplishes the same goal using rectifiers.

Modern designs now use solid-state technology to perform the same function that the relays perform in electromechanical regulators.

Electromechanical regulators are used for mains voltage stabilisation.

## Automatic voltage regulator

Generators, as used in power stations, ship electrical power production, or standby power systems, will have automatic voltage regulators (AVR) to stabilize their voltages as the load on the generators changes. The first AVRs for generators were electromechanical systems, but a modern AVR uses solid-state devices. An AVR is a feedback control system that measures the output voltage of the generator, compares that output to a set point, and generates an error signal that is used to adjust the excitation of the generator. As the excitation current in the field winding of the generator increases, its terminal voltage will increase. The AVR will control current by using power electronic devices; generally, a small part of the generator's output is used to provide current for the field winding.

Where a generator is connected in parallel with other sources such as an electrical transmission grid, changing the excitation has more of an effect on the reactive power produced by the generator than on its terminal voltage, which is mostly set by the connected power system. Where multiple generators are connected in parallel, the AVR system will have circuits to ensure all generators operate at the same power factor. AVRs on grid-connected power station generators may have additional control features to help stabilize the electrical grid against upsets due to sudden load loss or faults.

## DC voltage stabilizers

Many simple DC power supplies regulate the voltage using either series or shunt regulators, but most apply a voltage reference using a *shunt regulator* such as a Zener diode, avalanche breakdown diode, or voltage regulator tube. Each of these devices begins conducting at a specified voltage and will conduct as much current as required to hold its terminal voltage to that specified voltage by diverting excess current from a non-ideal power source to ground, often through a relatively low-value resistor to dissipate the excess energy. The power supply is designed to only supply a maximum amount of current that is within the safe operating capability of the shunt regulating device.

If the stabilizer must provide more power, the shunt output is only used to provide the standard voltage reference for the electronic device, known as the voltage stabilizer. The voltage stabilizer is the electronic device, able to deliver much larger currents on demand.

_(Later sections of the original article were left out of this offline copy for length: AC voltage stabilizers, Active regulators, Example of linear regulators, Regulator specification.)_
