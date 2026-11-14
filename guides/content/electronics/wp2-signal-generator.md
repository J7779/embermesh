---
id: electronics/wp2-signal-generator
title: Signal generator
category: electronics
tags: [signal generator, electronics]
summary: A signal generator is one of a class of electronic devices that generates electrical signals with set properties of amplitude, frequency, and wave shape.
source: https://en.wikipedia.org/w/index.php?title=Signal_generator&oldid=1378749236
source_title: Wikipedia: Signal generator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Signal generator", Wikipedia, The Free Encyclopedia (revision 1378749236), https://en.wikipedia.org/w/index.php?title=Signal_generator&oldid=1378749236 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Signal generator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **signal generator** is one of a class of electronic devices that generates electrical signals with set properties of amplitude, frequency, and wave shape. These generated signals are used as a stimulus for electronic measurements, typically used in designing, testing, troubleshooting, and repairing electronic or electroacoustic devices, though it often has artistic uses as well.

There are many different types of signal generators with different purposes and applications and at varying levels of expense. These types include function generators, RF and microwave signal generators, pitch generators, arbitrary waveform generators, digital pattern generators, and frequency generators. In general, no device is suitable for all possible applications.

A signal generator may be as simple as an oscillator with calibrated frequency and amplitude. More general-purpose signal generators allow control of all the characteristics of a signal. Modern general-purpose signal generators will have a microprocessor control and may also permit control from a personal computer. Signal generators may be free-standing self-contained instruments, or may be incorporated into more complex automatic test systems.

## General-purpose signal generators

### Function generator

A function generator is a device which produces simple repetitive waveforms. Such devices contain an electronic oscillator, a circuit that is capable of creating a repetitive waveform. (Modern devices may use digital signal processing to synthesize waveforms, followed by a digital-to-analog converter, or DAC, to produce an analog output.) The most common waveform is a sine wave, but sawtooth, step (pulse), square, and triangular waveform oscillators are commonly available as are arbitrary waveform generators (AWGs). If the oscillator operates above the human hearing range (>20 kHz), the generator will often include some sort of modulation function such as amplitude modulation (AM), frequency modulation (FM), or phase modulation (PM) as well as a second oscillator that provides an audio frequency modulation waveform.

### Arbitrary waveform generator

An arbitrary waveform generator (AWG or ARB) is a sophisticated signal generator that generates arbitrary waveforms within published limits of frequency range, accuracy, and output level. Unlike a function generator that produces a small set of specific waveforms, an AWG allows the user to specify a source waveform in a variety of different ways. An AWG is generally more expensive than a function generator and often has less bandwidth. An AWG is used in higher-end design and test applications.

### RF and microwave signal generators

RF (radio frequency) and microwave signal generators are used for testing components, receivers and test systems in a wide variety of applications including cellular communications, WiFi, WiMAX, GPS, audio and video broadcasting, satellite communications, radar and electronic warfare. RF and microwave signal generators normally have similar features and capabilities, but are differentiated by frequency range. RF signal generators typically range from a few kHz to 6 GHz, while microwave signal generators cover a much wider frequency range, from less than 1 MHz to at least 20 GHz. Some models go as high as 70 GHz with a direct coaxial output, and up to hundreds of GHz when used with external waveguide multiplier modules. RF and microwave signal generators can be classified further as analog or vector signal generators.

#### Analog signal generators

Analog signal generators based on a sine-wave oscillator were common before the inception of digital electronics, and are still used. There was a sharp distinction in purpose and design of radio-frequency and audio-frequency signal generators.

**RF**
RF signal generators produce continuous wave radio frequency signals of defined, adjustable, amplitude and frequency. Many models offer various types of analog modulation, either as standard equipment or as an optional capability to the base unit. This could include AM, FM, ΦM (phase modulation) and pulse modulation. A common feature is an attenuator to vary the signal’s output power. Depending on the manufacturer and model, output powers can range from −135 to +30 dBm. A wide range of output power is desirable, since different applications require different amounts of signal power. For example, if a signal has to travel through a very long cable out to an antenna, a high output signal may be needed to overcome the losses through the cable and still have sufficient power at the antenna. But when testing receiver sensitivity, a low signal level is required to see how the receiver behaves under low signal-to-noise conditions.

RF signal generators are available as benchtop instruments, rackmount instruments, embeddable modules and in card-level formats. Mobile, field-testing and airborne applications benefit from lighter, battery-operated platforms. In automated and production testing, web-browser access, which allows multi-source control, and faster frequency switching speeds improve test times and throughput.

RF signal generators are required for servicing and setting up radio receivers, and are used for professional RF applications.

RF signal generators are characterized by their frequency bands, power capabilities (−100 to +25 dBc), single side band phase noise at various carrier frequencies, spurs and harmonics, frequency and amplitude switching speeds and modulation capabilities.

**AF**
Audio-frequency signal generators generate signals in the audio-frequency range and above. An early example was the HP200A audio oscillator, the first product sold by the Hewlett-Packard Company in 1939. Applications include checking frequency response of audio equipment, and many uses in the electronic laboratory.

Equipment distortion can be measured using a very-low-distortion audio generator as the signal source, with appropriate equipment to measure output distortion harmonic-by-harmonic with a wave analyser, or simply total harmonic distortion. A distortion of 0.0001% can be achieved by an audio signal generator with a relatively simple circuit.

#### Vector signal generator

With the advent of digital communications systems, it is no longer possible to adequately test these systems with traditional analog signal generators. This has led to the development of the vector signal generator, which is also known as a digital signal generator. These signal generators are capable of generating digitally-modulated radio signals that may use any of a large number of digital modulation formats such as QAM, QPSK, FSK, BPSK, and OFDM. In addition, since modern commercial digital communication systems are almost all based on well-defined industry standards, many vector signal generators can generate signals based on these standards. Examples include GSM, W-CDMA (UMTS), CDMA2000, LTE, Wi-Fi (IEEE 802.11), and WiMAX (IEEE 802.16). In contrast, military communication systems such as JTRS, which place a great deal of importance on robustness and information security, typically use very proprietary methods. To test these types of communication systems, users will often create their own custom waveforms and download them into the vector signal generator to create the desired test signal.

#### Digital pattern generator

A logic signal generator or data pattern generator or digital pattern generator produces logic signals—that is, logical 1s and 0s in the form of conventional voltage levels. The usual voltage standards are LVTTL and LVCMOS.
It is different from a "pulse/pattern generator", which refers to signal generators able to generate logic pulses with different analog characteristics (such as pulse rise/fall time, high level length, ...).

A digital pattern generator is used as stimulus source for digital integrated circuits and embedded systems - for functional validation and testing.

_(Later sections of the original article were left out of this offline copy for length: Special purpose signal generators.)_
