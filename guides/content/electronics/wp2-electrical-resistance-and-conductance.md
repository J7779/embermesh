---
id: electronics/wp2-electrical-resistance-and-conductance
title: Electrical resistance and conductance
category: electronics
tags: [electrical resistance and conductance, electrical resistance, electronics]
summary: The electrical resistance of an object is a measure of its opposition to the flow of electric current.
source: https://en.wikipedia.org/w/index.php?title=Electrical_resistance_and_conductance&oldid=1381722547
source_title: Wikipedia: Electrical resistance and conductance
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Electrical resistance and conductance", Wikipedia, The Free Encyclopedia (revision 1381722547), https://en.wikipedia.org/w/index.php?title=Electrical_resistance_and_conductance&oldid=1381722547 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Electrical resistance and conductance

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **electrical resistance** of an object is a measure of its opposition to the flow of electric current. Its reciprocal quantity is **electrical conductance**, measuring the ease with which an electric current passes. Electrical resistance shares some conceptual parallels with mechanical friction. The SI unit of electrical resistance is the ohm (Ω), while electrical conductance is measured in siemens (S).

The resistance of an object depends in large part on the material it is made of. Objects made of electrical insulators like rubber tend to have very high resistance and low conductance, while objects made of electrical conductors like metals tend to have very low resistance and high conductance. This relationship is quantified by resistivity or conductivity. The nature of a material is not the only factor in resistance and conductance, however; it also depends on the size and shape of an object because these properties are extensive rather than intensive. For example, a wire's resistance is higher if it is long and thin, and lower if it is short and thick. All objects resist electrical current, except for superconductors, which have a resistance of zero.

The resistance R of an object is defined as the ratio of voltage V across it to current I through it, while the conductance G is the reciprocal:
R = V/I, G = I/V = 1/R.

For a wide variety of materials and conditions, V and I are directly proportional to each other, and therefore R and G are constants (although they will depend on the size and shape of the object, the material it is made of, and other factors like temperature or strain). This proportionality is called Ohm's law, and materials that satisfy it are called *ohmic* materials.

In other cases, such as a transformer, diode, incandescent light bulb or battery, V and I are not directly proportional. The ratio V/I is sometimes still useful, and is referred to as a *chordal resistance* or *static resistance*, since it corresponds to the inverse slope of a chord between the origin and an I–V curve. In other situations, the derivative dV/dI may be most useful; this is called the *differential resistance*.

## Introduction

In the hydraulic analogy, current flowing through a wire (or resistor) is like water flowing through a pipe, and the voltage drop across the wire is like the pressure drop that pushes water through the pipe. Conductance is proportional to how much flow occurs for a given pressure, and resistance is proportional to how much pressure is required to achieve a given flow.

The voltage drop (i.e., difference between voltages on one side of the resistor and the other), not the voltage itself, provides the driving force pushing current through a resistor. In hydraulics, it is similar: the pressure difference between two sides of a pipe, not the pressure itself, determines the flow through it. For example, there may be a large water pressure above the pipe, which tries to push water down through the pipe. But there may be an equally large water pressure below the pipe, which tries to push water back up through the pipe. If these pressures are equal, no water flows. (In the hydraulic analogy, the water pressure below the pipe is zero.)

The resistance and conductance of a wire, resistor, or other element is mostly determined by two properties:

- geometry (shape), and
- material

Geometry is important because it is more difficult to push water through a long, narrow pipe than a wide, short pipe. In the same way, a long, thin copper wire has higher resistance (lower conductance) than a short, thick copper wire.

Materials are important as well. A pipe filled with hair restricts the flow of water more than a clean pipe of the same shape and size. Similarly, electrons can flow freely and easily through a copper wire, but cannot flow as easily through a steel wire of the same shape and size, and they essentially cannot flow at all through an insulator like rubber, regardless of its shape. The difference between copper, steel, and rubber is related to their microscopic structure and electron configuration, and is quantified by a property called resistivity.

In addition to geometry and material, there are various other factors that influence resistance and conductance, such as temperature; see below.

## Conductors and resistors

Substances in which electricity can flow are called conductors. A piece of conducting material of a particular resistance meant for use in a circuit is called a resistor. Conductors are made of high-conductivity materials such as metals, in particular copper and aluminium. Resistors, on the other hand, are made of a wide variety of materials depending on factors such as the desired resistance, amount of energy that it needs to dissipate, precision, and costs.

## Ohm's law

For many materials, the current I through the material is proportional to the voltage V applied across it:
I ∝ V
over a wide range of voltages and currents. Therefore, the resistance and conductance of objects or electronic components made of these materials is constant. This relationship is called Ohm's law, and materials which obey it are called *ohmic* materials. Examples of ohmic components are wires and resistors. The current–voltage graph of an ohmic device consists of a straight line through the origin with positive slope.

Other components and materials used in electronics do not obey Ohm's law; the current is not proportional to the voltage, so the resistance varies with the voltage and current through them. These are called *nonlinear* or *non-ohmic*. Examples include diodes and fluorescent lamps.

## Measurement

An instrument for measuring resistance is called an ohmmeter. Simple ohmmeters cannot measure low resistances accurately because the resistance of their measuring leads causes a voltage drop that interferes with the measurement, so more accurate devices use four-terminal sensing.

## Typical values

**Typical resistance values for selected objects**

| Component | Resistance |
|---|---|
| 1 m of copper wire with 1 mm diameter | 0.02 Ω |
| 1 km overhead power line (*typical*) | 0.03 Ω |
| AA battery (*typical internal resistance*) | 0.1 Ω |
| Incandescent light bulb filament (*typical*) | 200 Ω |
| Human body | 1000 Ω |

## Static and differential resistance

Many electrical elements, such as diodes and batteries do *not* satisfy Ohm's law. These are called *non-ohmic* or *non-linear*, and their current–voltage curves are *not* straight lines through the origin.

Resistance and conductance can still be defined for non-ohmic elements. However, unlike ohmic resistance, non-linear resistance is not constant but varies with the voltage or current through the device; i.e., its operating point. There are two types of resistance:

## Energy dissipation and Joule heating

Resistors (and other elements with resistance) oppose the flow of electric current; therefore, electrical energy is required to push current through the resistance. This electrical energy is dissipated, heating the resistor in the process. This is called *Joule heating* (after James Prescott Joule), also called *ohmic heating* or *resistive heating*.

The dissipation of electrical energy is often undesired, particularly in the case of transmission losses in power lines. High voltage transmission helps reduce the losses by reducing the current for a given power.

On the other hand, Joule heating is sometimes useful, for example in electric stoves and other electric heaters (also called *resistive heaters*). As another example, incandescent lamps rely on Joule heating: the filament is heated to such a high temperature that it glows "white hot" with thermal radiation (also called incandescence).

The formula for Joule heating is:
P=I^2R
where P is the power (energy per unit time) converted from electrical energy to thermal energy, R is the resistance, and I is the current through the resistor.

_(Later sections of the original article were left out of this offline copy for length: Relation to resistivity and conductivity, AC circuits, Dependence on other conditions, Superconductivity.)_
