---
id: electronics/wp2-relay
title: Relay
category: electronics
tags: [relay, electronics]
summary: A relay is an electrically operated switch.
source: https://en.wikipedia.org/w/index.php?title=Relay&oldid=1382774038
source_title: Wikipedia: Relay
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Relay", Wikipedia, The Free Encyclopedia (revision 1382774038), https://en.wikipedia.org/w/index.php?title=Relay&oldid=1382774038 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Relay

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **relay** is an electrically operated switch. It has a set of input terminals for one or more control signals, and a set of operating contact terminals. The switch may have any number of contacts in multiple contact forms, such as make contacts, break contacts, or combinations thereof.

Relays are used to control a circuit by an independent low-power signal and to control several circuits by one signal. They were first used in long-distance telegraph circuits as signal repeaters that transmit a refreshed copy of the incoming signal onto another circuit. Relays were used extensively in telephone exchanges and early computers to perform logical operations.

The traditional electromechanical relay uses an electromagnet to close or open the contacts, but relays using other operating principles have also been invented, such as in solid-state relays which use semiconductor properties for control without relying on moving parts. Relays with calibrated operating characteristics and sometimes multiple operating coils are used to protect electrical circuits from overload or faults; in modern electric power systems these functions are performed by digital instruments still called *protective relays* or *safety relays*.

Latching relays require only a single pulse of control power to operate the switch persistently. Another pulse applied to a second set of control terminals, or a pulse with opposite polarity, resets the switch, while repeated pulses of the same kind have no effects. Magnetic latching relays are useful in applications when interrupted power should not affect the circuits that the relay is controlling.

## Basic design and operation

A simple electromagnetic relay consists of a coil of wire wrapped around a soft iron core (a solenoid), an iron yoke which provides a low reluctance path for magnetic flux, a movable iron armature, and one or more sets of contacts (there are two contacts in the relay pictured). The armature is hinged to the yoke and mechanically linked to one or more sets of moving contacts. The armature is held in place by a spring so that when the relay is de-energized there is an air gap in the magnetic circuit. In this condition, one of the two sets of contacts in the relay pictured is closed, and the other set is open. Other relays may have more or fewer sets of contacts depending on their function. The relay in the picture also has a wire connecting the armature to the yoke. This ensures continuity of the circuit between the moving contacts on the armature, and the circuit track on the printed circuit board (PCB) via the yoke, which is soldered to the PCB.

When an electric current is passed through the coil it generates a magnetic field that activates the armature, and the consequent movement of the movable contact(s) either makes or breaks (depending upon construction) a connection with a fixed contact. If the set of contacts was closed when the relay was de-energized, then the movement opens the contacts and breaks the connection, and vice versa if the contacts were open. When the current to the coil is switched off, the armature is returned by a force, approximately half as strong as the magnetic force, to its relaxed position. Usually this force is provided by a spring, but gravity is also used commonly in industrial motor starters. Most relays are manufactured to operate quickly. In a low-voltage application this reduces noise; in a high voltage or current application it reduces arcing.

When the coil is energized with direct current, a flyback diode or snubber resistor is often placed across the coil to dissipate the energy from the collapsing magnetic field (back EMF) at deactivation, which would otherwise generate a voltage spike dangerous to semiconductor circuit components. Such diodes were not widely used before the application of transistors as relay drivers, but soon became ubiquitous as early germanium transistors were easily destroyed by this surge. Some automotive relays include a diode inside the relay case. Resistors, while more durable than diodes, are less efficient at eliminating voltage spikes generated by relays and therefore not as commonly used.

If the relay is driving a large, or especially a reactive load, there may be a similar problem of surge currents around the relay output contacts. In this case a snubber circuit (a capacitor and resistor in series) across the contacts may absorb the surge. Suitably rated capacitors and the associated resistor are sold as a single packaged component for this commonplace use.

If the coil is designed to be energized with alternating current (AC), some method is used to split the flux into two out-of-phase components which add together, increasing the minimum pull on the armature during the AC cycle. Typically this is done with a small copper "shading ring" crimped around a portion of the core that creates the delayed, out-of-phase component, which holds the contacts during the zero crossings of the control voltage.

Contact materials for relays vary by application. Materials with low contact resistance may be oxidized by the air, or may tend to "stick" instead of cleanly parting when opening. Contact material may be optimized for low electrical resistance, high strength to withstand repeated operations, or high capacity to withstand the heat of an arc. Where very low resistance is required, or low thermally-induced voltages are desired, gold-plated contacts may be used, along with palladium and other non-oxidizing, semi-precious metals. Silver or silver-plated contacts are used for signal switching. Mercury-wetted relays make and break circuits using a thin, self-renewing film of liquid mercury. For higher-power relays switching many amperes, such as motor circuit contactors, contacts are made with a mixtures of silver and cadmium oxide, providing low contact resistance and high resistance to the heat of arcing. Contacts used in circuits carrying scores or hundreds of amperes may include additional structures for heat dissipation and management of the arc produced when interrupting the circuit. Some relays have field-replaceable contacts, such as certain machine tool relays; these may be replaced when worn out, or changed between normally open and normally closed state, to allow for changes in the controlled circuit.

## Safety and reliability

Switching while "wet" (under load) causes undesired arcing between the contacts, eventually leading to contacts that weld shut or contacts that fail due to a buildup of surface damage caused by the destructive arc energy.

Inside the Number One Electronic Switching System (1ESS) crossbar switch and certain other high-reliability designs, the reed switches are always switched "dry" (without load) to avoid that problem, leading to much longer contact life.

Without adequate contact protection, the occurrence of electric current arcing causes significant degradation of the contacts, which suffer significant and visible damage. Every time the relay contacts open or close under load, an electrical arc can occur between the contacts of the relay, either a *break* arc (when opening), or a *make* / *bounce* arc (when closing). In many situations, the *break* arc is more energetic and thus more destructive, in particular with inductive loads, but this can be mitigated by bridging the contacts with a snubber circuit. The inrush current of tungsten filament incandescent lamps is typically ten times the normal operating current. Thus, relays intended for tungsten loads may use special contact composition, or the relay may have lower contact ratings for tungsten loads than for purely resistive loads.

An electrical arc across relay contacts can be very hot — thousands of degrees Fahrenheit — causing the metal on the contact surfaces to melt, pool, and migrate with the current. The extremely high temperature of the arc splits the surrounding gas molecules, creating ozone, carbon monoxide, and other compounds. Over time, the arc energy slowly destroys the contact metal, causing some material to escape into the air as fine particulate matter. This action causes the material in the contacts to degrade and coordination, resulting in device failure. This contact degradation drastically limits the overall life of a relay to a range of about 10,000 to 100,000 operations, a level far below the mechanical life of the device, which can be in excess of 20 million operations.

_(Later sections of the original article were left out of this offline copy for length: Types, Applications.)_
