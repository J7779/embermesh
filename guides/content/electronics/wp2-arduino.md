---
id: electronics/wp2-arduino
title: Arduino
category: electronics
tags: [arduino, electronics]
summary: Arduino is an Italian open-source hardware and software company (now owned by Qualcomm), and is a project and user community that designs and manufactures single-board microcontrollers and microcontroller kits for building digital and other kinds of devices.
source: https://en.wikipedia.org/w/index.php?title=Arduino&oldid=1385507494
source_title: Wikipedia: Arduino
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Arduino", Wikipedia, The Free Encyclopedia (revision 1385507494), https://en.wikipedia.org/w/index.php?title=Arduino&oldid=1385507494 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Arduino

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Arduino** is an Italian open-source hardware and software company (now owned by Qualcomm), and is a project and user community that designs and manufactures single-board microcontrollers and microcontroller kits for building digital and other kinds of devices. The hardware products are licensed under a Creative Commons license (CC BY-SA), and the software is licensed under the GNU Lesser General Public License (LGPL) or the GNU General Public License (GPL), permitting the manufacturing of Arduino boards and software distribution by anyone. Arduino boards are available commercially from the official website or through authorized distributors.

Arduino board designs use a variety of microprocessors and controllers. The boards are equipped with sets of digital and analog input/output (I/O) pins that may be interfaced to various expansion boards ('shields') or breadboards (for prototyping) and other electronic circuits. The boards feature serial communications interfaces, including Universal Serial Bus (USB) on some models, which are also used for loading programs. The microcontrollers can be programmed using the programming languages C (Embedded C) and C++, using a standard API which is also known as the *Arduino Programming Language*, inspired by the language Processing and used with a modified version of the Processing IDE. Beyond using traditional compiler toolchains, the Arduino project provides an integrated development environment (IDE) and a command-line interface tool developed in Go.

The Arduino project began in 2005 as a tool for students at the Interaction Design Institute Ivrea, Italy, aiming to provide a low-cost and easy way for novices and professionals to create devices that interact with their environment using sensors and actuators. Common examples of such devices intended for makers include simple robots, thermostats, and motion detectors.

The name *Arduino* comes from the Bar di Re Arduino, where some of the project's founders used to meet. This bar is in Ivrea, Italy and was named after Arduin.

## Hardware

Arduino is open-source hardware. The hardware reference designs are distributed under a Creative Commons Attribution Share-Alike 2.5 license and are available on the Arduino website. Layout and production files for some versions of the hardware are also available.

Although the hardware and software designs are freely available under copyleft licenses, the developers have requested the name *Arduino* to be exclusive to the official product and not be used for derived works without permission. The official policy document on the use of the Arduino name emphasizes that the project is open to incorporating work by others into the official product. Several Arduino-compatible products commercially released have avoided the project name by using various names ending in *-duino*.

Most Arduino boards consist of an Atmel 8-bit AVR microcontroller (ATmega8, ATmega168, ATmega328, ATmega1280, or ATmega2560) with varying amounts of flash memory, pins, and features. The 32-bit Arduino Due, based on the Atmel SAM3X8E was introduced in 2012. The boards use single or double-row pins or female headers that facilitate connections for programming and incorporation into other circuits. These may connect with add-on modules termed *shields*. Multiple and possibly stacked shields may be individually addressable via an I²C serial bus. Most boards include a 5 V linear regulator and a 16 MHz crystal oscillator or ceramic resonator. Some designs, such as the LilyPad, run at 8 MHz and dispense with the onboard voltage regulator due to specific form factor restrictions.

Arduino microcontrollers are pre-programmed with a bootloader that simplifies the uploading of programs to the on-chip flash memory. The default bootloader of the Arduino Uno is the Optiboot bootloader. Boards are loaded with program code via a serial connection to another computer. Some serial Arduino boards contain a level shifter circuit to convert between RS-232 logic levels and transistor–transistor logic (TTL serial) level signals. Current Arduino boards are programmed via Universal Serial Bus (USB), implemented using USB-to-serial adapter chips such as the FTDI FT232. Some boards, such as later-model Uno boards, substitute the FTDI chip with a separate AVR chip containing USB-to-serial firmware, which is reprogrammable via its own ICSP header. Other variants, such as the Arduino Mini and the unofficial Boarduino, use a detachable USB-to-serial adapter board or cable, Bluetooth or other methods. When used with traditional microcontroller tools, instead of the Arduino IDE, standard AVR in-system programming (ISP) programming is used.

The Arduino board exposes most of the microcontroller's I/O pins for use by other circuits. The *Diecimila*, *Duemilanove*, and current *Uno* provide 14 digital I/O pins, six of which can produce pulse-width modulated(PWM) signals, and six analog inputs, which can also be used as six digital I/O pins. These pins are on the top of the board, via female 0.1-inch (2.54 mm) headers. Several plug-in application shields are also commercially available. The Arduino Nano and Arduino-compatible Bare Bones Board and Boarduino boards may provide male header pins on the underside of the board that can plug into solderless breadboards.

Many Arduino-compatible and Arduino-derived boards exist. Some are functionally equivalent to an Arduino and can be used interchangeably. Many enhance the basic Arduino by adding output drivers, often for use in school-level education, to simplify making buggies and small robots. Others are electrically equivalent, but change the form factor, sometimes retaining compatibility with shields, sometimes not. Some variants use different processors, of varying compatibility.

In addition to hardware variations, open source libraries have been developed to support Arduino hardware in EDA tools. One such project provides KiCad schematic symbols and PCB footprints for Arduino modules, expansion boards, and connectors, making it easier for engineers to integrate Arduino into their designs.

### Official boards

The original Arduino hardware was manufactured by the Italian company Smart Projects. Some Arduino-branded boards have been designed by the American companies SparkFun Electronics and Adafruit Industries. As of 2016, 17 versions of the Arduino hardware have been commercially produced.

### Shields

Arduino shields are add-on boards that stack onto Arduino mainboards to extend their functionality for specific tasks. Common uses include controlling DC motors for a robot, for 3D printer, adding a LCD display, enabling Ethernet connectivity, providing GPS/ GNSS navigation and converting pin headers into Grove connectors. Shields are typically designed and manufactured by third-party hardware companies, including SparkFun Electronics, Adafruit Industries,, and Cytron Technologies, among others. Several shields can also be made do it yourself (DIY).

## Applications

- Arduboy, a handheld game console based on Arduino
- Arduinome, a MIDI controller device that mimics the Monome
- Ardupilot, drone software and hardware
- ArduSat, a cubesat based on Arduino
- C-STEM Studio, a platform for hands-on integrated learning of computing, science, technology, engineering, and mathematics (C-STEM) with robotics
- Data loggers for scientific research
- OBDuino, a trip computer that uses the on-board diagnostics interface found in most modern cars
- OpenEVSE an open-source electric vehicle charger
- XOD, a visual programming language for Arduino

## Simulation

- Tinkercad Circuits – an analog and digital simulator supporting Arduino Simulation, which is commonly used to create 3D models

## Recognitions

The Arduino project received an honorary mention in the Digital Communities category at the 2006 Prix Ars Electronica.

The Arduino Engineering Kit won the Bett Award for "Higher Education or Further Education Digital Services" in 2020.

_(Later sections of the original article were left out of this offline copy for length: Software.)_
