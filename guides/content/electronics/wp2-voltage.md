---
id: electronics/wp2-voltage
title: Voltage
category: electronics
tags: [voltage, electrical, potential difference, electric pressure, electric tension, electronics]
summary: Voltage, also known as (electrical) potential difference, electric pressure, or electric tension, is the difference in electric potential between two points.
source: https://en.wikipedia.org/w/index.php?title=Voltage&oldid=1370694066
source_title: Wikipedia: Voltage
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Voltage", Wikipedia, The Free Encyclopedia (revision 1370694066), https://en.wikipedia.org/w/index.php?title=Voltage&oldid=1370694066 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Voltage

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Voltage**, also known as (**electrical**) **potential difference**, **electric pressure**, or **electric tension**, is the difference in electric potential between two points. In a static electric field, it corresponds to the work needed per unit of charge to move a positive test charge from the first point to the second point. In the International System of Units (SI), the derived unit for voltage is the *volt* (V).

The voltage between points can be caused by the build-up of electric charge (e.g., a capacitor), and from an electromotive force (e.g., electromagnetic induction in a generator). On a macroscopic scale, a potential difference can be caused by electrochemical processes (e.g., cells and batteries), the pressure-induced piezoelectric effect, photovoltaic effect, and the thermoelectric effect. Since voltage is the difference in electric potential, it is a physical scalar quantity.

A voltmeter can be used to measure the voltage between two points in a system. Often a common reference potential such as the ground of the system is used as one of the points. In this case, voltage is often mentioned at a point without completely mentioning the other measurement point. A voltage can be associated with either a source of energy or the loss, dissipation, or storage of energy.

## Definition

The SI unit of work per unit charge is the joule per coulomb, where 1 volt = 1 joule (of work) per 1 coulomb of charge. The old SI definition for *volt* used power and current; starting in 1990, the Josephson effect was used, and in 2019 physical constants were given defined values for the definition of all SI units.

Voltage is denoted symbolically by Δ V, simplified *V*, especially in English-speaking countries. Internationally, the symbol *U* is standardized.

The electrochemical potential is the voltage that can be directly measured with a voltmeter. The Galvani potential that exists in structures with junctions of dissimilar materials, is also work per charge but cannot be measured with a voltmeter in the external circuit (see ).

Voltage is defined so that negatively charged objects are pulled towards higher voltages, while positively charged objects are pulled towards lower voltages. Therefore, the conventional current in a wire or resistor always flows from higher voltage to lower voltage.

Historically, voltage has been referred to using terms like "tension" and "pressure". Even today, the term "tension" is still used, for example within the phrase "high tension" (HT) which is commonly used in the contexts of automotive electronics and systems using thermionic valves (vacuum tubes).

### Electrostatics

In electrostatics, the voltage increase from point r_A to some point r_B is given by the change in electrostatic potential V from r_A to r_B. By definition, this is:
Δ V_AB = V(r_B) - V(r_A); = -∫_r_0^r_B E · dell - (-∫_r_0^r_A E · dell ); = -∫_r_A^r_B E · dell
where E is the intensity of the electric field.

In this case, the voltage increase from point A to point B is equal to the work done per unit charge, against the electric field, to move the charge from A to B without causing any acceleration. Mathematically, this is expressed as the line integral of the electric field along that path. In electrostatics, this line integral is independent of the path taken.

Under this definition, any circuit where there are time-varying magnetic fields, such as AC circuits, will not have a well-defined voltage between nodes in the circuit, since the electric force is not a conservative force in those cases. However, at lower frequencies when the electric and magnetic fields are not rapidly changing, this can be neglected (see ).

### Electrodynamics

The electric potential can be generalized to electrodynamics, so that differences in electric potential between points are well-defined even in the presence of time-varying fields. However, unlike in electrostatics, the electric field can no longer be expressed only in terms of the electric potential. Furthermore, the potential is no longer uniquely determined up to a constant, and can take significantly different forms depending on the choice of gauge.

In this general case, some authors use the word "voltage" to refer to the line integral of the electric field, rather than to differences in electric potential. In this case, the voltage rise along some path P from r_A to r_B is given by:
Δ V_AB = -∫_P E · dell
However, in this case the "voltage" between two points depends on the path taken.

### Circuit theory

In circuit analysis and electrical engineering, lumped element models are used to represent and analyze circuits. These elements are idealized and self-contained circuit elements used to model physical components.

When using a lumped element model, it is assumed that the effects of changing magnetic fields produced by the circuit are suitably contained to each element. Under these assumptions, the electric field in the region exterior to each component is conservative, and voltages between nodes in the circuit are well-defined, where
Δ V_AB = -∫_r_A^r_B E · dell
as long as the path of integration does not pass through the inside of any component. The above is the same formula used in electrostatics. This integral, with the path of integration being along the test leads, is what a voltmeter will actually measure.

If uncontained magnetic fields throughout the circuit are not negligible, then their effects can be modelled by adding mutual inductance elements. In the case of a physical inductor though, the ideal lumped representation is often accurate. This is because the external fields of inductors are generally negligible, especially if the inductor has a closed magnetic path. If external fields are negligible, we find that
Δ V_AB = -∫_exteriorE· dell=LdI/dt
is path-independent, and there is a well-defined voltage across the inductor's terminals. This is the reason that measurements with a voltmeter across an inductor are often reasonably independent of the placement of the test leads.

## Volt

The volt (symbol: **V**) is the derived unit for electric potential, voltage, and electromotive force. The volt is named in honour of the Italian physicist Alessandro Volta (1745–1827), who invented the voltaic pile, possibly the first chemical battery.

## Hydraulic analogy

A simple analogy for an electric circuit is water flowing in a closed circuit of pipework, driven by a mechanical pump. This can be called a "water circuit". The potential difference between two points corresponds to the pressure difference between two points. If the pump creates a pressure difference between two points, then water flowing from one point to the other will be able to do work, such as driving a turbine. Similarly, work can be done by an electric current driven by the potential difference provided by a battery. For example, the voltage provided by a sufficiently-charged automobile battery can "push" a large current through the windings of an automobile's starter motor. If the pump is not working, it produces no pressure difference, and the turbine will not rotate. Likewise, if the automobile's battery is very weak or "dead" (or "flat"), then it will not turn the starter motor.

The hydraulic analogy is a useful way of understanding many electrical concepts. In such a system, the work done to move water is equal to the "pressure drop" (compare p.d.) multiplied by the volume of water moved. Similarly, in an electrical circuit, the work done to move electrons or other charge carriers is equal to "electrical pressure difference" multiplied by the quantity of electrical charges moved. In relation to "flow", the larger the "pressure difference" between two points (potential difference or water pressure difference), the greater the flow between them (electric current or water flow). (See "electric power".)

_(Later sections of the original article were left out of this offline copy for length: Applications, Measuring instruments, Typical voltages, Galvani potential vs. electrochemical potential.)_
