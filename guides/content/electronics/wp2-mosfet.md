---
id: electronics/wp2-mosfet
title: MOSFET
category: electronics
tags: [mosfet, mos-fet, mos fet, mos transistor, electronics]
summary: In electronics, the metal–oxide–semiconductor field-effect transistor (MOSFET, MOS-FET, MOS FET, or MOS transistor) is a type of field-effect transistor (FET), most commonly fabricated by the controlled oxidation of silicon.
source: https://en.wikipedia.org/w/index.php?title=MOSFET&oldid=1384578492
source_title: Wikipedia: MOSFET
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "MOSFET", Wikipedia, The Free Encyclopedia (revision 1384578492), https://en.wikipedia.org/w/index.php?title=MOSFET&oldid=1384578492 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# MOSFET

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electronics, the **metal–oxide–semiconductor field-effect transistor** (**MOSFET**, **MOS-FET**, **MOS FET**, or **MOS transistor**) is a type of field-effect transistor (FET), most commonly fabricated by the controlled oxidation of silicon. It has an insulated gate, the voltage of which determines the conductivity of the device. This ability to change conductivity with the amount of applied voltage can be used for amplifying or switching electronic signals. The term *metal–insulator–semiconductor field-effect transistor* (*MISFET*) is almost synonymous with *MOSFET*. Another near-synonym is *insulated-gate field-effect transistor* (*IGFET*).

Physicist Julius Edgar Lilienfeld first proposed the concept of a field-effect transistor (FET) in 1925, but it was not possible to construct a working device at that time. The first working metal–oxide–semiconductor field-effect transistor (MOSFET) was invented in 1959 by engineers Mohamed Atalla and Dawon Kahng at Bell Labs. Their breakthrough—fabricating a practical field-effect transistor enabled by Atalla’s earlier work on silicon surface passivation and thermal oxidation—revolutionized electronics and paved the way for smaller and cheaper radios, calculators, computers, and other electronic devices.

The main advantage of a MOSFET is that it requires almost no input current to control the load current under steady-state or low-frequency conditions, especially compared to bipolar junction transistors (BJTs). However, at high frequencies or when switching rapidly, a MOSFET may require significant current to charge and discharge its gate capacitance. In an *enhancement mode* MOSFET, voltage applied to the gate terminal increases the conductivity of the device. In *depletion mode* transistors, voltage applied at the gate reduces the conductivity.

The "metal" in the name MOSFET is sometimes a misnomer, because the gate material can be a layer of polysilicon (polycrystalline silicon). Similarly, "oxide" in the name can also be a misnomer, as different dielectric materials are used with the aim of obtaining strong channels with smaller applied voltages.

The MOSFET is by far the most common transistor in digital circuits, as billions may be included in a memory chip or microprocessor. As MOSFETs can be made with either a p-type or n-type channel, complementary pairs of MOS transistors can be used to make switching circuits with very low power consumption, in the form of CMOS logic.

## Composition

Silicon remains the primary semiconductor in CMOS devices, but to enhance carrier mobility, manufacturers—most notably IBM and Intel—induce strain in the silicon channel by incorporating silicon-germanium (SiGe) in nearby regions or applying stress engineering techniques, thereby improving transistor performance without using SiGe alloys as the channel material itself.

Many semiconductors with better electrical properties than silicon, such as gallium arsenide, do not form good semiconductor-to-insulator interfaces, and thus are not suitable for MOSFETs. Research continues on creating insulators with acceptable electrical characteristics on other semiconductor materials.

To overcome the increase in power consumption due to gate current leakage, a high-κ dielectric is used instead of silicon dioxide for the gate insulator, while polysilicon is replaced by metal gates (e.g. Intel, 2009).

The gate is separated from the channel by a thin insulating layer, traditionally of silicon dioxide and later of silicon oxynitride. Some companies use a high-κ dielectric and metal gate combination in the 45 nanometer node.

When a voltage is applied between the gate and the source, the electric field generated penetrates through the oxide and creates an *inversion layer* or *channel* at the semiconductor-insulator interface. The inversion layer provides a channel through which current can pass between source and drain terminals. Varying the voltage between the gate and body modulates the conductivity of this layer and thereby controls the current flow between drain and source. This is known as enhancement mode.

## Operation

### Metal–oxide–semiconductor structure

The traditional metal–oxide–semiconductor (MOS) structure is obtained by growing a layer of silicon dioxide (SiO2) on top of a silicon substrate, commonly by thermal oxidation and depositing a layer of metal or polycrystalline silicon (the latter is commonly used). As silicon dioxide is a dielectric material, its structure is equivalent to a planar capacitor, with one of the electrodes replaced by a semiconductor.

When a voltage is applied across a MOS structure, it modifies the distribution of charges in the semiconductor. If we consider a p-type semiconductor (with *N*_A the density of acceptors, *p* the density of holes; *p = N*_A in neutral bulk), a positive voltage, *V*_G, from gate to body (see figure) creates a depletion layer by forcing the positively charged holes away from the gate-insulator/semiconductor interface, leaving exposed a carrier-free region of immobile, negatively charged acceptor ions (see doping). If *V*_G is high enough, a high concentration of negative charge carriers forms in an *inversion layer* located in a thin layer next to the interface between the semiconductor and the insulator.

Conventionally, the gate voltage at which the volume density of electrons in the inversion layer is the same as the volume density of holes in the body is called the threshold voltage. When the voltage between transistor gate and source (*V*_G) exceeds the threshold voltage (*V*_th), the difference is known as overdrive voltage.

This structure with p-type body is the basis of the n-type MOSFET, which requires the addition of n-type source and drain regions.

### MOS capacitors and band diagrams

The MOS capacitor structure is the heart of the MOSFET. Consider a MOS capacitor where the silicon base is of p-type. If a positive voltage is applied at the gate, holes which are at the surface of the p-type substrate will be repelled by the electric field generated by the voltage applied. At first, the holes will simply be repelled and what will remain on the surface will be immobile (negative) atoms of the acceptor type, which creates a depletion region on the surface. A hole is created by an acceptor atom, e.g., boron, which has one less electron than a silicon atom. Holes are not actually repelled, being non-entities; electrons are attracted by the positive field, and fill these holes. This creates a depletion region where no charge carriers exist because the electron is now fixed onto the atom and immobile.

As the voltage at the gate increases, there will be a point at which the surface above the depletion region will be converted from p-type into n-type, as electrons from the bulk area will start to get attracted by the larger electric field. This is known as *inversion*. The threshold voltage at which this conversion happens is one of the most important parameters in a MOSFET.

In the case of a p-type MOSFET, bulk inversion happens when the intrinsic energy level at the surface becomes smaller than the Fermi level at the surface. This can be seen on a band diagram. The Fermi level defines the type of semiconductor in discussion. If the Fermi level is equal to the Intrinsic level, the semiconductor is of intrinsic, or pure type. If the Fermi level lies closer to the conduction band (valence band) then the semiconductor type will be of n-type (p-type).

When the gate voltage is increased in a positive sense this will shift the intrinsic energy level band so that it will curve downwards towards the valence band. If the Fermi level lies closer to the valence band (for p-type), there will be a point when the Intrinsic level will start to cross the Fermi level and when the voltage reaches the threshold voltage, the intrinsic level does cross the Fermi level, and that is what is known as inversion. At that point, the surface of the semiconductor is inverted from p-type into n-type.

If the Fermi level lies above the intrinsic level, the semiconductor is of n-type, therefore at inversion, when the intrinsic level reaches and crosses the Fermi level (which lies closer to the valence band), the semiconductor type changes at the surface as dictated by the relative positions of the Fermi and intrinsic energy levels.

### Structure and channel formation

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Operation, Circuit symbols, Applications, Construction, Scaling, Other types.)_
