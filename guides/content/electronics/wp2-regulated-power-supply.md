---
id: electronics/wp2-regulated-power-supply
title: Regulated power supply
category: electronics
tags: [regulated power supply, electronics]
summary: A regulated power supply is an embedded circuit; it converts unregulated AC (alternating current) into a constant DC. With the help of a rectifier it converts AC supply into DC. Its function is to supply a stable voltage (or less often current), to a circuit or device that must be…
source: https://en.wikipedia.org/w/index.php?title=Regulated_power_supply&oldid=1381495927
source_title: Wikipedia: Regulated power supply
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Regulated power supply", Wikipedia, The Free Encyclopedia (revision 1381495927), https://en.wikipedia.org/w/index.php?title=Regulated_power_supply&oldid=1381495927 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Regulated power supply

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **regulated power supply** is an embedded circuit; it converts unregulated AC (alternating current) into a constant DC. With the help of a rectifier it converts AC supply into DC. Its function is to supply a stable voltage (or less often current), to a circuit or device that must be operated within certain power supply limits. The output from the regulated power supply may be alternating or unidirectional, but is nearly always DC (direct current). The type of stabilization used may be restricted to ensuring that the output remains within certain limits under various load conditions, or it may also include compensation for variations in its own supply source. The latter is much more common today.

## Applications

### D.C. variable bench supply

A **bench power supply** usually refers to a power supply capable of supplying a variety of output voltages useful for BE (bench testing) electronic circuits, possibly with continuous variation of the output voltage, or just some preset voltages. Some have multiple selectable ranges of current/voltage limits which tend to be anti-proportional.

A laboratory ("lab") power supply normally implies an accurate bench power supply, while a balanced or tracking power supply refers to twin supplies for use when a circuit requires both positive and negative supply rails).

#### Types

Variable bench power supplies exist both as linear (transformer first) and switched-mode power supply (full-bridge rectifier first), each with a different set of benefits and disadvantages:

##### Linear

The linear type produces only very little noise (or "ripple voltage") and is less prone to external electromagnetic and radio frequency interference (EMI, RFI), making it preferable for audio equipment and radio-related applications and for powering delicate circuitry. Linear power supplies also have fewer failable parts which increases longevity, and have a quicker transient response. Linear variable bench power supplies have existed longer, dating back at least to the 1980s. Linear power supplies typically make clicking sounds while adjusting the voltage, caused by adjustment between transformer taps. This is done to reduce energy wasted to heat by widening or narrowing the selected section of the secondary side of the transformer to be as close above the user-selected output voltage as possible.

##### Switching

Switching-mode power supplies are more lightweight, efficient, and more compact at a similar power rating, making them suitable for high-power applications. The higher efficiency means less heat production at the same output power, thus less wasted energy and needed cooling. Additionally, they may be able to operate on a wider Mains input voltage range, typically around 110 to 240 volts rather than a section of approximately twenty volts on only end of the range, to be able to operate internationally. Similar switching technology is used in AC adapters that charge battery-powered devices including mobile phones, laptop computers, and electric bicycles.

##### Other

Some high-current power supplies have a rear-sided output for high-current operation. Its poles are larger to support such currents and are usually initially covered by plastic caps. The poles can be connected to accordingly-sized crocodile clamps.

#### Functionality and controls

The front panel typically has LED indicators for "C.V." ("constant voltage") and "C.C." ("constant current"). When the current demanded by the load exceeds the limit set by the user, the power supply automatically switches to the latter mode by regulating the voltage down so much as to prevent the current limit from being exceeded.

Controls vary between power supply models. Many have rotary knobs for setting voltage and current, each usually with a "coarse" and "fine" knob, the former of which adjusts the parameter throughout the entire range, whereas the latter facilitates adjustment within a small surrounding range. Some models lack the "fine" knob for current, as drawn electrical current is decided by the load at a given voltage, thus less important to be throttled precisely.

Some, typically higher-end models, are equipped with additional features such as a button for toggling output power without having to switch the whole device on/off, a memory for shortcuts to voltage/current combinations, an option to automatically cut out power when reaching a current limit specified by the user ("over-current protection"), likewise over-voltage protection that cuts out power if the external voltage exceeds output, indicator displays with more digits (four or five instead of three), digital rather than analogue voltage control, and the ability to lock the power setting to prevent accidental adjustment. In addition to output voltage/current meters, few are equipped with a wattage (power) meter which indicates the product of both in real time, and a USB charging port.

Many models have a handle at the top for carrying. For cooling, a heat sink, fan, or both may be used. Heat sinks may be mounted externally or internally. Heat sinks are silent, whereas fans are more effective in cooling. Fan speed may adjust based on temperature or output current, the former of which cools faster while idle or low throughout intermittent power output, whereas the latter provides acoustic feedback of how much current is output.

Some power supplies have two output channels. These may be used at schools, as a multi-channel unit tends to cost less than two separate units due to shared hardware. High-end ones might allow internally connecting two channels to a series circuit to double the voltage limit. Few power supplies have a third auxiliary output channel that is typically less powerful. It is typically located near the right edge or at the center.

Bench power supply units equipped with an integrated voltmeter panel to measure external voltages have existed in the 1990s and possibly earlier, but portable multimeters have made that feature obsolete.

### Other applications

- Mobile phone power adaptors
- Regulated power supplies in appliances
- Various amplifiers and oscillators

## Topology and technology

Many topologies have been used since the regulated supply was invented. Early technologies included iron-hydrogen resistors, resonant transformers, nonlinear resistors, loading resistors, neon stabiliser tubes, vibrating contact regulators etc.

Modern regulated supplies mostly use a transformer, silicon diode bridge rectifier, reservoir capacitor and voltage regulator IC. There are variations on this theme, such as supplies with multiple voltage lines, variable regulators, power control lines, discrete circuits and so on. Switched mode regulator supplies also include an inductor.

At times regulated supplies can be much more complex. An example supply from a 1980s TV which used bidirectional interaction between the main supply and the line output stage to operate, generating a range of output voltages with varying amounts of stabilisation. Since neither stage could start without the other running, the supply also included a kickstart system to pulse the system into operation. The supply also monitored voltages in the TV power circuitry, shutting down if these voltages went out of spec. For special applications, supplies can become even more complex.
