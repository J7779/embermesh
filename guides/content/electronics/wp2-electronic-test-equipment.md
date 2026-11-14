---
id: electronics/wp2-electronic-test-equipment
title: Electronic test equipment
category: electronics
tags: [electronic test equipment, electronics]
summary: Electronic test equipment is used to create signals and capture responses from electronic devices under test (DUTs).
source: https://en.wikipedia.org/w/index.php?title=Electronic_test_equipment&oldid=1345183217
source_title: Wikipedia: Electronic test equipment
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Electronic test equipment", Wikipedia, The Free Encyclopedia (revision 1345183217), https://en.wikipedia.org/w/index.php?title=Electronic_test_equipment&oldid=1345183217 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Electronic test equipment

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Electronic test equipment** is used to create signals and capture responses from electronic devices under test (DUTs). In this way, the proper operation of the DUT can be proven or faults in the device can be traced. Use of electronic test equipment is essential to any serious work on electronics systems.

Practical electronics engineering and assembly requires the use of many different kinds of electronic test equipment ranging from the very simple and inexpensive (such as a test light consisting of just a light bulb and a test lead) to extremely complex and sophisticated such as automatic test equipment (ATE). ATE often includes many of these instruments in real and simulated forms.

Generally, more advanced test gear is necessary when developing circuits and systems than is needed when doing production testing or when troubleshooting existing production units in the field.

## Types of test equipment

### Basic equipment

The following items are used for basic measurement of voltages, currents, and components in the circuit under test.

- Voltmeter (Measures voltage)
- Ohmmeter (Measures resistance)
- Ammeter, e.g. Galvanometer or Milliammeter (Measures current)
- Multimeter e.g., VOM (Volt-Ohm-Milliammeter) or DMM (Digital Multimeter) (Measures all of the above)
- LCR meter - inductance (L), capacitance (C) and resistance (R) meter (measure LCR values)

The following are used for stimulus of the circuit under test:

- Power supplies
- Signal generator
- Digital pattern generator
- Pulse generator

The following analyze the response of the circuit under test:

- Oscilloscope (Displays voltage as it changes over time)
- Frequency counter (Measures frequency)

And connecting it all together:

- Test probes

### Advanced or less commonly used equipment

**Meters**

- Solenoid voltmeter (*Wiggy*)
- Clamp meter (current transducer)
- Wheatstone bridge (Precisely measures resistance)
- Capacitance meter (Measures capacitance)
- LCR meter (Measures inductance, capacitance, resistance and combinations thereof)
- EMF Meter (Measures Electric and Magnetic Fields)
- Electrometer (Measures voltages, sometimes even tiny ones, via a charge effect)

#### Probes

- RF probe
- Signal tracer

#### Analyzers

- Logic analyzer (Tests digital circuits)
- Spectrum analyzer (SA) (Measures spectral energy of signals)
- Protocol analyzer (Tests functionality, performance and conformance of protocols)
- Vector signal analyzer (VSA) (Like the SA but it can also perform many more useful digital demodulation functions)
- Time-domain reflectometer (Tests integrity of long cables)
- Semiconductor curve tracer

#### Signal-generating devices

- Signal generator usually distinguished by frequency range (e.g., audio or radio frequencies) or waveform type (e.g., sine, square, sawtooth, ramp, sweep, modulated, ...)
- Frequency synthesiser
- Function generator
- Digital pattern generator
- Pulse generator
- Signal injector

### Miscellaneous devices

- Boxcar averager
- Continuity tester
- Cable tester
- Hipot tester
- Network analyzer (used to characterize an electrical network of components)
- Test light
- Transistor tester
- Tube tester
- Source measure unit (SMU), a test tool capable to supply power and measure voltage or current at the same time.

## Platforms

Several modular electronic instrumentation platforms are currently in common use for configuring automated electronic test and measurement systems. These systems are widely employed for incoming inspection, quality assurance, and production testing of electronic devices and subassemblies. Industry-standard communication interfaces link signal sources with measurement instruments in “rack-and-stack” or chassis-/mainframe-based systems, often under the control of a custom software application running on an external PC.

### GPIB/IEEE-488

The General Purpose Interface Bus (GPIB) is an IEEE-488 (a standard created by the Institute of Electrical and Electronics Engineers) standard parallel interface used for attaching sensors and programmable instruments to a computer. GPIB is a digital 8-bit parallel communications interface capable of achieving data transfers of more than 8 Mbytes/s. It allows daisy-chaining up to 14 instruments to a system controller using a 24-pin connector. It is one of the most common I/O interfaces present in instruments and is designed specifically for instrument control applications. The IEEE-488 specifications standardized this bus and defined its electrical, mechanical, and functional specifications, while also defining its basic software communication rules. GPIB works best for applications in industrial settings that require a rugged connection for instrument control.

The original GPIB standard was developed in the late 1960s by Hewlett-Packard to connect and control the programmable instruments the company manufactured. The introduction of digital controllers and programmable test equipment created a need for a standard, high-speed interface for communication between instruments and controllers from various vendors. In 1975, the IEEE published ANSI/IEEE Standard 488–1975, IEEE Standard Digital Interface for Programmable Instrumentation, which contained the electrical, mechanical, and functional specifications of an interfacing system. This standard was subsequently revised in 1978 (IEEE-488.1) and 1990 (IEEE-488.2). The IEEE 488.2 specification includes the Standard Commands for Programmable Instrumentation (SCPI), which define specific commands that each instrument class must obey. SCPI ensures compatibility and configurability among these instruments.

The IEEE-488 bus has long been popular because it is simple to use and takes advantage of a large selection of programmable instruments and stimuli. Large systems, however, have the following limitations:

- **Driver fanout** capacity limits the system to 14 devices plus a controller.
- **Cable length** limits the controller-device distance to two meters per device or 20 meters total, whichever is less. This imposes transmission problems on systems spread out in a room or on systems that require remote measurements.
- **Primary addresses** limit the system to 30 devices with primary addresses. Modern instruments rarely use secondary addresses so this puts a 30-device limit on system size.

### LAN eXtensions for Instrumentation

The LXI (LXI) Standard defines the communication protocols for instrumentation and data acquisition systems using Ethernet. These systems are based on small, modular instruments, using low-cost, open-standard LAN (Ethernet). LXI-compliant instruments offer the size and integration advantages of modular instruments without the cost and form factor constraints of card-cage architectures. Through the use of Ethernet communications, the LXI Standard allows for flexible packaging, high-speed I/O, and standardized use of LAN connectivity in a broad range of commercial, industrial, aerospace, and military applications. Every LXI-compliant instrument includes an Interchangeable Virtual Instrument (IVI) driver to simplify communication with non-LXI instruments, so LXI-compliant devices can communicate with devices that are not themselves LXI compliant (i.e., instruments that employ GPIB, VXI, PXI, etc.). This simplifies building and operating hybrid configurations of instruments.

LXI instruments sometimes employ scripting using embedded test script processors for configuring test and measurement applications. Script-based instruments provide architectural flexibility, improved performance, and lower cost for many applications. Scripting enhances the benefits of LXI instruments, and LXI offers features that both enable and enhance scripting. Although the current LXI standards for instrumentation do not require that instruments be programmable or implement scripting, several features in the LXI specification anticipate programmable instruments and provide useful functionality that enhances scripting's capabilities on LXI-compliant instruments.

### VME eXtensions for Instrumentation

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Platforms, Test equipment switching.)_
