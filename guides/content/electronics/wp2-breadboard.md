---
id: electronics/wp2-breadboard
title: Breadboard
category: electronics
tags: [breadboard, solderless breadboard, protoboard, electronics]
summary: A breadboard, solderless breadboard, or protoboard is a construction base used to build semi-permanent prototypes of electronic circuits.
source: https://en.wikipedia.org/w/index.php?title=Breadboard&oldid=1383720465
source_title: Wikipedia: Breadboard
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Breadboard", Wikipedia, The Free Encyclopedia (revision 1383720465), https://en.wikipedia.org/w/index.php?title=Breadboard&oldid=1383720465 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Breadboard

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **breadboard**, **solderless breadboard**, or **protoboard** is a construction base used to build semi-permanent prototypes of electronic circuits. Unlike a perfboard or stripboard, breadboards do not require soldering or destruction of tracks and are hence reusable. For this reason, breadboards are also popular with students and in technological education.

A variety of electronic systems may be prototyped by using breadboards, from small analog and digital circuits to complete central processing units (CPUs).

Compared to more permanent circuit connection methods, modern breadboards have high parasitic capacitance, relatively high resistance, and less reliable connections, which are subject to jostle and physical degradation. Signaling is limited to about 10 MHz, and even well below that frequency not everything works properly.

## Design

A modern solderless breadboard socket consists of a perforated block of plastic with numerous tin plated phosphor bronze or nickel silver alloy spring clips under the perforations. The clips are often called *tie points* or *contact points*. The number of tie points is often given in the specification of the breadboard.

The spacing between the clips (lead pitch) is typically 0.1 in. Integrated circuits (ICs) in dual in-line packages (DIPs) can be inserted to straddle the centerline of the block. Interconnecting wires and the leads of discrete components (such as capacitors, resistors, and inductors) can be inserted into the remaining free holes to complete the circuit. Where ICs are not used, discrete components and connecting wires may use any of the holes. Typically the spring clips are rated for 1 ampere at 5 volts and 0.333 amperes at 15 volts (5 watts).

### Bus and terminal strips

Solderless breadboards connect pin to pin by metal strips inside the breadboard. The layout of a typical solderless breadboard is made up from two types of areas, called strips. Strips consist of interconnected electrical terminals. Often breadboard strips or blocks of one brand have male and female dovetail notches so boards can be clipped together to form a large breadboard.

The main areas, to hold most of the electronic components, are called *terminal strips*. In the middle of a terminal strip of a breadboard, one typically finds a notch running in parallel to the long side. The notch is to mark the centerline of the terminal strip. The clips on the right and left of the notch are each connected in a radial way; typically five clips (i.e., beneath five holes) in a row on each side of the notch are electrically connected. The five columns on the left of the notch are often marked as A, B, C, D, and E, while the ones on the right are marked F, G, H, I and J. When a "skinny" dual in-line pin package (DIP) integrated circuit (such as a typical DIP-14 or DIP-16, which have a 0.3 in separation between the pin rows) is plugged into a breadboard, the pins of one side of the chip are supposed to go into column E while the pins of the other side go into column F on the other side of the notch. The rows are identified by numbers from 1 to as many the breadboard design goes. A full-size terminal breadboard strip typically consists of around 56 to 65 rows of connectors. Together with bus strips on each side this makes up a typical 784 to 910 tie point solderless breadboard. Most breadboards are designed to accommodate 17, 30 or 64 rows in the mini, half, and full configurations respectively.

To provide power to the electronic components, *bus strips* are used. A bus strip usually contains two columns: one for ground and one for a supply voltage. However, some breadboards only provide a single-column power distribution bus strip on each long side. Typically the row intended for a supply voltage is marked in red, while the row for ground is marked in blue or black. Some manufacturers connect all terminals in a column. Others just connect groups of, for example, 25 consecutive terminals in a column. The latter design provides a circuit designer with some more control over crosstalk (inductively coupled noise) on the power supply bus. Often the groups in a bus strip are indicated by gaps in the color marking. Bus strips typically run down one or both sides of a terminal strip or between terminal strips. On large breadboards additional bus strips can often be found on the top and bottom of terminal strips.

Some manufacturers provide separate bus and terminal strips. Others just provide breadboard blocks which contain both in one block.

### Jump wires

Jump wires (also called jumper wires) for solderless breadboarding can be obtained in ready-to-use jump wire sets or can be manually manufactured. The latter can become tedious work for larger circuits. Ready-to-use jump wires come in different qualities, some even with tiny plugs attached to the wire ends. Jump wire material for ready-made or homemade wires should usually be 22 AWG (0.33 mm²) solid copper, tin-plated wire - assuming no tiny plugs are to be attached to the wire ends. The wire ends should be stripped 3/16 to 5/16 in. Shorter stripped wires might result in bad contact with the board's spring clips (insulation being caught in the springs). Longer stripped wires increase the likelihood of short-circuits on the board. Needle-nose pliers and tweezers are helpful when inserting or removing wires, particularly on crowded boards.

Differently colored wires and color-coding discipline are often adhered to for consistency. However, the number of available colors is typically far fewer than the number of signal types or paths. Typically, a few wire colors are reserved for the supply voltages and ground (e.g., red, blue, black), some are reserved for main signals, and the rest are simply used where convenient. Some ready-to-use jump wire sets use the color to indicate the length of the wires, but these sets do not allow a meaningful color-coding schema.

### Advanced designs

In a more robust variant, one or more breadboard strips are mounted on a sheet of metal. Typically, that backing sheet also holds a number of binding posts. These posts provide a clean way to connect an external power supply. This type of breadboard may be slightly easier to handle.

Some manufacturers provide high-end versions of solderless breadboards. These are typically high-quality breadboard modules mounted on a flat casing. The casing contains additional equipment for breadboarding, such as a power supply, one or more signal generators, serial interfaces, LED display or LCD modules, and logic probes.

For high-frequency development, a metal breadboard affords a desirable solderable ground plane, often an unetched piece of printed circuit board; integrated circuits are sometimes stuck upside down to the breadboard and soldered to directly, a technique sometimes called "dead bug" construction because of its appearance. Examples of dead bug with ground plane construction are illustrated in a Linear Technologies application note.

## Uses

A common use in the system on a chip (SoC) era is to obtain an microcontroller (MCU) on a pre-assembled printed circuit board (PCB) which exposes an array of input/output (IO) pins in a header suitable to plug into a breadboard, and then to prototype a circuit which exploits one or more of the MCU's peripherals, such as general-purpose input/output (GPIO), UART/USART serial transceivers, analog-to-digital converter (ADC), digital-to-analog converter (DAC), pulse-width modulation (PWM; used in motor control), Serial Peripheral Interface (SPI), or I²C.

Firmware is then developed for the MCU to test, debug, and interact with the circuit prototype. High frequency operation is then largely confined to the SoC's PCB. In the case of high speed interconnects such as SPI and I²C, these can be debugged at a lower speed and later rewired using a different circuit assembly methodology to exploit full-speed operation. A single small SoC often provides most of these electrical interface options in a form factor barely larger than a large postage stamp, available in the American hobby market (and elsewhere) for a few dollars, allowing fairly sophisticated breadboard projects to be created at modest expense.

_(Later sections of the original article were left out of this offline copy for length: Limitations, Alternatives.)_
