---
id: electronics/wp2-amplifier
title: Amplifier
category: electronics
tags: [amplifier, electronic amplifier, amp, electronics]
summary: An amplifier, electronic amplifier or (informally) amp is an electronic device that can increase the magnitude of a signal (a time-varying voltage or current).
source: https://en.wikipedia.org/w/index.php?title=Amplifier&oldid=1381035130
source_title: Wikipedia: Amplifier
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Amplifier", Wikipedia, The Free Encyclopedia (revision 1381035130), https://en.wikipedia.org/w/index.php?title=Amplifier&oldid=1381035130 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Amplifier

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **amplifier**, **electronic amplifier** or (informally) **amp** is an electronic device that can increase the magnitude of a signal (a time-varying voltage or current). It is a two-port electronic circuit that uses electric power from a power supply to increase the amplitude (magnitude of the voltage or current) of a signal applied to its input terminals, producing a proportionally greater amplitude signal at its output. The amount of amplification provided by an amplifier is measured by its gain: the ratio of output voltage, current, or power to input. An amplifier is defined as a circuit that has a power gain greater than one.

An amplifier can be either a separate piece of equipment or an electrical circuit contained within another device. Amplification is fundamental to modern electronics, and amplifiers are widely used in almost all electronic equipment. Amplifiers can be categorized in different ways. One is by the frequency of the electronic signal being amplified. For example, audio amplifiers amplify signals of less than 20 kHz, radio frequency (RF) amplifiers amplify frequencies in the range between 20 kHz and 300 GHz, and servo amplifiers and instrumentation amplifiers may work with very low frequencies down to direct current. Amplifiers can also be categorized by their physical placement in the signal chain; a preamplifier may precede other signal processing stages, for example, while a power amplifier is usually used after other amplifier stages to provide enough output power for the final use of the signal. The first practical electrical device which could amplify was the triode vacuum tube, invented in 1906 by Lee de Forest, which led to the first amplifiers around 1912. Today most amplifiers use transistors.

## Theory of operation

In principle, an amplifier is an electrical two-port network that produces a signal at the output port that is a replica of the signal applied to the input port, but increased in magnitude.

The input port can be idealized as either being a voltage input, which takes no current, with the output proportional to the voltage across the port; or a current input, with no voltage across it, in which the output is proportional to the current through the port. The output port can be idealized as being either a dependent voltage source, with zero source resistance and its output voltage dependent on the input; or a dependent current source, with infinite source resistance and the output current dependent on the input. Combinations of these choices lead to four types of ideal amplifiers. In idealized form they are represented by each of the four types of dependent source used in linear analysis, as shown in the figure, namely:

| Input | Output | Dependent source | Amplifier type | Gain units |
|---|---|---|---|---|
| I | I | Current controlled current source, CCCS | Current amplifier | Unitless |
| I | V | Current controlled voltage source, CCVS | Transresistance amplifier | Ohm |
| V | I | Voltage controlled current source, VCCS | Transconductance amplifier | Siemens |
| V | V | Voltage controlled voltage source, VCVS | Voltage amplifier | Unitless |

Each type of amplifier in its ideal form has an ideal input and output resistance that is the same as that of the corresponding dependent source:

| Amplifier type | Dependent source | Input impedance | Output impedance |
|---|---|---|---|
| Current | CCCS | 0 | ∞ |
| Transresistance | CCVS | 0 | 0 |
| Transconductance | VCCS | ∞ | ∞ |
| Voltage | VCVS | ∞ | 0 |

In real amplifiers the ideal impedances are not possible to achieve, but these ideal elements can be used to construct equivalent circuits of real amplifiers by adding impedances (resistance, capacitance and inductance) to the input and output. For any particular circuit, a small-signal analysis is often used to find the actual impedance. A small-signal AC test current *I_x* is applied to the input or output node, all external sources are set to AC zero, and the corresponding alternating voltage *V_x* across the test current source determines the impedance seen at that node as *R = V_x / I_x*.

Amplifiers designed to attach to a transmission line at input and output, especially RF amplifiers, do not fit into this classification approach. Rather than dealing with voltage or current individually, they ideally couple with an input or output impedance matched to the transmission line impedance, that is, match *ratios* of voltage to current. Many real RF amplifiers come close to this ideal. Although, for a given appropriate source and load impedance, RF amplifiers can be characterized as amplifying voltage or current, they fundamentally are amplifying power.

## Properties

Amplifier properties are given by parameters that include:

- Gain, the ratio between the magnitude of output and input signals
- Bandwidth, the width of the useful frequency range
- Efficiency, the ratio between the power of the output and total power consumption
- Linearity, the extent to which the proportion between input and output amplitude is the same for high amplitude and low amplitude input
- Noise, a measure of undesired noise mixed into the output
- Output dynamic range, the ratio of the largest and the smallest useful output levels
- Slew rate, the maximum rate of change of the output
- Rise time, settling time, ringing and overshoot that characterize the step response
- Stability, the ability to avoid self-oscillation

Amplifiers are described according to the properties of their inputs, their outputs, and how they relate. All amplifiers have gain, a multiplication factor that relates the magnitude of some property of the output signal to a property of the input signal. The gain may be specified as the ratio of output voltage to input voltage (voltage gain), output power to input power (power gain), or some combination of current, voltage, and power. In many cases the property of the output that varies is dependent on the same property of the input, making the gain unitless (though often expressed in decibels (dB)).

Most amplifiers are designed to be linear. That is, they provide constant gain for any normal input level and output signal. If an amplifier's gain is not linear, the output signal can become distorted. There are, however, cases where variable gain is useful. Certain signal processing applications use exponential gain amplifiers.

Amplifiers are usually designed to function well in a specific application, for example: radio and television transmitters and receivers, high-fidelity ("hi-fi") stereo equipment, microcomputers and other digital equipment, and guitar and other instrument amplifiers. Every amplifier includes at least one active device, such as a vacuum tube or transistor.

## Special types

- Charge transfer amplifier
- CMOS amplifiers
- Current sense amplifier
- Distributed amplifier
- Doherty amplifier
- Double-tuned amplifier
- Faithful amplifier
- Intermediate power amplifier
- Lock-in amplifier
- Low-noise amplifier
- Negative feedback amplifier
- Optical amplifier
- Programmable-gain amplifier
- Tuned amplifier
- Valve amplifier

_(Later sections of the original article were left out of this offline copy for length: Negative feedback, Categories, Applications, Classification of amplifier stages and systems, Example amplifier circuit, Notes on implementation.)_
