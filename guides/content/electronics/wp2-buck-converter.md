---
id: electronics/wp2-buck-converter
title: Buck converter
category: electronics
tags: [buck converter, step-down converter, electronics]
summary: A buck converter or step-down converter is a DC-to-DC converter which decreases voltage, while increasing current, from its input (supply) to its output (load).
source: https://en.wikipedia.org/w/index.php?title=Buck_converter&oldid=1367033299
source_title: Wikipedia: Buck converter
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Buck converter", Wikipedia, The Free Encyclopedia (revision 1367033299), https://en.wikipedia.org/w/index.php?title=Buck_converter&oldid=1367033299 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Buck converter

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **buck converter** or **step-down converter** is a DC-to-DC converter which decreases voltage, while increasing current, from its input (*supply*) to its output (*load*). It is a class of switched-mode power supply. Switching converters (such as buck converters) provide much greater power efficiency as DC-to-DC converters than linear regulators, which are simpler circuits that dissipate power as heat, but do not step up output current. The efficiency of buck converters can be very high, often over 90%, making them useful for tasks such as converting a computer's main supply voltage, which is usually 12 V, down to lower voltages needed by USB, DRAM and the CPU, which are usually 5, 3.3 or 1.8 V.

Buck converters typically contain at least two semiconductors (a diode and a transistor, although modern buck converters frequently replace the diode with a second transistor used for synchronous rectification) and at least an inductor as energy storage element, usually in combination with a capacitor. To reduce voltage ripple, filters made of capacitors (sometimes in combination with additional inductors) are normally added to such a converter's output (load-side filter) and input (supply-side filter). Its name derives from the inductor that “bucks” or opposes the supply voltage.

Buck converters typically operate with a switching frequency range from 100 kHz to a few MHz. A higher switching frequency allows for use of smaller inductors and capacitors, but also increases lost efficiency to more frequent transistor switching.

## Theory

The basic concept of a buck converter is:

1. Use the higher-than-needed voltage of the source to quickly induce a current into an inductor ("on" in fig. 2 and 4).
1. Disconnect the source and use the inertia of the current in the inductor to provide more current than the source delivers ("off" in fig. 2 and 4). To close the circuit with the source disconnected, a second switch, usually a diode, is needed.

During on-state, the source may need to momentarily provide more current than its rating for constant load allows, but the on-time is too short for the source to take damage. During off-state, no current is drawn from the source, and the components can cool down. The average current draw over both states needs to be below the source specification.

To even out voltage spikes from the switching between on-state and off-state, a capacitor is used on the output side.

A mechanical analogy for a buck converter would be to pedal a bicycle in single, strong bursts (force ~ voltage), and let the bicycle roll in between (inertia ~ inductor).

The basic operation of the buck converter has the current in an inductor controlled by two switches (fig. 2). In a physical implementation, these switches are realized by a transistor and a diode, or two transistors (which avoids the loss associated with the diode's voltage drop).

### Idealised case

The conceptual model of the buck converter is best understood in terms of the relation between current and voltage of the inductor. Beginning with the switch open (off-state), the current in the circuit is zero. When the switch is first closed (on-state), the current will begin to increase, and the inductor will produce an opposing voltage across its terminals in response to the changing current. This voltage drop counteracts the voltage of the source and therefore reduces the net voltage across the load. Over time, the rate of change of current decreases, and the voltage across the inductor also then decreases, increasing the voltage at the load. During this time, the inductor stores energy in the form of a magnetic field.

If the switch is opened while the current is still changing, then there will always be a voltage drop across the inductor, so the net voltage at the load will always be less than the input voltage source. When the switch is opened again (off-state), the voltage source will be removed from the circuit, and the current will decrease. The decreasing current will produce a voltage drop across the inductor (opposite to the drop at on-state), and now the inductor becomes a current source. The stored energy in the inductor's magnetic field supports the current flow through the load. This current, flowing while the input voltage source is disconnected, when appended to the current flowing during on-state, totals to current greater than the average input current (being zero during off-state).

The increase in average current makes up for the reduction in voltage, and ideally preserves the power provided to the load. During the off-state, the inductor is discharging its stored energy into the rest of the circuit. If the switch is closed again before the inductor fully discharges (on-state), the voltage at the load will always be greater than zero.

#### Continuous mode

Buck converters operate in continuous mode if the current through the inductor (I_L) never falls to zero during the commutation cycle. In this mode, the operating principle is described by the plots in figure 4:

- When the switch pictured above is closed (top of figure 2), the voltage across the inductor is V_L = V_i - V_o. The current through the inductor rises linearly (in approximation, so long as the voltage drop is almost constant). As the diode is reverse-biased by the voltage source V_i, no current flows through it;
- When the switch is opened (bottom of figure 2), the diode is forward biased. The voltage across the inductor is V_L = - V_o (neglecting diode drop). Current I_L decreases.

The energy stored in inductor L is
E = 1/2 L I_L^2

Therefore, it can be seen that the energy stored in L increases during on-time as I_L increases and then decreases during the off-state. L is used to transfer energy from the input to the output of the converter.

The rate of change of I_L can be calculated from:
V_L = L(dI_L)/(dt)

With V_L equal to V_i - V_o during the on-state and to -V_o during the off-state. Therefore, the increase in current during the on-state is given by:
Δ I_L_on = ∫_0^t_on (V_L)/(L) dt = (V_i - V_o)/(L) t_on, t_on = DT

where D is a scalar called the duty cycle with a value between 0 and 1.

Conversely, the decrease in current during the off-state is given by:
Δ I_L_off = ∫_t_on^T = t_on + t_off(V_L)/(L) dt = -(V_o)/(L) t_off, t_off = (1 - D)T

Assuming that the converter operates in the steady state, the energy stored in each component at the end of a commutation cycle T is equal to that at the beginning of the cycle. That means that the current I_L is the same at t = 0 and at t = T (figure 4).

So, from the above equations it can be written as:
Δ I_L_on + Δ I_L_off = 0; (V_i - V_o)/(L)t_on - (V_o)/(L)t_off = 0

The above integrations can be done graphically. In figure 4, Δ I_L_on is proportional to the area of the yellow surface, and Δ I_L_off to the area of the orange surface, as these surfaces are defined by the inductor voltage (red lines). As these surfaces are simple rectangles, their areas can be found easily: (V_i - V_o) t_on for the yellow rectangle and -V_o t_off for the orange one. For steady state operation, these areas must be equal.

As can be seen in figure 4, t_on = DT and t_off = (1 - D)T.

This yields:
(V_i - V_o)DT - V_o(1 - D)T = 0; DV_i - V_o = 0; Rightarrow D = (V_o)/(V_i)

From this equation, it can be seen that the output voltage of the converter varies linearly with the duty cycle for a given input voltage. As the duty cycle D is equal to the ratio between t_on and the period T, it cannot be more than 1. Therefore, V_o ≤ V_i. This is why this converter is referred to as *step-down converter*.

So, for example, stepping 12 V down to 3 V (output voltage equal to one quarter of the input voltage) would require a duty cycle of 25%, in this theoretically ideal circuit.

#### Discontinuous mode

In some cases, the amount of energy required by the load is too small. In this case, the current through the inductor falls to zero during part of the period. The only difference in the principle described above is that the inductor is completely discharged at the end of the commutation cycle (see figure 5). This has, however, some effect on the previous equations.

The inductor current falling below zero results in the discharging of the output capacitor during each cycle and therefore higher switching losses. A different control technique known as pulse-frequency modulation can be used to minimize these losses.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Theory, Efficiency, Impedance matching.)_
