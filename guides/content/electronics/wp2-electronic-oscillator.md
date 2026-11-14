---
id: electronics/wp2-electronic-oscillator
title: Electronic oscillator
category: electronics
tags: [electronic oscillator, linear, harmonic oscillator, nonlinear, relaxation oscillator, electronics]
summary: An electronic oscillator is an electronic circuit that produces a periodic, oscillating or alternating current (AC) signal, usually a sine wave, square wave or a triangle wave, powered by a direct current (DC) source.
source: https://en.wikipedia.org/w/index.php?title=Electronic_oscillator&oldid=1380780410
source_title: Wikipedia: Electronic oscillator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Electronic oscillator", Wikipedia, The Free Encyclopedia (revision 1380780410), https://en.wikipedia.org/w/index.php?title=Electronic_oscillator&oldid=1380780410 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Electronic oscillator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **electronic oscillator** is an electronic circuit that produces a periodic, oscillating or alternating current (AC) signal, usually a sine wave, square wave or a triangle wave, powered by a direct current (DC) source. Oscillators are found in many electronic devices, such as radio receivers, television sets, radio and television broadcast transmitters, computers, computer peripherals, cellphones, radar, and many other devices.

Oscillators are often characterized by the frequency of their output signal:

- A low-frequency oscillator (LFO) is an oscillator that generates a frequency below approximately 20 Hz. This term is typically used in the field of audio synthesizers, to distinguish it from an audio frequency oscillator.
- An audio oscillator produces frequencies in the audio range, 20 Hz to 20 kHz.
- A radio frequency (RF) oscillator produces signals above the audio range, more generally in the range of 100 kHz to 100 GHz.

There are two general types of electronic oscillators: the **linear** or **harmonic oscillator**, and the **nonlinear** or **relaxation oscillator**. The two types are fundamentally different in how oscillation is produced, as well as in the characteristic type of output signal that is generated.

The most-common linear oscillator in use is the crystal oscillator, in which the output frequency is controlled by a piezo-electric resonator consisting of a vibrating quartz crystal. Crystal oscillators are ubiquitous in modern electronics, being the source for the clock signal in computers and digital watches, as well as a source for the signals generated in radio transmitters and receivers. As a crystal oscillator's “native” output waveform is sinusoidal, a signal-conditioning circuit may be used to convert the output to other waveform types, such as the square wave typically utilized in computer clock circuits.

## Harmonic oscillators

**Linear** or **harmonic oscillators** generate a sinusoidal (or nearly-sinusoidal) signal. There are two types:

### Feedback oscillator

The most common form of linear oscillator is an electronic amplifier such as a transistor or operational amplifier connected in a feedback loop with its output fed back into its input through a frequency selective electronic filter to provide positive feedback. When the power supply to the amplifier is switched on initially, electronic noise in the circuit provides a non-zero signal to get oscillations started. The noise travels around the loop and is amplified and filtered until very quickly it converges on a sine wave at a single frequency.

Feedback oscillator circuits can be classified according to the type of frequency selective filter they use in the feedback loop:

- In an *RC oscillator* circuit, the filter is a network of resistors and capacitors. RC oscillators are mostly used to generate lower frequencies, for example in the audio range. Common types of RC oscillator circuits are the phase shift oscillator and the Wien bridge oscillator. LR oscillators, using inductor and resistor filters also exist, however they are much less common due to the required size of an inductor to achieve a value appropriate for use at lower frequencies.

- In an *LC oscillator* circuit, the filter is a tuned circuit (often called a *tank circuit*) consisting of an inductor (L) and capacitor (C) connected together, which acts as a resonator. Charge flows back and forth between the capacitor's plates through the inductor, so the tuned circuit can store electrical energy oscillating at its resonant frequency. The amplifier adds power to compensate for resistive energy losses in the circuit and supplies the power for the output signal. LC oscillators are often used at radio frequencies, when a tunable frequency source is necessary, such as in signal generators, tunable radio transmitters and the local oscillators in radio receivers. Typical LC oscillator circuits are the Hartley, Colpitts, Clapp and cross-coupled LC oscillator.
- In a *crystal oscillator* circuit the filter is a piezoelectric crystal (commonly a quartz crystal). The crystal mechanically vibrates as a resonator, and its frequency of vibration determines the oscillation frequency. Since the resonant frequency of the crystal is determined by its dimensions, crystal oscillators are fixed frequency oscillators, their frequency can only be adjusted over a tiny range of less than one percent. Crystals have a very high Q-factor and also better temperature stability than tuned circuits, so crystal oscillators have much better frequency stability than LC or RC oscillators. Crystal oscillators are the most common type of linear oscillator, used to stabilize the frequency of most radio transmitters, and to generate the clock signal in computers and quartz clocks. Crystal oscillators often use the same circuits as LC oscillators, with the crystal replacing the tuned circuit; the Pierce oscillator circuit is also commonly used. Quartz crystals are generally limited to frequencies of 30 MHz or below. Other types of resonators, dielectric resonators and surface acoustic wave (SAW) devices, are used to control higher frequency oscillators, up into the microwave range. For example, SAW oscillators are used to generate the radio signal in cell phones.

### Negative-resistance oscillator

In addition to the feedback oscillators described above, which use two-port amplifying active elements such as transistors and operational amplifiers, linear oscillators can also be built using one-port (two terminal) devices with negative resistance, such as magnetron tubes, tunnel diodes, IMPATT diodes and Gunn diodes. Negative-resistance oscillators are usually used at high frequencies in the microwave range and above, since at these frequencies feedback oscillators perform poorly due to excessive phase shift in the feedback path.

In negative-resistance oscillators, a resonant circuit, such as an LC circuit, crystal, or cavity resonator, is connected across a device with negative differential resistance, and a DC bias voltage is applied to supply energy. A resonant circuit by itself is "almost" an oscillator; it can store energy in the form of electronic oscillations if excited, but because it has electrical resistance and other losses the oscillations are damped and decay to zero. The negative resistance of the active device cancels the (positive) internal loss resistance in the resonator, in effect creating a resonator circuit with no damping, which generates spontaneous continuous oscillations at its resonant frequency.

The negative-resistance oscillator model is not limited to one-port devices like diodes; feedback oscillator circuits with two-port amplifying devices such as transistors and tubes also have negative resistance. At high frequencies, three terminal devices such as transistors and FETs are also used in negative resistance oscillators. At high frequencies these devices do not need a feedback loop, but with certain loads applied to one port can become unstable at the other port and show negative resistance due to internal feedback. The negative resistance port is connected to a tuned circuit or resonant cavity, causing them to oscillate. High-frequency oscillators in general are designed using negative-resistance techniques.

## Voltage-controlled oscillator (VCO)

An oscillator can be designed so that the oscillation frequency can be varied over some range by an input voltage or current. These voltage-controlled oscillators are widely used in phase-locked loops, in which the oscillator's frequency can be locked to the frequency of another oscillator. These are ubiquitous in modern communications circuits, used in filters, modulators, demodulators, and forming the basis of frequency synthesizer circuits which are used to tune radios and televisions.

Radio frequency VCOs are usually made by adding a varactor diode to the tuned circuit or resonator in an oscillator circuit. Changing the DC voltage across the varactor changes its capacitance, which changes the resonant frequency of the tuned circuit. Voltage-controlled relaxation oscillators can be constructed by charging and discharging the energy storage capacitor with a voltage-controlled current source. Increasing the input voltage increases the rate of charging the capacitor, decreasing the time between switching events.

_(Later sections of the original article were left out of this offline copy for length: Relaxation oscillator, Theory of feedback oscillators.)_
