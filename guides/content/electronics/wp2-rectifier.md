---
id: electronics/wp2-rectifier
title: Rectifier
category: electronics
tags: [rectifier, electronics]
summary: A rectifier is an electrical device that converts alternating current (AC), which periodically reverses direction, to direct current (DC), which flows in only one direction.
source: https://en.wikipedia.org/w/index.php?title=Rectifier&oldid=1367200865
source_title: Wikipedia: Rectifier
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Rectifier", Wikipedia, The Free Encyclopedia (revision 1367200865), https://en.wikipedia.org/w/index.php?title=Rectifier&oldid=1367200865 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Rectifier

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **rectifier** is an electrical device that converts alternating current (AC), which periodically reverses direction, to direct current (DC), which flows in only one direction.

The process is known as *rectification*, since it "straightens" the direction of current. Physically, rectifiers take a number of forms, including vacuum tube diodes, wet chemical cells, mercury-arc valves, stacks of copper and selenium oxide plates, semiconductor diodes, silicon-controlled rectifiers and other silicon-based semiconductor switches. Historically, even synchronous electromechanical switches and motor–generator sets have been used. Early radio receivers, called crystal radios, used a "cat's whisker" of fine wire pressing on a crystal of galena (lead sulfide) to serve as a point-contact rectifier or "crystal detector".

Rectifiers have many uses, but are often found serving as components of DC power supplies and high-voltage direct current power transmission systems. Rectification may serve in roles other than to generate direct current for use as a source of power. As noted, rectifiers can serve as detectors of radio signals. In gas heating systems, flame rectification is used to detect the presence of a flame.

Depending on the type of alternating current supply and the arrangement of the rectifier circuit, the output voltage may require additional smoothing to produce a uniform steady voltage. Many applications of rectifiers, such as power supplies for radio, television and computer equipment, require a *steady* constant DC voltage (as would be produced by a battery). In these applications, the output of the rectifier is smoothed by an electronic filter, which may be a capacitor, choke, or set of capacitors, chokes and resistors, possibly followed by a voltage regulator to produce a steady voltage.

A device that performs the opposite function, that is converting DC to AC, is called an inverter.

## Rectifier devices

Before the development of silicon semiconductor rectifiers, vacuum tube thermionic diodes and copper oxide- or selenium-based metal rectifier stacks were used. The first vacuum tube diodes designed for rectifier application in power supply circuits were introduced in April 1915 by Saul Dushman of General Electric. With the introduction of semiconductor electronics, vacuum tube rectifiers became obsolete, except for some enthusiasts of vacuum tube audio equipment. For power rectification from very low to very high current, semiconductor diodes of various types (junction diodes, Schottky diodes, etc.) are widely used.

Other devices that have control electrodes as well as acting as unidirectional current valves are used where more than simple rectification is required—e.g., where variable output voltage is needed. High-power rectifiers, such as those used in high-voltage direct current power transmission, employ silicon semiconductor devices of various types. These are thyristors or other controlled switching solid-state switches, which effectively function as diodes to pass current in only one direction.

## Rectifier circuits

Rectifier circuits may be single-phase or multi-phase. Most low-power rectifiers for domestic equipment are single-phase, but three-phase rectification is very important for industrial applications and for the transmission of energy as DC (HVDC).

### Single-phase rectifiers

#### Half-wave rectification

In half-wave rectification of a single-phase supply, either the positive or negative half of the AC wave is passed, while the other half is blocked. Because only one half of the input waveform reaches the output, mean voltage is lower. Half-wave rectification requires a single diode in a single-phase supply, or three in a three-phase supply. Rectifiers yield a unidirectional but pulsating direct current; half-wave rectifiers produce far more ripple than full-wave rectifiers, and much more filtering is needed to eliminate harmonics of the AC frequency from the output.

The no-load output DC voltage of an ideal half-wave rectifier for a sinusoidal input voltage is:

: V_rms = (V_peak)/(2); [8pt] V_dc = (V_peak)/(π)

where:
*V*_dc, *V*_av – the DC or average output voltage,
*V*_peak, the peak value of the phase input voltages,
*V*_rms, the root mean square (RMS) value of output voltage.

#### Full-wave rectification

A full-wave rectifier converts the whole of the input waveform to one of constant polarity (positive or negative) at its output. Mathematically, this corresponds to the absolute value function. Full-wave rectification converts both polarities of the input waveform to pulsating DC (direct current), and yields a higher average output voltage. Two diodes and a center-tapped transformer, or four diodes in a bridge configuration and any AC source (including a transformer without center tap), are needed. Single semiconductor diodes, double diodes with a common cathode or common anode, and four- or six-diode bridges are manufactured as single components.

For single-phase AC, if the transformer is center-tapped, then two diodes back-to-back (cathode-to-cathode or anode-to-anode, depending on output polarity required) can form a full-wave rectifier. Twice as many turns are required on the transformer secondary to obtain the same output voltage than for a bridge rectifier, but the power rating is unchanged.

The average and RMS no-load output voltages of an ideal single-phase full-wave rectifier are:

V_dc=V_av =(2 · V_peak)/(π); [8pt] V_rms =(V_peak)/(sqrt 2)

Very common double-diode rectifier vacuum tubes contained a single common cathode and two anodes inside a single envelope, achieving full-wave rectification with positive output. The 5U4 and the 80/5Y3 (4 pin)/(octal) were popular examples of this configuration.

### Three-phase rectifiers

Single-phase rectifiers are commonly used for power supplies for domestic equipment. However, for most industrial and high-power applications, three-phase rectifier circuits are the norm. As with single-phase rectifiers, three-phase rectifiers can take the form of a half-wave circuit, a full-wave circuit using a center-tapped transformer, or a full-wave bridge circuit.

Thyristors are commonly used in place of diodes to create a circuit that can regulate the output voltage. Many devices that provide direct current actually 'generate' three-phase AC. For example, an automobile alternator contains nine diodes, six of which function as a full-wave rectifier for battery charging.

#### Three-phase, half-wave circuit

An uncontrolled three-phase, half-wave midpoint circuit requires three diodes, one connected to each phase. This is the simplest type of three-phase rectifier but suffers from relatively high harmonic distortion on both the AC and DC connections. This type of rectifier is said to have a pulse number of three, since the output voltage on the DC side contains three distinct pulses per cycle of the grid frequency:

The peak values V_peak of this three-pulse DC voltage are calculated from the RMS value V_LN of the input phase voltage (line to neutral voltage, 120 V in North America, 240 V within Europe at mains operation): V_peak = sqrt 2 · V_LN. The average no-load output voltage V_av results from the integral under the graph of a positive half-wave with the period duration of 2/3 π (from 30° to 150°):

V_dc = V_av = (1)/(2/3 π) ∫_30^°^150^° V_peak sinφ mathrm dφ = (3 V_peak)/(2 π) · (-cos 150^° + cos 30^° ); [8pt] = (3 V_peak)/(2 π) · Biggl[ -(-sqrt3/2 )+sqrt3/2 Biggl] = (3sqrt3 · V_peak)/(2 π); [8pt] Longrightarrow V_dc = V_av = (3 sqrt3 · sqrt 2 · V_LN)/(2 π); [8pt] Longrightarrow V_av = (3 sqrt 6 · V_LN)/(2 π) ≈ 1.17 V_LN

#### Three-phase, full-wave circuit using center-tapped transformer

If the AC supply is fed via a transformer with a center tap, a rectifier circuit with improved harmonic performance can be obtained. This rectifier now requires six diodes, one connected to each end of each transformer secondary winding. This circuit has a pulse number of six, and in effect, can be thought of as a six-phase, half-wave circuit.

Before solid-state devices became available, the half-wave circuit and the full-wave circuit using a center-tapped transformer were very commonly used in industrial rectifiers using mercury-arc valves. This was because the three or six AC supply inputs could be fed to a corresponding number of anode electrodes on a single tank, sharing a common cathode.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Rectifier circuits, Quantification of rectifiers, Rectifier voltage drop, Harmonic distortion, Rectifier output smoothing, Applications, Rectification technologies.)_
