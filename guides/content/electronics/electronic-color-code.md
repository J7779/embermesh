---
id: electronics/electronic-color-code
title: Electronic color code
category: electronics
tags: [electronics, colour code, resistor, reference]
summary: The resistor/capacitor colour code table for reading component values.
source: "https://en.wikipedia.org/w/index.php?title=Electronic_color_code&oldid=1384919029"
source_title: Electronic color code (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Electronic color code' (rev 1384919029), Wikipedia, https://en.wikipedia.org/wiki/Electronic_color_code. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# Electronic color code

An electronic color code or electronic colour code (see spelling differences) is used to indicate the values or ratings of electronic components, usually for resistors, but also for capacitors, inductors, diodes and others. A separate code, the 25-pair color code, is used to identify wires in some telecommunications cables. Different codes are used for wire leads on devices such as transformers or in building wiring.

## History

Before industry standards were established, each manufacturer used its own unique system for color coding or marking their components.

In the 1920s, the RMA resistor color code was developed by the Radio Manufacturers Association (RMA) as a fixed resistor coloring code marking. In 1930, the first radios with RMA color-coded resistors were built. Over many decades, as the organization name changed (RMA, RTMA, RETMA, EIA) so was the name of the code. Though known most recently as EIA color code, the four name variations are found in books, magazines, catalogs, and other documents over more than 96 years.

In 1952, it was standardized in IEC 62:1952 by the International Electrotechnical Commission (IEC) and since 1963 also published as EIA RS-279. Originally only meant to be used for fixed resistors, the color code was extended to also cover capacitors with IEC 62:1968. The code was adopted by many national standards like DIN 40825 (1973), BS 1852 (1974) and IS 8186 (1976). The current international standard defining marking codes for resistors and capacitors is IEC 60062:2016. In addition to the color code, these standards define a letter and digit code named RKM code for resistors and capacitors.

Color bands were used because they were easily and cheaply printed on tiny components. However, there were drawbacks, especially for color blind people. Overheating of a component or dirt accumulation may make it impossible to distinguish brown from red or orange. Advances in printing technology have now made printed numbers more practical on small components. The values of components in surface mount packages are marked with printed alphanumeric codes instead of a color code.

## Resistors

### Color band system

To distinguish left from right there is a gap between the C and D bands:

1. The first significant figure of component value (left side)
2. The second significant figure (some precision resistors have a third significant figure, and thus five bands).
3. The decimal multiplier (number of trailing zeroes, or power of 10 multiplier)
4. If present, indicates tolerance of value in percent (no band means 20%)

In the above example, a resistor with bands of red, violet, green, and gold has first digit 2 (red; see table below), second digit 7 (violet), followed by 5 (green) zeroes: 2700000 ohms. Gold signifies that the tolerance is ±5%.

Precision resistors may be marked with a five band system, to include three significant digits, a power of 10 multiplier (number of trailing zeroes, and a tolerance band. An extra-wide first band indicates a wire-wound resistor.

Resistors manufactured for military use may also include a fifth band which indicates component failure rate (reliability); refer to MIL-HDBK-199 for further details.

Tight tolerance resistors may have three bands for significant figures rather than two, or an additional band indicating temperature coefficient of resistance (TCR), in units of ppm/K.

All coded components have at least two value bands and a multiplier; other bands are optional.

The standard color code per IEC 60062:2016 is as follows:

| Ring Colour |   |   | Significant numeral | Multiplier |   | Tolerance |   | TCR, Temperature coefficient |   |
|---|---|---|---|---|---|---|---|---|---|
| Name | Code | RAL | Significant numeral | Multiplier | Multiplier | Percent [%] | Letter | [ppm/K] | Letter |
| None | – | – | – | – |   | ±20 | M | – |   |
| Pink | PK | 3015 | – | ×10^−3 | ×0.001 | – |   | – |   |
| Silver | SR | – | – | ×10^−2 | ×0.01 | ±10 | K | – |   |
| Gold | GD | – | – | ×10^−1 | ×0.1 | ±5 | J | – |   |
| Black | BK | 9005 | 0 | ×10^0 | ×1 | – |   | ±250 | U |
| Brown | BN | 8003 | 1 | ×10^1 | ×10 | ±1 | F | ±100 | S |
| Red | RD | 3000 | 2 | ×10^2 | ×100 | ±2 | G | ±50 | R |
| Orange | OG | 2003 | 3 | ×10^3 | ×1000 | ±0.05 | W | ±15 | P |
| Yellow | YE | 1021 | 4 | ×10^4 | ×10000 | ±0.02 | P | ±25 | Q |
| Green | GN | 6018 | 5 | ×10^5 | ×100000 | ±0.5 | D | ±20 | Z |
| Blue | BU | 5015 | 6 | ×10^6 | ×1000000 | ±0.25 | C | ±10 | Z |
| Violet | VT | 4005 | 7 | ×10^7 | ×10000000 | ±0.1 | B | ±5 | M |
| Grey | GY | 7000 | 8 | ×10^8 | ×100000000 | ±0.01 | L (A) | ±1 | K |
| White | WH | 1013 | 9 | ×10^9 | ×1000000000 | – |   | – |   |

| Ring Colour |   |   | Significant numeral | Multiplier |   | Tolerance |   | TCR, Temperature coefficient |   |
|---|---|---|---|---|---|---|---|---|---|
| Name | Code | RAL | Significant numeral | Multiplier | Multiplier | Percent [%] | Letter | [ppm/K] | Letter |
| None | – | – | – | – |   | ±20 | M | – |   |
| Pink | PK | 3015 | – | ×10^−3 | ×0.001 | – |   | – |   |
| Silver | SR | – | – | ×10^−2 | ×0.01 | ±10 | K | – |   |
| Gold | GD | – | – | ×10^−1 | ×0.1 | ±5 | J | – |   |
| Black | BK | 9005 | 0 | ×10^0 | ×1 | – |   | ±250 | U |
| Brown | BN | 8003 | 1 | ×10^1 | ×10 | ±1 | F | ±100 | S |
| Red | RD | 3000 | 2 | ×10^2 | ×100 | ±2 | G | ±50 | R |
| Orange | OG | 2003 | 3 | ×10^3 | ×1000 | ±0.05 | W | ±15 | P |
| Yellow | YE | 1021 | 4 | ×10^4 | ×10000 | ±0.02 | P | ±25 | Q |
| Green | GN | 6018 | 5 | ×10^5 | ×100000 | ±0.5 | D | ±20 | Z |
| Blue | BU | 5015 | 6 | ×10^6 | ×1000000 | ±0.25 | C | ±10 | Z |
| Violet | VT | 4005 | 7 | ×10^7 | ×10000000 | ±0.1 | B | ±5 | M |
| Grey | GY | 7000 | 8 | ×10^8 | ×100000000 | ±0.01 | L (A) | ±1 | K |
| White | WH | 1013 | 9 | ×10^9 | ×1000000000 | – |   | – |   |

Show dark mode version of file

| Ring Colour |   |   | Significant numeral | Multiplier |   | Tolerance |   | TCR, Temperature coefficient |   |
|---|---|---|---|---|---|---|---|---|---|
| Name | Code | RAL | Significant numeral | Multiplier | Multiplier | Percent [%] | Letter | [ppm/K] | Letter |
| None | – | – | – | – |   | ±20 | M | – |   |
| Pink | PK | 3015 | – | ×10^−3 | ×0.001 | – |   | – |   |
| Silver | SR | – | – | ×10^−2 | ×0.01 | ±10 | K | – |   |
| Gold | GD | – | – | ×10^−1 | ×0.1 | ±5 | J | – |   |
| Black | BK | 9005 | 0 | ×10^0 | ×1 | – |   | ±250 | U |
| Brown | BN | 8003 | 1 | ×10^1 | ×10 | ±1 | F | ±100 | S |
| Red | RD | 3000 | 2 | ×10^2 | ×100 | ±2 | G | ±50 | R |
| Orange | OG | 2003 | 3 | ×10^3 | ×1000 | ±0.05 | W | ±15 | P |
| Yellow | YE | 1021 | 4 | ×10^4 | ×10000 | ±0.02 | P | ±25 | Q |
| Green | GN | 6018 | 5 | ×10^5 | ×100000 | ±0.5 | D | ±20 | Z |
| Blue | BU | 5015 | 6 | ×10^6 | ×1000000 | ±0.25 | C | ±10 | Z |
| Violet | VT | 4005 | 7 | ×10^7 | ×10000000 | ±0.1 | B | ±5 | M |
| Grey | GY | 7000 | 8 | ×10^8 | ×100000000 | ±0.01 | L (A) | ±1 | K |
| White | WH | 1013 | 9 | ×10^9 | ×1000000000 | – |   | – |   |

Show light mode version of file

Resistors use various E series of preferred numbers for their specific values, which are determined by their tolerance. These values repeat for every decade of magnitude:... 0.68, 6.8, 68, 680,... For resistors of 20% tolerance the E6 series, with six values: 10, 15, 22, 33, 47, 68, then 100, 150,... is used; each value is approximately the previous value multiplied by ^6√10. For 10% tolerance resistors the E12 series, with ^12√10 as multiplier, is used; similar schemes up to E192, for 0.5% or tighter tolerance are used. The separation between the values is related to the tolerance so that adjacent values at the extremes of tolerance approximately just overlap; for example, in the E6 series 10 + 20% is 12, while 15 − 20% is also 12.

Zero ohm resistors, marked with a single black band, are lengths of wire wrapped in a resistor-like body which can be mounted on a printed-circuit board (PCB) by automatic component-insertion equipment. They are typically used on PCBs as insulating "bridges" where two tracks would otherwise cross, or as soldered-in jumper wires for setting configurations.

### Body-end-dot system

The "body-end-dot" or "body-tip-spot" system was used for cylindrical composition resistors sometimes still found in very old equipment (built before the Second World War); the first band was given by the body color, the second band by the color of one end of the resistor, and the multiplier by a dot or band around the middle of the resistor. The other end of the resistor was in the body color, silver, or gold for 20%, 10%, 5% tolerance (tighter tolerances were not routinely used).

### Examples

From top to bottom:

- Green, blue, black, black, brown
  - 560 ohms ±1%
- Red, red, orange, gold
  - 22000 ohms ±5%
- Yellow, violet, brown, gold
  - 470 ohms ±5%
- Blue, grey, black, gold
  - 68 ohms ±5%

The physical size of a resistor is indicative of the power it can dissipate.

There is an important difference between the use of three and of four bands to indicate resistance. The same resistance is encoded by:

- Red, red, *orange* = *22* followed by *3* zeroes = 22000 (excluding default, silver, or gold tolerance)
- Red, red, *black*, *red* = *220* followed by *2* zeroes = 22000 (excluding brown or other band for tolerance)

### Mnemonics

Useful mnemonics have been created to make it easier to remember the numeric order of resistor color bands:

- Betty Brown Runs Over Your Garden But Violet Gingerly Walks.
- Bad Bears Raid Our Yummy Grub But Veto Grey Waffles.
- BBROY from Great Britain has a Very Good Wife.

The following example includes the tolerance codes — gold, silver and none:

- Bad Beer Rots Out Your Guts But Vodka Goes Well – Get Some Now.

The colors are sorted in ascending order of frequency to make them easy to remember and to reduce the significance of possible read errors due to color shifts and fading over time: red (2), orange (3), yellow (4), green (5), blue (6), violet (7). Black (0) has no energy, brown (1) has a little more, white (9) has everything and grey (8) is like white, but less intense.

## Capacitors

Capacitors may be marked with 4 or more colored bands or dots. The colors encode the first and second most significant digits of the value in picofarads, and the third color the decimal multiplier. Additional bands have meanings which may vary from one type to another. Low-tolerance capacitors may begin with the first 3 (rather than 2) digits of the value. It is usually, but not always, possible to work out what scheme is used by the particular colors used. Cylindrical capacitors marked with bands may look like resistors.

| Color |   | Significant digits | Multiplier | Tolerance [%] | Characteristic | DC working voltage [V] | Operating temperature [°C] | EIA/vibration [Hz] |
|---|---|---|---|---|---|---|---|---|
|   | Black | 0 | 1 | — | — | — | −55 to +70 | 10 to 55 |
|   | Brown | 1 | 10 | ±1 | B | 100 | — | — |
|   | Red | 2 | 100 | ±2 | C | — | −55 to +85 | — |
|   | Orange | 3 | 1000 | — | D | 300 | — | — |
|   | Yellow | 4 | 10000 | — | E | — | −55 to +125 | 10 to 2000 |
|   | Green | 5 | 100000 | ±0.5 | F | 500 | — | — |
|   | Blue | 6 | 1000000 | — | — | — | −55 to +150 | — |
|   | Violet | 7 | 10000000 | — | — | — | — | — |
|   | Grey | 8 | — | — | — | — | — | — |
|   | White | 9 | — | — | — | — | — | EIA |
|   | Gold | — | — | ±5 | — | 1000 | — | — |
|   | Silver | — | — | ±10 | — | — | — | — |

Extra bands on ceramic capacitors identify the voltage rating class and temperature coefficient characteristics. A broad black band was applied to some tubular paper capacitors to indicate the end that had the outer electrode; this allowed this end to be connected to chassis ground to provide some shielding against hum and noise pickup.

Polyester film and "gum drop" tantalum electrolytic capacitors may also be color-coded to give the value, working voltage and tolerance.

### Postage stamp capacitors and war standard coding

Capacitors of the rectangular "postage stamp" form made for military use during World War II used *American War Standard* (AWS) or *Joint Army-Navy* (JAN) coding in six dots stamped on the capacitor. An arrow on the top row of dots pointed to the right, indicating the reading order. From left to right the top dots were: either black, indicating JAN mica, or silver, indicating AWS paper; first significant digit; and second significant digit. The bottom three dots indicated temperature characteristic, tolerance, and decimal multiplier. The characteristic was black for ±1000 ppm/°C, brown for ±500, red for ±200, orange for ±100, yellow for −20 to +100 ppm/°C, and green for 0 to +70 ppm/°C.

A similar six-dot code by EIA had the top row as first, second and third significant digits and the bottom row as voltage rating (in hundreds of volts; no color indicated 500 volts), tolerance, and multiplier. A three-dot EIA code was used for 500 volt 20% tolerance capacitors, and the dots signified first and second significant digits and the multiplier. Such capacitors were common in vacuum tube equipment and in surplus for a generation after the war but are unavailable now.

## Inductors

Standards IEC 60062 / EN 60062 do not define a color code for inductors, but manufacturers of small inductors use the resistor color code, typically encoding inductance in microhenries. A white tolerance ring is used by TDK to indicate custom specifications.

## Diodes

The part number for small JEDEC "1N"-coded diodes – in the form "1N4148" – is sometimes encoded as three or four rings in the standard color code, omitting the "1N" prefix. The 1N4148 would then be coded as yellow (4), brown (1), yellow (4), grey (8).

## Wire

### Transformer

Power transformers used in North American vacuum-tube equipment were often color-coded to identify the leads. Black was the primary connection, red secondary for the B+ (plate voltage), red with a yellow tracer was the center tap for the B+ full-wave rectifier winding, green or brown was the heater voltage for all tubes, yellow was the filament voltage for the rectifier tube (often a different voltage than other tube heaters). Two wires of each color were provided for each circuit, and phasing was not identified by the color code.

Audio transformers for vacuum tube equipment were coded blue for the finishing lead of the primary, red for the B+ lead of the primary, brown for a primary center tap, green for the finishing lead of the secondary, black for grid lead of the secondary, and yellow for a tapped secondary. Each lead had a different color since relative polarity or phase was more important for these transformers. Intermediate-frequency tuned transformers were coded blue and red for the primary and green and black for the secondary.

### Other

Wires may be color-coded to identify their function, voltage class, polarity, phase or to identify the circuit in which they are used. The insulation of the wire may be solidly colored, or where more combinations are needed, one or two tracer stripes may be added. Some wiring color codes are set by national regulations, but often a color code is specific to a manufacturer or industry.

Building wiring under the US National Electrical Code and the Canadian Electrical Code is identified by colors to show energized, neutral, and grounding conductors, and to identify phases. Other color codes are used in the UK and other areas to identify building wiring or flexible cable wiring.

Mains electrical wiring, both in a building and on equipment, was once usually red for live, black for neutral, and green for earth, but this was changed as it was a hazard for color-blind people, who might confuse red and green; different countries use different conventions. Red and black are frequently used for positive and negative of battery or other single-voltage DC wiring.

Thermocouple wires and extension cables are identified by color code for the type of thermocouple; interchanging thermocouples with unsuitable extension wires destroys the accuracy of the measurement.

Automotive wiring is color-coded but standards vary by manufacturer; differing SAE and DIN standards exist.

Modern personal computer peripheral cables and connectors are color-coded to simplify connection of speakers, microphones, mice, keyboards and other peripherals, usually according to coloring schemes following recommendations such as PC System Design Guide, PoweredUSB, ATX, etc.

A common convention for wiring systems in industrial buildings is: black jacket – AC less than 1,000 volts, blue jacket – DC or communications, orange jacket – medium voltage 2,300 or 4,160 V, red jacket 13,800 V or higher. Red-jacketed cable is also used for relatively low-voltage fire alarm wiring, but has a much different appearance.

Local area network cables may also have non-standardised jacket colors identifying, for example, process control network vs. office automation networks, or to identify redundant network connections, but these codes vary by organization and facility.
