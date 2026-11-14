---
id: electronics/wp2-transistor
title: Transistor
category: electronics
tags: [transistor, electronics]
summary: A transistor is a semiconductor device used to amplify or switch electrical signals and power.
source: https://en.wikipedia.org/w/index.php?title=Transistor&oldid=1383498898
source_title: Wikipedia: Transistor
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Transistor", Wikipedia, The Free Encyclopedia (revision 1383498898), https://en.wikipedia.org/w/index.php?title=Transistor&oldid=1383498898 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Transistor

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **transistor** is a semiconductor device used to amplify or switch electrical signals and power. It is one of the fundamental building blocks of modern electronics. A transistor is composed of semiconductor material, often with at least three terminals for connection to an electronic circuit. A voltage or current applied to one pair of the transistor's terminals controls the current through another pair of terminals. Because the controlled (output) power can be higher than the controlling (input) power, a transistor can be used to amplify a signal. Some transistors are packaged individually, but many more in miniature form are found embedded in integrated circuits. Because transistors are the key active components in practically all modern electronics, they are considered one of the 20th century's greatest inventions.

Physicist Julius Edgar Lilienfeld proposed the concept of a field-effect transistor (FET) in 1925, but it was not possible to construct a working device at that time. The first working device was a point-contact transistor invented in 1947 by physicists John Bardeen, Walter Brattain, and William Shockley at Bell Labs who shared the 1956 Nobel Prize in Physics for their achievement. The most widely used type of transistor, the metal–oxide–semiconductor field-effect transistor (MOSFET), was invented at Bell Labs between 1955 and 1960. Transistors revolutionized the field of electronics and paved the way for smaller and cheaper radios, calculators, computers, and other electronic devices.

Most transistors are made from very pure silicon, and some from germanium, but certain other semiconductor materials are sometimes used. A transistor may have only one kind of charge carrier in a field-effect transistor, or may have two kinds of charge carriers in bipolar junction transistor devices. Compared with the vacuum tube, transistors are generally smaller and require less power to operate. Certain vacuum tubes have advantages over transistors at very high operating frequencies or high operating voltages, such as traveling-wave tubes and gyrotrons. Many types of transistors are made to standardized specifications by multiple manufacturers.

## Importance

Because transistors are the key active components in practically all modern electronics, many people consider them one of the 20th century's greatest inventions.

The 1947 invention of the first transistor at Bell Labs was named an IEEE Milestone in 2009. Other Milestones include the inventions of the junction transistor in 1948 and the MOSFET in 1959.

The MOSFET is by far the most widely used transistor, in applications ranging from computers and electronics to communications technology such as smartphones. It has been considered the most important transistor, possibly the most important invention in electronics, and the device that enabled modern electronics. It has been the basis of modern digital electronics since the late 20th century, paving the way for the digital age. The US Patent and Trademark Office calls it a "groundbreaking invention that transformed life and culture around the world". Its ability to be mass-produced by a highly automated process (semiconductor device fabrication), from relatively basic materials, allows astonishingly low per-transistor costs. MOSFETs are the most numerously produced artificial objects in history, with more than 13 sextillion manufactured by 2018.

Although several companies each produce over a billion individually packaged (known as *discrete*) MOS transistors every year, the vast majority are produced in integrated circuits (also known as *ICs*, *microchips,* or simply *chips*), along with diodes, resistors, capacitors and other electronic components, to produce complete electronic circuits. A logic gate consists of up to about 20 transistors, whereas an advanced microprocessor, As of 2023, may contain as many as 92 billion transistors on a die, and double that 184 billion when dual die (and for exceptional chips, 2.6 trillion transistors, as of 2020). Transistors are often organized into logic gates in microprocessors to perform computation.

The transistor's low cost, flexibility and reliability have made it ubiquitous. Transistorized mechatronic circuits have replaced electromechanical devices in controlling appliances and machinery. It is often easier and cheaper to use a standard microcontroller and write a computer program to carry out a control function than to design an equivalent mechanical system.

## Simplified operation

A transistor can use a small signal applied between one pair of its terminals to control a much larger signal at another pair of terminals, a property called gain. It can produce a stronger output signal, a voltage or current, proportional to a weaker input signal, acting as an amplifier. It can also be used as an electrically controlled switch, where the amount of current is determined by other circuit elements.

There are two types of transistors, with slight differences in how they are used:

- A *bipolar junction transistor (BJT)* has terminals labeled **base**, **collector** and **emitter**. A small current at the base terminal, flowing between the base and the emitter, can control or switch a much larger current between the collector and emitter.

- A *field-effect transistor (FET)* has terminals labeled **gate**, **source** and **drain**. A voltage at the gate can control a current between source and drain.

The top image in this section represents a typical bipolar transistor in a circuit. A charge flows between emitter and collector terminals depending on the current in the base. Because the base and emitter connections behave like a semiconductor diode, a voltage drop develops between them. The amount of this drop, determined by the transistor's material, is referred to as *V*_BE. (Base Emitter Voltage)

### Transistor as a switch

Transistors are commonly used in digital circuits as electronic switches which can be either in an *on* or *off* state, both for high-power applications such as switched-mode power supplies and for low-power applications such as logic gates. Important parameters for this application include the current switched, the voltage handled, and the switching speed, characterized by the rise and fall times.

In a switching circuit, the goal is to simulate, as near as possible, the ideal switch having the properties of an open circuit when off, the short circuit when on, and an instantaneous transition between the two states. Parameters are chosen such that the *off* output is limited to leakage currents too small to affect connected circuitry, the resistance of the transistor in the *on* state is too small to affect circuitry, and the transition between the two states is fast enough not to have a detrimental effect.

In a grounded-emitter transistor circuit, such as the light-switch circuit shown, as the base voltage rises, the emitter and collector currents rise exponentially. The collector voltage drops because of reduced resistance from the collector to the emitter. If the voltage difference between the collector and emitter were zero (or near zero), the collector current would be limited only by the load resistance (light bulb) and the supply voltage. This is called *saturation* because the current is flowing from collector to emitter freely. When saturated, the switch is said to be *on*.

The use of bipolar transistors for switching applications requires biasing the transistor so that it operates between its cut-off region in the off-state and the saturation region (*on*). This requires sufficient base drive current. As the transistor provides current gain, it facilitates the switching of a relatively large current in the collector by a much smaller current into the base terminal. The ratio of these currents varies depending on the type of transistor, and even for a particular type, varies depending on the collector current. In the example of a light-switch circuit, as shown, the resistor is chosen to provide enough base current to ensure the transistor is saturated. The base resistor value is calculated from the supply voltage, transistor C-E junction voltage drop, collector current, and amplification factor beta.

### Transistor as an amplifier

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Simplified operation, Comparison with vacuum tubes, Types, Device identification, Construction.)_
