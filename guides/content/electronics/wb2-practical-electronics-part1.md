---
id: electronics/wb2-practical-electronics-part1
title: Practical Electronics (Wikibooks) (part 1 of 7: Overview, Need to Have, Basic Theory…)
category: electronics
tags: [practical electronics, electronics, components, soldering, resistors, capacitors, transistors, circuits]
summary: Hands-on electronics: components, basic theory, soldering, test gear, PCBs, transistors, op-amps and 555 timers.
source: https://en.wikibooks.org/w/index.php?title=Practical_Electronics&oldid=4620925
source_title: Wikibooks: Practical Electronics
license: CC BY-SA 4.0
attribution: Wikibooks contributors, "Practical Electronics" (revision 4620925, https://en.wikibooks.org/w/index.php?oldid=4620925 ); "Practical Electronics/Need to Have" (revision 3348584, https://en.wikibooks.org/w/index.php?oldid=3348584 ); "Practical Electronics/Basic Theory" (revision 4236124, https://en.wikibooks.org/w/index.php?oldid=4236124 ); "Practical Electronics/Units and Quantities" (revision 3657572, https://en.wikibooks.org/w/index.php?oldid=3657572 ); "Practical Electronics/Basic Components" (revision 4227866, https://en.wikibooks.org/w/index.php?oldid=4227866 ); "Practical Electronics/Resistors" (revision 3819776, https://en.wikibooks.org/w/index.php?oldid=3819776 ); "Practical Electronics/Finding component information/Resistors" (revision 4647861, https://en.wikibooks.org/w/index.php?oldid=4647861 ), Wikibooks, The Free Textbook Project. Converted to Markdown for the EmberMesh offline library; navigation templates, images, references and link lists omitted; pages combined into one guide. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-14
part_of: electronics/wb2-practical-electronics
order: 1
---

# Practical Electronics (Wikibooks) (part 1 of 7: Overview, Need to Have, Basic Theory…)

_Wikibooks textbook pages (Wikibooks, CC BY-SA 4.0), converted to plain Markdown: navigation templates, images, references and link lists were removed; several pages are combined here._

## Overview

| | |
|---|---|
| ***A Guide to Understanding and Creating Electronic Circuits.***; The aim of this book is to teach you simple analogue and digital electronics at a practical level. For the in-depth theory of electronics, please read the Electronics Textbook. By the end of this book, you should be able to combine circuit elements to create more complex circuits, and have an understanding of how it works. To begin, you need just basic knowledge of high-school mathematics and physics, common tools and piece of enthusiasm. | - This textbook is still under development. If you would like to be part of this project, add your name to the list on the Collaboration Page!; - In order to keep this Wikibook neat and to ensure high quality of communication, please read the /Manual of Style/ for this book. Please refer to this before submitting diagrams or pictures, and also heed the rules on units and conventions. This text is written in collaboration with. |

| **Sections** | **Chapters** and subchapters (icons show the completion) | | | | |
|---|---|---|---|---|---|
| **Beginning**; How to begin? What I do need to have and know? | **What you need to have**; Tools; Material | **Basic Theory**; /Atoms structure/; General Formulae | **Printed Circuit Boards**; Layout; Manufacture; /Soldering/; /Desoldering/; /Breadboard/; /Perfboard/ | | |
| **Simple components**; Introduction to linear, passive components. | **/Basic Components/**; **Resistors**; **Capacitors**; **Inductors** | **Sources**; Switches; Filters; Relays | | | |
| **Semiconductor components**; Nonlinear semiconductor components and examples of circuits, of course. | **/Diodes/** | /Transistors/; Bipolar Transistors; FET | | | |
| **Circuits Analysis**; We are going to learn circuits and how to analyses it | **Simplest Circuits**; Series Circuit; Parallel Circuit; Kirchhoff laws; Short connection | Mesh Analysis; Superposition; Two Port Network | | | |
| **Alternating current**; Properties and circuits concerning the current varying in time | **/Alternating current/**; /Transformers/ | **Amplifiers**; Types of Amplifiers; Class; Feedback; Examples of circuits; Integrated_Amplifiers | **/Oscillators/**; Astable multivibrator; 555 Timer; **Oscillators**; LC oscillator; RC oscillator; Crystal_oscillator; HF Oscillators; /Decreasing Sin Wave Oscillator/ | | |
| **Integrated circuits**; Work with obscure and mighty IC's | Introduction to Integrated Circuits; **/Logic/**; Boolean Algebra; Boolean Identities; Flip-flops; Astables; Binary Numbers; BCD; Adders | **Logic Families**; Diode Logic (DL); Resistor-Transistor Logic (RTL); Diode-Transistor Logic (DTL); Transistor-Transistor Logic (TTL); Emitter-Coupled Logic (ECL); Integrated Injection Logic (I2L); CMOS; **/Operational amplifiers/** | **AD and DA Converters** | | |
| **Complex Systems**; From what we learned earlier, we can build these amazing systems | **Systems**; Control System; Instrumentation System; Telecommunications System | **/Memory Devices/**; /RAM/; /Silicon Chips/; CompactFlash | | | |
| **Test Equipments**; We can learn more about test equipment that are used frequently in electronic project | **/Test Equipment/**; /Ammeter/; /Ohmmeter/; /Voltmeter/ | /Multimeters/; /Oscilloscopes/; /Electrometer/ | /Frequency Counter/; /Spectrum Analyser/; /Logic Analyzer/ | | |
| **Appendices**; Useful info that didn't pass elsewhere | **/Finding component information/**; Resistors − colour code; Capacitors − number code; Datasheets; **/Overlays, Grids and Guides/**; **/Standards/**; **/Device pinouts/** | **Tables**; **/Units and Quantities/**; **Prefixes**; SWG Wire Sizes - merge; Preferred Values - merge; Greek Alphabet - merge; 74 Series ICs - merge; 4000 Series - merge; 7400 Series - merge; Other ICs - merge; Manufacturer's Prefixes - merge | **/Common Problems/**; Crowbar Circuit; Circuits to Make Yourself; Glossary | | |

## Need to Have

You don't need too much to begin creating your own circuits. In most cases, you will be perfectly fine with following stuff:

### Tools

The first, and most important tool you will need is some sort of **soldering iron** or soldering gun, which you can purchase at your local electronics store. They come in many different wattages and designs, and may be powered from the AC mains, a rechargeable battery, or even butane gas. You will use your iron to melt the solder in order to make the connections in your circuit. For serious work with sensitive semiconductor components, look for a pencil-style iron powered through a low-voltage transformer connected to the AC mains, with a rating of about 40 watts, having an assortment of interchangeable soldering tips and some type of automatic temperature control. You should also have a moist **cellulose sponge** to wipe dross from the iron tip while working. Better soldering stations usually include a tray for a sponge in their base.

Second, you will need **solder**. Solder can be one of various metal alloys with a relatively low melting point (lower than the component leads, and printed circuit board). The most common soldering alloy for electronic work is called Sn63, because it contains 63% tin and 37% lead. Also known as *eutectic solder*, it has the lowest melting point of any combination of tin and lead. Choose a type with a rosin flux core. Choose a spool of small-gauge solder for work with modern surface-mount components, as a large-gauge solder wire can cause one to overheat parts and circuit board traces while waiting for the solder to melt.

Third, you will need **electronic components** with which to work. You can purchase them individually at an electronics store, or simply salvage them from broken or unneeded electronic devices you have lying around. To de-solder them, **desoldering braid** (e.g., "Soder-Wick") is very useful. The desoldering braid acts like a sponge, soaking up the molten solder and "un-gluing" the component. A spring-loaded, manual **desoldering vacuum pump** can be useful for rapidly removing a blob of liquid solder from a joint, and has the advantage over desoldering braid in that it can be cleaned out and reused again and again. An electric hot-air gun, the high-temperature type used for stripping paint, or a small propane blowtorch can also be used to good effect for salvaging components from old circuit boards: If you are like most people, you don't have five heat-proof hands that you can use to melt the solder on all of the leads of a component and pull it out at the same time.

Other essentials include a small pair of **needle-nose pliers**, **side-cutting pliers**, stainless steel or nickel-plated **tweezers**, **wire strippers**, a **hobby knife** with replaceable blades (e.g., X-Acto), and an analogue or digital **multimeter with test leads**.

A **magnifying glass** is useful for reading small markings on components, and if mounted on a goose-neck or pantograph stand, can also be used while soldering. **Task lighting** on your work surface should be the equivalent of a 100-watt incandescent bulb at a distance of 1 meter (3 feet), or even brighter. If the heat from incandescent lighting at close range bothers you, use fluorescent lighting instead. The light should approximate daylight in color, to aid in correctly identifying color codes on components.

Not to be overlooked, obtain and faithfully use a pair of **safety glasses** with plastic or tempered glass lenses, as a flying droplet of liquid solder or a piece of wire or component lead clipped with wire cutters can cause serious, permanent damage to your eyes. (If you already wear corrective eyeglasses, they will serve this purpose adequately, as long as the lenses are large enough to cover the entire orbit of each eye. If the corrective lenses are small, choose plastic goggles that will comfortably fit over your glasses.)

## Basic Theory

### Electrostatic Charge

Electrostatic referring to the stationary electric charge that are built up on an insulating material.

#### Electric Charge

Normally all matter are neutral or have net Charge equal to zero. When an object loses or gains an electron, that object will either becomes positively charged or negatively charged.
Matter + e → -
Matter - e → +

All Charges possess a quantity of Electric Charge called Electric Charge denoted as Q measured in Coulomb (C).
An Electric Field made of Electric Field Lines radiate outward or inward.
For Negatively Charge, the Electric Field Lines radiates inward ,otherwise for Positively Charge, the Electric Field Lines radiates outward.
Electric Field is denoted as E measured in N/C.

Negative Charge ,-Q = → - Charge longleftarrow
Positive Charge ,+Q = longleftarrow + Charge →

Charges interact with each other according to Coulomb's Law.
*Same Charges repulse. Different Charges attract ,i.e Negative Charge attract Positive Charge*

### Electricity

The interaction of charges cause a force of repulsion or attraction between charges. this force of repulsion or attraction cause electricity

#### ElectroStatic Force

ElectroStatic Force or Coulomb Force is the force of attraction between two unlike Charges. For 2 static point charges of different polarities lying in a straight line on a plane. The force of attraction of the two point charge can be calculated by Coulomb's Law
F = k (Q_+ Q_-)/(r^2)
 F, Electric Force of Attraction
 k, Constant of Attraction
 r, Distance of separation

#### ElectroDynamic Force

electroDynamic force produced by potential

#### Charge & Electric Force

If there is a Electric Force that sets stationary charge in straight motion to cause a flow of charge called Current then the Electric Field can be calculated by Ampere's Law
F = E Q
E = F/Q
I = Q/t

#### Charge & Magnetic Force

If there is a Magnetic Force that can change the direction of the moving charge perpendicular to the initial direction of moving charge, such that Positive charge will moved up perpendicular to the initial direction and Negative Charge will move down perpendicular to the initial direction, then Magnetic Field can be calculated by Lorentz's Law
F = B v Q
B = (F)/(v Q)

#### ElectroMagnetic Force

The total force acting on the moving charge is the sum of the Electric Force calculated by Ampere's Law plus the Magnetic Force calculated by Lorentz's Law. The sum of Ampere's Force and Lorentz's force is called ElectroMagnetic Force
F = E Q + B v Q = Q ( E + v B )

### Matter and Electricity

Matter that interact with Electricity is divided into three groups Conductor,Non Conductor,Semi Conductor depends on the ease of how current flow in the matter

- Conductor
All matter that allows current to flow with ease are called Conductor. For example all Metals like Zinc (Zn), Copper (Cu) are used to make Conductor

- Non Conductor
All matter that does not allow current to flow in it are called Non Conductor. For example Rubber

- Semi Conductor
All matter that allows current to flow somewhere between conductor and non conductor are called Semi Conductor. For example Silicon (Si), Gemanium (Ge)

### Conductor and Electricity

When connect conductor with source of Electricity in a closed loop circuit. The Force of Electricity will exert a pressure to make conductor's charges to move in a straight line. The pressure of the Electricity Force is called Voltage denoted as V measured in Volt (v). The straight line movemoment of charges in the conductor is called Current denoted as I measured in Amp (A)

#### Voltage

Voltage is defined as the potential differences of the electricity force to make charges in the conductor to move in straight line and calculated by the ratio of work done on an electric charge .The larger the potential difference, the larger the flow of charge. It appears across a pair of points in a circuit, such as the terminals of a battery.

Voltage is denoted as V measured in Volt (V) and the formula is shown below:

V = W/Q, where Voltage = Joule/Charge

#### Current

Current is defined as flow rate of electric charges flow through an area of conductor in a period of time and is calculated by the ratios of electric charges flow rate over time.
Current is denoted as I measured in Ampere (A) and the formula is shown below:

I = Q/t, where as Ampere = Charge/Time

#### Power

Power is defined as work done over time and is calculated by the product of voltage and current. Power is denoted as P measured in Watt or Volt Amp (VA) and the formula is shown below:

P = V I = W/Q Q/t = W/t = E where Watt = Voltage * Ampere

#### Resistance

Resistance is defined as voltage across the conductor divided by the current flowing and is calculated as voltage over current.
It is also one of properties of a conductor by virtue of when the passage of current is opposed, it will cause electric energy to be transformed into heat

Resistance is denoted as Ω measured in Ohm Ω with formula shown below:

R = V/I where Resistance = Voltage/Ampere

##### Resistance & Temperature

It's been observed that *Resistance of a conductor changes with change in temperature*

R2 = R1 * [α1 + (T2 - T1)] where:

R1 = Conductors resistance at temperature T1

R2 = Conductors resistance at temperature T2

α1 = Temperature coefficient of the material

T1 = Reference temperature at which α1 is specified

T2 = Conductor present temperature

##### Resistance & Electric Power Loss

Also, *When conductor of resistance R conducts current. Conductor releases Heat Energy into the surrounding result in loss of Electric Power Energy directly proportional to the resistance of the conductor*
P_R = I^2 R = (V^2)/(R)

Without Energy loss the Power supplied is P_i
P_i = VI

With energy loss P_R known as Power Loss or Dissipated Power
P_R = V_R I_R = I^2 R = (V^2)/(R)

The real Power supplied would be
P_o = P - P_R
P_o = VI - I^2 R = I (V - IR)
P_o = VI - (V^2)/(R) = V (I - V/R)

The efficiecy of Power transmission can be calculated as the percentage of Real Power over the Supplied Power
n = (P_o)/(P_i)
n = (P_i Cos Theta)/(P_i) = Cos Theta
n = (V - IR)/(V)
n = (I - V/R)/(I)

#### Conductance

Conductance is defined as the ratio of Current over Voltage. Conductance is denoted as Y measured in Siemen 1 / Ω
Y = I/V
1S = 1A / 1V

## Units and Quantities

There are many technical terms and quantities associated with the study of electronics, each with a specific, well-defined meaning and units. It is important to understand the meanings of and relationships between these.

### Quantities

#### Time

**Time** is considered one of the few fundamental quantities of the universe, and is used to define many other quantities in science. Thus, these quantities cannot be used to define time, as this would mean it would be circularly defined. Time is operationally defined (i.e. we assume that there is a quantity "time" that we can measure) in terms of the **SI second** (symbol: **s**), which is defined as:

> the duration of 9 192 631 770 periods of the radiation corresponding to the transition between the two hyperfine levels of the ground state of the caesium 133 atom.

#### Length

**Length** is a quantity of distance and is defined using the above definition for length and the speed of light, which is held to be constant in our current understanding of the universe. The base unit of length is the **metre** (symbol: **m**), which is defined to be:

> the length traversed by light in a vacuum in 1/299 792 458 s.

#### Mass

**Mass** is redefined to the fundamental constant of nature besides relying on the synthetic artifacts for past the century. The outdated base unit of mass is the **kilogram** (symbol: **kg**), which is defined as:

> a mass equal to that of the International Prototype Kilogram (IPK)

As per previous definition,a kilogram is defined by IPK. It is a platinum-iridium cylinder stored in a vault at the BIPM in Sèvres, France. Mass is unique in having the base unit having a prefix - the unprefixed "gram" is not considered to be a base unit in the SI system. The kilogram is approximately equal to the mass of one litre of water.

However, in November 2018, the definition of mass has changed to

> taking the fixed numerical value of the Planck constant h to be 6.626 070 15 × 10-34 when expressed in the unit J s, which is equal to kg m2 s -1, where the meter and the second are defined in terms of c and ∆νCs

#### Area

**Area** is a quantity expressing the two-dimensional size of a defined part of a surface. The base unit of area is the **square metre** (symbol: **m²**). This is:

> the area of a square that is one metre on a side

#### Volume

**Volume** is the quantity expressing the three-dimesional size of a given space. The base unit of volume is the **cubic metre** (symbol: **m³**). This is:

> the volume of a cube that is one metre on a side

Another measure of volume is the *litre*, equal to the volume of a cube 10 cm on a side, or 0.00**1** m³.

#### Force

**Force** is a measure of how much acceleration a mass is provided. The SI unit of force is the **newton** (symbol: **N**), which is:

> the force required to accelerate a one kilogram mass at a rate of one meter per second per second, or 1 kg·m·s⁻².

A force of one newton is approximately the gravitational force exerted on a mass of 0.1 kg on Earth's surface.

#### Current

**Current** is a measure of how much electric charge is flowing in object. It is defined in terms of the attractive force between two wires caused by current flowing in the wires. The SI unit is the **ampere** (symbol: **A**), and it is defined as:

> the constant current which will produce an attractive force of 2×10^–7 newton per metre of length between two straight, parallel conductors of infinite length and negligible circular cross-section placed one metre apart in a vacuum.

The ampere is an SI base unit: it is defined without reference to the quantity of electric charge, though this could change in future with a proposed redefinition of the unit of electric charge.

#### Electric charge

**Electric charge** is the most fundamental of electrical properties. It is a quantity carried by many subatomic particles and is the whole reason electronics as a science exists. The base unit of charge is the **coulomb** (symbol: **C**), which is defined as:

> the charge transferred by a steady current of one ampere in one second

According to conventional values for some other constants, the Coulomb could also be defined as the charge carried by 6.241 509 629 152 65 × 10¹⁸ electrons, however, this is not (yet) the formal definition of the unit.

## Basic Components

### Power Sources

| | | | | | |
|---|---|---|---|---|---|
| Voltage; Source | Current; Source | Controlled; Voltage; Source | Controlled; Current; Source | Single; cell | Battery; of cells |

### Grounds

| | | | | |
|---|---|---|---|---|
| Signal; Ground | | Chassis; Ground | | Earth; Ground |

### Resistors

| | | |
|---|---|---|
| Resistor | Variable; Resistor | Potentiometer |

### Capacitors

| | | |
|---|---|---|
| Capacitor | Polarized; Capacitor | Variable; Capacitor |

### Transformers

| | | | | | | |
|---|---|---|---|---|---|---|
| Air-core; Transformer | Iron-core; Transformer | Center-tapped; Transformer | Autotransformer | Coil with tickler | Step up | Step down |

### Diodes

| | | | | | | |
|---|---|---|---|---|---|---|
| Diode | | Light-Emitting; Diode | | Zener; Diode | | Schottky; Diode |

### Transistors

| | | | P-channel |
|---|---|---|---|
| | | | N-channel |
| JFET | IGFET enh | IGFET dep | |

| | PNP |
|---|---|
| | NPN |

### Switches

| | | |
|---|---|---|
| Open Switch | | Closed Switch |

## Resistors

**Resistors** are passive devices, meaning that they cannot provide any power gain (amplification). They have a linear IV characteristic, meaning that the voltage across the resistor is directly proportional to the current through the resistor. The coefficient of proportionality is called the **resistance** (R). This is Ohm's Law:

V=IR

### Resistor's Characteristics

#### Resistance

Resistors have the symbol on the right. Resistance is defined as the voltage developed across the resistor per ampere of current through the resistor. Resistance has a symbol of R measured in unit called Ohm which has a symbol Ω.

R = V/I

#### Conductance

Conductance is the reciprocal of resistance (current per unit voltage), and is given the symbol *G* and the units *siemens* with symbol *S*.

G = 1/R = I/V

#### Resistor's Construction

If we assume a resistor to be made of a prismatic (invariant cross-sectional area A along its length) conductor with length *l*, and conductivity *ρ*, we can express the conductance as follows:
G = ρ A/l

Form above, conductivity of materials can be calculated by
ρ = I/V l/A

When a resistor is connected with a DC source voltage, the resistance is calculated by Ohm's law:
 - **R = V/I**

When a resistor is connected with an AC voltage source, voltage and current in the resistor have zero phase difference. The impedance of a resistor is calculated in the same way as in the DC case:
Z_R = V/I = R

### Identification

Resistors used in practical electronics range from one ohm to several million ohms

| Resistance in Ohms | Unit | Symbol |
|---|---|---|
| 1 | Ohm | Ω |
| 1,000 | Kilo-ohm | KΩ |
| 1,000,000 | Megaohm | MΩ |

Often a shorthand is used which means that the Ω symbol, which is usually not easily accessed on a computer, is not needed. The shorthand also eliminates the need for decimal points which are sometimes lost or missed off when documents are copied. The shorthand works by replacing a decimal point with the prefix of the resistance (e.g. K for kilo-ohms) or, for resistances in just ohms, R:

| Standard Notation | Shorthand Notation |
|---|---|
| 1 Ω | 1R |
| 1.2 Ω | 1R2 |
| 1 KΩ | 1K |
| 1.2 KΩ | 1K2 |

This notation is preferred, and will be used in this book. please note that the resistances are still *said* the same, so the value of a 1K2 resistor is pronounced "one point two kilo-ohms". The shorthand is not used when we are not talking about an individual resistor, for example, when measuring the resistance of a combination of resistors, we then express the value in ohms.

Resistors are not made perfect, and so they each have a **tolerance**. This is the maximum that a resistor can deviate from its specified value. It is expressed in percent, and the standard tolerance is 10%, which is more than adequate for most of our needs.

#### Preferred Values

To prevent thousands of different values of resistors clogging everything up, resistors only come in specified values. These values are spread across multiples of ten so that each is a constant multiple of the one beneath. The number of divisions per multiple of ten (decade) depends on the tolerance of the resistors. Standard (10%) resistors belong to the **E12** series. The values are spread according to the rounded results of the following rule:

 R_n=R_n-1 × sqrt[12]10

These numbers are:

 1.0 1.2 1.5 1.8 2.2 2.7 3.3 3.9 4.7 5.6 6.8 8.2

After that, the cycle repeats, but a power of ten higher. Therefore, the first three decades are as follows:

 1R 1R2 1R5 1R8 2R2 2R7 3R3 3R9 4R7 5R6 6R8 8R2
 10R 12R 15R 18R 22R 27R 33R 39R 47R 56R 68R 82R
 100R 120R 150R 180R 220R 270R 330R 390R 470R 560R 680R 820R

This pattern continues right up to the top of the resistor values that are available. For more accurate resistors, there exists an E24 (5%), E96 (1%) and an E192 (0.5%) series. There is also an E6 series for 20% resistors. To see a list of all values in these series, see this appendix.

Another way to understand the choice of resistor values in a given series (based on tolerance of the resistors) is as follows:
Say there is a 10K resistor (nominal value) of tolerance 10%, then the actual value can be anything between 9K and 11K (approximately).
So specifying another resistor value within this range is nonsense. The next available value is actually 12K (nominal), since its actual
value can spread from 11K to 13.5K (approx). The following nominal value is 15K (with actual stagger within 13.5K and 16.5K) and so on.
Hence the values available in this series are 10K, 12K, 15K, 18K, 22K, 27K, 33K, 39K, 47K, 56K, 68K, 82K and 100K (which starts the next
series).

Check:

68K (range (68K - 6K8) to (68K + 6K8) or between 61K to 75K)

82K (range (82K -8K2) to (82K + 8K2) or between 74K to 90K)
etc.

#### Colour Codes

For more in-depth info, read up on this chapter **Practical Electronics/Finding component information/Resistors**

#### Power dissipation

Besides resistance, the resistors available in the market also vary according to the power they can dissipate. In general terms, the greater the power dissipation, the larger the resistor. Thus size can be taken as a rough measure of power dissipation.

### Resistors in Series and Parallel

Resistors can be connected in series to increase resistance or in parallel to decrease resistance

#### Series

When resistors are wired in series, the total resistance is the sum of all the individual resistances. For an explanation of this, see this proof. So for the resistor network to the right, the total resistance, R_tot, is:

 R_tot=R_1+R_2+…+R_n

If *n* identical resistors are in series, then the total resistance is *n* times the resistance of those identical resistors. This is useful when you want a simple multiple of the resistance.

When a resistance is added in series with others, the total resistance always increases. The total resistance will therefore be more than the greatest value of resistor present.

#### Parallel

When resistors are wired in parallel, the reciprocal of the total resistance is the sum of the reciprocals of the individual resistances. For an explanation of this, see this proof. So for the resistor network to the right, the total resistance, R_tot, is:

 frac1R_tot=(1)/(R_1)+(1)/(R_2)+…+(1)/(R_n)

If *n* identical resistors are in parallel, then the total resistance is 1/*n* the resistance of those identical resistors. This is useful when you want a simple fraction of the resistance.

When a resistance is added in parallel with others, the total resistance always decreases. The total resistance will therefore be less than the smallest value of resistor present.

#### Series and Parallel

Resistors can also be combined in a combination of series and parallel. To calculate the total resistance in these cases, simply break it into smaller parts that are basic series/parallel combinations and treat each one as one resistor. Consider the arrangement to the right. This is made up of two resistors in parallel, in series with another resistor.

The resistance of the two in parallel is:

 (1)/(R_p)=1/100+1/10
 R_p=100/11

When combined in series with the other resistance,

 R_tot=R_p+3.3
 R_tot=100/11+3.3
 R_tot=1363/110
 R_tot=12.4 Ω

### Uses of Resistors

As we saw in a previous chapter, the current flowing in a simple circuit depends only on the voltage and resistance. This means that given a fixed voltage, by changing the resistance, the current can be tuned. The same applies when the voltage is what we want to fix, and the current is constant (or cannot be changed). By altering the resistance, we can set the voltage.

Because they resist flow of charge, resistors are also used to limit the current flowing through sensitive components. For example, a component that has a low resistance but cannot tolerate too much current should be used in series with a resistor, which will limit the current. The exact value of the resistor depends on the specifications of the device being used. A common application like this is when an LED is used. LEDs generally need 2V at 20mA to work properly. Say we have a 9V circuit, we need to lose 7V to operate the LED. Putting this into Ohm's Law, we get:

 R=V/I
 R=frac720 × 10^-3
 R=350

Therefore, the resistor should be 350R to give the LED 2V at 20mA. However, this is not in the E12 series, so we find the next best, which is 390R. 330R is closer, but may exceed the LEDs current capacity, damaging it.

### 2 Port Resistor Network

- Voltage Divider
- π Network
- T Network
- WheatStone Network

## Finding component information: Resistors

### Identifying resistors

Most axial resistors use a pattern of colored stripes to indicate resistance.
Meanwhile, SMD resistors follow a numerical pattern.

### Resistor colour code

Resistors are labelled to allow you to see what their nominal value is. A "standard" through-hole mounted resistor has coloured bands painted onto it. These bands denote the value. Depending on the tolerance of the resistor, these my be 4, 5, or 6 bands on a resistor.

#### 4 - digits resistor code table

4 band identification is the most commonly used color coding scheme on all resistors. It consists of four colored bands that are painted around the body of the resistor. The scheme is simple: The first two numbers are the first two significant digits of the resistance value, the third is a multiplier, and the fourth is the tolerance of the value. Each color corresponds to a certain number, shown in the chart below. The tolerance for a 4-band resistor will be 2%, 5%, or 10%

#### 5 / 6 - digits resistor code table

For 5-band identification is used for higher tolerance resistors (1%, 0.5%, 0.25%, 0.1%), to notate the extra digit.
The first three bands represent the significant digits, the fourth is the multiplier, and the fifth is the tolerance.

For 6-band identification is used in case of high precision resistors, there is an extra band to indicate the temperature coefficient.
The first 5 bands are same as five band resistors.

The most common color used for sixth band is black which represents 100ppm/K. This indicates that for a change of 100C in temperature, there can be a change of 0.1% in the value of resistance. Generally the sixth band represents temperature coefficient. But in some cases it may represent reliability and failure rate.

|Black||0||0||0||×10⁰|| ||
|Brown ||1||1||1||×10¹||±1% (F) ||100 ppm
|Red ||2||2||2||×10²||±2% (G) ||50 ppm
|Orange||3||3||3||×10³|| ||15 ppm
|Yellow||4||4||4||×10⁴|| ||25 ppm
|Green ||5||5||5||×10⁵||±0.5% (D) ||
|Blue ||6||6||6||×10⁶||±0.25% (C)||
|Violet||7||7||7||×10⁷||±0.1% (B) ||
|Gray ||8||8||8||×10⁸||±0.05% (A)||
|White ||9||9||9||×10⁹|| ||
|Gold || || || ||×0.1 (10⁻¹) ||±5% (J) ||
|Silver|| || || ||×0.01 (10⁻²) ||±10% (K) ||
|None || || || || ||±20% (M) ||

There are some mnemonics to remember the sequence of color code in the above table. A few of them are as follows:
 - Bad Boys Race Our Young Girls, But Violet Generally Wins.
 - Better Be Ready, Or Your Great Big Venture Goes West.
 - B. B. ROY of Great Britain has a Very Good Wife.

### Number codes

Surface mount resistors are too small for coloured bands to be easily read, but they have a flat surface instead, so the value is usually marked un the top surface of the device. This code is much the same as the colour code above, but explicitly written.

There are 3 types of number codes for resistor

(a) 3 digit codes

(b) 4 digit codes and;

(c) EIA 96 codes

So, the code is written as two or three (depending on the tolerance of the device) numbers, followed by a multiplier digit, giving a three- or four-digit designation.

#### 3 digit codes

In three digit coding, the first two numbers indicate the significant value of the resistance and the third number indicates the multiplier like 10 in case the digit is 1, 100 in case the digit is 2 or 1000 in case the digit is 3 and so on.

For example as seen on the picture on the right there are SMD resistors labelled 270 and 0:

- The label 270 can be read as **27** * 10^**0** = 27*1 = 27Ω
- The label 0 can be read as 0Ω (It is also known as "jumper resistor" and are used to bypass tracks on a PCB.

See the picture on the left .If the resistance value is less than 10Ω, the numbering will include letter **R** to denote decimal placing.

- The label 4R7 can be read as 4.7Ω
- The label 101 can be read as **10***10^**1**=10Ω
- The label 100 can be read as **10***10⁰=1Ω

#### 4 digit codes

For 4 decimal points, a four digit code is marked on them.

The calculation is similar to three digit code. The first three numbers indicate the significant value of the resistance and the fourth number indicates the multiplier.

- The label 2211 can be read as 221*10¹= 221Ω
- The label 1002 can be read as 100*10²=10,000Ω

#### EIA 96 codes

The EIA 96 code usually have 3 digits, first two are to indicate the three significant digits of the value of resistance (See table EIA 96 Resistor Marking Code) and third alphabet is to indicate multiplier (See table EIA 96 Multiplier Marking Code)

As example, the picture below shows a resistor of marking **85A** which is read as
85 = 750Ω (See table EIA 96 Resistor Marking Code)
A = 1 (See table EIA 96 Multiplier Marking Code)

750Ω*1 = **750Ω**

Another example is marking **01C** which is read as
01 = 100Ω (See table EIA 96 Resistor Marking Code)
C = 100 (See table EIA 96 Multiplier Marking Code)

100Ω *100 = **10,000Ω**
