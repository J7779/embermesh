---
id: electronics/wp2-kirchhoffs-circuit-laws
title: Kirchhoff's circuit laws
category: electronics
tags: [kirchhoff's circuit laws, kirchhoff's rules, kirchhoff's laws, electronics]
summary: Kirchhoff's circuit laws are two equalities that deal with the current and potential difference (commonly known as voltage) in the lumped element model of electrical circuits.
source: https://en.wikipedia.org/w/index.php?title=Kirchhoff%27s_circuit_laws&oldid=1372152542
source_title: Wikipedia: Kirchhoff's circuit laws
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Kirchhoff's circuit laws", Wikipedia, The Free Encyclopedia (revision 1372152542), https://en.wikipedia.org/w/index.php?title=Kirchhoff%27s_circuit_laws&oldid=1372152542 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Kirchhoff's circuit laws

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Kirchhoff's circuit laws** are two equalities that deal with the current and potential difference (commonly known as voltage) in the lumped element model of electrical circuits. They were first described in 1845 by German physicist Gustav Kirchhoff. This generalized the work of Georg Ohm and preceded the work of James Clerk Maxwell. Widely used in electrical engineering, they are also called **Kirchhoff's rules** or simply **Kirchhoff's laws**. These laws can be applied in time and frequency domains and form the basis for network analysis.

Both of Kirchhoff's laws can be understood as corollaries of Maxwell's equations in the low-frequency limit. They are accurate for DC circuits, and for AC circuits at frequencies where the wavelengths of electromagnetic radiation are very large compared to the circuits.

## Kirchhoff's current law

This law, also called **Kirchhoff's first law**, or **Kirchhoff's junction rule**, states that, for any node (junction) in an electrical circuit, the sum of currents flowing into that node is equal to the sum of currents flowing out of that node; or equivalently:

> *The algebraic sum of currents in a network of conductors meeting at a point is zero.*

Recalling that current is a signed (positive or negative) quantity reflecting direction towards or away from a node, this principle can be succinctly stated as: Σ_i=1^n I_i = 0 where *n* is the total number of branches with currents flowing towards or away from the node.

Kirchhoff's circuit laws were originally obtained from experimental results. However, the current law can be viewed as an extension of the conservation of charge, since charge is the product of current and the time the current has been flowing. If the net charge in a region is constant, the current law will hold on the boundaries of the region. This means that the current law relies on the fact that the net charge in the wires and components is constant.

### Uses

A matrix version of Kirchhoff's current law is the basis of most circuit simulation software, such as SPICE. The current law is used with Ohm's law to perform nodal analysis.

The current law is applicable to any lumped network irrespective of the nature of the network; whether unilateral or bilateral, active or passive.

## Kirchhoff's voltage law

This law, also called **Kirchhoff's second law**, or **Kirchhoff's loop rule**, states the following:

> *The directed sum of the potential differences (voltages) around any closed loop is zero.*

Similarly to Kirchhoff's current law, the voltage law can be stated as: Σ_i=1^n V_i = 0

Here, n is the total number of voltages measured.

### Generalization

In the low-frequency limit, the voltage drop around any loop is zero. This includes imaginary loops arranged arbitrarily in space – not limited to the loops delineated by the circuit elements and conductors. In the low-frequency limit, this is a corollary of Faraday's law of induction (which is one of Maxwell's equations).

This has practical application in situations involving "static electricity".

## Limitations

Kirchhoff's circuit laws are the result of the lumped-element model and both depend on the model being applicable to the circuit in question. When the model is not applicable, the laws do not apply.

The current law is dependent on the assumption that the net charge in any wire, junction or lumped component is constant. Whenever the electric field between parts of the circuit is non-negligible, such as when two wires are capacitively coupled, this may not be the case. This occurs in high-frequency AC circuits, where the lumped element model is no longer applicable. For example, in a transmission line, the charge density in the conductor may be constantly changing.

On the other hand, the voltage law relies on the fact that the actions of time-varying magnetic fields are confined to individual components, such as inductors. In reality, the induced electric field produced by an inductor is not confined, but the leaked fields are often negligible.

### Modelling real circuits with lumped elements

The lumped element approximation for a circuit is accurate at low frequencies. At higher frequencies, leaked fluxes and varying charge densities in conductors become significant. To an extent, it is possible to still model such circuits using parasitic components. If frequencies are too high, it may be more appropriate to simulate the fields directly using finite element modelling or other techniques.

To model circuits so that both laws can still be used, it is important to understand the distinction between *physical* circuit elements and the *ideal* lumped elements. For example, a wire is not an ideal conductor. Unlike an ideal conductor, wires can inductively and capacitively couple to each other (and to themselves), and have a finite propagation delay. Real conductors can be modeled in terms of lumped elements by considering parasitic capacitances distributed between the conductors to model capacitive coupling, or parasitic (mutual) inductances to model inductive coupling. Wires also have some self-inductance.

## Example

Assume an electric network consisting of two voltage sources and three resistors.

According to the first law:
i_1 - i_2 - i_3 = 0
Applying the second law to the closed circuit *s*₁, and substituting for voltage using Ohm's law gives:
-R_2 i_2 + E_1 - R_1 i_1 = 0
The second law, again combined with Ohm's law, applied to the closed circuit *s*₂ gives:
-R_3 i_3 - E_2 - E_1 + R_2 i_2 = 0

This yields a system of linear equations in *i*₁, *i*₂, *i*₃:
i_1 - i_2 - i_3 = 0; -R_2 i_2 + E_1 - R_1 i_1 = 0; -R_3 i_3 - E_2 - E_1 + R_2 i_2 = 0
which is equivalent to
i_1 + (- i_2) + (- i_3) = 0; R_1 i_1 + R_2 i_2 + 0 i_3 = E_1; 0 i_1 + R_2 i_2 - R_3 i_3 = E_1 + E_2
Assuming
R_1 = 100Ω, R_2 = 200Ω, R_3 = 300Ω,; E_1 = 3V, E_2 = 4V
the solution is
i_1 = 1/1100A; [6pt] i_2 = 4/275A; [6pt] i_3 = -3/220A

The current *i*₃ has a negative sign which means the assumed direction of *i*₃ was incorrect and *i*₃ is actually flowing in the direction opposite to the red arrow labeled *i*₃. The current in *R*₃ flows from left to right.
