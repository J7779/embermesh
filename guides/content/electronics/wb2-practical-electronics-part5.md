---
id: electronics/wb2-practical-electronics-part5
title: Practical Electronics (Wikibooks) (part 5 of 7: PCB Layout, Stepper Motors)
category: electronics
tags: [practical electronics, electronics, components, soldering, resistors, capacitors, transistors, circuits]
summary: Practical Electronics (Wikibooks), part 5 of 7: PCB Layout, Stepper Motors.
source: https://en.wikibooks.org/w/index.php?title=Practical_Electronics/PCB_Layout&oldid=4640445
source_title: Wikibooks: Practical Electronics
license: CC BY-SA 4.0
attribution: Wikibooks contributors, "Practical Electronics/PCB Layout" (revision 4640445, https://en.wikibooks.org/w/index.php?oldid=4640445 ); "Practical Electronics/Stepper Motors" (revision 2699278, https://en.wikibooks.org/w/index.php?oldid=2699278 ), Wikibooks, The Free Textbook Project. Converted to Markdown for the EmberMesh offline library; navigation templates, images, references and link lists omitted; pages combined into one guide. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-14
part_of: electronics/wb2-practical-electronics
order: 5
---

# Practical Electronics (Wikibooks) (part 5 of 7: PCB Layout, Stepper Motors)

_Wikibooks textbook pages (Wikibooks, CC BY-SA 4.0), converted to plain Markdown: navigation templates, images, references and link lists were removed; several pages are combined here._

## PCB Layout

The efficient laying out of traces on a PCB is a complex skill, and requires much patience. This task has been made vastly easier with the advent of readily available PCB layout software, but it is still challenging.

Usually an electronics or electrical engineer designs the circuit, and a layout specialist designs the PCB. PCB design is a specialized skill. There are numerous techniques and standards used to design a PCB that is easy to manufacture and yet small and inexpensive.

These are the usual steps in PCB layout for most customers that allow the customer maximum flexibility to make changes to embrace the normal engineering changes that happen during the design process. By utilizing this structured approach maximizes the engineering teams' efforts to design and build a successful product.

1. Schematic / Components design review.

2. Determine what footprints are required to be built, and build them incorporating 3D models.

3. Board outline definition process. DXF board outline file import is the most stable method.

4. Component placement.

5. Customer review of component placement.

6. Power and Ground Plane assignment.

7. Customer review of Power Plane and Ground Plane Assignments.

8. Fanout.

9. Critical net routing.

10. Customer review of critical net routing.

11. Non critical net routing.

12. Customer review.

13. Silkscreen adjustment and board part number assignment/placement.

14. Gerber, DXF, Step, IDF/LDF final file generation for final customer review and approval.

15. Final cleanup and any touch up required from customer feedback.

### Copper Thickness

The vast majority of PCBs are manufactured with "1 ounce copper" on the outer layers. If there are inner layers, they are almost always manufactured with "1/2 ounce copper".

The thickness of the copper layer on the PCB affects the behaviour of the circuit. PCB copper thickness is usually measured in *ounces per square foot*, frequently just called ounces. It can also be given in micrometres, inches or mils (thousandths of an inch). The measurements for commonly used thickness are given below.

| oz/ft² | μm | in | mil |
|---|---|---|---|
| 0.5 | 17.5 | 0.0007 | 0.7 |
| 0.75 | 25.5 | 0.0010 | 1.0 |
| 1 | 35 | 0.0014 | 1.4 |
| 2 | 71 | 0.0028 | 2.8 |
| 3 | 105 | 0.0042 | 4.2 |

As always, the thickness of a thin slab of metal with a given top surface area is always exactly
 thickness = mass over area × density

The area is "1 square foot" (144 square inches), the density of copper is 8.96 mg/mm³ = 5.18 ounce/(inch³),
and usually the mass is "1 ounce of copper", but occasionally 1/2, 2, 3, or 4 oz.

### Trace Width

Different widths of traces have different properties that could affect the operation of the circuit. For, example, a thin trace has a higher resistance than a thick one, and can therefore carry less current or will heat up more for the same current.

Due to the large number of tables and charts, this information is presented on different pages:

- For current capacities, see /Trace Current Capacity/.

Most manufacturers can manufacture a minimum trace width of 0.006 inch.
(Many can manufacture traces 0.004 inch wide).
Such minimum-size traces are more than adequate for most digital and analogue signals.

### Footprints or Geometries

The manufacturer of each part recommends a "footprint", a copper pattern for the part to be soldered to the PCB.

Sometimes the footprint also includes a drawing of the outline of the part.
Some people put that outline on the silk screen.
Other people make a "virtual outline" that shows up on the computer screen and on the assembly drawings, but not on the actual PCB.

Footprints for capacitors and batteries should include plus sign (+) polarity marking on the silk screen next to the positive pad.
(The positive-end-indicator stripe on the capacitor itself should be placed nearest that plus sign).

Footprints for LEDs and other diodes should have a polarity mark -- the "diode arrow symbol" (triangle + bar), or at least the bar, in silk-screen. The bar matches the cathode-end-indicator stripe on the diode itself.

Footprints for ICs should have a polarity mark "dot" or "1" near pin 1.
Most people give pin 1 a "squared-off" pad, and all other pins a "rounded pad".
Some people also like additional "10", "20", "30", etc., marks in silkscreen next to pin 10, pin 20, pin 30, etc.

The polarity mark must be visible after mounting the part so it can be seen after inspection.
(It's useless to put the polarity mark where no one can see it).
It's okay to put the polarity mark underneath a tubular-packaged diode, since people can look "around" the mounted diode and see the polarity mark.

### Holes

There are 3 general types of holes:

A **via** -- literally, a "way" to get from one layer of copper to another layer of copper.
The vias on a particular PCB should all be the same size.
Some people recommend 0.025" (0.6mm) diameter via holes, surrounded by a 0.0394" (1.0mm) diameter via copper pad, if at all possible.
Some very dense SMT boards require smaller vias.
Some manufacturers can handle 0.012" diameter via holes, surrounded by 0.024" diameter via pad.

Since there is no actual component put into a via, many PCBs are manufactured with "plugged" vias (vias completely filled with metal) and "tented" vias (vias completely covered with solder mask).

A **through-hole** -- many components (called "through-hole components") require a hole (a "through hole") for each pin.
The part manufacturer should specify a "footprint" including the location and size of each hole.
If there is no recommendation, common practice is

- round leads: add 6 mils to the nominal round lead diameter to get the recommended PCB hole diameter.
- Rectangular leads: find the lead diagonal (√(x^2 + y^2)). Then add 6 mils to get the recommended PCB hole diameter. ( 6 mils ≈ 0.15 mm )
- voltage regulators and other components in TO-220 packages typically fit in 0.040" (1 mm) holes. Some people prefer a "universal layout" that supports practically any 3-pin regulator.
- DIP package ICs and TO-95 typically fit in 0.031″ diameter holes (0.8 mm diameter).
- "The component lead - hole clearance should be 0.4 mm"

- "The optimum pad diameter for a through-hole component is twice its finished hole diameter."
- 0.1" pitch pin headers from many manufacturers are typically 0.025 inch (0.64 mm) square. Manufacturers typically recommend a hole 0.040 inch (1.00 mm) diameter. It should be noted that 0.025 inch square terminals can be used for AWG 30 wire-wrapping as well.

A **tooling hole or mounting holes** ...
A "tooling hole" -- used to temporarily attach the board to test fixtures, during assembly and test. They are almost always 0.125" in diameter, unplated, and placed in opposite corners. Two are required, but a third can prevent reverse installation of the PCB. It is possible to use these as mounting holes for the end-use of the PCB.

Many manufacturers publish their "standard hole sizes" (standard drill bit sizes minus the standard barrel thickness of through-hole metal plating).
The person running the drill press prefers to see standard sizes; otherwise they are forced to either
(a) pick the next-smaller standard drill bit -- then the physical part won't fit in the hole;
(b) pick the next-larger drill bit -- this pushes the through-hole metal plating out, possibly hitting some internal trace and rendering the entire board unusable; or
(c) reject the board as unmanufacturable.
To avoid all 3 problems, often PCB designers "round up" non-standard hole sizes to the next larger standard hole size -- i.e., often holes for DIP packages are rounded up to 0.9 mm diameter -- and then run the DRC check.
If the DRC finds any traces that are now "too close" to the bigger hole, the designer pushes those traces a safe distance away before sending the design to the manufacturer.

### Board Thickness and Layers

The vast majority of PCBs have an overall thickness of 1/16 inch (1.58 mm).
Some very dense SMT boards have an overall thickness of 1/32 inch (0.79 mm), which allows smaller via holes to be drilled, allowing denser packing.
Occasionally boards are made with an overall thickness of 3/32 inch (2.3 mm), which makes it more rigid (but requires bigger via holes).

Often some or all layers are almost entirely covered with a "copper pour" ("ground plane" or "power plane").
Such pours typically have a signal-to-pour clearance of 0.010 inch and clearance from the cut edge (perimeter of the board edge) to pour of 0.020 inch.

*(more layer stackup tips)*

Most PCBs have between one and twenty conductive layers laminated (glued) together in a sandwich with insulating plastic. PCBs with more than two layers help construct complex or dense circuits. They are not always used because they are more expensive, and the inner layers are more difficult to inspect and repair.

In more complex PCBs, two or more of the layers are dedicated to providing ground and power. These *ground planes* and *power planes* distribute power well. They also prevent radio waves from antennas unintentionally formed by tracks. These planes are rectangular sheets of foil that occupy entire layers (except for small holes to avoid unwanted connection to vias and through-hole components). They distribute electrical power and heat better than narrow traces. Sometimes solid metal PCBs with thin layers of insulation are used. The *power electronic substrate* carries away waste heat when air cooling is impossible.

Four-layer PCBs with a ground and power plane are often used in high-quality, but cost-conscious audio, avionics and medical electronics. Most consumer products have one or two layers.

If you want to use a BGA-packaged part, that one part typically dictates the number of layers the PCB must have.
With a few low-density BGA packages, it's possible to use a 2-layer PCB
Some BGA packages require at least 4-layer PCBs,
and many BGA packages force designers to use even more PCB layers.
For example, a BGA package with 7 or 8 rows of balls (from the center to the perimeter) generally a minimum of 6 layers to fan-out, including a power plane and a ground plane.
Some BGA packages with over 1000 pins and a pin-pitch of 0.8 mm require at least 8 layers and blind, buried, and micro-vias.

Often all the electronic parts of a system work fine with a 4-layer board or even a 2-layer board except for one part in a BGA package.
Rather than designing one big full-custom 10-layer board to hold everything,
many people prefer -- at least for the first prototype --
to partition the design into two PCBs -- or more.
One board that that includes that BGA part --
often a commercial off-the-shelf (COTS) board,
preferably one that includes as much as possible of the rest of the system --
and custom design a much simpler 2-layer or 4-layer board(s)
to support the remaining parts.
Often there is a more-or-less standardized interface --
carrier boards and stacking modules,
FPGA Mezzanine Card (FMC)
for I/O modules to connect to a FPGA;
etc.

### Trace clearance

The width and spacing of conductors (or "traces") on a PCB is very important. If the traces are too close, solder can short adjacent traces, and the PCB will be difficult to construct or repair. If too far apart, the PCB may be too large and expensive. When a PCB carries high frequencies, traces may need to be exact widths and lengths to control the characteristic impedance of the trace.

The "clearance" is the shortest distance through the air between two conductors.

For mains-powered information technology equipment, UL 60950-1 gives the minimum allowed PCB spacing.
With typical 120-230 VAC mains input, creepage between primary
and low-voltage secondary circuitry per UL/IEC 60950 should be
6.4 mm minimum.

The IPC recommends a clearance between tracks of 0.0254 mm/V on uncoated boards expected to ever be used above 10,000 ft, 0.005 mm/V on uncoated boards expected to always stay below 10,000 feet, and 0.00305 mm/V between traces under a solder mask.
Some safety standards require a minimum of 8 mm clearance between mains input (240 V) and signal tracks.

Other standards related to clearance and creepage include
ECMA 287, IEC 60664,
NEMA 1-111-1, NEMA 1-111-2,
etc.

Some designs cut the ground plane or the entire PCB in strategic locations to control the return paths of currents. The usual desire is to keep high voltages or frequencies away from sensitive portions of a circuit. The actual properties of the design are critical, because in some cases, cutting the ground plane makes the PCB into an antenna that radiates radio noise into nearby equipment.

### Remove less copper

Removing large areas of copper wastes etchant and can increase waste (although commercial fabricators reclaim the copper and regenerate the etchant). Also, a PCB etches more consistently and tends to resist warping if all regions have the same average ratio of copper to bare board. Therefore, designers may widen connectors, leave unconnected copper in place, or cover large areas of what would otherwise be bare board with arrays of small, electrically isolated copper diamonds or squares but these can lead to increased emi and emc problems unless grounded.

### Fiducial

Most PCBs have alignment marks (called fiducials) and tooling holes to align layers.

The preferred fiducial is a solid circle 1 mm diameter.
These permit the PCB to be mounted in equipment that automatically places and solders components. Some designs also have quality control patterns to measure soldering and etching processes. In some cases, the test patterns are on break-off tabs that can be removed before the PCB is installed.

### Via

Signal traces on different layers may be connected together through plated holes called *vias*. The via basic structure include a drilled hole which is electroplated with copper and pads which are circular copper elements encircling the plated hole on the copper layers. High-density PCBs may have *blind vias*, which are visible only on one surface, or *buried vias*, which are visible on neither, but these are considered a cost adder because they require multiple lamination stages. The most simple via is the multilayer via (sometimes called also PTV) which is drilled all the way through from one side of the board to the other. Typically, there is no cost adder for any number of reasonable via drills per PCB, the cost adder may come when requiring via with epoxy filling (VIPPO or via in pad) or when the drill is very small.

### Solder mask and silkscreen

A solder mask is a plastic layer that resists wetting by solder (the solder is said to "bead up"), and keeps islands of solder from running together. It also protects the outside conductors layers from abrasion and corrosion. Without the solder mask, the fibreglass-reinforced epoxy appears a translucent off-white. Solder masks are usually green, but they may be found in other colours.

A silkscreen legend on the top or bottom surface of the board provides readable information about component part numbers and placement. This aids in manufacturing and repair. To aid manual construction and repair, diodes, capacitors and integrated circuits are sometimes oriented in the same direction.

New technology allows for the component designators to be printed directly onto the board surface, saving time and money by doing away with silkscreens. This is sometimes done by a special inkjet printer. A similar process has experimentally produced solder masks.

### Basic guidelines

Radio transmitters and radio receivers are difficult to design. PCB designers working on them must minimize parasitic effects due to layout of components, or take them into account with a *general model* and use simulation software such as SPICE.

Fortunately, many practical circuits can be laid out using a much simpler *lumped element model*.

**PCB layout Basic guidelines**:
- it is often a good idea to have made a prototype circuit using point-to-point construction or wire wrap, as you will have solved certain basic issues to do with component selection: (e.g., should I use a 1/4 watt resistor here, or do I need 1/2 watt?)
- consider physical constraints on the assembled board's size and heat dissipation requirements; choose your heat sinks if needed.
- consider carefully the physical size of the components you are laying out; the circuit schematic doesn't tell you this. Equivalent components often have different packages.
- How do the components attach to the board? Are they surface mount components? or do they require holes, screws, washers, etc.?
- are there mechanical parts directly mounted to the board? For example, switches or variable resistors?
- How will the board mount in its container? What stresses (shock, strain, shear) will there be upon it and upon components?
- How will the board connect to its power source? What other connectors will be required (e.g., signal inputs, outputs)?
- use construction paper and a pencil and sketch the board in its actual size; or use component layout software that includes information about the component outlines.
- decide appropriate widths for each of the signal traces; this depends on the current each trace is expected to carry.
- decide whether you will have a single-layer board, 2-layer, or multi-layer based on the circuit complexity and fabrication costs.
- begin by placing component outlines, then by placing signal traces; leave a little room around each for tolerances.
- for a single layer board, spend more effort to avoid having traces cross each other; play with component placement or run traces underneath components; sometimes a jumper wire is needed.
- in 2-layer and multilayer boards simply run the traces on different layers, and use plated-through holes to jump from one layer to another.
- try to predict and avoid assembly errors: where there are multiple components of the same kind, or where pins have a polarity (e.g., electrolytic capacitors), try to place them in parallel and orient the positive pin in the same direction.
- If your PCB design software has a DRC (design rule check), use it.

**PCB layout guidelines** for RF circuits on a 2-layer or multilayer board:
- identify the critical parts of the circuit and lay them out first
- have one of the layers act as a continuous ground plane.
- if signal traces are constant width and height above the ground plane, and are properly terminated, then their characteristic impedance is more well-behaved and may be calculated.
- avoid sharp corners.
- keep signal traces and component leads as short as possible.
- inputs and outputs should be far apart, so that RF energy will not leak back from output to input. stages should line up, rather than snake around.
- decouple the RF parts of the circuit from the DC parts of the circuit.
- shield AF and IF components from RF components.
- high-speed communication (over 100 Mb/s per wire) generally requires point-to-point connections, rather than multi-point multi-drop bus connections. Communication over 622 Mb/s per wire requires even more specialized techniques and careful connector selection.

Many people have a checklist of things to review before sending the Gerber layout files to the PCB fabricator.

## Stepper Motors

**Stepper Motors** are devices that turn a shaft by a small set angle (usually between 1 and 5 degrees) at a time. This is done very precisely, and so they are very useful for application requiring motion that does not have any feedback to govern the motor speed. However, they cannot be simply driven by a DC or AC voltage like simpler motors; they need more complex circuitry to drive them.

For more information on stepper motors, please visit the "Stepper" page in the *Wikibook of Electric Motors and Generators.* For this book, we will just look at the basics. There are two kinds of stepper motor - **unipolar** and **bipolar**. Bipolar motors are the simplest, so we will look at those first.

### Bipolar Motors

Bipolar motors have two coils, each with a connection at each end, giving a total of four wires. It is possible to identify which wires are which, as the resistance between wires of different coils will be infinite. Generally, the connections are named in schematics, 1a and 1b being the two connections for one coil and 2a and 2b being for the other coil.

To drive a bipolar stepper motor by one step, the coils have to be energized in a particular sequence. Consider just one coil; if 1a is held high and 1b is held low, the coil is said to conduct forwards. If 1a is at ground and 1b is high, then it conducts backwards. To drive the stepper motor, the coils have to be switched from conducting forwards to conducting backwards alternately.

The input voltages to the four wires in order to drive the motor forward on step are shown in the table below. The coil polarities generated by this sequence are shown on the far right of the table.

| | Seq. | 1a | 1b | 2a | 2b | Coil 1 Polarity | Coil 2 Polarity |
|---|---|---|---|---|---|---|---|
| | 1 | + | - | + | - | Forwards | Forwards |
| | 2 | - | + | + | - | Backwards | Forwards |
| | 3 | - | + | - | + | Backwards | Backwards |
| | 4 | + | - | - | + | Forwards | Backwards |

There are many ways to produce this sequence, from using basic logic to incorporating the control into a micro-controller. Below is the circuit diagram for a motor controller made of simple logic components. Every time there is a low-to-high transition on the STEP input, the circuit advances the output on stage. The DIR input controls the direction of the step; if it is high, it steps one way, if it is low it steps the other way.

However, this circuit cannot drive a stepper motor by itself, as the logic outputs cannot supply nearly enough current. Some sort of amplification is needed. A dual H-Bridge arrangement can provide the necessary current and voltage reversal:

Recommended transistors for driving stepper motors are MOSFETs IRF510 or IRF530 (N-channel) and IRF9520 or IRF9530 (P-channel). These have built in clamping diodes, so the D1-D8 in the above circuit are unnecessary. If you are operating at high currents, it is wise to keep them for added protection. However any kind of power transistor (FET or BJT) should be suitable for this, as long as the current required to drive the motor does not exceed the rating of the transistor.

### Unipolar Motors

Unipolar motors have a more complex system of coil windings, but a simpler method of driving. Instead of having two connections, a unipolar motor coil has three - there is a centre tap on each coil. Unipolar motors have six leads therefore, in two groups of three. Sometimes there are just five, as the centre taps may be joined internally.

To identify unmarked leads, first find the two triplets of wires - there will be no connection at all between the triplets. Then, find the centre tap - this is the wire with equal resistance between it and the other two wires in the triplet. If the centre taps are joined together internally, this will be more difficult, but by trial and error, it should be possible. Usually, however, the connection can be worked out by looking carefully as the wires as they come out of the motor - they will often be physically grouped into triplets or connected to a small PCB with the connections laid out logically.

By holding this centre tap at a common voltage (probably ground) and switching the voltage used to power to motor from one end of the coil to the other, the direction of voltage flow can be reversed.

The windings in unipolar motors are **bifilar**, meaning there are two coils wound on top of each other - one connecting the "a" connector to the centre tap and one connecting the "b" connector to the centre tap. Because each half of the bifilar winding is as big as one coil in a bipolar motor, unipolar motors tend to be wider than bipolar motors.

The advantage of unipolar motors is that there is no need for H-bridge drivers because all that needs to be done is to apply a high voltage to one of the connectors when the coil is to be energised - the other end does not need an opposite treatment, it can just be disconnected rather than having to be connected to the opposite power rail.

The sequence of voltages applied to connectors 1a, 1b, 2a and 2b is the same as before, so the system of XOR gates and JK flip-flops (or PIC, etc) can be used for unipolar motors. The transistor driver circuit is simpler, however:

Any kind of power transistor (FET or BJT) should be suitable for this, as long as the current required to drive the motor does not exceed the rating of the transistor. Suggested transistors are IRF520, TIP31, TIP120, etc. Again, if the transistors have built in clamping diodes, the separate diodes are not needed, but will not do any damage if left in (suitable is any diode with voltage and forward current values similar to the transistor).

If need be, unipolar motors can have the centre taps ignored and be used as bipolar motors. This will result in less torque as the coils are twice as long and so have twice the resistance and half the current. Alternatives, ignore one of the end connections and use one half of the winding. This should provide exactly the same characteristics as the motor when used normally. However, care should be taken to insulate the dangling wire as it will act like an auto-transformer and double the voltage applied to the lower half-coil.
