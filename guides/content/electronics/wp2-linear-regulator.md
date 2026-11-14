---
id: electronics/wp2-linear-regulator
title: Linear regulator
category: electronics
tags: [linear regulator, electronics]
summary: In electronics, a linear regulator is a voltage regulator used to maintain a steady voltage.
source: https://en.wikipedia.org/w/index.php?title=Linear_regulator&oldid=1326322781
source_title: Wikipedia: Linear regulator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Linear regulator", Wikipedia, The Free Encyclopedia (revision 1326322781), https://en.wikipedia.org/w/index.php?title=Linear_regulator&oldid=1326322781 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Linear regulator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electronics, a **linear regulator** is a voltage regulator used to maintain a steady voltage. The resistance of the regulator varies in accordance with both the input voltage and the load, resulting in a constant voltage output. The regulating circuit varies its resistance, continuously adjusting a voltage divider network to maintain a constant output voltage and continually dissipating the difference between the input and regulated voltages as waste heat. By contrast, a *switching regulator* uses an active device that switches on and off to maintain an average value of output. Because the regulated voltage of a linear regulator must always be lower than input voltage, efficiency is limited and the input voltage must be high enough to always allow the active device to reduce the voltage by some amount.

Linear regulators may place the regulating device in parallel with the load (shunt regulator) or may place the regulating device between the source and the regulated load (a series regulator). Simple linear regulators may only contain as little as a Zener diode and a series resistor; more complicated regulators include separate stages of voltage reference, error amplifier and power pass element. Because a linear voltage regulator is a common element of many devices, single-chip regulators ICs are very common. Linear regulators may also be made up of assemblies of discrete solid-state or vacuum tube components.

Despite their name, linear regulators are non-linear circuits because they contain non-linear components (such as Zener diodes, as shown below in the simple shunt regulator) and because the output voltage is ideally constant (and a circuit with a constant output that does not depend on its input is a non-linear circuit).

## Overview

The transistor (or other device) is used as one half of a voltage divider to establish the regulated output voltage. The output voltage is compared to a reference voltage to produce a control signal to the transistor which will drive its gate or base. With negative feedback and good choice of compensation, the output voltage is kept reasonably constant. Linear regulators are often inefficient: since the transistor is acting like a resistor, it will waste electrical energy by converting it to heat. In fact, the power loss due to heating in the transistor is the current multiplied by the voltage difference between input and output voltage. The same function can often be performed much more efficiently by a switched-mode power supply, but a linear regulator may be preferred for light loads or where the desired output voltage approaches the source voltage. In these cases, the linear regulator may dissipate less power than a switcher. The linear regulator also has the advantage of not requiring magnetic devices (inductors or transformers) which can be relatively expensive or bulky, being often of simpler design, and cause less electromagnetic interference. Some designs of linear regulators use only transistors, diodes and resistors, which are easier to fabricate into an integrated circuit, further reducing their weight, footprint on a PCB, and price.

All linear regulators require an input voltage at least some minimum amount higher than the desired output voltage. That minimum amount is called the dropout voltage. For example, a common regulator such as the 7805 has an output voltage of 5 V, but can only maintain this if the input voltage remains above about 7 V, before the output voltage begins sagging below the rated output. Its dropout voltage is therefore 7 V − 5 V = 2 V. When the supply voltage is less than about 2 V above the desired output voltage, as is the case in low-voltage microprocessor power supplies, so-called low dropout regulators (LDOs) must be used.

When the output regulated voltage must be higher than the available input voltage, no linear regulator will work (not even a low dropout regulator). In this situation, a boost converter or a charge pump must be used. Most linear regulators will continue to provide some output voltage approximately the dropout voltage below the input voltage for inputs below the nominal output voltage until the input voltage drops significantly.

Linear regulators exist in two basic forms: shunt regulators and series regulators. Most linear regulators have a maximum rated output current. This is generally limited by either power dissipation capability, or by the current carrying capability of the output transistor.

### Shunt regulators

The shunt regulator works by providing a path from the supply voltage to ground through a variable resistance (the main transistor is in the "bottom half" of the voltage divider). The current through the shunt regulator is diverted away from the load and flows directly to ground, making this form usually less efficient than the series regulator. It is, however, simpler, sometimes consisting of just a voltage-reference diode, and is used in very low-powered circuits where the wasted current is too small to be of concern. This form is very common for voltage reference circuits. A shunt regulator can usually only sink (absorb) current.

For example, the following circuit is a simple shunt voltage regulator that operates through the Zener effect:
ShuntEx
The resistor *R*₁ supplies the Zener current *I*_Z as well as the load current *I*_R₂ (*R*₂ is the load). *R*₁ satisfies R_1 = fracV_S - V_ZI_Z + I_R_2 where *V*_Z is the Zener voltage.

This regulator is used for very simple low-power applications where the currents involved are very small and the load is permanently connected across the Zener diode (such as voltage reference or voltage source circuits). Once *R*₁ has been calculated, removing *R*₂ will allow the full load current (plus the Zener current) through the diode and may exceed the diode's maximum current rating, thereby damaging it.

The regulation of this circuit is also not very good because the Zener current (and hence the Zener voltage) will vary depending on *V*_S and inversely depending on the load current. In some designs, the Zener diode may be replaced with another similarly functioning device, especially in an ultra-low-voltage scenario, like (under forward bias) several normal diodes or LEDs in series.

### Series regulators

Series regulators are the more common form; they are more efficient than shunt designs. The series regulator works by providing a path from the supply voltage to the load through a variable resistance, usually a transistor (in this role it is usually termed the series **pass transistor**); it is in the "top half" of the voltage divider - the bottom half being the load. The power dissipated by the regulating device is equal to the power supply output current times the *voltage drop* in the regulating device. For efficiency and reduced stress on the pass transistor, designers try to minimize the voltage drop but not all circuits regulate well once the input (unregulated) voltage comes close to the required output voltage; those that do are termed **low dropout** regulators, A series regulator can usually only source (supply) current, unlike shunt regulators.

For example, adding an emitter follower stage to the shunt example forms a simple series regulator:

Here, the load current *I*_R₂ is supplied by the transistor whose base is now connected to the Zener diode. Thus the transistor's base current (*I*_B) forms the load current for the Zener diode and is much smaller than the current through *R*₂. This regulator is classified as "series" because the regulating element, viz., the transistor, appears in series with the load.

*R*₁ sets the Zener current (*I*_Z) and is determined as R_1 = (V_S - V_Z)/(I_Z + K · I_B) where *V*_Z is the Zener voltage; *I*_B is the transistor's base current; *K* is a design constant, chosen to ensure that *R*₁ is low enough for adequate *I*_B and typically between 1.2 and 2; and I_B = fracI_R_2h_FE(min) where *I*_R₂ is the required load current and also the transistor's emitter current (assumed to be equal to the collector current) and *h*_FE(min) is the minimum acceptable DC current gain for the transistor.

_(This section is shortened.)_

## Protection

Linear IC voltage regulators may include a variety of protection methods:

- Current limiting such as constant-current limiting or foldback
- Thermal shutdown
- Safe operating area protection

Sometimes external protection is used, such as crowbar protection.

_(Later sections of the original article were left out of this offline copy for length: the rest of Overview, Fixed regulators, Variable regulators, Dual tracking regulators, Using a linear regulator.)_
