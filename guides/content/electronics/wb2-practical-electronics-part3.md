---
id: electronics/wb2-practical-electronics-part3
title: Practical Electronics (Wikibooks) (part 3 of 7: Astables, Astables: 555, Crowbar circuit…)
category: electronics
tags: [practical electronics, electronics, components, soldering, resistors, capacitors, transistors, circuits]
summary: Practical Electronics (Wikibooks), part 3 of 7: Astables, Astables: 555, Crowbar circuit, Parallel RC, Soldering.
source: https://en.wikibooks.org/w/index.php?title=Practical_Electronics/Astables&oldid=4093979
source_title: Wikibooks: Practical Electronics
license: CC BY-SA 4.0
attribution: Wikibooks contributors, "Practical Electronics/Astables" (revision 4093979, https://en.wikibooks.org/w/index.php?oldid=4093979 ); "Practical Electronics/Astables/555" (revision 4093982, https://en.wikibooks.org/w/index.php?oldid=4093982 ); "Practical Electronics/Crowbar circuit" (revision 3354228, https://en.wikibooks.org/w/index.php?oldid=3354228 ); "Practical Electronics/Parallel RC" (revision 3559950, https://en.wikibooks.org/w/index.php?oldid=3559950 ); "Practical Electronics/Soldering" (revision 4034721, https://en.wikibooks.org/w/index.php?oldid=4034721 ), Wikibooks, The Free Textbook Project. Converted to Markdown for the EmberMesh offline library; navigation templates, images, references and link lists omitted; pages combined into one guide. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-14
part_of: electronics/wb2-practical-electronics
order: 3
---

# Practical Electronics (Wikibooks) (part 3 of 7: Astables, Astables: 555, Crowbar circuit…)

_Wikibooks textbook pages (Wikibooks, CC BY-SA 4.0), converted to plain Markdown: navigation templates, images, references and link lists were removed; several pages are combined here._

## Astables

The 555 timer IC provides a very versatile, effective, and easy-to-use astable mode. By changing component values, one can get frequencies from a maximum of about 300KHz to as low as limited by the leakage of the capacitors used in the circuit. It can also have the mark-space ratio altered almost limitlessly.

### Basic Circuit

The basic 555 astable circuit is set up like this.

- At higher frequencies and loads, it is advisable to put a 100nF capacitor between Pin 5 and 0V.

The frequency and mark-space ratio are defined by the values of R1, R2 and C1 only. For a roughly even (1:1) mark-spce ratio, R1 is usually set at 1K, and the other resistor (R2) and capacitor are changed to alter the frequency. If better control over the mark-space ratio is required, use the next circuit.

#### Design equations

 f=(1.44)/(( R_1 + 2R_2 )× C).

 High time=0.69 × ( R_1 + R_2 ) × C
 Low time=0.69 × R_2 × C

 Mark-space ratio=(R_1+R_2)/(R_2)

There is an online calculator here that will give the frequency, high time and low time for a given set of components.

### Variable mark-space ratio

This variation allows much easier control of the mark-space ratio without too much impact on the frequency. The only difference is a diode between Pins 6 and 7, with the cathode at Pin 6. The effect of this is to shunt resistor R2 when charging C1. This means that C1 charges though R1 only. This removes R2 from the "high-time" equation, and make calculating the mark-space ratio easier and prevents the frequency from being altered excessively by changes in the mark-space ratio ( although this still happens to a certain extent).

#### Design equations

 f=(1.44)/(( R_1 + R_2 )× C).

 *This is different to the design equation for the standard 555 Astable. (R₂ instead of 2R₂)*

 High time=0.69 × R_1 × C
 Low time=0.69 × R_2 × C

 Mark-space ratio=(R_1)/(R_2)

### Mini-astable

This astable uses fewer components than the basic astable, and requires only the IC, one resistor and one capacitor, plus the Pin 5 capacitor for final versions.

This astable has a MSR of approximately 1:1, but this tends to increase under heavy current load, as the output does not swing fully to the power supply. This astable is not a reliable source of frequency, and should never be used where an accurate, stable clock is needed.

#### Design equations

 f=(0.72)/(R_1× C).

 High time=Low time=0.69 × R_1 × C

### Pin 4 Enable

Pin 4 is normally held high, and in this state it does nothing. However, when Pin 4 is brought low, the 555 is disabled, and the output is held low. This can be used to turn an astable on when desired. An example of this is the double-astable bleeper, which uses the output from one astable to enable another, faster astable.

Pin 8 should **not** be used as the enable pin, as this will draw all the current needed to run the device from the triggering circuit.

### Other astable circuits

- Frequency Modulation
- Diminishing Frequency

### Units of Design Equations

The units that the components values and frequency are measure in are the standard base units: ohms, farads and hertz. This are often inconvenient, as the value for componets are usually several orders of magnitude away from the base unit. It is possible to use more convenient units to calculate the frequency, and the common combinations are given below:

| Resistance | Capacitance | Period | Frequency |
|---|---|---|---|
| Ω | F | s | Hz |
| MΩ | μF | s | Hz |
| kΩ | μF | ms | kHz |

## Astables: 555

The 555 timer IC provides a very versatile, effective, and easy-to-use astable mode. By changing component values, one can get frequencies from a maximum of about 300KHz to as low as limited by the leakage of the capacitors used in the circuit. It can also have the mark-space ratio altered almost limitlessly.

### Basic Circuit

The basic 555 astable circuit is set up like this.

- At higher frequencies and loads, it is advisable to put a 100nF capacitor between Pin 5 and 0V.

The frequency and mark-space ratio are defined by the values of R1, R2 and C1 only. For a roughly even (1:1) mark-spce ratio, R1 is usually set at 1K, and the other resistor (R2) and capacitor are changed to alter the frequency. If better control over the mark-space ratio is required, use the next circuit.

#### Design equations

 f=(1.44)/(( R_1 + 2R_2 )× C).

 High time=0.69 × ( R_1 + R_2 ) × C
 Low time=0.69 × R_2 × C

 Mark-space ratio=(R_1+R_2)/(R_2)

### Variable duty cycle

This variation allow much easier control of the duty cycle without too much impact on the frequency. The only difference is a diode between Pins 6 and 7, with the cathode at Pin 6. The effect of this is to shunt resistor R2 when charging C1. This means that C1 charges though R1 only. This removes R2 from the "high-time" equation, and make calculating the duty cycle easier and prevents the frequency from being altered excessively by changes in the duty cycle ( although this still happens to a certain extent).

#### Design equations

 f=(1.44)/(( R_1 + R_2 )× C).

 *This is different to the design equation for the standard 555 Astable. (R₂ instead of 2R₂)*

 High time=0.69 × R_1 × C
 Low time=0.69 × R_2 × C

 mark-space ratio=(R_1)/(R_2)

### Mini-astable

This astable uses fewer components than the basic astable, and requires only the IC, one resistor and one capacitor, plus the Pin 5 capacitor for final versions.

This astable has a MSR of approximately 1:1, but this tends to increase under heavy current load, as the output does not swing fully to the power supply. This astable is not a reliable source of frequency, and should never be used where an accurate, stable clock is needed.

#### Design equations

 f=(0.72)/(R_1× C).

 High time=Low time=0.69 × R_1 × C

### Pin 4 Enable

Pin 4 is normally held high, and in this state it does nothing. However, when Pin 4 is brought low, the 555 is disabled, and the output is held low. This can be used to turn an astable on when desired. An example of this is the double-astable bleeper, which uses the output from one astable to enable another, faster astable.

Pin 8 should **not** be used as the enable pin, as this will draw all the current needed to run the device from the triggering circuit.

### Other astable circuits

- Frequency Modulation
- Diminishing Frequency

### Units of Design Equations

The units that the components values and frequency are measure in are the standard base units: ohms, farads and hertz. This are often inconvenient, as the value for componets are usually several orders of magnitude away from the base unit. It is possible to use more convenient units to calculate the frequency, and the common combinations are given below:

| Resistance | Capacitance | Period | Frequency |
|---|---|---|---|
| Ω | F | s | Hz |
| MΩ | μF | s | Hz |
| kΩ | μF | ms | kHz |

## Crowbar circuit

A **crowbar circuit** is a method of protecting a circuit against high voltages (overvoltage) in the event of a power supply malfunction or power surge. This is especially useful in a device using TTL components as these are very sensitive to overvoltage. However, there are many other devices which can be damaged by overvoltage.

A crowbar circuit works by sensing a voltage that is above a certain threshold and shorting out the power supply. This causes a voltage drop in the rest of the circuit and current surge through the power supply that will trip a circuit-breaker or blow a fuse. The circuit must have a fuse or circuit-breaker, as without it the power supply or the crowbar circuit will be damaged. Crowbar circuits are so named because their activation is similar in effect to dropping a crowbar across bus bars (heavy duty power supply lines).

A typical crowbar circuit is as follows:

This crowbar circuit has an 8V power supply, and triggers at around 10v. To change the power supply rating, the zener diode, ZD1, needs to be changed to reflect the new trigger voltage. It should be about 1V higher than the nominal supply voltage.

- Fuse F1 is the fuse that blows if the current drawn by the circuit significantly exceeds 250mA. This can be increased as needed, but make sure the thyristor Q1 has a higher current rating than the fuse. Also make sure the power supply can supply enough current to blow the fuse, as a current limited supply may fail to, and could overheat if the circuit is triggered.

- Capacitor C1's purpose is to reduce small, harmless, voltage spikes or noise which may trigger the circuit.

- ZD1 is the Zener diode which detects the overvoltage condition. At the threshhold voltage (here 9.1V), it starts to conduct, and a further increase in supply voltage raises the voltage on R1 and Q1 gate.

- R1 is a pull-down resistor which holds the gate of thyristor Q1 low when ZD1 is not conducting.

- C2 is a snubber capacitor to prevent the thyristor being triggered by accident on being powered up. It must have a high enough value to filter transients, but low enough to avoid delaying the operation of Q1 too much.

- Q1 is the thyristor which provides a short-circuit between the power rails, blowing the fuse. When the gate voltage is pulled above its trigger voltage, typically in the 0.6 to 1v range, it suddenly switches to conducting. The circuit trigger voltage is the sum of this and ZD1's theshhold voltage.

- SD1 is a Schottky diode to prevent kickback from the main circuit from triggering the crowbar circuit. Normal diodes can be used, but they have a larger voltage drop than Schottky diodes. This precaution is rarely needed.

## Parallel RC

### Parallel RC

#### Circuit Impedance

1/Z = (1)/(Z_R) + (1)/(Z_C)
1/Z = 1/R + jω C = (jω CR + 1)/(R)
Z = R (1)/(jω CR + 1)

#### Circuit Response

I = I_R + I_C
I = V/R + C dV/dt

V = (IR - RC dV/dt)

### Parallel RL

\

#### Circuit Impedance

1/Z = (1)/(Z_R) + (1)/(Z_L)
1/Z = 1/R + (1)/(jω L) = (R + jω L)/(jω RL)
Z = (jω RL)/(R + jω L) = jω L(1)/(1 + jω L/R)

#### Circuit Response

I = I_R + I_L
I = V/R + 1/L ∫ V dt

V = IR - R/L ∫ V dt

### Parallel LC

#### Circuit Impedance

1/Z = (1)/(Z_L) + (1)/(Z_C)
1/Z = (1)/(jω L) + jω C = ((jω)^2 LC + 1)/(jω L)
Z = (jω L)/((jω)^2 LC + 1)

#### Circuit response

I = I_L + I_C
I = 1/L ∫ V dt + C dV/dt

### Parallel RLC

#### Circuit Impedance

1/Z = (1)/(Z_R) + (1)/(Z_L) + (1)/(Z_C)
1/Z = 1/R + (1)/(jω L) + jω C
1/Z = ((jω)^2 RLC + jω L + R )/(jω RL)
1/Z = ((jω)^2 LC + jω L/R + 1 )/(jω L)

#### Circuit response

I = I_R + I_L + I_C
I = V/R + 1/L ∫ V dt + C dV/dt
I = V/R + 1/L ∫ V dt + C dV/dt

V = IR - R/L ∫ V dt - CR dV/dt

##### Natural Respond

0 = V/R + 1/L ∫ V dt + C dV/dt

##### Forced Respond

I_t = IR + L dI/dt + 1/C ∫ I dt

Second ordered equation that has two roots
 - **ω = -α ± √(α^2 - β^2)**
Where
 - **α = R/2L**
 - **β = (1)/(√(LC))**

The current of the network is given by
 - **A e^ω1 t + B e^ω2 t**

From above

When α^2 = β^2, there is only one real root
ω = -α

When α^2 > β^2, there are two real roots
ω = -α ± √(α^2 - β^2)

When α^2 < β^2, there are two complex roots
ω = -α ± j√(β^2 - α^2 )

##### Resonance Response

At resonance, the impedance of the frequency dependent components cancel out. Therefore the net voltage of the circui is zero

Z_L - Z_C = 0 and V_L + V_C = 0
ω L = (1)/(ω C)
ω = √(1/LC)
Z = Z_R + (Z_L - Z_C) = Z_R = R
I = V/R

At Resonance Frequency
ω = √(1/LC).
I = V/R. Current is at its maximum value

Further analyse the circuit
At ω = 0, Capacitor Opened circuit. Therefore, I = 0.
At ω = 00, Inductor Opened circuit. Therefore, I = 0.

With the values of Current at three ω = 0, √(1/LC), 00 we have the plot of I versus ω. From the plot
If current is reduced to halved of the value of peak current I = V/2R, this current value is stable over a Frequency Band ω₁ - ω₂ where ω₁ = ω_o - Δω, ω₂ = ω_o + Δω

- In RLC series, it is possible to have a band of frequencies where current is stable, ie. current does not change with frequency. For a wide band of frequencies respond, current must be reduced from it's peak value. The more current is reduced, the wider the bandwidth. Therefore, this network can be used as Tuned Selected Band Pass Filter. If tune either L or C to the resonance frequency ω = √(1/LC). Current is at its maximum value I = V/R. Then, adjust the value of R to have a value less than the peak current I = V/R by increasing R to have a desired frequency band.

- If R is increased from R to 2R then the current now is I = V/2R which is stable over a band of frequency
ω₁ - ω₂ where
 ω₁ = ω_o - Δω
 ω₂ = ω_o + Δω

For value of I I = V/2R. The circuit respond to Narrow Band of frequencies

### Summary

| Circuit | Symbol | Series | Parallel |
|---|---|---|---|
| RC | | | |
| Impedance | Z | Z_t = R + (1)/(ω C) = (ω CR + 1)/(ω C) | (1)/(Z_t) = (1)/(Z_R) + (1)/(Z_C) = 1/R + ω C = (R)/(ω CR + 1) |
| Frequency | ω_o = 2 f_o | Z_R = Z_C; R = (1)/(ω C); ω = 1/CR | 1/R = (1)/(ω C); 1/R = ω C; ω = 1/CR |
| Voltage | V | V = IR + 1/C ∫ I dt | I = V/R + CdV/dt |
| Current | I | ∫ I dt = C (V - IR) | dV/dt = 1/C(I - V/R) |
| Phase Angle | | Tan θ = 1/2πf RC; f = 1/2π Tan CR; t = 2π Tan CR | Tan θ = 1/2πf RC; f = 1/2π Tan CR; t = 2π Tan CR |

## Soldering

Soldering is the use of a conductive substance with a low melting point (solder) to electrically connect components together. It is frequently used to join wires to leads of components such as switches or to join components of all kinds to a printed circuit board. The primary tool used for applying solder is a **soldering iron**, a device whose metal tip heats to temperatures well above the melting point of solder. This is used to melt the solder and allow it to flow into a joint.

Soldering is an acquired skill, and it takes practice to become adept. There are many tips, tricks and guidelines on how to produce good soldered joints, and this module aims to present them to you.

The first and most important rule of soldering is to choose your tools with care. Both the solder and the iron must be chosen to suit the application. The first two sections below deal with the different kinds of solder and iron and which to choose for what.

### About solder

Solder is a metallic compound that has a low melting point, usually around 200°C. The composition of solder varies depending on the type, but usually contains lead or tin or both. The most common types are given below. It is available in wire, stick or pellet form. Sticks and pellets are for solder-pots; for normal soldering, you will need solder wire.

Solder wire is a very flexible, silvery coloured wire that is usually supplied on reels that are sized by weight. Common weights are 250 g, 500 g, 1 kg or 2 kg. It generally costs around £6 or $12 for a 500 g reel. It is very soft, and is easily cut by wire cutters and smaller diameters can be snapped by hand (though this is not recommended, as you will see below).

#### Gauge

Solder wire is available in widths given in "standard wire gauge" (SWG). The larger the SWG number, the thinner the wire. Common gauges are 18 and 22, although others are available.

18 gauge solder is suitable for soldering large components and thick wire, as a large quantity of solder can be delivered quickly. However, for smaller, more detailed work, 18 gauge is too thick. Soldering the pins of standard dual in-line packages (DIPs) is about as fine as 18 gauge solder can usefully go.

22 gauge solder is thinner than 18 gauge, and should be used for most electronics work, as it allows much greater control over the quantity of solder delivered, and the chances of accidentally bridging a gap due to over-application or the wire's width are greatly reduced.

Finer gauges such as 26 are available for very fine work with SMT (surface-mount) components.

#### 60/40

60/40 solder is made of 60% tin and 40% lead. It has a melting point of around 190°C, depending on the exact composition. Iron tip temperatures of at least 300°C are recommended. It is also very soft, meaning that cracks do not form so readily if the joint moves during cooling.

#### 63/37

63/37 solder is made of 63% tin and 37% lead. It has a melting point of 183°C, slightly lower than the more common 60/40 blend. The primary advantage of this solder is not the lower melting point, but its eutectic property. Non-eutectic solders, like the 60/40 solder, have a semi-solid state between solid and liquid. If a joint is moved during this stage, it can result in what is called a cold solder joint. Eutectic solders, like the 63/37, do not have this semi-solid state and are thus considered easier to work with as it produces fewer bad joints. However, these solders typically cost more than their non-eutectic counterparts.

#### 50/50

This is made of a half and half mix of tin and lead. Never use 50/50 solder for electronics — it is meant for plumbing. (There are brands[Worthington 50/50 leaded #331887], which is used for electronics and specifically not intended for plumbing). Otherwise, you may end up with failed joints, as the 50/50 solder does not have the same properties as 60/40, having a higher melting point and lower ductility. Also, it is very unlikely to have flux included in it like rosin core solders.

#### Lead-free solder

Solder containing lead is slowly being phased out under new EU directives (especially the RoHS and WEEE directives) and being replaced with solders consisting of tin-antimony alloys. It will likely be many years before lead solder is supplanted for good, but even now many shops do not sell lead solder any more. Lead-free solder has a higher melting point than lead solder and uses more aggressive fluxes. This means that the soldering iron will have to be made for lead-free solder in order to supply the right temperature — it melts at around 230°C — and the iron bits need a different coating to withstand the flux. Using a soldering iron meant for lead solders may result in dry joints and shortened bit life.

Lead-free solder is generally about 20%-50% more expensive than lead solder.

There are many different alloys and their properties can be found in this Wikipedia article.

#### Flux and rosin core solder

Flux is a compound that is used to improve the quality of the soldered joint. It does this in three ways:

- It chemically removes oxidation from the surfaces being soldered.
- It prevents air from oxidising the surfaces once they have been cleaned.
- It increases the "wetting" of the surfaces when the solder is applied.

Wetting is the degree to which the solder flows across the surfaces being joined. Without flux, a **dry joint** may be formed, making a poor connection. See below for a description of dry joints.

Rosin core solder has channels inside filled with rosin flux. Because flux melts before the solder, the embedded flux automatically cleans the joint before the solder flows into it. Rosin core solder should *always* be used for electronics. Both lead and lead-free solder can be rosin core. *Never use acid core solder for electronics* — it is intended for plumbing and sheet metal work only, and the acid residue will corrode and ruin electrical joints. Acid flux remains chemically active at room temperature, while rosin flux becomes inert and harmless once the joint cools.

After soldering, a layer of flux may be seen around the joint. This is usually a burnt-looking brown layer (usually found when the joint has been overheated), or a shiny clear layer. This is not usually a problem, but you may want to remove it with a cotton bud soaked in methylated spirits or alcohol to produce a more professional appearance.

Flux is also available in "pen" applicators. This allows it to be applied, before soldering with rosin core solder, to surfaces that need to be very well fluxed such as SMT pads and unfluxed solder braid in order to be reliable.

### About soldering irons

Soldering irons come in all shapes, sizes and power ratings. Most irons are made of a heating element attached to a handle, with interchangeable **tips** used to work on the joint.

#### Power rating and temperature control

The power rating governs how hot the iron gets and how quickly it heats up. A high power rating means that if a joint with a large thermal mass is being soldered, the bit will not cool down much. However, a high power rating used for fine work may overheat the joint, damaging the component or circuit board.

Generally, a rating of around 18 W is good for fine work, while a 25 W to 50 W rating is good for heavier electrical soldering, such as on mains lighting.

Inexpensive soldering irons have a fixed power consumption — they rely on the dissipation of heat into the surroundings to achieve thermal equilibrium at the desired temperature. These are sufficient for most hobby electronics work. A simple iron like this costs around £20 to £30 (US$30 to US$50).

For more advanced work, especially work involving very small components that overheat easily, a temperature-regulated or variable temperature soldering iron is needed. These usually are sold as a **soldering station**, comprising a soldering iron and a separate power supply. The station monitors the temperature at the soldering tip and adjusts the power output accordingly. Some stations use a magnetic switch that cycles the power on and off, using the Curie Effect; the soldering tips are designed to lose their ferromagnetic properties at a defined temperature, usually 370°C (700°F), 425°C (800°F) or 480°C (900°F), shutting off heater power until the tip cools and regains its magnetic properties. Tip temperature in such irons may fluctuate in a ~4°C (~7°F) range. Such stations are frequently used in manufacturing environments. For laboratory use, stations are available with a thermistor at the tip to monitor temperature and the temperature can be continuously varied by a rotary control. Tip temperature may also be displayed by an LED readout on the base unit. More expensive stations have an hot air pencil for desoldering purposes, as well. You can expect to pay up to £500 (US$800) for an industrial-quality soldering station.

A **soldering gun**, running on AC mains power, has the advantage of quickly heating up and cooling down, and can deliver significant heat to a joint. They are excellent for general electrical work, but not particularly useful for fine electronics work, as they are bulky, inhibiting accurate positioning of the tip, and supporting the weight of the transformer will cause rapid operator fatigue. Tip selection for these tools is limited to a few styles, generally blunt, for larger joints. Moreover, the tip generates an intense electromagnetic field when operating, which can damage sensitive semiconductors.

A **soldering pencil**, heated by a battery or refillable butane cartridge, can be used for small repair jobs in the field where AC mains power is not available, but they are not practical for continuous use.

#### Choosing a soldering tip

The most vital part of choosing the soldering iron is having a suitable tip on the iron. Larger tips have a higher thermal mass and hold their temperature better when working on large joints, but they are physically too big for many applications, as the tip touches more than one joint at a time. Small tips are much easier to use for most work, but they will tend to cool down when presented to a large joint.

Some tips are shaped as an obliquely cut cone, with many different sizes available. They are often called bevel or sloped tips. Typical sizes may range from 1 mm up to around 5 mm across. The advantage of this versatile tip shape is that the cut face can be used to transfer a large amount of heat to the joint, but when the edge of the face is used, it works like a fine tip for small joints or sensitive components, greatly reducing the heat transfer rate.

### Soldering technique

Good soldering is all about technique and practice. It is vital that all parts are prepared properly, otherwise a defective joint will result.

#### Preparation

First, the solder must be prepared. As noted before, the solder you use should have a flux core. When pulling solder off the roll, you may stretch the soft tin/lead alloy past the flux, leaving a pure metal tip with no flux. This will not stick to anything very well, and may cause problems. To avoid this, always cut a few millimetres from the end of your solder with side cutters, or melt off a small piece with a hot soldering iron.

Next prepare your iron. You need a soldering iron stand, both to stop the iron from setting anything on fire and to protect the iron from being knocked to the floor by accident. Every good stand has a tray for a piece of cellulose sponge. This is kept moist and used to clean the tip of your iron. Always wet your sponge before using the iron — having it can improve your soldering immensely. Distilled water is best, but you may use tap water.

First, wait until your iron is up to temperature. Using your iron before it is hot enough will just take longer as you will have to fix the damage you did by forcing it into the joint. Once it has heated up, clean the end. Usually when you pick up an iron, its end is blackened and covered in old flux that has burnt onto the tip. This will not work as a fluxing agent, and will impair the ability of the iron to conduct heat. Add some solder to the tip (it will probably form a sphere due to the burnt coating repelling it) and wipe it onto the wet sponge. Do this until you have a shiny, silver tip to your iron. Every time you remove the iron from its stand, and also every now and again while holding the iron, clean the tip like this. The second and later times in a session you do this it will take usually only one wipe to clean the end. Always ensure that there is no excess solder on the tip before you start to solder.

#### Through-hole components

Through-hole mounted (THM) components are components with leads that protrude through a PCB and are soldered in place on the other side to the components.

When the PCB has copper tracks on only one side of the circuit board, the two sides of the board are usually called "solder side" and "component side".

When placing THM components on the PCB, insert the leads through the hole, and press the device down so it sits flat against the top surface of the PCB. It is often helpful to bend the leads outwards on the other side of the board to prevent the device falling out when the board is turned over for soldering. This is normally only needed for devices with two or three leads, such as axial-lead resistors or transistors.

Often, when a device is being soldered without having the leads bent to secure it mechanically, it may "sag" slightly and fall out of the hole by a millimetre or two. To prevent the components being crooked after soldering, you can tack-solder only one or two of the leads to temporarily secure it. Then, while pushing the component against the circuit board from the other side, reheat the joint(s) and allow the solder to solidify. Finish soldering the remaining leads, and, if needed, return to the original joint(s) to add solder as needed. This technique is particularly useful with DIP packages and similar multi-lead packages.

#### SMT components

Surface-mount technology (SMT) has been gradually replacing older components with wire leads. Some special tools and techniques are used to solder and unsolder SMT components.

### Safety

Always wear safety glasses or goggles with plastic or tempered glass lenses while soldering, to protect your eyes from flying droplets of hot, liquid solder.

The tip of the soldering iron is, by design, very hot, exceeding 370°C (700°F). Avoid letting the tip touch skin, clothing, or anything that might melt, scorch, or burn. Use a proper soldering iron holder when the iron is powered on. These offer a convenient place to rest the iron and keep anything from touching the hot tip. The small plastic-and-wire stands that come with inexpensive soldering irons can be dangerous, as they leave the tip exposed and the iron can easily roll off the stand.

The flux in solder (as well as additional flux applied to the joint) will boil and burn when exposed to the heat of the soldering iron. Although smoke from rosin flux isn't particularly toxic, it is an irritant to the lungs and breathing passages, and extended exposure can lead to health problems. You can avoid breathing the smoke from flux by not "hovering" over your work, setting up a fan to keep your face "upwind" of the work, or by using a fume extractor specifically designed for soldering stations. Always solder in an area with adequate ventilation to keep the fumes from accumulating. A room with windows that open is preferred.

The lead in solder will not typically get hot enough to vaporize and become airborne. However, you will end up with lead on your hands after handling solder, so it is necessary to wash your hands with soap and water afterwards. Lead is toxic and can cause a variety of health problems. Children and pregnant women should avoid all contact with lead due to the severe developmental problems it causes.
