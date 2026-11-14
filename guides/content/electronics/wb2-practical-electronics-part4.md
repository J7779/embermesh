---
id: electronics/wb2-practical-electronics-part4
title: Practical Electronics (Wikibooks) (part 4 of 7: Desoldering, Oscilloscopes, PCB Manufacture)
category: electronics
tags: [practical electronics, electronics, components, soldering, resistors, capacitors, transistors, circuits]
summary: Practical Electronics (Wikibooks), part 4 of 7: Desoldering, Oscilloscopes, PCB Manufacture.
source: https://en.wikibooks.org/w/index.php?title=Practical_Electronics/Desoldering&oldid=3672274
source_title: Wikibooks: Practical Electronics
license: CC BY-SA 4.0
attribution: Wikibooks contributors, "Practical Electronics/Desoldering" (revision 3672274, https://en.wikibooks.org/w/index.php?oldid=3672274 ); "Practical Electronics/Oscilloscopes" (revision 4374827, https://en.wikibooks.org/w/index.php?oldid=4374827 ); "Practical Electronics/PCB Manufacture" (revision 4528733, https://en.wikibooks.org/w/index.php?oldid=4528733 ), Wikibooks, The Free Textbook Project. Converted to Markdown for the EmberMesh offline library; navigation templates, images, references and link lists omitted; pages combined into one guide. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-14
part_of: electronics/wb2-practical-electronics
order: 4
---

# Practical Electronics (Wikibooks) (part 4 of 7: Desoldering, Oscilloscopes, PCB Manufacture)

_Wikibooks textbook pages (Wikibooks, CC BY-SA 4.0), converted to plain Markdown: navigation templates, images, references and link lists were removed; several pages are combined here._

## Desoldering

Desoldering is as much of a skill as soldering. The aim is to remove all the solder from a component and detach the component from the PCB without damaging the component or the PCB, either from excessive heat or from rough handling.

### Solder Braid

Also called solder wick, solder braid is a ribbon of extremely fine copper strands woven together. It is usually supplied on small reels of about 1.5m of braid and comes in a variety of widths. The braid is pressed against a molten soldered joint, and capillary action draws the solder out of the joint and into the braid. The portion of braid which contains solder is then cut off the reel and discarded. It is usually sold treated with a flux so that the solder is drawn up more effectively.

#### Using Solder Braid

- First, a short length (about 50mm or 2 inches) is drawn off the reel. Do not cut it off as this will reduce the usable portion of the braid, and will make it difficult to hold (the braid is made of an excellent thermal conductor, so will get very hot).
- Next the end of this section is placed on top of the solder joint requiring desoldering.
- The soldering iron is placed on top of the braid and very gentle pressure is applied.
- As the joint melts and the braid is pressed into the molten solder, the solder will flow up the braid, turning it a silvery colour.
 - If the solder braid stops pulling up solder but the joint has not been cleared, move on to the next unused bit of braid - it has become full of solder and can no longer draw it up.
- Once the solder has all been removed from the joint, withdraw the iron and the braid together. If the iron is withdrawn and the braid is left, the braid will become stuck to the pad that has just been desoldered.
- The pad should now be exposed and the component leg should be free to be removed. The pad will have a thin layer of solder left on it, turning it a bright silver colour. This will actually help with resoldering later on.
- Cut off the used portion of braid and discard - it cannot be reused.

**Do not melt the joint and stick the end of the braid in**. This will draw up very little solder, if any, and will serve only to overheat the joint, underlying pad and the component.

For best results, try to use a width of braid that matches the width of the joint. Failing that, 1.5mm is a good general-purpose width of braid to use.

### Solder Sucker

Solder suckers are devices that use an inrush of air to suck up molten solder from a joint. They can look either like a chunky pen or like a rubber bulb. To use them, prime the solder sucker by pushing down the plunger or squeezing the bulb, place the nozzle of the device next to the joint, then heat the solder. Once it is molten, release the plunger or bulb and the solder will be sucked into the device.

This technique is good for joints with a large amount of solder on them which would use an unnecessarily large length of solder braid. It generally does not remove enough solder to allow the component to be removed, but application of braid will clear out the remaining solder.

### Impact Method

This simply involves heating up the joint and then banging the PCB, hard, on the bench. The impact will send the solder in the joint flying off in a blob, leaving a reasonably clean pad. Beware that the blob may become stuck to the PCB further down, but due to the rapid cooling, it can easily be removed with a fingernail. The impact method only works with fairly large joints as a large thermal mass is required to keep the joint liquid until it is struck on the bench.

This method is generally not advised as it can cause unnoticed solder bridges on other parts of the PCB and could damage delicate components or the PCB. In the absence of other tools, however, it can be used to good effect.

### SMT components

#### Resistors and Capacitors

These are by far the easiest SMT components to desolder. First use solder braid to remove most of the solder from each end of the component. Then, heat each end alternately for about a second at a time until the component moves off its pads. It is wise to have a small screwdriver or pick ready to remove the component from the soldering iron in case the remaining solder causes it to stick there due to surface tension. This is quite common, as SMT components weigh very little.

#### ICs with two rows of pins

This method, called the "blob method," will only work with IC with a row of pins on two sides, for example SOICs, etc.

These are the next-easiest components to remove. Add solder to each row of pins until you have two long blobs of solder, one on each side of the IC. Now, melt each blob alternately, letting the soldering iron stay in the blob for just longer than it takes to melt. After a couple of alternations, the IC should slide off its pads. Once this happens, remove the iron and, if needed, clean up the leads of the IC and the PCB pads from which you just removed it with solder braid.

#### Large Components

Large components like Quad Flat Packs (QFPs) need more drastic measures to desolder them. Since they can have over 300 pins distributed around four sides, they cannot be desoldered by the "blob method" above, as each side will cool before the rest has been melted.

The only viable way of desoldering QFPs and the like is a heat gun, which blows air hot enough to melt the solder over the device, melting all the connections at once.

Hot air from a heat gun may burn the PCB and component of interest instantly and can also melt connectors and other components if you do not use caution. Shield adjacent components by draping or wrapping aluminium foil over them.

#### Reflow Oven

If a large circuit board needs to be completely desoldered for any reason, consider using a small toaster oven (not a microwave oven). Set the temperature to about 180°C to 200°C, place the PCB assembly in the oven and wait for the solder to melt. Then, quickly remove as many components as you can, or, if you are in a hurry, turn the board over and tap it sharply on the backside. Repeat as many times as needed. There is no guarantee that all the components or the circuit board will emerge undamaged by repeated heating, but it is a viable method, nonetheless.

**Do not use the same oven you use for food preparation. Most solders contain lead, a toxic heavy metal.**

#### Hot Plate

An electric hot plate is sometimes used in combination with the above techniques.

The hot plate set to the "pre-heat" temperature — a little below the melting point of the solder, around 100°C to 150°C (212°F to 302°F) — and allowed to stabilize before proceeding. The PCB assembly is then placed on the hot plate and warmed for a brief period. It then takes much less heat and time from the soldering iron to nudge the temperature above the melting point, using any of the above techniques

In the alternative, one can set the hot plate above the melting point of the solder, wait for *all* the solder to melt, then pull off each of the desired components using tweezers.

If you don't have a hot plate, you can try using a skillet on a gas or electric range, although temperature control may be more difficult. **Do not use the same utensils you use for food preparation to avoid lead poisoning.**

## Oscilloscopes

Commonly termed 'scopes, are a vital piece of equipment when designing and making high frequency analogue circuits (i.e. where you can't see the signal changing using a logic probe, etc), as well as being very useful in many other cases.

### What is an Oscilloscope?

An oscilloscope is a device that draws a graph of a voltage over time, or a graph of the relationship between two different voltages. This information is given as a 'trace' on a screen. There are three main kinds of oscilloscopes: Cathode-Ray, Digital and Computer-based.

- Cathode-ray oscilloscopes (CROs) have an which emits a beam of electrons (historically called "cathode rays", hence the name) which is deflected according to the signal being measured. The trace is produced by the electrons striking a screen, which glows green where they hit.

- Digital models take the input signal and convert it into a digital signal, which is then analysed and used to plot an image on an LCD screen. Depending on the model, the trace may be monochrome or it may be in colour. The advantage of these is that a trace can be saved and displayed alongside others for comparison or calibration.

- Computer-based oscilloscopes are becoming more popular. Software is installed on a computer that can analyse a signal after some processing in an external analogue-to-digital converter. The advantage of this is that the computer can be used to supply the necessary processing for dealing with the signal, and the trace can be saved and exported.

For more detailed information about the specific features of the different kinds of oscilloscope, please see the Wikipedia Article.

### Using an Oscilloscope

#### Terminology

There are many hundreds of models of oscilloscopes, and each may be somewhat unique in its form and function. They can be differentiated not just by the types listed above, but also by other features.

The number of **channels** on a scope is the number of separate signals it can deal with. Simple oscilloscopes have only one channel, while most scopes are **dual-channel** - they can handle two signals at once. Advanced scopes can have many more channels.

**Bandwidth** is the "time-resolution" of the oscilloscope. The higher the bandwidth, the more detail the scope can give on a signal. Bandwidth is measured in Hertz and is defined as the maximum frequency at which the scope can sample a sinusoidal signal before the trace falls below 0.707 (√2/2) the value of the actual signal. It is a recommended practice to use an oscilloscope with a bandwidth at least 5 times that of the maximum frequency in the circuit. Using a scope with insufficient bandwidth can lead to missed details or distorted signals.

The **rise-time** of a scope measures the fastest time a signal can change from high to low or low to high and still be resolved by the scope. This is related to bandwidth by an inversely proportional relationship, so if you only know the rise time of your circuit, divide by five and you have the minimum recommended rise time of the scope. Rise-times for common electronics you will come into contact with are long enough for almost any low-bandwidth scope to suffice, down to about 10MHz (which is pretty much the lower limit anyway). Some scopes can capture rise-times of tens of picoseconds, have bandwidths of 20GHz or higher and cost over £100,000.

#### Controls

Below is a diagram of a dual-channel CRO of the kind you will most likely use. The following sections will explain the controls of such an oscilloscope.

##### Display

**A** is the display. This can be a phosphor screen or an LCD, and is usually about 100 mm corner to corner.

**B** shows the 'trace'. This is the line drawn by the scope to represent the signal. On a CRO, this line is created by a bright dot moving across the screen at high speed. On a digital scope, the line is drawn on the LCD like a graphical calculator.

The screen is overlaid with a grid of horizontal (**C**) and vertical (**D**) lines, called the 'graticule', which divides the screen into squares, called 'major divisions'. The graticule is usually 10 major divisions wide and 8 tall.

The central horizontal and vertical lines (**E**) are usually thicker than the others and are divided into 'minor divisions', usually five per major division. When we talk about "divisions" in later sections, we will always mean the major divisions - the minor divisions are just to aid measuring.

There are also special horizontal lines labeled "0" (2.5 divisions below the centre) and "100" (2.5 divisions above it). The "10" and "90" lines have tick marks like the central axes. These four horizontal lines are guides for scaling the signal for rise-time measurement. This will be discussed later.

##### Power, Calibration and Display Controls

**1** is the Power On/Off Button. **2** is the Power Indicator which lights when the oscilloscope is on. This may be an LED in newer scopes or a neon tube in older scopes.

**3** is the **trace rotation** (TR) control. This sets the inclination of a flat signal relative to the graticule. This is usually a Trimpot and needs to be set using a flat-bladed screwdriver. Once set, this control should retain its position and will rarely need adjusting.

**4** is the **intensity** of the trace. Turning this up increases the brightness of the trace, and turning it down makes it dimmer. An overly bright trace can damage the phosphor of the screen if the dot is moving too slowly.

The trace can get fuzzy if the electron beam is not focused correctly. The **focus** control (**5**) sets this. Most scopes can focus the beam to form a trace about 1mm wide.

**6** is the calibration point. This gives a steady square wave at a set frequency and voltage, allowing the scaling of the trace to be set accurately. Sometimes, more than one frequency and voltage is available to give a more representative calibration. The standard calibration signal is between 0V and 2V at 1KHz.

##### Vertical Axis Controls

When plotting a signal against time (the standard use for a scope), the vertical axis represents voltage. Most controls for the vertical axis are duplicated for each channel to give independent control over each signal.

**7** controls the position of the trace. It can be adjusted to set the voltage relative to a ground, or it can be adjusted to separate the two signals - perhaps the first channel in the top half of the screen and the second channel in the bottom.

**8** inverts the relevant channel. That is, the negative voltage is displayed, and the trace is upside-down.

**9** is the vertical scale control, often called the volts/div. control. This sets the height of the trace. It operates in discrete steps.

**10** is a variable height control. It can adjust the height of the trace up to the next set increment on the volts/div. control. When set to CAL, the height is as stated on the volts/div. control.

**11** is the **AC/DC toggle**. When set to AC, any DC component of the voltage is filtered out by switching a capacitor in series with the input signal, leaving just an AC voltage. This is useful when the DC component swamps the AC component, making it either too small to see or driving it off the top of the screen. When set to DC, the signal is displayed as is.

**12** is the **GND toggle**. By selecting this, the input signal is ignored, and the trace shows 0V. This can be useful to measure a voltage or to eliminate one of the traces from the display.

**13** is the Channel 1 signal input and **14** is the Channel 2 input. This is where the oscilloscope's probe is plugged in.

Each channel has a copy of most of these controls (except chop/alt, which applies to all channels). The way the channels are combined is set using **15**, which is usually a sliding switch. When set to **CH. 1**, only the trace from Channel 1 is displayed, and likewise for **CH. 2**. When **DUAL** is selected, the traces are shown side by side. This is when the chop/alt control applies. **ADD** shows the sum of the two traces as one trace. By inverting the traces, one can be subtracted from the other. This can be seen in the illustration below. This shows a square wave on one channel and a sinusoidal wave on the other. On the left, the scope is set to "dual", and the two traces are shown side by side. On the right, the scope is set to "add", and the trace is the sum of the two signals.

##### Horizontal Axis Controls

When operating in the normal voltage vs. time mode, this axis represents time. The primary control is the **time base** selector, **19**. The time base is the length of time displayed per major horizontal division on the screen. This ranges from about 0.1 milliseconds to about 1 second (or more on digital scopes).

The position of the trace from side to side is controlled by **17**. This is useful if part of the trace is off the edge of the screen but you don't want to change the time base.

The **×10 MAG** control, **16**, is a very useful control if you want to quickly zoom in on a feature without changing the timebase and losing your settings. This button magnifies the central area of the trace by a factor of 10 in the horizontal direction (but leaves the voltage height unchanged).

**18** toggles the mode between the usual voltage vs. time format and the **XY** mode. This continuously plots the voltage on Channel 1 along the horizontal axis against the voltage on Channel 2 (the vertical axis). This can be extremely useful to analyse frequency or phase relationships. This is a complex topic and will be covered in its own section later in the module.

**20** and **21** act in much the same way as 10 does on the vertical axis. This diagram shows it to be slightly different from the vertical control. To select a non-standard timebase, press 20, and adjust 20 until the correct setting is obtained. To return to a calibrated time base, press 20 again. Sometimes these controls are the same style as 10, sometimes the vertical controls are like these.

**22** is the **GND** terminal of the scope. This is used to set a "datum" voltage against which to measure the voltages on the input channels. Be careful when using isolated mains voltage circuits, as the "ground" is sometimes floating at mains voltage, and can short to the real ground, causing injury or death.

**23** toggles between **chop-mode** and **alt-mode**. Chop-mode means that when the scope is drawing two signals side by side it alternates rapidly between the two over the course of passing across the screen. This action is called **chopping**. Alt-mode alternates at the end of each pass and can appear to flicker at slow speeds.

##### The trigger level (25) and slope controls (31) provide the basic trigger point definition and determine how a waveform is displayed.

The trigger circuit acts as a comparator. You select the slope and voltage level on one input of the comparator. When the trigger signal on the other comparator input matches your settings, the oscilloscope generates a trigger.

The slope control determines whether the trigger point is on the rising or the falling edge of a signal. A rising edge is a positive slope and a falling edge is a negative slope. The level control determines where on the edge the trigger point occurs.

##### 26 Trigger Modes

The trigger mode determines whether or not the oscilloscope draws a waveform based on a signal condition. Common trigger modes include normal and auto:

- In normal mode the oscilloscope only sweeps if the input signal reaches the set trigger point. Otherwise, the screen is blank (on an analog oscilloscope) or frozen (on a digital oscilloscope) on the last acquired waveform. Normal mode can be disorienting since you may not see the signal at first if the level control is not adjusted correctly.
- Auto mode causes the oscilloscope to sweep, even without a trigger. If no signal is present, a timer in the oscilloscope triggers the sweep. This ensures that the display will not disappear if the signal does not cause a trigger.

##### 27 Pattern Lock Triggering

Pattern lock triggering adds a new dimension to NRZ serial pattern triggering by enabling the oscilloscope to take synchronized acquisitions of a long serial test pattern with outstanding time base accuracy.

##### 28 Trigger Coupling

Just as you can select either AC or DC coupling for the vertical system, you can choose the kind of coupling for the trigger signal. Besides AC and DC coupling, your oscilloscope may also have high frequency rejection, low frequency rejection, and noise rejection trigger coupling. These special settings are useful for eliminating noise from the trigger signal to prevent false triggering.

##### 29 Trigger Sources

The oscilloscope does not necessarily need to trigger on the signal being displayed. Several sources can trigger the sweep:

- Any input channel
- An external source other than the signal applied to an input channel
- The power source signal
- A signal internally defined by the oscilloscope, from one or more input channels.

## PCB Manufacture

The circuit board provides a reliable contact between components (even in huge circuits), looks nicely and is quite cheap. We can design them for particular circuit or create a universal circuit, where we deliberately place the copper areas (usually as a grid) and then solder the components on the copper side without drilling.

For reasonably small circuits it is simpliest to draw the connections on PCB with a felt pen and etch the rest. There is also the possibility of making the template with computer, more suitable for large PCBs or serial production.

#### Needed material

- Cuprextite (laminate board with copper)
- New felt pen
- Hobby drill (1 and 1,5mm)
- Some dish for etching (not from the kitchen!)
- Roll of a toilet paper for fast wiping the etching solution from anything
- Iron chloride

#### Design of the schematic

Draw a diagram on paper and think about it so that the path of least exceeded (wire jumper doing the work).

We will prepare all components that we use. Then chart redraw soft pencil (XWarning, graphite conductive connection may be) on top (laminated) side kuprextitu, we highlight dots for future hole for the legs. Here, we XWarning to ensure that all parts fit and feet to fit into your joints.

#### Drilling

The board of our PCB kuprextitu separated from the rest. Usually it is best to re-laminate odlamovacím incised with a knife under the sheet metal fence (with pressure) and carefully break off. You can also carefully odbrušovat laminate 1.5 mm drill.

Prortáme all the holes in the boards. For most components are 1 mm drill bit suitable for tlustonohých 1.5 to 2 mm. Provrtáním we transfer perfectly inverted image of the holes as shown on the bottom (měděnpu) party.
Warning When drilling is appropriate kuprextit substantiate something that we have neprovrtali fingers or table.

Coupling of the bottom (copper) Thoroughly clean the steel wool / sponge, a sharp edge will be deducted from burrs after drilling.

#### Drawing the template

Draw connections between the holes as shown. Around each hole (if possible) leave 1 mm edge to give good solder. We draw carefully and concisely.

We try to cover as much surface. Thus it is not a slender connections between holes, but on the contrary, etching narrow gaps between the copper areas. It is important for the success of the etching and the etching solution for saving. Zabarvíme areas that do not lead nowhere. For a slim gap between the joints can tint the entire surface, and then carved a groove into the paint with a sharp object by the ruler.

#### Etching

Top plate is drawn with a template placed on the surface and let go due to surface tension. When installation is hit first by one edge and then slowly sklopíme the surface to sink or board, or bubbles are left in it. It should be below the depth of 5 mm to ensure complete etching.

The process of etching (for well-drawn plate and the normal concentration of FeCl3) takes about 15-20 minutes. During it's possible to lift the plate and look at the etching process, but very carefully - when handling the paint flakes off easily. Preview Therefore we commit as little as possible and the board should not yet dry from the etching solution.

After doleptání solution pour off back (or to another bottle), it can be reused. Rinse the PCB.

#### Final enhancements

Before the solder surface can be cleaned with alcohol or paint, steel wool, or in advance Can pokalafunovat and contacts (especially to delicate heat and power in components). Errors can scramble into the copper wire together, or vice versa. Now we can join components fitted.
Note the PCB sample is a control circuit and a source of rectangular signal for high voltage generator (polomostového connection with two FET transistors IRFP460). The generator generated (> 6000 VDC,> 10 mA?), but was tuned to the wrong frequency at which the transformer started to resonate (in the 3rd harmonic square wave), which led to early termination transistors. After their exchange, and careful tuning of Resonant frequency now works consistently.

#### PCB Prototype

Different manufacturers have different specifications to choose. Some companies offer up to 100 sqcm of PCB starting at $13. PCB specs start out with 6 mil spacing, 12 mil min. hole size, 1 oz copper, 2 layers, and 4-day lead time (plus 3-4 day shipping). Tighter spacing is available for tighter spacing down to 4 mil spacing. Small minimum drills are also available for additional charges with drills going down to 8 mils. Up to 10 layer boards, multiple colder mask colors, copper thicknesses, and other advanced options are also available.
