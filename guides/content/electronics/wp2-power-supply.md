---
id: electronics/wp2-power-supply
title: Power supply
category: electronics
tags: [power supply, power rail, electronics]
summary: A power supply is an electrical device that supplies electric power to an electrical load.
source: https://en.wikipedia.org/w/index.php?title=Power_supply&oldid=1378650984
source_title: Wikipedia: Power supply
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Power supply", Wikipedia, The Free Encyclopedia (revision 1378650984), https://en.wikipedia.org/w/index.php?title=Power_supply&oldid=1378650984 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Power supply

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **power supply** is an electrical device that supplies electric power to an electrical load. The main purpose of a power supply is to convert electric current from a source to the correct voltage, current, and frequency to power the load. As a result, power supplies are sometimes referred to as electric power converters. Some power supplies are separate standalone pieces of equipment, while others are built into the load appliances that they power. Examples of the latter include power supplies found in desktop computers and consumer electronics devices. Other functions that power supplies may perform include limiting the current drawn by the load to safe levels, shutting off the current in the event of an electrical fault, power conditioning to prevent electronic noise or voltage surges on the input from reaching the load, power-factor correction, and storing energy so it can continue to power the load in the event of a temporary interruption in the source power (uninterruptible power supply).

All power supplies have a *power input* connection, which receives energy in the form of electric current from a source, and one or more *power output* or **power rail** connections that deliver current to the load. The source power may come from the electric power grid, such as an electrical outlet, energy storage devices such as batteries or fuel cells, generators or alternators, solar power converters, or another power supply. The input and output are usually hardwired circuit connections, though some power supplies employ wireless energy transfer to power their loads without wired connections. Some power supplies have other types of inputs and outputs as well, for functions such as external monitoring and control.

## General classification

### Functional

Power supplies are categorized in various ways, including by functional features. For example, a *regulated* power supply is one that maintains constant output voltage or current despite variations in load current or input voltage. Conversely, the output of an *unregulated* power supply can change significantly when its input voltage or load current changes. *Adjustable* power supplies allow the output voltage or current to be programmed by mechanical controls (e.g., knobs on the power supply front panel), or by means of a control input, or both. An *adjustable regulated* power supply is one that is both adjustable and regulated. An *isolated* power supply has a power output that is electrically independent of its power input; this is in contrast to other power supplies that share a common connection between power input and output.

### Packaging

Power supplies are packaged in different ways and classified accordingly. A *bench* power supply is a stand-alone desktop unit used in applications such as circuit test and development. *Open frame* power supplies have only a partial mechanical enclosure, sometimes consisting of only a mounting base; these are typically built into machinery or other equipment. *Rack mount* power supplies are designed to be secured into standard electronic equipment racks. An *integrated* power supply is one that shares a common printed circuit board with its load. An *external* power supply, *AC adapter* or *power brick*, is a power supply located in the load's AC power cord that plugs into a wall outlet; a *wall wart* is an external supply integrated with the outlet plug itself. These are popular in consumer electronics because of their safety; the hazardous 120- or 240-volt main current is transformed down to a safer voltage before it enters the appliance body.

### Power conversion method

Power supplies can be broadly divided into *linear* and *switching* types. Linear power converters process the input power directly, with all active power conversion components operating in their linear operating regions. In switching power converters, the input power is converted to AC or to DC pulses before processing, by components that operate predominantly in non-linear modes (e.g., transistors that spend most of their time in cutoff or saturation). Power is "lost" (converted to heat) when components operate in their linear regions and, consequently, switching converters are usually more efficient than linear converters because their components spend less time in linear operating regions.

### Purpose

Power supplies are sometimes categorized according to their intended purpose, such as "AC-DC front end for grid bias" or "DC-DC converter for point-of-load (POL) regulation".

## Types

### DC power supplies

An AC-to-DC power supply operates on an AC input voltage and generates a DC output voltage. Depending on the application requirements, the output voltage may contain varying amounts of AC frequency components, known as ripple voltage, which are influenced by the AC input voltage frequency and the power supply's operation. A DC power supply operating on DC input voltage is called a DC-to-DC converter. This section focuses mostly on the AC-to-DC variant.

#### Linear power supply

In a linear power supply, the AC input voltage first passes through a power transformer, then undergoes rectification and filtering to produce a DC voltage. Filtering minimizes the AC mains frequency component present in the rectifier output. It can be as simple as a single capacitor or as complex as a pi filter. The required level of filtering in the power supply depends on the electric load's tolerance to ripple. In some applications, ripple can be entirely ignored. For instance, in certain battery charging applications, the power supply may consist only of a transformer and a diode, with a resistor at the output to limit the charging current.

#### Switched-mode power supply

In a switched-mode power supply (SMPS), the AC mains input is directly rectified and then filtered to obtain a DC voltage. The resulting DC voltage is then switched on and off at a high frequency by electronic switching circuitry, thus producing an AC current that will pass through a high-frequency transformer or inductor. Switching occurs at a very high frequency (typically 10 kHz — 1 MHz), thereby enabling the use of transformers and filter capacitors that are much smaller, lighter, and less expensive than those found in linear power supplies operating at mains frequency. After the inductor or transformer secondary, the high frequency AC is rectified and filtered to produce the DC output voltage. If the SMPS uses an adequately insulated high-frequency transformer, the output will be electrically isolated from the mains; this feature is often essential for safety.

Switched-mode power supplies are usually regulated, and to keep the output voltage constant, the power supply employs a feedback controller that monitors current drawn by the load. The switching duty cycle increases as power output requirements increase.

SMPSs often include safety features such as current limiting or a crowbar circuit to help protect the device and the user from harm. In the event that an abnormal high-current power draw is detected, the switched-mode supply can assume this is a direct short and will shut itself down before damage is done. PC power supplies often provide a *power good* signal to the motherboard; the absence of this signal prevents operation when abnormal supply voltages are present.

Some SMPSs have an absolute limit on their minimum current output. They are only able to output above a certain power level and cannot function below that point. In a no-load condition the frequency of the power slicing circuit increases to great speed, causing the isolated transformer to act as a Tesla coil, causing damage due to the resulting very high voltage power spikes. Switched-mode supplies with protection circuits may briefly turn on but then shut down when no load has been detected. A very small low-power dummy load such as a ceramic power resistor or 10-watt light bulb can be attached to the supply to allow it to run with no primary load attached.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Types, Thermal management, Overload protection, Applications.)_
