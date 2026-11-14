---
id: power/wp2-earthing-system
title: Earthing system
category: power
tags: [earthing system, grounding system, energy]
summary: An earthing system (internationally) or grounding system (US) connects specific parts of an electric power system, such as the conductive surfaces of equipment, with the ground for safety and functional purposes.
source: https://en.wikipedia.org/w/index.php?title=Earthing_system&oldid=1361707333
source_title: Wikipedia: Earthing system
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Earthing system", Wikipedia, The Free Encyclopedia (revision 1361707333), https://en.wikipedia.org/w/index.php?title=Earthing_system&oldid=1361707333 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Earthing system

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **earthing system** (internationally) or **grounding system** (US) connects specific parts of an electric power system, such as the conductive surfaces of equipment, with the ground for safety and functional purposes. The choice of earthing system can affect the safety and electromagnetic compatibility of the installation. Regulations for earthing systems vary among countries, though most follow the recommendations of the International Electrotechnical Commission (IEC). Regulations may identify special cases for earthing in mines, in patient care areas, or in hazardous areas of industrial plants.

## Purpose

### Protection against Electric Shock

System earthing serves as a key component of one of the most commonly used forms of protection against electric shock. International standard IEC 61140 *Protection against electric shock* sets out requirements for shock protection under both *normal conditions* (so called *basic protection*), and *fault conditions* (i.e. *earth faults*). Basic protection involves concepts such as insulation. Fault protection (with respect to risk of shock, as opposed to other forms of electrical system faults) concerns the concept of accessible conductive parts within an installation (building) which were never intended to be electrically energised, unintentionally becoming energised and hazardous.

One of the methods described by IEC 61140 for addressing fault protection, and one of the most commonly used, is *automatic disconnection of supply (ADS)*. This involves the use of an earthing system and protective devices (fuses or circuit breakers) which will automatically break circuits when a fault is detected, removing the hazard. Under this method any accessible conductive parts of an installation (including those of Class I appliances) which could develop a fault and become hazardous are connected to an earthing system such that in the event of such a fault developing (due to unintended contact of the part with a live conductor), a circuit is completed, allowing current to flow, and that current flow can cause a protective device to be triggered.

As shown later, only in some earthing system arrangements does the Earth form part of this *fault path*. In all cases one or more literal connections to Earth are utilised, which acts as a reference point, restricting the degree to which voltage of any part of the earthing system can stray from that of the surrounding ground.

### Static build-up prevention

A connection to Earth prevents the build-up of static electricity, as induced by friction for example, such as when wind blows onto a radio mast.

### Surge protection

Electrical surges can occur due to switching events or lightning strikes and can potentially cause damage to equipment, property, or even pose a threat to life. While in some cases surge protection devices will simply divert surges between electrical conductors, surges will sometimes be diverted to Earth.

### Functional earthing

Functional earthing serves a purpose other than electrical safety. Example purposes include electromagnetic interference (EMI) filtering in an EMI filter, and the use of the Earth as a return path in a single-wire earth return distribution system.

### Other earthing systems

This article only concerns grounding for electrical power. Examples of other earthing systems are:

- As part of a single-wire earth return power and signal lines, such as were used for low wattage power delivery and for telegraph lines.
- In radio, as a ground plane for large monopole antenna.
- As ancillary voltage balance for other kinds of radio antennas, such as dipoles.
- As the feed-point of a ground dipole antenna for VLF and ELF radio.

## Low-voltage systems

In low-voltage networks, which distribute the electric power to the widest class of end users, the main concern for the design of earthing systems is the safety of consumers who use the electric appliances and their protection against electric shocks. The earthing system, in combination with protective devices such as fuses and residual current devices, must ultimately ensure that a person does not come into contact with a metallic object whose potential relative to the person's potential exceeds a *safe* threshold, typically set at about 50 V AC.

Whilst historically there has been considerable national variation, most developed countries introduced 220 V, 230 V, or 240 V AC sockets with earthed contacts either just before or soon after World War II. However, in the United States and Canada, where the supply voltage is only 120 V relative to ground (two opposite-phase 120 V lines are used to provide 240 V), power outlets installed before the mid-1960s generally did not include a ground (earth) pin. In the developing world, local wiring practices may or may not provide a connection to an earth conductor.

On low-voltage electricity networks with a phase-to-neutral voltage, which are mostly used in industry, mining equipment, and machines rather than publicly accessible networks, the earthing system design is equally important from a safety point of view as for domestic users.

The US National Electrical Code permitted the use of the supply neutral wire as the equipment enclosure connection to ground from 1947 to 1996 for ranges (including separate cooktops and ovens) and from 1953 to 1996 for clothes dryers, whether plug-in or permanently fixed, provided that the circuit originated in the main service panel. Normal imbalances in the circuit would create small equipment voltages with respect to Earth; a failure of the neutral conductor or connections would allow the equipment to go to full 120 volts, an easily lethal situation. The 1996 and newer editions of the NEC no longer permit this practice. For similar reasons, most countries now mandate dedicated protective earth connections in consumer wiring, a practice that has become nearly universal. In distribution networks however, where connections are fewer and less vulnerable, many countries do permit earth and neutral functions to share a conductor (see PEN conductor).

If the fault path between exposed conductive parts and the supply has sufficiently low impedance, then should such a part accidentally become energized, the fault current will cause the circuit overcurrent protection device (fuse or circuit breaker) to open, clearing the fault. However, if the impedance of the fault path is too high, then fault currents may not trip the overcurrent protection device quickly enough to meet the requirements of local electrical regulations. This is often the case with a TT-type earthing system. In such cases use of a residual-current device (RCD) may allow the required disconnection times to be met.

### IEC terminology

International standard IEC 60364 distinguishes three families of earthing arrangements, using the two-letter codes — **TN**, **TT**, and **IT**.

The first letter indicates the relationship between the power-supply (generator or transformer) and Earth:

**"T"** — Direct connection of a point to Earth.
**"I"** — All live parts isolated from Earth, or one point connected to Earth through an impedance.

The second letter indicates the relationship between the exposed-conductive-parts of the installation, and Earth:

**"T"** — Direct connection to Earth, independent of a power-supply connection to Earth.
**"N"** — Direct connection to the point on the power-supply where the power-supply connects to Earth. This point is typically the neutral point of a star-connected transformer, from where a neutral connection might also be provided.

Any subsequent letter(s) indicate:

**"S"** — Neutral and protective functions provided by separate conductors.
**"C"** — Neutral and protective functions provided by the same single conductor (PEN conductor).

The letters can reasonably be assumed to derive from French wording (the IEC standard is written in both French and English), with *T* deriving from *terre* (Earth); *N* from *neutre* (neutral); *S* from *séparé* (separate); *C* from *combiné* (combined); and *I* from both *isolé* and *impédance* (isolation and impedance — the two key details of an IT system).

#### Types of TN system

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Low-voltage systems, High-voltage systems, Grounding rods, Grounding connectors, Soil resistance.)_
