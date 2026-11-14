---
id: electronics/wp2-bipolar-junction-transistor
title: Bipolar junction transistor
category: electronics
tags: [bipolar junction transistor, bjt, electronics]
summary: A bipolar junction transistor (BJT) is a type of transistor that uses both electrons and electron holes as charge carriers.
source: https://en.wikipedia.org/w/index.php?title=Bipolar_junction_transistor&oldid=1385177807
source_title: Wikipedia: Bipolar junction transistor
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Bipolar junction transistor", Wikipedia, The Free Encyclopedia (revision 1385177807), https://en.wikipedia.org/w/index.php?title=Bipolar_junction_transistor&oldid=1385177807 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Bipolar junction transistor

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **bipolar junction transistor** (**BJT**) is a type of transistor that uses both electrons and electron holes as charge carriers. In contrast, a unipolar transistor, such as a field-effect transistor (FET), uses only one kind of charge carrier. A bipolar transistor allows a small current injected at one of its terminals to control a much larger current between the remaining two terminals, making the device capable of amplification or switching.

BJTs use two p–n junctions between two semiconductor types, n-type and p-type, which are regions in a single crystal of material. The junctions can be made in several different ways, such as changing the doping of the semiconductor material as it is grown, by depositing metal pellets to form alloy junctions, or by such methods as diffusion of n-type and p-type doping substances into the crystal. The superior predictability and performance of junction transistors quickly displaced the original point-contact transistor. Diffused transistors, along with other components, are elements of integrated circuits for analog and digital functions. Hundreds of bipolar junction transistors can be made in one circuit at a very low cost.

Bipolar transistor integrated circuits were the main active devices of a generation of mainframe and minicomputers, but computer systems now use complementary metal–oxide–semiconductor (CMOS) integrated circuits relying on the field-effect transistor (FET). Bipolar transistors are still used for amplification of signals, switching, and in mixed-signal integrated circuits using BiCMOS. Specialized types are used for high voltage and high current switches, or for radio-frequency (RF) amplifiers.

## Function

BJTs exist as PNP and NPN types, based on the doping types of the three main terminal regions. An NPN transistor comprises two semiconductor junctions that share a thin p-doped region, and a PNP transistor comprises two semiconductor junctions that share a thin n-doped region. N-type means doped with impurities (such as phosphorus or arsenic) that provide mobile electrons, while p-type means doped with impurities (such as boron) that provide holes that readily accept electrons.

Charge flow in a BJT is due to diffusion of charge carriers (electrons and holes) across a junction between two regions of different charge carrier concentration. The regions of a BJT are called *emitter*, *base*, and *collector*. A discrete transistor has three leads for connection to these regions. Typically, the emitter region is heavily doped compared to the other two layers, and the collector is doped more lightly (typically ten times lighter) than the base. By design, most of the BJT collector current is due to the flow of charge carriers injected from a heavily doped emitter into the base where they are minority carriers (electrons in NPNs, holes in PNPs) that diffuse toward the collector, so BJTs are classified as *minority-carrier devices*.

In typical operation, the base–emitter junction is forward biased, which means that the p-doped side of the junction is at a more positive potential than the n-doped side, and the base–collector junction is reverse biased. When forward bias is applied to the base–emitter junction, the equilibrium between the thermally generated carriers and the repelling electric field of the emitter depletion region is disturbed. This allows thermally excited carriers (electrons in NPNs, holes in PNPs) to inject from the emitter into the base region. These carriers create a diffusion current through the base from the region of high concentration near the emitter toward the region of low concentration near the collector.

To minimize the fraction of carriers that recombine before reaching the collector–base junction, the transistor's base region must be thin enough that carriers can diffuse across it in much less time than the semiconductor's minority-carrier lifetime. Having a lightly doped base ensures recombination rates are low. In particular, the thickness of the base must be much less than the diffusion length of the carriers. The collector–base junction is reverse-biased, and so negligible carrier injection occurs from the collector to the base, but carriers that are injected into the base from the emitter, and diffuse to reach the collector–base depletion region, are swept into the collector by the electric field in the depletion region. The thin *shared* base and asymmetric collector–emitter doping are what differentiates a bipolar transistor from two *separate* diodes connected in series.

### Voltage, current, and charge control

The collector–emitter current can be viewed as being controlled by the base–emitter current (current control), or by the base–emitter voltage (voltage control). These views are related by the current–voltage relation of the base–emitter junction, which is the usual exponential current–voltage curve of a p–n junction (diode).

The explanation for collector current is the concentration gradient of minority carriers in the base region. Due to low-level injection (in which there are many fewer excess carriers than normal majority carriers) the ambipolar transport rates (in which the excess majority and minority carriers flow at the same rate) is in effect determined by the excess minority carriers.

Detailed transistor models of transistor action, such as the Gummel–Poon model, account for the distribution of this charge explicitly to explain transistor behavior more exactly. The charge-control view easily handles phototransistors, where minority carriers in the base region are created by the absorption of photons, and handles the dynamics of turn-off, or recovery time, which depends on charge in the base region recombining. However, because base charge is not a signal that is visible at the terminals, the current- and voltage-control views are generally used in circuit design and analysis.

In analog circuit design, the current-control view is sometimes used because it is approximately linear. That is, the collector current is approximately β_F times the base current. Some basic circuits can be designed by assuming that the base–emitter voltage is approximately constant and that collector current is β times the base current. However, to accurately and reliably design production BJT circuits, the voltage-control model (e.g. the Ebers–Moll model) is required. The voltage-control model requires an exponential function to be taken into account, but when it is linearized such that the transistor can be modeled as a transconductance, as in the Ebers–Moll model, design for circuits such as differential amplifiers again becomes a mostly linear problem, so the voltage-control view is often preferred. For translinear circuits, in which the exponential I–V curve is key to the operation, the transistors are usually modeled as voltage-controlled current sources whose transconductance is proportional to their collector current. In general, transistor-level circuit analysis is performed using SPICE or a comparable analog-circuit simulator, so mathematical model complexity is usually not of much concern to the designer, but a simplified view of the characteristics allows designs to be created following a logical process.

### Turn-on, turn-off, and storage delay

Bipolar transistors, and particularly power transistors, have long base-storage times when they are driven into saturation; the base storage limits turn-off time in switching applications. A Baker clamp can prevent the transistor from heavily saturating, which reduces the amount of charge stored in the base and thus improves switching time.

### Transistor characteristics: alpha (α) and beta (β)

The proportion of carriers able to cross the base and reach the collector is a measure of the BJT efficiency. The heavy doping of the emitter region and light doping of the base region causes many more electrons to be injected from the emitter into the base than holes to be injected from the base into the emitter. A thin and lightly doped base region means that most of the minority carriers that are injected into the base will diffuse to the collector and not recombine.

#### Common-emitter current gain

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Function, Structure, Regions of operation, Theory and modeling, Current direction conventions, Applications.)_
