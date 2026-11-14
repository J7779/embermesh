---
id: power/wp2-thermoelectric-generator
title: Thermoelectric generator
category: power
tags: [thermoelectric generator, teg, seebeck generator, energy]
summary: A thermoelectric generator (TEG), also called a Seebeck generator, is a solid state device that converts heat (driven by temperature differences) directly into electrical energy through a phenomenon called the Seebeck effect (a form of thermoelectric effect).
source: https://en.wikipedia.org/w/index.php?title=Thermoelectric_generator&oldid=1385574100
source_title: Wikipedia: Thermoelectric generator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Thermoelectric generator", Wikipedia, The Free Encyclopedia (revision 1385574100), https://en.wikipedia.org/w/index.php?title=Thermoelectric_generator&oldid=1385574100 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Thermoelectric generator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **thermoelectric generator** (**TEG**), also called a **Seebeck generator**, is a solid state device that converts heat (driven by temperature differences) directly into electrical energy through a phenomenon called the *Seebeck effect* (a form of thermoelectric effect). Thermoelectric generators function like heat engines, but are less bulky and have no moving parts. However, TEGs are typically more expensive and less efficient. When the same principle is used in reverse to create a heat gradient from an electric current, it is called a thermoelectric (or Peltier) cooler.

Thermoelectric generators could be used in power plants and factories to convert waste heat into additional electrical power and in automobiles as automotive thermoelectric generators (ATGs) to increase fuel efficiency. Radioisotope thermoelectric generators use radioisotopes to generate the required temperature difference to power space probes. Thermoelectric generators can also be used alongside solar panels.

## Efficiency

The typical efficiency of TEGs is around 5–8%, although it can be higher. Older devices used bimetallic junctions and were bulky. More recent devices use highly doped semiconductors made from bismuth telluride (Bi₂Te₃), lead telluride (PbTe), calcium manganese oxide (Ca₂Mn₃O₈), or combinations thereof, depending on application temperature. These are solid-state devices and unlike dynamos have no moving parts, with the occasional exception of a fan or pump to improve heat transfer. If the hot region is around 1273K and the ZT values of 3 - 4 are implemented, the efficiency is approximately 33-37%; allowing TEG's to compete with certain heat engine efficiencies.

As of 2021, there are materials (some containing widely available and inexpensive arsenic and tin) reaching a ZT value > 3; monolayer arsenic triphosphide (AsP3) (ZT = 3.36 on the armchair axis); n-type doped indium triphosphide (InP3) (ZT = 3.23); p-type doped tin triphosphide (SnP3) (ZT = 3.46); p-type doped antimony triphosphide (SbP3) (ZT = 3.5).

## Construction

Thermoelectric power generators consist of three major components: thermoelectric materials, thermoelectric modules and thermoelectric systems that interface with the heat source.

### Thermoelectric materials

Thermoelectric materials generate power directly from the heat by converting temperature differences into electric voltage. These materials must have both high electrical conductivity (σ) and low thermal conductivity (κ) to be good thermoelectric materials. Having low thermal conductivity ensures that when one side is made hot, the other side stays cold, which helps to generate a large voltage while in a temperature gradient. The measure of the magnitude of electrons flow in response to a temperature difference across that material is given by the Seebeck coefficient (S). The efficiency of a given material to produce a thermoelectric power is simply estimated by its "figure of merit" zT = S²σT/κ.

For many years, the main three semiconductors known to have both low thermal conductivity and high power factor were bismuth telluride (Bi₂Te₃), lead telluride (PbTe), and silicon germanium (SiGe). Some of these materials have somewhat rare elements which make them expensive.

Today, the thermal conductivity of semiconductors can be lowered without affecting their high electrical properties using nanotechnology. This can be achieved by creating nanoscale features such as particles, wires or interfaces in bulk semiconductor materials. However, the manufacturing processes of nano-materials are still challenging.

### Thermoelectric advantages

Thermoelectric generators are all-solid-state devices that do not require any fluids for fuel or cooling, making them non-orientation dependent allowing for use in zero-gravity or deep-sea applications. The solid-state design allows for operation in severe environments. Thermoelectric generators have no moving parts which produce a more reliable device that does not require maintenance for long periods. The durability and environmental stability have made thermoelectrics a favorite for NASA's deep space explorers among other applications. One of the key advantages of thermoelectric generators outside of such specialized applications is that they can potentially be integrated into existing technologies to boost efficiency and reduce environmental impact by producing usable power from waste heat.

### Thermoelectric module

A thermoelectric module is a circuit containing thermoelectric materials which generate electricity from heat directly. A thermoelectric module consists of two dissimilar thermoelectric materials joined at their ends: an n-type (with negative charge carriers), and a p-type (with positive charge carriers) semiconductor. Direct electric current will flow in the circuit when there is a temperature difference between the ends of the materials. Generally, the current magnitude is directly proportional to the temperature difference:

mathbf J = -σ S nabla T

where σ is the local conductivity, S is the Seebeck coefficient (also known as thermopower), a property of the local material, and nabla T is the temperature gradient.

In application, thermoelectric modules in power generation work in very tough mechanical and thermal conditions. Because they operate in a very high-temperature gradient, the modules are subject to large thermally induced stresses and strains for long periods. They also are subject to mechanical fatigue caused by a large number of thermal cycles.

Thus, the junctions and materials must be selected so that they survive these tough mechanical and thermal conditions. Also, the module must be designed such that the two thermoelectric materials are thermally in parallel, but electrically in series. The efficiency of a thermoelectric module is greatly affected by the geometry of its design.

### Thermoelectric design

Thermoelectric generators are made of several thermopiles, each consisting of many thermocouples made of a connected n-type and p-type material. The arrangement of the thermocouples is typically in three main designs: planar, vertical, and mixed. Planar design involves thermocouples put onto a substrate horizontally between the heat source and cool side, resulting in the ability to create longer and thinner thermocouples, thereby increasing the thermal resistance and temperature gradient and eventually increasing voltage output. Vertical design has thermocouples arranged vertically between the hot and cool plates, leading to high integration of thermocouples as well as a high output voltage, making this design the most widely used design commercially. The mixed design has the thermocouples arranged laterally on the substrate while the heat flow is vertical between plates. Microcavities under the hot contacts of the device allow for a temperature gradient, which allows for the substrate's thermal conductivity to affect the gradient and efficiency of the device.

For microelectromechanical systems, TEGs can be designed on the scale of handheld devices to use body heat in the form of thin films. Flexible TEGs for wearable electronics are able to be made with novel polymers through additive manufacturing or thermal spraying processes. Cylindrical TEGs for using heat from vehicle exhaust pipes can also be made using circular thermocouples arranged in a cylinder. Many designs for TEGs can be made for the different devices they are applied to.

### Thermoelectric systems

Using thermoelectric modules, a thermoelectric system generates power by taking in heat from a source such as a hot exhaust flue. To operate, the system needs a large temperature gradient, which is not easy in real-world applications. The cold side must be cooled by air or water. Heat exchangers are used on both sides of the modules to supply this heating and cooling.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Construction, Materials for TEG, Uses, Practical limitations, More on photovoltaic-TEG (PV-TEG) hybrid systems, Future market.)_
