---
id: electronics/voltage-drop
title: Voltage drop
category: electronics
tags: [electronics, voltage drop, wiring, 12 V]
summary: Voltage drop in wires and how to size cables for low-voltage DC runs.
source: "https://en.wikipedia.org/w/index.php?title=Voltage_drop&oldid=1384299568"
source_title: Voltage drop (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Voltage drop' (rev 1384299568), Wikipedia, https://en.wikipedia.org/wiki/Voltage_drop. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# Voltage drop

In electronics, voltage drop is the decrease of electric potential along the path of a current flowing in a circuit. Voltage drops in the internal resistance of the source, across conductors, across contacts, and across connectors are undesirable because some of the energy supplied is dissipated as heat due to resistance. The voltage drop across the load is proportional to the power available to be converted in that load to some other useful form of energy.

For example, an electric space heater may have a resistance of 10 ohms, and the wires that supply it may have a resistance of 0.2 ohms, about 2% of the total circuit resistance. This means that approximately 2% of the supplied voltage is lost in the wire itself. An excessive voltage drop may result in the unsatisfactory performance of the space heater and the overheating of the wires and connections.

National and local electrical codes may set guidelines for the maximum voltage drop allowed in electrical wiring to ensure efficiency of distribution and proper operation of electrical equipment. The maximum permitted voltage drop varies from one country to another. In electronic design and power transmission, various techniques are employed to compensate for the effect of voltage drop on long circuits or where voltage levels must be accurately maintained. The simplest way to reduce voltage drop is to increase the diameter of the conductor between the source and the load, which lowers the overall resistance. In electric power distribution systems, a given amount of power can be transmitted with less voltage drop if a higher voltage is used. More sophisticated techniques use active elements to compensate for excessive voltage drop.

## Resistive DC

Ohm's law can be used to determine the DC voltage drop by multiplying current times resistance: *V* = *IR*, where *V* is the voltage drop, *I* is the current, and *R* is the resistance. Also, Kirchhoff's circuit laws state that in any DC circuit, the sum of the voltage drops across each component of the circuit is equal to the supply voltage.

Consider a direct-current circuit with a nine-volt DC source; three resistors of 67 ohms, 100 ohms, and 470 ohms; and a light bulb—all connected in series. The DC source, the conductors (wires), the resistors, and the light bulb (the load) all have resistance; all use and dissipate supplied energy to some degree. Their physical characteristics determine how much energy. For example, the DC resistance of a conductor depends upon the conductor's length, cross-sectional area, type of material, and temperature.

If the voltage between the DC source and the first resistor (67 ohms) is measured, the voltage potential at the first resistor will be slightly less than nine volts. The current passes through the conductor (wire) from the DC source to the first resistor; as this occurs, some of the supplied energy is "lost" (unavailable to the load), due to the resistance of the conductor. Voltage drop exists in both the supply and return wires of a circuit. If the voltage drop across each resistor is measured, the measurement will be a significant number. That represents the energy used by the resistor. The larger the resistor, the more energy used by that resistor, and the bigger the voltage drop across that resistor.

## Reactive AC

AC voltages additionally have a second kind of opposition to current flow: reactance. The sum of resistance and reactance is called impedance.

Electrical impedance is commonly represented by the variable *Z* and measured in ohms at a specific frequency. Electrical impedance is computed as the vector sum of electrical resistance, capacitive reactance, and inductive reactance.

The amount of impedance in an alternating-current circuit depends on the frequency of the alternating current and the magnetic permeability of electrical conductors and electrically isolated elements (including surrounding elements), which varies with their size and spacing.

Analogous to Ohm's law for direct-current circuits, electrical impedance may be expressed by the formula *E* = *IZ*. So, the voltage drop in an AC circuit is the product of the current and the impedance of the circuit.

## Diode

Forward-biased p–n junctions in diodes and transistors experience a voltage drop that is slightly above the diode's threshold voltage. The energy is dissipated through photons, which for the case of light-emitting diodes are emitted and visible.
