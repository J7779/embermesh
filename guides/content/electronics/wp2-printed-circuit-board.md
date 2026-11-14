---
id: electronics/wp2-printed-circuit-board
title: Printed circuit board
category: electronics
tags: [printed circuit board, pcb, printed wiring board, pwb, electronics]
summary: A printed circuit board (PCB), also known as a printed wiring board (PWB), is a laminated sandwich structure of conductive and insulating layers, each with a pattern of traces, planes and other features (similar to wires on a flat surface) etched from one or more sheet layers of copper…
source: https://en.wikipedia.org/w/index.php?title=Printed_circuit_board&oldid=1383953788
source_title: Wikipedia: Printed circuit board
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Printed circuit board", Wikipedia, The Free Encyclopedia (revision 1383953788), https://en.wikipedia.org/w/index.php?title=Printed_circuit_board&oldid=1383953788 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Printed circuit board

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **printed circuit board** (**PCB**), also known as a **printed wiring board** (**PWB**), is a laminated sandwich structure of conductive and insulating layers, each with a pattern of traces, planes and other features (similar to wires on a flat surface) etched from one or more sheet layers of copper laminated onto or between sheet layers of a non-conductive substrate. PCBs are used to connect or "wire" components to one another in an electronic circuit. Electrical components may be fixed to conductive pads on the outer layers, generally by soldering, which both electrically connects and mechanically fastens the components to the board. Another manufacturing process adds vias, metal-lined drilled holes that enable electrical interconnections between conductive layers, to boards with more than a single side.

Printed circuit boards are used in nearly all electronic products today. Alternatives to PCBs include wire wrap and point-to-point construction, both once popular but now rarely used. PCBs require additional design effort to lay out the circuit, but manufacturing and assembly can be automated. Electronic design automation software is available to do much of the work of layout. Mass-producing circuits with PCBs is cheaper and faster than with other wiring methods, as components are mounted and wired in one operation. Large numbers of PCBs can be fabricated at the same time, and the layout has to be done only once. PCBs can also be made manually in small quantities, with reduced benefits.

PCBs can be single-sided (one copper layer), double-sided (two copper layers on both sides of one substrate layer), or multi-layer (stacked layers of substrate with copper plating sandwiched between each and on the outside layers). Multi-layer PCBs provide much higher component density, because circuit traces on the inner layers would otherwise take up surface space between components. The rise in popularity of multilayer PCBs with more than two, and especially with more than four, copper planes was concurrent with the adoption of surface-mount technology. However, multilayer PCBs make repair, analysis, and field modification of circuits much more difficult and usually impractical.

The world market for bare PCBs exceeded US$60.2 billion in 2014, and was estimated at $80.33 billion in 2024, forecast to be $96.57 billion for 2029, growing at 4.87% per annum.

## Compositions

A basic PCB consists of a flat sheet of insulating material and a layer of copper foil, laminated to the substrate. Chemical etching divides the copper into separate conducting lines called tracks or *circuit traces*, pads for connections, vias to pass connections between layers of copper, and features such as solid conductive areas for electromagnetic shielding or other purposes. The tracks function as wires fixed in place, and are insulated from each other by air and the board substrate material. The surface of a PCB may have a coating that protects the copper from corrosion and reduces the chances of solder shorts between traces or undesired electrical contact with stray bare wires. For its function in helping to prevent solder shorts, the coating is called solder resist or solder mask.

The pattern to be etched into each copper layer of a PCB is called the "artwork". The etching is usually done using photoresist which is coated onto the PCB, then exposed to light projected in the pattern of the artwork. The resist material protects the copper from dissolution into the etching solution. The etched board is then cleaned. A PCB design can be mass-reproduced in a way similar to the way photographs can be mass-duplicated from film negatives using a photographic printer.

FR-4 glass epoxy is the most common insulating substrate. Another substrate material is cotton paper impregnated with phenolic resin, often tan or brown.

A PCB may be printed with a legend identifying the components, test points, or identifying text. Originally, silkscreen printing was used for this purpose, but today other, finer quality printing methods are usually used. Normally the legend does not affect the function of a PCBA.

### Layers

A printed circuit board can have multiple layers of copper which almost always are arranged in pairs. The number of layers and the interconnection designed between them (vias, PTHs) provide a general estimate of the board complexity. Using more layers allow for more routing options and better control of signal integrity, but are also time-consuming and costly to manufacture. Likewise, selection of the vias for the board also allow fine tuning of the board size, escaping of signals off complex ICs, routing, and long term reliability, but are tightly coupled with production complexity and cost.

One of the simplest boards to produce is the two-layer board. It has copper on both sides that are referred to as external layers; multi layer boards sandwich additional internal layers of copper and insulation. After two-layer PCBs, the next step up is the four-layer. The four layer board adds significantly more routing options in the internal layers as compared to the two layer board, and often some portion of the internal layers is used as ground plane or power plane, to achieve better signal integrity, higher signaling frequencies, lower EMI, and better power supply decoupling.

In multi-layer boards, the layers of material are laminated together in an alternating sandwich: copper, substrate, copper, substrate, copper, etc.; each plane of copper is etched, and any internal vias (that will not extend to both outer surfaces of the finished multilayer board) are plated-through, before the layers are laminated together. Only the outer layers need be coated; the inner copper layers are protected by the adjacent substrate layers.

### Component mounting

"Through hole" components are mounted by their wire leads passing through the board and soldered to traces on the other side. "Surface mount" components are attached by their leads to copper traces on the same side of the board. A board may use both methods for mounting components. PCBs with only through-hole mounted components are now uncommon. Surface mounting is used for transistors, diodes, IC chips, resistors, and capacitors. Through-hole mounting may be used for some large components such as electrolytic capacitors and connectors.

The first PCBs used through-hole technology, mounting electronic components by inserting leads through holes on one side of the board and soldering onto copper traces on the other side. Boards may be single-sided, with an unplated component side, or more compact double-sided boards, with components soldered on both sides. Horizontal installation of through-hole parts with two axial leads (such as resistors, capacitors, and diodes) is done by bending the leads 90 degrees in the same direction, inserting the part in the board (often bending leads located on the back of the board in opposite directions to improve the part's mechanical strength), soldering the leads, and trimming off the ends. Leads may be soldered either manually or by a wave soldering machine.

Surface-mount technology emerged in the 1960s, gained momentum in the early 1980s, and became widely used by the mid-1990s. Components were mechanically redesigned to have small metal tabs or end caps that could be soldered directly onto the PCB surface, instead of wire leads to pass through holes. Components became much smaller and component placement on both sides of the board became more common than with through-hole mounting, allowing much smaller PCB assemblies with much higher circuit densities. Surface mounting lends itself well to a high degree of automation, reducing labor costs and greatly increasing production rates compared with through-hole circuit boards. Components can be supplied mounted on carrier tapes. Surface mount components can be about one-quarter to one-tenth of the size and weight of through-hole components, and passive components much cheaper. However, prices of semiconductor surface mount devices (SMDs) are determined more by the chip itself than the package, with little price advantage over larger packages, and some wire-ended components, such as 1N4148 small-signal switch diodes, are actually significantly cheaper than SMD equivalents.

### Electrical properties

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Compositions, Materials, Types, Uses, Repair.)_
