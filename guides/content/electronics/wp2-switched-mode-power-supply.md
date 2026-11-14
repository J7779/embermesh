---
id: electronics/wp2-switched-mode-power-supply
title: Switched-mode power supply
category: electronics
tags: [switched-mode power supply, smps, switching-mode power supply, switch-mode power supply, switched power supply, electronics]
summary: A switched-mode power supply (SMPS), also called switching-mode power supply, switch-mode power supply, switched power supply, or simply switcher, is an electronic power supply that incorporates a switching regulator to convert electrical power efficiently.
source: https://en.wikipedia.org/w/index.php?title=Switched-mode_power_supply&oldid=1384564572
source_title: Wikipedia: Switched-mode power supply
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Switched-mode power supply", Wikipedia, The Free Encyclopedia (revision 1384564572), https://en.wikipedia.org/w/index.php?title=Switched-mode_power_supply&oldid=1384564572 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Switched-mode power supply

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **switched-mode power supply** (**SMPS**), also called **switching-mode power supply**, **switch-mode power supply**, **switched power supply**, or simply **switcher**, is an electronic power supply that incorporates a switching regulator to convert electrical power efficiently.

Like other power supplies, a SMPS transfers power from a DC or AC source (often mains power, see AC adapter) to DC loads, such as a personal computer, while converting voltage and current characteristics. Unlike a linear power supply, the pass transistor of a switching-mode supply continually switches between low-dissipation, full-on and full-off states, and spends very little time in the high-dissipation transitions, minimizing wasted energy. Voltage regulation is achieved by varying the ratio of on-to-off time (also known as duty cycle). In contrast, a linear power supply regulates the output voltage by continually dissipating power in the pass transistor. The switched-mode power supply's higher electrical efficiency is an important advantage.

Switched-mode power supplies can also be substantially smaller and lighter than a linear supply because the transformer can be much smaller. This is because it operates at a high switching frequency which ranges from several hundred kHz to several MHz in contrast to the 50 or 60 Hz mains frequency used by the transformer in a linear power supply. Despite the reduced transformer size, the power supply topology and electromagnetic compatibility requirements in commercial designs result in a usually much greater component count and corresponding circuit complexity.

Switching regulators are used as replacements for linear regulators when higher efficiency, smaller size or lighter weight is required. They are, however, more complicated; switching currents can cause electrical noise problems if not carefully suppressed, and simple designs may have a poor power factor.

## Explanation

A linear power supply (non-SMPS) uses a linear regulator to provide the desired output voltage by dissipating power in ohmic losses (e.g., in a resistor or in the collector–emitter region of a pass transistor in its active mode). A linear regulator regulates either output voltage or current by dissipating the electric power in the form of heat, and hence its maximum power efficiency is voltage-out divided by voltage-in since the voltage difference between input and output is wasted.

In contrast, a SMPS changes output voltage and current by switching ideally lossless storage elements, such as inductors and capacitors, between different electrical configurations. Ideal switching elements (approximated by transistors operated outside of their active mode) have no resistance when *on* and carry no current when *off*, so converters with ideal components would operate with 100% efficiency (i.e., all input power is delivered to the load; no power is wasted as dissipated heat). In reality, these ideal components do not exist, so a switching power supply cannot be 100% efficient, but still provides a significant improvement in efficiency over a linear regulator.

Different switching configurations are used in SMPS designs. A boost converter acts like a step-up transformer for DC signals. A buck–boost converter works in a similar manner, but yields an output voltage which is opposite in polarity to the input voltage. Other buck circuits exist to boost the average output current with a reduction of voltage.

## Advantages and disadvantages

The main advantage of the switching power supply is greater efficiency (up to ~98–99%) and associated lower heat generation than linear regulators because the switching transistor dissipates little power when acting as a switch.

Other advantages include smaller size and lighter weight from the elimination of heavy and expensive line-frequency transformers. Standby power loss is often also reduced with the elimination of these transformers.

Disadvantages include greater complexity, the generation of high-amplitude, high-frequency energy that the low-pass filter must block to avoid electromagnetic interference (EMI), a ripple voltage at the switching frequency and its harmonic frequencies.

Very low-cost SMPSs may couple electrical switching noise back onto the mains power line, causing interference with devices connected to the same AC circuit, such as audio equipment. Non-power-factor-corrected SMPSs also cause harmonic distortion.

## Comparison with linear power supply

There are two main types of regulated power supplies available: SMPS and linear. The following table compares linear with switching power supplies in general:

**Comparison of a linear power supply and a switched-mode power supply**

_(This section is shortened.)_

## Theory of operation

### Input rectifier

If the SMPS has an AC input, then the first stage is to convert the input to DC. This is called rectification. An SMPS with a DC input does not require this stage. In some power supplies (mostly computer ATX power supplies), the rectifier circuit can be configured as a voltage doubler by the addition of a switch operated either manually or automatically. This feature permits operation from power sources that are normally at 115 VAC or at 230 VAC. The rectifier produces an unregulated DC voltage, which is then sent to a large filter capacitor. The current drawn from the mains supply by this rectifier circuit occurs in short pulses around the AC voltage peaks. These pulses have significant high frequency energy, which reduces the power factor. To correct for this, many newer SMPS will use a special power factor correction (PFC) circuit to make the input current follow the sinusoidal shape of the AC input voltage, correcting the power factor. Power supplies that use active PFC usually are auto-ranging, supporting input voltages from 90 VAC, with no input voltage selector switch.

An SMPS designed for AC input can usually be run from a DC supply, because the DC will pass through the rectifier unchanged. If the power supply is designed for 115 VAC and has no voltage selector switch, the required DC voltage would be 163 VDC (115 × √2). On the other hand, if the power supply has a voltage selector switch, based on the Delon circuit, for 115/230 V (computer ATX power supplies typically are in this category), the selector switch would have to be put in the 230 V position, and the required voltage would be 325 VDC (230 × √2).

### Inverter

This section refers to the block marked *chopper* in the above diagram.

The inverter stage converts DC, whether directly from the input or from the rectifier stage described above, to AC by running it through a power oscillator operating at a frequency of tens or hundreds of kilohertz. The frequency is usually chosen to be above 20 kHz, to make it inaudible to humans. The switching is implemented as a multistage (to achieve high gain) MOSFET amplifier. MOSFETs are a type of transistor with a low on-resistance and a high current-handling capacity.

### Output transformer, rectifier and filter

If the output is required to be isolated from the input, as is usually the case in mains power supplies, the inverted AC is used to drive the primary winding of a high-frequency transformer. Because of the high frequency produced by the inverter, the output transformer is very small with few windings. This converts the voltage up or down to the required output level on its secondary winding.

If a DC output is required, the AC output from the transformer is rectified. For output voltages above ten volts or so, ordinary silicon diodes are commonly used. For lower voltages, Schottky diodes are commonly used as the rectifier elements; they have the advantages of faster recovery times than silicon diodes, allowing low-loss operation at higher frequencies and a lower voltage drop when conducting. For even lower output voltages, MOSFETs may be used as synchronous rectifiers; compared to Schottky diodes, these have even lower conducting state voltage drops.

The rectified output is then smoothed by a filter consisting of inductors and capacitors. Because of the high switching frequencies, components with lower capacitance and inductance are needed than for a power supply operating at line frequency.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Comparison with linear power supply, the rest of Theory of operation, Transformer design, Power factor, Types, Efficiency and EMI, Failure modes, Precautions, Applications.)_
