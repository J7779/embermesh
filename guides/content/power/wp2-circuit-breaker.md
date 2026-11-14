---
id: power/wp2-circuit-breaker
title: Circuit breaker
category: power
tags: [circuit breaker, energy]
summary: A circuit breaker is an electrical safety device designed to protect an electrical circuit from damage caused by current in excess of that which the equipment can safely carry (overcurrent).
source: https://en.wikipedia.org/w/index.php?title=Circuit_breaker&oldid=1381589912
source_title: Wikipedia: Circuit breaker
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Circuit breaker", Wikipedia, The Free Encyclopedia (revision 1381589912), https://en.wikipedia.org/w/index.php?title=Circuit_breaker&oldid=1381589912 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Circuit breaker

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **circuit breaker** is an electrical safety device designed to protect an electrical circuit from damage caused by current in excess of that which the equipment can safely carry (overcurrent). Its basic function is to interrupt current flow to protect equipment and to prevent fire. Unlike a fuse, which interrupts once and then must be replaced, a circuit breaker can be reset (either manually or automatically) to resume normal operation.

Circuit breakers are commonly installed in distribution boards. Apart from its safety purpose, a circuit breaker is also often used as a main switch to manually disconnect ("rack out") and connect ("rack in") electrical power to a whole electrical sub-network.

Circuit breakers are made in varying current ratings, from devices that protect low-current circuits or individual household appliances, to switchgear designed to protect high-voltage circuits feeding an entire city. Any device which protects against excessive current by automatically removing power from a faulty system, such as a circuit breaker or fuse, can be referred to as an *over-current protection device* (*OCPD*).

## Operation

All circuit breaker systems have common features in their operation, but details vary substantially depending on the voltage class, current rating and type of the circuit breaker.

The circuit breaker must first detect a fault condition. In small mains and low-voltage circuit breakers, this is usually done within the device itself. Typically, the heating or magnetic effects of electric current are employed. Circuit breakers for large currents or high voltages are usually arranged with protective relay pilot devices to sense a fault condition and to operate the opening mechanism. These typically require a separate power source, such as a battery, although some high-voltage circuit breakers are self-contained with current transformers, protective relays, and internal power sources.

Once a fault is detected, the circuit breaker contacts must open to interrupt the circuit; this is commonly done using mechanically stored energy contained within the breaker, such as a spring or compressed air to separate the contacts. A breaker may also use the higher current caused by the fault to separate the contacts, via thermal expansion or increased magnetic field. A small circuit breaker typically has a manual control lever to switch the circuit off or reset a tripped breaker, while a larger unit may use a solenoid to trip the mechanism, and an electric motor to restore energy to springs (which rapidly separate contacts when the breaker is tripped).

The circuit breaker contacts must carry the load current without excessive heating, and must also withstand the heat of the arc produced when interrupting (opening) the circuit. Contacts are made of copper or copper alloys, silver alloys and other highly conductive materials. Service life of the contacts is limited by the erosion of contact material due to arcing while interrupting the current. Miniature and molded-case circuit breakers are usually discarded when the contacts have worn, but power circuit breakers and high-voltage circuit breakers have replaceable contacts.

When a high current or voltage is interrupted, an arc is generated. The maximum length of the arc is generally proportional to the voltage while the intensity (or heat) is proportional to the current. This arc must be contained, cooled and extinguished in a controlled way, so that the gap between the contacts can again withstand the voltage in the circuit. Different circuit breakers use vacuum, air, insulating gas, or oil as the medium the arc forms in. Different techniques are used to extinguish the arc including

- Lengthening or deflecting the arc
- Intensive cooling (in jet chambers)
- Division into partial arcs
- Zero-point quenching (contacts open at the moment in the AC waveform at which the current and potential are near zero, effectively breaking no load current at the time of opening. The zero-crossing occurs at twice the line frequency; i.e., 100 times per second for 50 Hz and 120 times per second for 60 Hz AC.)
- Connecting capacitors in parallel with contacts in DC circuits.

Finally, once the fault condition has been cleared, the contacts must again be closed to restore power to the interrupted circuit.

## Arc interruption

Low-voltage **miniature circuit breakers** (**MCB**) use air alone to extinguish the arc. These circuit breakers contain so-called arc chutes, a stack of mutually insulated parallel metal plates that divide and cool the arc. Splitting the arc into smaller arcs makes the arc cooler as the arc voltage increases and serves as an additional impedance that limits the current through the circuit breaker. The current-carrying parts near the contacts provide easy deflection of the arc into the arc chutes by a magnetic force of a current path, although magnetic blowout coils or permanent magnets could also deflect the arc into the arc chute (used on circuit breakers for higher ratings). The number of plates in the arc chute is dependent on the short-circuit rating and nominal voltage of the circuit breaker.

In larger ratings, oil circuit breakers rely upon vaporization of some of the oil to blast a jet of oil through the arc.

Gas (usually sulfur hexafluoride) circuit breakers sometimes stretch the arc using a magnetic field, and then rely upon the dielectric strength of the sulfur hexafluoride (SF₆) to quench the stretched arc.

Vacuum circuit breakers have minimal arcing (as there is nothing to ionize other than the contact material). The arc is quenched when it is stretched a very small amount (less than 3 mm). Vacuum circuit breakers are frequently used in modern medium-voltage switch gear to 38000 volts.

Air circuit breakers may use compressed air to blow out the arc, or alternatively, the contacts are rapidly swung into a small sealed chamber, the escaping of the displaced air thus blowing out the arc.

Circuit breakers are usually able to terminate all current very quickly: typically the arc is extinguished between 30 and 150 ms after the mechanism has been tripped, depending upon age and construction of the device. The maximum current value and let-through energy determine the quality of the circuit breakers.

## Short circuit

Circuit breakers are rated both by the normal current that they are expected to carry, and the maximum short-circuit current that they can safely interrupt. This latter figure is the **ampere interrupting capacity** (**AIC**) of the breaker.

Under short-circuit conditions, the calculated or measured maximum prospective short-circuit current may be many times the normal, rated current of the circuit. When electrical contacts open to interrupt a large current, there is a tendency for an arc to form between the opened contacts, which would allow the current to continue. This condition can create conductive ionized gases and molten or vaporized metal, which can cause the further continuation of the arc or create additional short circuits, potentially resulting in the explosion of the circuit breaker and the equipment that it is installed in. Therefore, circuit breakers incorporate various features to divide and extinguish the arcs.

The maximum short-circuit current that a breaker can interrupt is determined by testing. Application of a breaker in a circuit with a prospective short-circuit current higher than the breaker's interrupting capacity rating may result in failure of the breaker to safely interrupt a fault. In a worst-case scenario, a breaker may successfully interrupt a fault only to explode when reset.

Typical domestic panel circuit breakers are rated to interrupt 10 kA (10000 A) short-circuit current.

Miniature circuit breakers used to protect control circuits or small appliances may not have sufficient interrupting capacity to use at a panel board; these circuit breakers are called "supplemental circuit protectors" to distinguish them from distribution-type circuit breakers.

_(Later sections of the original article were left out of this offline copy for length: Standard current ratings, Types, "Smart" circuit breakers, Other breakers.)_
