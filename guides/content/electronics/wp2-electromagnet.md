---
id: electronics/wp2-electromagnet
title: Electromagnet
category: electronics
tags: [electromagnet, electronics]
summary: An electromagnet is a type of magnet in which the magnetic field is produced by an electric current.
source: https://en.wikipedia.org/w/index.php?title=Electromagnet&oldid=1382781306
source_title: Wikipedia: Electromagnet
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Electromagnet", Wikipedia, The Free Encyclopedia (revision 1382781306), https://en.wikipedia.org/w/index.php?title=Electromagnet&oldid=1382781306 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Electromagnet

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **electromagnet** is a type of magnet in which the magnetic field is produced by an electric current. Electromagnets usually consist of copper wire wound into a coil. A current through the wire creates a magnetic field which is concentrated along the center of the coil. The magnetic field disappears when the current is turned off. The wire turns are often wound around a magnetic core made from a ferromagnetic or ferrimagnetic material such as iron; the magnetic core concentrates the magnetic flux and makes a more powerful magnet.

The main advantage of an electromagnet over a permanent magnet is that the magnetic field can be quickly changed by controlling the amount of electric current in the winding. However, unlike a permanent magnet, which needs no power, an electromagnet requires a continuous supply of current to maintain the magnetic field.

Electromagnets are widely used as components of other electrical devices, such as motors, generators, electromechanical solenoids, relays, loudspeakers, hard disks, MRI machines, scientific instruments, and magnetic separation equipment. Electromagnets are also employed in industry for picking up and moving heavy iron objects such as scrap iron and steel.

## Applications of electromagnets

Electromagnets are very widely used in electric and electromechanical devices, including:

- Motors and generators
- Transformers
- Relays
- Electric bells and buzzers
- Loudspeakers and headphones
- Actuators such as valves
- Magnetic recording and data storage equipment: tape recorders, VCRs, hard disks
- MRI machines
- Scientific equipment such as mass spectrometers
- Particle accelerators
- Magnetic locks
- Magnetic separation equipment used for separating magnetic from nonmagnetic material; for example, separating ferrous metal in scrap
- Industrial lifting magnets
- Magnetic levitation, used in maglev trains
- Induction heating for cooking, manufacturing, and hyperthermia therapy
A *portative electromagnet* is one designed to just hold material in place; an example is a lifting magnet. A *tractive electromagnet* applies a force and moves something.

## Simple solenoid

A common tractive electromagnet is a uniformly wound solenoid and plunger. The solenoid is a coil of wire, and the plunger is made of a material such as soft iron. Applying a current to the solenoid applies a force to the plunger and may make it move. The plunger stops moving when the forces upon it are balanced. For example, the forces are balanced when the plunger is centered in the solenoid.

The maximum uniform pull happens when one end of the plunger is at the middle of the solenoid. An approximation for the force F is

where C is a proportionality constant, A is the cross-sectional area of the plunger, N is the number of turns in the solenoid, I is the current through the solenoid wire, and ℓ is the length of the solenoid. For long, slender, solenoids (in units using inches, pounds force, and amperes), the value of C is around 0.009 to 0.010 psi (maximum pull pounds per square inch of plunger cross-sectional area). For example, a 12-inch-long coil with a long plunger with a cross section of one inch square and 11,200 ampere-turns had a maximum pull of 8.75 pounds (corresponding to ).

The maximum pull is increased when a magnetic stop is inserted into the solenoid. The stop becomes a magnet that will attract the plunger; it adds little to the solenoid pull when the plunger is far away but dramatically increases the pull when the plunger is close. An approximation for the pull P is
P = A N I [(N I)/((ell_a)^2 (C_1)^2) + frac C ell] = (A N^2 I^2)/((ell_a)^2 (C_1)^2) + (C A N I)/(ell)
Here *ℓ*_a is the distance between the end of the stop and the end of the plunger. The additional constant *C*₁ for units of inches, pounds, and amperes with slender solenoids is about 2660. The first term inside the bracket represents the attraction between the stop and the plunger; the second term represents the same force as the solenoid without a stop.

Some improvements can be made on this basic design. The ends of the stop and plunger are often conical. For example, the plunger may have a pointed end that fits into a matching recess in the stop. The shape makes the solenoid's pull more uniform as a function of separation. Another improvement is to add a magnetic return path around the outside of the solenoid (an "iron-clad solenoid"). The magnetic return path, just as the stop, has little impact until the air gap is small.

## Physics

An electric current flowing in a wire creates a magnetic field around the wire, due to Ampere's law *(see drawing of wire with magnetic field)*. To concentrate the magnetic field in an electromagnet, the wire is wound into a coil with many turns of wire lying side-by-side. The magnetic field of all the turns of wire passes through the center of the coil, creating a strong magnetic field there. A coil forming the shape of a straight tube (a helix) is called a solenoid.

The direction of the magnetic field through a coil of wire can be determined by the right-hand rule. If the fingers of the right hand are curled around the coil in the direction of current flow (conventional current, flow of positive charge) through the windings, the thumb points in the direction of the field inside the coil. The side of the magnet that the field lines emerge from is defined to be the *north pole*.

### Magnetic core

**For definitions of the variables below, see box at end of article.**

Much stronger magnetic fields can be produced if a magnetic core, made of a soft ferromagnetic (or ferrimagnetic) material such as iron, is placed inside the coil. A core can increase the magnetic field to thousands of times the strength of the field of the coil alone, due to the high magnetic permeability µ of the material. Not all electromagnets use cores, so this is called a *ferromagnetic-core* or *iron-core* electromagnet.

This phenomenon occurs because the magnetic core's material (often iron or steel) is composed of small regions called magnetic domains that act like tiny magnets (see ferromagnetism). Before the current in the electromagnet is turned on, these domains point in random directions, so their tiny magnetic fields cancel each other out, and the core has no large-scale magnetic field. When a current passes through the wire wrapped around the core, its magnetic field penetrates the core and turns the domains to align in parallel with the field. As they align, all their tiny magnetic fields add to the wire's field, which creates a large magnetic field that extends into the space around the magnet. The core concentrates the field, and the magnetic field passes through the core in lower reluctance than it would when passing through air.

The larger the current passed through the wire coil, the more the domains align, and the stronger the magnetic field is. Once all the domains are aligned, any additional current only causes a slight increase in the strength of the magnetic field. Eventually, the field strength levels off and becomes nearly constant, regardless of how much current is sent through the windings. This phenomenon is called saturation, and is the main nonlinear feature of ferromagnetic materials. For most high-permeability core steels, the maximum possible strength of the magnetic field is around 1.6 to 2 teslas (T). This is why the very strongest electromagnets, such as superconducting and very high current electromagnets, cannot use cores.

When the current in the coil is turned off, most of the domains in the core material lose alignment and return to a random state, and the electromagnetic field disappears. However, some of the alignment persists because the domains resist turning their direction of magnetization, which leaves the core magnetized as a weak permanent magnet. This phenomenon is called hysteresis and the remaining magnetic field is called remanent magnetism. The residual magnetization of the core can be removed by degaussing. In alternating current electromagnets, such as those used in motors, the core's magnetization is constantly reversed, and the remanence contributes to the motor's losses.

### Ampere's law

The magnetic field of electromagnets in the general case is given by Ampere's law:

∫ J· dA = oint H· dell

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Physics, Side effects, High-field electromagnets, Definition of terms.)_
