---
id: electronics/wp2-schottky-diode
title: Schottky diode
category: electronics
tags: [schottky diode, schottky barrier diode, hot-carrier diode, electronics]
summary: The Schottky diode (named after the German physicist Walter Schottky), also known as Schottky barrier diode or hot-carrier diode, is a semiconductor diode formed by the junction of a semiconductor with a metal.
source: https://en.wikipedia.org/w/index.php?title=Schottky_diode&oldid=1378301622
source_title: Wikipedia: Schottky diode
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Schottky diode", Wikipedia, The Free Encyclopedia (revision 1378301622), https://en.wikipedia.org/w/index.php?title=Schottky_diode&oldid=1378301622 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Schottky diode

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **Schottky diode** (named after the German physicist Walter Schottky), also known as **Schottky barrier diode** or **hot-carrier diode**, is a semiconductor diode formed by the junction of a semiconductor with a metal. It has a low forward voltage drop and a very fast switching action. The cat's-whisker detectors used in the early days of wireless and metal rectifiers used in early power applications can be considered primitive Schottky diodes.

When sufficient forward voltage is applied, a current flows in the forward direction. A silicon p–n diode has a typical forward voltage of 600–700 mV, while the Schottky's forward voltage is 150–450 mV. This lower forward voltage requirement allows higher switching speeds and better system efficiency.

## Construction

A metal–semiconductor junction is formed between a metal and a semiconductor, creating a Schottky barrier (instead of a semiconductor–semiconductor junction as in conventional diodes). Typical metals used are molybdenum, platinum, chromium or tungsten, and certain silicides (e.g., palladium silicide and platinum silicide), whereas the semiconductor would typically be n-type silicon. The metal side acts as the anode, and n-type semiconductor acts as the cathode of the diode; meaning conventional current can flow from the metal side to the semiconductor side, but not in the opposite direction. This Schottky barrier results in both very fast switching and low forward voltage drop.

The choice of the combination of the metal and semiconductor determines the forward voltage of the diode. Both n- and p-type semiconductors can develop Schottky barriers. However, the p-type typically has a much lower forward voltage. As the reverse leakage current increases dramatically with lowering the forward voltage, it cannot be too low, so the usually employed range is about 0.15–0.45 V, and p-type semiconductors are employed only rarely. Titanium silicide and other refractory silicides, which are able to withstand the temperatures needed for source/drain annealing in CMOS processes, usually have too low a forward voltage to be useful, so processes using these silicides therefore usually do not offer Schottky diodes.

With increased doping of the semiconductor, the width of the depletion region drops. Below a certain width, the charge carriers can tunnel through the depletion region. At very high doping levels, the junction does not behave as a rectifier any more and becomes an ohmic contact. This can be used for the simultaneous formation of ohmic contacts and diodes, as a diode will form between the silicide and lightly doped n-type region, and an ohmic contact will form between the silicide and the heavily doped n- or p-type region. Lightly doped p-type regions pose a problem, as the resulting contact has too high a resistance for a good ohmic contact, but too low a forward voltage and too high a reverse leakage to make a good diode.

As the edges of the Schottky contact are fairly sharp, a high electric field occurs around them, which limits how large the reverse breakdown voltage threshold can be. Various strategies are used, from guard rings to overlaps of metallisation to reduce the field. The guard rings consume valuable die area and are used primarily for larger higher-voltage diodes, while overlapping metallization is employed primarily with smaller low-voltage diodes.

Schottky diodes are often used as antisaturation clamps in Schottky transistors. Schottky diodes made from palladium silicide (PdSi) are excellent due to their lower forward voltage (which has to be lower than the forward voltage of the base-collector junction). The Schottky temperature coefficient is lower than the coefficient of the B–C junction, which limits the use of PdSi at higher temperatures.

For power Schottky diodes, the parasitic resistances of the buried n+ layer and the epitaxial n-type layer become important. The resistance of the epitaxial layer is more important than it is for a transistor, as the current must cross its entire thickness. However, it serves as a distributed ballasting resistor over the entire area of the junction and, under usual conditions, prevents localized thermal runaway.

In comparison with the power p–n diodes, the Schottky diodes are less rugged. The junction is in direct contact with the thermally sensitive metallization; a Schottky diode can therefore dissipate less power than an equivalent-size p–n counterpart with a deep-buried junction before failing (especially during reverse breakdown). The relative advantage of the lower forward voltage of Schottky diodes is diminished at higher forward currents, where the voltage drop is dominated by the series resistance.

## Reverse recovery time

The most important difference between the p–n diode and the Schottky diode is the reverse recovery time (t_rr) when the diode switches from the conducting to the non-conducting state. In a p–n diode, the reverse recovery time can be in the order of several microseconds to less than 100 ns for fast diodes, and it is mainly limited by the diffusion capacitance caused by minority carriers accumulated in the diffusion region during the conducting state. Schottky diodes are significantly faster since they are unipolar devices and their speed is only limited by the junction capacitance. The switching time is ~100 ps for the small-signal diodes, and up to tens of nanoseconds for special high-capacity power diodes. With p–n-junction switching, there is also a reverse recovery current, which in high-power semiconductors brings increased EMI noise. With Schottky diodes, switching is essentially "instantaneous" with only a slight capacitive loading, which is much less of a concern.

This "instantaneous" switching is not always the case. In higher voltage Schottky devices, in particular, the guard ring structure needed to control breakdown field geometry creates a parasitic p–n diode with the usual recovery time attributes. As long as this guard ring diode is not forward biased, it adds only capacitance. If the Schottky junction is driven hard enough however, the forward voltage eventually will bias both diodes forward and actual t_rr will be greatly impacted.

It is often said that the Schottky diode is a "majority carrier" semiconductor device. This means that if the semiconductor body is a doped n-type, only the n-type carriers (mobile electrons) play a significant role in the normal operation of the device. The majority carriers are quickly injected into the conduction band of the metal contact on the other side of the diode to become free moving electrons. Therefore, no slow random recombination of n and p-type carriers is involved, so that this diode can cease conduction faster than an ordinary p–n rectifier diode. This property, in turn, allows a smaller device area, which also makes for a faster transition. This is another reason why Schottky diodes are useful in switch-mode power converters: the high speed of the diode means that the circuit can operate at frequencies in the range 200 kHz to 2 MHz, allowing the use of small inductors and capacitors with greater efficiency than would be possible with other diode types. Small-area Schottky diodes are the heart of RF detectors and mixers, which often operate at frequencies up to 50 GHz.

## Limitations

The most evident limitations of Schottky diodes are their relatively low reverse voltage ratings, and their relatively high reverse leakage current. For silicon-metal Schottky diodes, the reverse voltage is typically 50 V or less. Some higher-voltage designs are available (200 V is considered a high reverse voltage). Reverse leakage current, since it increases with temperature, leads to a thermal instability issue. This often limits the useful reverse voltage to well below the actual rating.

While higher reverse voltages are achievable, they would present a higher forward voltage, comparable to other types of standard diodes. Such Schottky diodes would have no advantage unless great switching speed is required.

## Electrowetting

Electrowetting can be observed when a Schottky diode is formed using a droplet of liquid metal, e.g. mercury, in contact with a semiconductor, e.g. silicon. Depending on the doping type and density in the semiconductor, the droplet spreading depends on the magnitude and sign of the voltage applied to the mercury droplet. This effect has been termed 'Schottky electrowetting'.

_(Later sections of the original article were left out of this offline copy for length: Silicon carbide Schottky diode, Applications, Designation, Alternatives.)_
