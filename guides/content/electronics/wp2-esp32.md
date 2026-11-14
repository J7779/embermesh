---
id: electronics/wp2-esp32
title: ESP32
category: electronics
tags: [esp32, electronics]
summary: ESP32 is a family of low-cost, energy-efficient microcontrollers that support both Wi-Fi and Bluetooth.
source: https://en.wikipedia.org/w/index.php?title=ESP32&oldid=1383570586
source_title: Wikipedia: ESP32
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "ESP32", Wikipedia, The Free Encyclopedia (revision 1383570586), https://en.wikipedia.org/w/index.php?title=ESP32&oldid=1383570586 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# ESP32

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**ESP32** is a family of low-cost, energy-efficient microcontrollers that support both Wi-Fi and Bluetooth. These chips feature several processing options, including the Tensilica Xtensa LX6 microprocessor available in dual-core and single-core variants, the Xtensa LX7 dual-core processor, or a RISC-V microprocessor. The ESP32 incorporates components for wireless data communication such as built-in antenna switches, an RF balun, power amplifiers, low-noise receivers, filters, and power-management modules.

Typically, the ESP32 is embedded on device-specific printed circuit boards (PCBs) or offered as part of development kits that include a variety of GPIO pins and connectors, with configurations varying by model and manufacturer. The ESP32 was designed by Espressif Systems and is manufactured by TSMC using their 40 nm process. It is a successor to the ESP8266 microcontroller.

## Features

Since the release of the original ESP32, a number of variants have been introduced and announced. They form the ESP32 family of microcontrollers. These chips have different CPUs and capabilities, but all share the same SDK and are largely code-compatible. Additionally, the original ESP32 was revised (see ESP32 ECO V3, esp 32 cam for example).

**ESP32 family**

| Feature | ESP32 | ESP32-S2 | ESP32-S3 | ESP32-S3 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S31 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **CPU** | Xtensa LX6 | Xtensa LX7 | Xtensa LX7 | | RV32IMAC | RV32IMC | RV32IMAC | RV32IMAC | RV32IMAC | RV32IMAC | RV32IMAFC | RV32IMAFC |
| **CPU cores** | Single, Dual | Single | Dual | | Single | | | | | | Dual | |
| **CPU frequency** | 240 MHz | | | | 120 MHz | 160 MHz | 240 MHz | 160 MHz | | 96 MHz | 400 MHz | 320 MHz |
| **Coprocessor** | FSM | | FSM | RV32IMC | No | | RV32IMC | | No | | RV32IMC | RISC-V |
| **Coprocessor speed** | 20 MHz | | 20 MHz | 17.5 MHz | No | No | 40 MHz | 20 MHz | No | No | 20 MHz |? |
| **FPU** | Single precision | No | Single precision | | No | | | | | | Single precision | |
| **SRAM** | 520 KiB | 320 KiB | 512 KiB | | 272 KiB | 400 KiB | 384 KiB | 512 KiB | 320 KiB | 256 KiB | 768 KiB | 512 KiB |
| **ROM** | 448 KiB | 128 KiB | 384 KiB | | 128 KiB | 384 KiB | | | 256 KiB | 128 KiB | 512 KiB | 320 KiB |
| **RTC SRAM** | 8 KiB | 16 KiB | | | 8 KiB | | 16 KiB | | | 8 KiB | 32 KiB | |
| **PSRAM** | 4 MiB | | 8 MiB | | No | | 8 MiB | No | 8 MiB | No | 16 MiB | 64 MiB |
| **SPI** | Quad | | Octal | | Quad | | | | | | Octal | |
| **UART** | 3 | 2 | 3 | | 2 | | | | 3 | 2 | 5 | 4 |
| **Wi-FI** | 802.11 b/g/n | | | | | | 802.11 b/g/n/ac/ax | 802.11 b/g/n/ax | | No | | 802.11 b/g/n/ax |
| **Ethernet MAC + RMII** | Yes | SPI only | | | | | | | | | Yes | Yes (Gigabit) |
| **Bluetooth** | 4.2 + BR/EDR, LE | No | 5 + LE | | | | | 5.3 + LE | 6 + LE | 5.3 + LE | No | 5.4 + BR/EDR, LE |
| **GPIO pins** | 34 | 43 | 45 | | 20 | 22 | 30 | | | 26 | 55 | 60 |
| **SAR ADCs** | 2 × 12-bit | 2 × 13-bit | 2 × 12-bit | | 1 × 12-bit | | | | | | No | 2 × 12-bit |
| **ADC channels** | 18 | 20 | | | 6 | | 7 | | 4 | 5 | No | 16 |
| **DAC** | 2 × 8-bit DAC | | No | | | | | | | | | |
| **USB** | No | USB OTG | | | No | | | | | | USB OTG | |
| **I²C controllers** | 2 | | | | 1 | | | | | | 2 | |
| **I²S controllers** | 2 | 1 | 2 | | 1 | | | | | | 3 | 2 |
| **DMA** | PDMA | GDMA | | | | GDMA + UHCI | GDMA | | | | | |
| **PWM** | LEDC, MCPWM | | | | LEDC | | | | | | LEDC, MCPWM | |
| **Temperature sensor** | Yes | | | | No | | | | Yes | No | Yes | |
| **Thread + Zigbee** | No | | | | | | Yes | | | | No | Yes |
| **Display support** | SPI | LCD, SPI | | | SPI | | | | | | LCD, SPI, HMI | LCD, SPI |
| **AI/vector ISA** | No | | Yes | | No | | | | | | Yes | |
| **Storage** | 4 bit SD, SPI SD | | | | SPI SD | | | | 4 bit SD, SPI SD, eMMC | SPI SD only | 4 & 8 bit SD, SPI SD, eMMC | 4 bit SD, SPI SD, eMMC |
| **CAN Bus** | CAN 2.0, SPI | | | | SPI | | | | | | CAN 2.0, SPI | |
| **Secure Boot** | V1, V2 | V2 | | | | | | | | | | |
| **Crypto hardware** | No | | Yes | | No | | | | Yes | | Yes | |
| **Other features** | I²S camera | hatI²S and PDM mic | PDM mic | | No | | | | | | Advanced audio | |

## QFN packaged chip and module

ESP32 is housed in quad-flat no-leads (QFN) packages of varying sizes with 49 pads. Specifically, 48 connection pads along the sides and one large thermal pad (connected to ground) on the bottom.

### Chips

The ESP32 system on a chip integrated circuit is packaged in both 6 mm × 6 mm and 5 mm × 5 mm sized QFN packages.

_(This section is shortened.)_

## Printed circuit boards

### Surface-mount module boards

ESP32 based surface-mount printed circuit board modules directly contain the ESP32 SoC and are designed to be easily integrated onto other circuit boards. Meandered inverted-F antenna designs are used for the PCB trace antennas on the modules listed below. In addition to flash memory, some modules include pseudostatic RAM (pSRAM).

_(This section is shortened.)_

## Programming

Programming languages, frameworks, platforms, and environments used for ESP32 programming:

- ESP-IDF – Espressif's official IoT Development Framework for the ESP32, ESP32-S, ESP32-C and ESP32-H series of SoCs.
- Arduino-ESP32 – Arduino core for the ESP32, ESP32-S2, ESP32-S3 and ESP32-C3.
- ESP32forth – FORTH implementation for ESP32
- Espruino – JavaScript SDK and firmware closely emulating Node.js
- MicroPython (and CircuitPython) – lean implementation of Python 3 for microcontrollers
- Mongoose OS – an operating system for connected products on microcontrollers; programmable with JavaScript or C. A recommended platform by Espressif Systems, AWS IoT, and Google Cloud IoT.
- mruby and picoruby for the ESP32
- Nim for the ESP32
- NodeMCU – Lua-based firmware
- Rust
- Swift
- Visual Studio Code with the officially supported Espressif Integrated Development Framework (ESP-IDF) Extension
- Zerynth – Python for IoT and microcontrollers, including the ESP32
- Matlab Simulink
- Zig for the ESP32

## Reception and use

Commercial, industrial and academic uses of ESP32:

### Use in commercial devices

- Alibaba Group's IoT LED wristband, used by participants at the group's 2017 annual gathering. Each wristband operated as a "pixel", receiving commands for coordinated LED light control, allowing formation of a "live and wireless" screen.
- DingTalk's M1, a biometric attendance-tracking system.
- Pium, a home fragrance and aromatherapy device.
- HardKernel's Odroid Go, an ESP32 based handheld gaming device kit made to commemorate Odroid's 10th anniversary.
- Playdate, a handheld video game console jointly developed by Panic Inc. and Teenage Engineering.
- Octopus Energy Mini, an ESP32-C6 based real-time energy monitor.
- Mysa smart thermostats, based on ESP32-WROOM.
- Low-end UniFi switches from Ubiquiti, based on ESP32-D0WD-V3.
- Xteink X4, a compact e-ink reader with support for community firmware.

### Use in industrial devices

- TECHBASE's Moduino X series X1 and X2 modules are ESP32-WROVER / ESP32-WROVER-B based computers for industrial automation and monitoring, supporting digital inputs/outputs, analog inputs, and various computer networking interfaces.
- NORVI IIOT Industrial Devices with ESP32-WROVER / ESP32-WROVER-B SOC for industrial automation and monitoring with digital inputs, analog inputs, relay outputs and multiple communications interfaces. Supports LoRa and Nb-IoT as expansion modules.

### Academic uses

- ESP32 devices are utilized in educational settings and academic research projects. Additionally, ESP32 is used in DIY projects such as building low-cost drones. The DJI Robomaster TT, an educational variant of the Ryze Tello drone, is equipped with an ESP32.

_(Later sections of the original article were left out of this offline copy for length: the rest of QFN packaged chip and module, the rest of Printed circuit boards, Undocumented HCI commands in ESP32.)_
