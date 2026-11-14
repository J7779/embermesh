---
id: electronics/wb2-practical-electronics-part2
title: Practical Electronics (Wikibooks) (part 2 of 7: Capacitors, Inductors, Transformers…)
category: electronics
tags: [practical electronics, electronics, components, soldering, resistors, capacitors, transistors, circuits]
summary: Practical Electronics (Wikibooks), part 2 of 7: Capacitors, Inductors, Transformers, Diodes: Diode operation characteristics, Bipolar Transistors, Transistors, Operational amplifiers.
source: https://en.wikibooks.org/w/index.php?title=Practical_Electronics/Capacitors&oldid=4567418
source_title: Wikibooks: Practical Electronics
license: CC BY-SA 4.0
attribution: Wikibooks contributors, "Practical Electronics/Capacitors" (revision 4567418, https://en.wikibooks.org/w/index.php?oldid=4567418 ); "Practical Electronics/Inductors" (revision 4486705, https://en.wikibooks.org/w/index.php?oldid=4486705 ); "Practical Electronics/Transformers" (revision 3806701, https://en.wikibooks.org/w/index.php?oldid=3806701 ); "Practical Electronics/Diodes/Diode operation characteristics" (revision 3808841, https://en.wikibooks.org/w/index.php?oldid=3808841 ); "Practical Electronics/Bipolar Transistors" (revision 3545468, https://en.wikibooks.org/w/index.php?oldid=3545468 ); "Practical Electronics/Transistors" (revision 4628311, https://en.wikibooks.org/w/index.php?oldid=4628311 ); "Practical Electronics/Operational amplifiers" (revision 3657044, https://en.wikibooks.org/w/index.php?oldid=3657044 ), Wikibooks, The Free Textbook Project. Converted to Markdown for the EmberMesh offline library; navigation templates, images, references and link lists omitted; pages combined into one guide. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-14
part_of: electronics/wb2-practical-electronics
order: 2
---

# Practical Electronics (Wikibooks) (part 2 of 7: Capacitors, Inductors, Transformers…)

_Wikibooks textbook pages (Wikibooks, CC BY-SA 4.0), converted to plain Markdown: navigation templates, images, references and link lists were removed; several pages are combined here._

## Capacitors

### Capacitor

Capacitor is a device that can be used to charge, discharge, or store electric energy in form of an electric field.

#### Capacitor's construction

A device is made from two conductive "plates" separated by a thin layer of insulation called dielectric.

#### Capacitance

Capacitance is defined as the capability of storing charge of a given voltage on each plate which is directly proportional to dielectric constant, the area of the plate and inversely proportional to the separation distance between two plates.
C=Q/V = ε A/l

Capacitance has a symbol C measured in unit farad which has symbol F.

### Capacitor and electricity

When a voltage is applied across the plates of a capacitor, charges of opposite sign will be developed across each plate until voltage across each plate equals to voltage applied, then each plate will have a charge Q and a voltage V. This process is called charging capacitor
 - **Q = q₁ + q₂ + ... = ∫ I dt**

The ratio of charge over voltage gives the capacitance of the capacitor.
 - **C = Q/V**

At this point, when each plate has the same voltage as the applied voltage, there is no current flow in the circuit. Each plate of the capacitor has a voltage V. Capacitor stores electric energy.

If one disconnect the capacitor from the voltage source, the capacitor still has voltage V and will cause shock if touching it. Hence care must be used when disconnecting a capacitor.

If a charged capacitor is connected to ground, current starts to flow from capacitor to the ground until voltage on the capacitor equals to zero. This process is called discharging capacitor.

### Characteristics

#### Charge

Q = q₁ + q₂ + ...
 - **Q = ∫ I dt**

#### Capacitance

 - **C = Q/V**

#### Voltage

 - **V = 1/C ∫ I dt**

#### Current

 - **I = C dV/dt**

#### Reactance

Reactance is defined as the ratio of voltage over current.
X_C = (1/C ∫ I dt)/(I)
 - **X_C = (1)/(ω C)/_-90**
 - **X_C = (1)/(j ω C)**
 - **X_C = (1)/(s C)**

#### Impedance

Since all conductor has resistance the real capacitor's impedance is defined as the sum of reactance and resistance of capacitor.
 - **Z_C = R_C + X_C**
 - **Z_C = √(R_C^2 + ((1)/(s C))^2 )/_ Tan⁻¹(1)/(ω RC)**
 - **Z_C= R_C+ (1)/(j ω C)**
 - **Z_C = R_C + (1)/(s C)**

#### Frequency respond

Capacitor is a tool depends on frequency ω.

- ω = 0, X_C = 00, Capacitor Opened circuit, I = 0
- ω = 00, X_C = 0, Capacitor Closed circuit, I ≠ 0
- ω = (1)/(CR_C)
X_C = R_C,
Z_C = √(2)R_C,
V_C = V/2
I_C = (V)/(2R_C)

With the value of I at three frequency points ω = 0, 00, 1 / CR_C I - f curve can be drawn to give a picture of current in the capacitor over time.

#### Phase angle

When a voltage is applied across capacitor, it will take some time before capacitor conducts current. Therefore, current on the capacitor will lags behind voltage at an angle θ.

For ideal losses capacitor which has no internal resistance, current will lage behind voltage at an angle 90°. For non-ideal lossless capacitor which has an internal resistance, current will lage behind Voltage at an angle θ.
 - **Tanθ = (1)/(2π f) 1/CR**

Phase angle relates to time frequency or time and the value of C and R_C. When there is a change in phase angle time and frequency also change. For this reason RC network is used in phase shifting.
 - **f = (1)/(2π) (1)/(Tanθ) 1/CR**
 - **t = 2π Tanθ CR_C**

### Capacitor network

#### Series connection

When n capacitor connected in series, the total capacitance:
 - **(1)/(C_t) = (1)/(C_1) + (1)/(C_2) + ... + (1)/(C_n)**

When there are two capacitor of the same capacitance connected in series, the total capacitance of the two capacitors is exactly halved.

#### Parallel Connection

When n capacitor connected in parallel, the total capacitance
 - **C_t = C_1 + C_2 + ... + C_n**

When there are two capacitor of the same capacitance connected in parallel, the total capacitance of the two capacitors is exactly doubled.

### Voltage

Capacitors can only take a certain voltage across the plates before the dielectric breaks down, and current flows through the capacitor. When this happens, the capacitor is destroyed. This can involve explosion in some types of capacitors.

The common voltage limits for capacitors are:

- 10V
- 16V
- 25V
- 35V
- 63V
- 100V
- 200V
- 450V

This list is not exhaustive, and there are many capacitors with a voltage rating in between these values.

### Temperature

Some capacitors contain liquid that must not be allowed to boil or freeze. These therefore have specific operating and storage temperature ranges which must not be exceeded if the capacitor is to kept intact. This is most important for electrolytic capacitors.

The common top temperature ranges for capacitors are:

- 85 °C
- 105 °C

### Capacitors

Capacitor basically is a device for storing electrical energy and releasing it when it is "full".
The built up usually consists of two conductors in close proximity and insulated from each other.

### Types of Capacitors

There are many different types of capacitors that can be used. We are most interested in electrolytic and ceramic capacitors, as they are the commonly available ones that are suitable for our level of working. For more types, see this Wikipedia article.

#### Tantalum capacitors

Tantalum capacitors are useful due to their low leakage current, reliability, and their ability to keep stable capacitance over a wide range of temperatures. below is a diagram:

They have special markings. The stripe on the side indicates polarity. Usually with a store bought component, the longer lead is positive. **Warning:** connecting tantalum capacitors in reverse polarity can easily damage or destroy the capacitor. Generally it will have the capacitance written in µF, and the working voltage below it. In some cases (often when µF is not printed), the value (in pF) is encoded with three numbers; the first two digits being the significant figures, with the third being the multiplier (i.e. number of zeros). Tantalum capacitors are most commonly dipped and have a shiny coating, as well as vary in color, being yellow, red, and sometimes blue. Most **axial** tantalum capacitors look completely different.

The dark line on the SMT variety is the positive terminal. The dipped axial through-hole variety will be marked with a + or a dark line to indicate the positive terminal.

#### Ceramic capacitors

Ceramic capacitors are generally non-polarized and almost as common as radial electrolytic capacitors.

#### Ceramic Disc

Ceramic capacitors generally take the form of a disc of ceramic with two legs protruding downwards. These capacitors are not polarised - it doesn't matter which way round they are put into a circuit. The capacitances vary from a lower limit of less than 1pF, up to about 100nF. They sometimes exhibit **drifting** which is where capacitance changes, due to ambient conditions. This is not usually a problem, but it is something you should be aware of.

#### Electrolytic

Axial electrolytic capacitors are electrolytic capacitors that have connections on both ends. These are most frequently used in devices where there is no space for vertically mounted capacitors.

The arrowed stripe indicates the polarity, with the arrows pointing towards the negative pin. **Warning:** connecting electrolytic capacitors in reverse polarity can easily damage or destroy the capacitor. Most large electrolytic capacitors have the voltage, capacitance, temperature ratings, and company name written on them without having any special color coding schemes. Most electrolytic capacitors in general have light blue, black, dark purple or brown colors, although some specialized ones come in yellow and other colors. If for some reason you are uncertain about the polarity, the can is always the negative connection. Most axial caps have both ends visible. The positive end has a rubber insulation and the negative side is only aluminium.

Radial electrolytic capacitors are like axial electrolytic ones, except both pins come out the same end. Usually that end (the "bottom end") is mounted flat against the PCB and the capacitor rises perpendicular to the PCB it is mounted on. This type of capacitor probably accounts for at least 70% of capacitors in consumer electronics (that don't use SMT components). See photograph above.

Like their axial counterparts, many radial electrolytic capacitors have the voltage, capacitance, temperature ratings, polarity, and company name directly written on them without having any special color coding schemes.

Electrolytic capacitors have very large capacitance per volume unit, which makes them good for capacitances over 1μF. They take the form of cylinders which range form a few millimetres in height to several hundred. There are two main types of packaging: axial and radial. Axial capacitors are generally rarer than radial capacitors and have a lead on each end of the cylinder. Radial capacitors have both leads on the same end.

These capacitors are polarised and **must** be connected the right way around. The negative terminal, or cathode, is usually marked in any of the following ways:

- A stripe down the cathode side of the capacitor, usually with minus signs printed on it
- A shorter lead at the anode than at the cathode (not found on axial capacitors)
- A minus sign printed on the end of the capacitor, next to the negative lead.

Electrolytic capacitors can explode violently if they are connected incorrectly, overheat, or have too much voltage across them. Larger capacitors have vents scored into the metal ends, but they will still make a very loud noise (which can damage hearing) and spray hot electrolyte from the rupture. It also might have the possibility of shrapnel from the explosion, which is why safety glasses are a good idea while working with them, especially if it is your first time

They generally have poor tolerances, and are used mostly where precise capacitance is unimportant, such as for power supply smoothing, stiffening, etc.

### Capacitor Labels

The markings on a capacitor usually identify the properties and safe usage parameters. The value of its capacitance is measured in farads, or metric divisions such as micro, nano, or picofarads. The markings are often encoded depending on the type of capacitor and vary by capacitor type based on room available for designation and intended use.

Electrolytic are written in decimal numbers and wester characters, for example 10uF means 10 microfarad, or 10x10^-6 farad.

Ceramic disk capacitors and certain others are marked in a number code based on the number of picofarad or 1x10^-12 farad. To decode, take the first two numbers of the 3 digit number code as the first two digits, then add a number of zero placeholders after it matching the third digit. This is now the capacitance in pF. So a 104 would be 100000pF, or 100nF. Some capacitors are marked similarly, but instead are coded in nanofarad. Manufacturers specifications would be necessary to distinguish. This type of capacitor varies its size by its capacitance and can be compared in value by size, provided the two in comparison are made of the same material.

Mica capacitors are not in common use in 2006, but are extremely stable and marked with a six spot color code. First find the arrow and view the package with it pointing right and read from top left, clockwise the six colors similar to resistor color code. First is the capacitor type, second and third are the significant digits, fourth (bottom right) is the multiplier (number of trailing zeros), fifth is the tolerance, and sixth (bottom left) is the class or characteristic code.

### Safety

Capacitors can explode if overloaded or connected incorrectly. This can result in loud bang and possible shrapnel, which can damage hearing and eyesight, and a spray of hot (or vaporized) electrolyte which can burn skin and damage eyesight. It is not a bad idea to wear safety glasses if it is your first time using capacitors

Large capacitors, especially high voltage ones such as the ones in televisions can store a lethal charge on them. Always ensure that the capacitors are fully discharged before going near them.

### It could happen

- A polarized radial capacitor with an aluminum cover (electrolytic) that has a curved tip is generally out of service or about to be out of order.
- A capacitor with a small black dot is usually a capacitor that has reached its breakdown voltage; that is an electric arc has passed through the dielectric and is no longer good for service.
- By discharging a charged capacitor at high capacities, it may happen to solder a conductor.

## Inductors

### Intro

An inductor is a device made from a wire conductor with several turns. This device generates a magnetic field as current passes through it similar to the magnetic field of a magnet. An inductor stores electrical energy in the form of a magnetic field.

Inductance is defined as the capability to store electrical energy in the form of a magnetic field for a given current which is directly proportional to permeability, length of inductor, and number of turns and inversely proportional to cross-sectional area.
 - **L = B/I = µ N^2A/l**

The symbol for inductance is L and is measured in Henry which has the symbol H.

### Characteristics

#### Magnetic Field

When a voltage is applied across the inductor, current generates Electric Field. Change of Electric Field in the turns generates Magnetic Field perpendicular to Electric Field
 - **B = IL**

#### Inductance

Inductance is the ability to generates Magnetic Field B for a given Current
 - **L = B/I**

#### Voltage

 - **V_L = L dI/dt = dB/dt**

#### Current

 - **I_L = 1/L ∫ V dt**

#### Reactance

Reactance is defined as the ratio of Voltage over current
X_L = (L dI/dt)/(I)
 - **X_L = ω L/_90**
 - **X_L = j ω L**
 - **X_L = s L**

#### Impedance

Impedance is defined as the sum of Reactance and Resistance of Inductor. Since all conductor has Resistance
 - **Z_L = R_L + X_L**
 - **Z_L = √(R_L^2 + (ω L)^2)/_Tan⁻¹ ω L/R**
 - **Z_L = R_L + j ω L**
 - **Z_L = R_L + s L**

#### Frequency Respond

Inductor is a device depends on frequency ω

- ω = 0, X_L = 0, Inductor Closed circuit, I ≠ 0
- ω = 00, X_L = 00, Capacitor Opened circuit, I = 0
- ω = (R_L)/(L)
X_L = R_L,
Z_L = [R_L]^1/3,
V_L = V/2
I_L = (V)/(2R_L)

With the value of I at three frequency points ω = 0, 00, 1 / CR_C I - f curve can be drawn to give a picture of current in the inductor over time

#### Phase Angle

When a Voltage is applied across inductor, current generates magnetic field. Change in current generate change in magnetic field which generate voltage across inductor. Therefore, current will lead voltage

For ideal losses inductor which has no internal resistance, Current will lead Voltage an angle 90. For Non - Ideal inductor which has an internal resistance, Current will lead Voltage an angle θ
 - **tanθ = ω LR_L = 2π f (L)/(R_L)**

Phase angle relates to time frequency or time and the value of R and L. When there is a change in phase angle Time and frequency also change
 - **f = (tanθ/2π) (R_L)/(L)**
 - **t = (2π/tanθ) (L)/(R_L)**

If choosing L = 1 and R = 10ⁿ then the formulas above become
 - **f = (tanθ/2π) × 10^n ≈ 0.3× 10^n tanθ**
 - **t = (2π/tanθ) × 10^-n ≈ 6 × 10^-n (1/tanθ)**

#### Induced Voltage

Induced Voltage is defined as the voltage of the turns which oppose the current flow

 - **-xi = N dB/dt = (dφ)/(dt) where φ = NB**

### Types of Inductors

#### Coil

For a straight wire with the following dimensions Length l, Area A, and Permitivity u and number of Turns N
 - **L = u N l/A**

### Network

Inductors can be connected in series to increase inductance or in parallel to decrease inductance

#### Parallel Connection

(1)/(L_eq) = (1)/(L_1) + (1)/(L_2) + … + (1)/(L_n)

#### Series Connection

L_eq = L_1 + L_2 + … + L_n

## Transformers

**Transformer** is a passive electrical device that contains electromagnetic component consists of two coils wrap around a (usually) square magnetic material that has the capability of transferring electrical energy from one circuit to another through the process of electromagnetic induction.

The main application of transformer is commonly used to increase (‘step up’) or decrease (‘step down’) voltage levels between circuits.

The three main parts of a transformer:

| (i) Primary Winding of Transformer | Produces magnetic flux when it is connected to an electrical source |
|---|---|
| (ii) Magnetic Core of Transformer | The magnetic flux produced by the primary winding, that will pass through this low reluctance path linked with secondary winding and create a closed magnetic circuit |
| (iii)Secondary Winding of Transformer | The flux, produced by primary winding, passes through the core, will link with the secondary winding. This winding also wounds on the same core and gives the desired output of the transformer. |

### Operating Principle

Transformer work based on principles of Faraday's Induction Law operating principle shown below.
Mutual induction between two or more windings (also known as coils) allows for electrical energy to be transferred between circuits.

Primary windings is supplied by an alternating electrical source, it will produces a continually changing and alternating flux that surrounds the winding.

If secondary winding is brought to close approximation to primary winding, some portion of this alternating flux will link with the second winding. As this flux is continually changing in its amplitude and direction, there must be a changing flux linkage in the second winding or coil.

The purpose of the transformer core is to provide a low resistance path for the magnetic flux, through which the maximum amount of flux produced by the primary winding is passed through and linked with the secondary winding.

According to Faraday's Induction Law, there will be an EMF induced in the second winding. If the circuit of this secondary winding is closed, then a current will flow through it. This is the basic working principle of a transformer.

### Characteristic

The formula of Transformers Turns Ratio is given as shown:
V_S = V_P (N_S)/(N_P)

Whether the transformer increases or decreases the voltage level depends on the relative number of turns between the primary and secondary side of the transformer.

**Step up** - If there are less turns on the primary coil than the secondary coil than the **output voltage will increase**

 (N_S)/(N_P) > 1

**Step down** - If there are more turns on the primary coil than the secondary coil than the **output voltage will decrease**

 (N_S)/(N_P) < 1

**Buffer Voltage** when (N_S)/(N_P) = 1

### Summary

|Symbol||
|Construction ||
|(V_o)/(V_i) || (V_o)/(V_i) = (N_2)/(N_1)
|Function || Step Up V_o > V_i, N_2 > N_1
Step DownV_o < V_i, N_2 < N_1
Buffer V_o = V_i, N_2 = N_1

## Diodes: Diode operation characteristics

### P-N Junction

#### Forward Bias

The rectifying action of semiconductor diode is due to the electrical effects that occur at the junction between the n-type semiconductor and p-type semiconductor, at a region known as *p-n junction* (gray areas in images shown below)

As seen in the images below: If the positive terminal of the power source is connected to the p-type semiconductor and the negative terminal of the power source is connected to the n-type semiconductor, the electron flows across the junction from n-type material to p-type material, and the holes flow in reverse. This will cause the junction area to become narrower allow easy movement of both holes and electrons.

These phenomena are known as **forward bias**. The p-n junction will be narrower and will allow more electrons to fill into holes and the holes will move to capture electrons.

#### Reverse Bias

If the power source is installed in reverse, the positive charge in n-type and negative charge in p-type will oppose electron movement causing **potential barrier**.

The p-n junction will widen, both electrons and holes will be moving away from the junction and causing no current that can flow across the junction. These phenomena are known as **reverse bias**.

Hence, it acted more or less like a one-way valve for electricity flow.

### IV curve

The graph shown is a current-voltage characteristic of p-n silicone diode.

From IV curve

| :v < v_d. i=0 | Diode does not conduct current |
|---|---|
| :v = v_d. i=1 mA | Current starts to flow |
| :v > v_d. i=e^(v)/(v_d) mA | Diode conducts current exponentially |

### Conducting voltage of diode

Conducting voltage of diode represents diode biasing voltage to conduct current. Conducting voltage of diode has a symbol v_d
Silicon (Si) diode is 0.7 V
Germanium (Ge) diode is 0.3 V

### Polarities & Biased Voltage

When Diode is connected with a voltage source in such that the polarities of the diode is in opposite with the polarities of the voltage source Diode is said to be Forward Biased and when voltage is greater than Forward biased voltage current will flow and increase exponentially with time in the direction of Pointing Arrow

When Diode is connected with a voltage source in such that the polarities of the diode is in the same with the polarities of the voltage source Diode is said to be Backward Biased Diode does not conduct current

## Bipolar Transistors

A transistor is made by joining one Positive Typed semiconductor in between two Negative Typed semiconductors, or one Negative Typed semiconductor in between two Positive Typed semiconductors. The first is called an NPN Transistor. The second is called a PNP Transistor. All transistors have three terminals, called (B)ase, (E)mitter, and (C)ollector. Therefore a transistor can be connected as a two port network. The Input port is at the B-E junction. The Output port is at the C-E junction. Hence, the transistor is called a Bipolar junction Device

Just like a diode, a Transistor needs to be biased to conduct or non-conduct current to turn a transistor on or turn it off. This makes a transistor act like a Switch. Just like a diode, a Transistor allows current to flow in one direction when the Transistor conducts current. An NPN transistor passes a positive half cycle of the signal. A PNP transistor passes a negative half cycle of the signal.

When a transistor conducts current, it will act as a voltage Amplifier, either Inverting or Non-Inverting, depending on the way resistors are connected to the transistor.

### Transistor construction

A BJT consists of three differently doped semiconductor regions, the *emitter* region, the *base* region and the *collector* region. These regions are, respectively, *p* type, *n* type and *p* type in a PNP, and *n* type, *p* type and *n* type in a NPN transistor. Each semiconductor region is connected to a terminal, appropriately labeled: *emitter* (E), *base* (B) and *collector* (C).

The *base* is physically located between the *emitter* and the *collector* and is made from lightly doped, high resistivity material. The collector surrounds the emitter region, making it almost impossible for the electrons injected into the base region to escape being collected, thus making the resulting value of α very close to unity, and so, giving the transistor a large β. A cross section view of a BJT indicates that the collector–base junction has a much larger area than the emitter–base junction.

The bipolar junction transistor, unlike other transistors, is usually not a symmetrical device. This means that interchanging the collector and the emitter makes the transistor leave the forward active mode and start to operate in reverse mode. Because the transistor's internal structure is usually optimized for forward-mode operation, interchanging the collector and the emitter makes the values of α and β in reverse operation much smaller than those in forward operation; often the α of the reverse mode is lower than 0.5. The lack of symmetry is primarily due to the doping ratios of the emitter and the collector. The emitter is heavily doped, while the collector is lightly doped, allowing a large reverse bias voltage to be applied before the collector–base junction breaks down. The collector–base junction is reverse biased in normal operation. The reason the emitter is heavily doped is to increase the emitter injection efficiency: the ratio of carriers injected by the emitter to those injected by the base. For high current gain, most of the carriers injected into the emitter–base junction must come from the emitter.

The low-performance "lateral" bipolar transistors sometimes used in CMOS processes are sometimes designed symmetrically, that is, with no difference between forward and backward operation.

Small changes in the voltage applied across the base–emitter terminals causes the current that flows between the *emitter* and the *collector* to change significantly. This effect can be used to amplify the input voltage or current. BJTs can be thought of as voltage-controlled current sources, but are more simply characterized as current-controlled current sources, or current amplifiers, due to the low impedance at the base.

Early transistors were made from germanium but most modern BJTs are made from silicon. A significant minority are also now made from gallium arsenide, especially for very high speed applications (see HBT, below).

### Transistor's Types

- /NPN Transistor/
- /PNP Transistor/

### I-V Curve

| **V** || **I**|| **Operation**
|V < V_d ||I = 0 || Transistor does not conduct current
|V = V_d ||I = 1mA ||Transistor starts to conduct current
|V > V_d ||I = e^((V)/(V_d))|| Transistor conduct current
|V = V_s ||I = I_s || Transistor saturates

### Applications

#### Electronics switch

v > v_d, transistor is ON or conducts current. Transistor works like a mechanical closed switch
v < v_d, transistor is OFF or does not conduct current. Transistor works like a mechanical opened switch

#### Electronics amplifier

When transistor conducts current
i_b ≠ 0
i_e=α i_b
i_c=β i_b

#### Feedback

For positive feedback
i_c=i_b+β i_b=i_b(1+β)
For negative feedback
i_c=i_b-β i_b=i_b (1-β)

## Transistors

### Transistor

Transistors are a solid state bi-junction device that can be used to amply voltage or current.Every transistor has three terminals: Base, Emitter, and Collector which form two junctions area namely BE and CE.

It was first invented in 1947 in the United States of America by a three-man team of Bardeen, Shockley, and Brattain at Bell's Telephone Lab.
The transistor is the wonder device that becomes the most important building block in all types of circuits today and it is responsible for miniaturization of circuits from the previous uses of the large vacuum tubes. Transistor is used mainly as electronic switches to control the flow of currents.

Earlier during the introduction of the transistor, it is used singly inside all types of circuits such as shown on the left picture but in this modern age, even the simplest of IC may consist hundred of thousands of transistors inside the circuits. The CPU chips shown on right are believed to have thousands of billions of transistors squeezed inside the area just larger than the thumb area.

There are two kinds of transistors namely:
 1. Bipolar Transistors
 1. Field Effect Transistors

The bipolar transistor is invented in 1947 first and the unipolar transistor (a later invention) is named as the field-effect transistor (FET).

### Bipolar Transistor

Bbipolar transistor have 2 types of transistor namely PNP and NPN types.

NPN Transistor: One positively charged semiconductor between two negatively charged semiconductors.
PNP Transistor: One negatively charged semiconductor between two positively charged semiconductors.
There are 3 leads called **base** (B), **collector** (C) and **emitter** (E)

|NPN Transistor
|| || ||
|PNP Transistor || || ||

### Field Effect Transistor

There are 2 main types of FET namely junction gate FET (**JFET / JUGFET**) or metal oxide FET (**MOSFET**)

### Applications of Transistors

Transistors can be used as electronic switches and audio amplifiers.

## Operational amplifiers

### Intro

Op Amp is a short hand term for Operational Amplifier. An operational amplifier is a circuit component that amplifies the difference of two input voltages:
 - **V_o = A (V₂ - V₁)**

Op Amps are usually packaged as an 8-pin integrated circuit.

| Pin | Usage |
|---|---|
| 1 | Offset Null |
| 2 | Inverted Input |
| 3 | Non-Inverted Input |
| 4 | -V Supply |
| 5 | No use |
| 6 | Output |
| 7 | +V Supply |
| 8 | No use |

Op Amp symbol

- V₊: non-inverting input
- V_−: inverting input
- V_out: output
- V_S+: positive power supply
- V_S−: negative power supply

Op amps amplify AC signal or AC Voltage better than a simple bipolar junction transistor.

### Op Amp Functions

#### Voltage Difference Amplifier

From above
 - **V₀ = A (V₂ - V₁)**

#### Voltage Comparator

V₂ > V₁, V₀ = +V_ss
V₂ < V₁, V₀ = -V_ss
V₂ = V₁, V₀ = 0

#### Inverting Amplifier

With one voltage is grounded
If V₂ = 0, V₀ = -A V₁. Inverting Amplifier

#### Non-Inverting Amplifier

With one voltage is grounded
If V₁ = 0, V₀ = A V₂. Non-Inverting Amplifier

### Linear Configurations

#### Differential amplifier

V_out = V_2 ( ( R_f + R_1 ) R_g over ( R_g + R_2 ) R_1 ) - V_1 ( R_f over R_1 )

- Differential Z_in (between the two input pins) = R_1 + R_2

##### Voltage Difference Amplifier

Whenever R_1 = R_2 and R_f = R_g,
V_out = R_f over R_1 ( V_2 - V_1 )

##### Voltage Difference

When R_1 = R_f and R_2 = R_g (including previous conditions, so that R_1 = R_2 = R_f = R_g):

V_out = V_2 - V_1

#### Inverting Amplifier

V_out = - V_in ( R_f over R_1 )

Inverting Amplification is dictated by the ratio of the two resistors

#### Non-Inverting Amplifier

V_out = V_in ( 1 + R_2 over R_1 )

Non-Inverting Amplification is dictated by the ratio of the two resistors plus one

#### Voltage Follower

From Non-Inverting Amplifier's formula. If the resistors has the same value of resistance then output voltage is exactly equal to the input voltage
V_out = V_in \

From Inverting Amplifier's formula. If the resistors has the same value of resistance then output voltage is exactly equal to the input voltage and inverted
V_out = - V_in \

#### Summing amplifier

V_out = - R_f ( V_1 over R_1 + V_2 over R_2 + … + V_n over R_n )

When R_1 = R_2 = … = R_n, and R_f independent

V_out = - ( R_f over R_1 ) (V_1 + V_2 + … + V_n ) \

When R_1 = R_2 = … = R_n = R_f

V_out = - ( V_1 + V_2 + … + V_n ) \

#### Integrator

Integrates the (inverted) signal over time

V_out = ∫_0^t - V_in over RC dt + V_initial

(where V_in and V_out are functions of time, V_initial is the output voltage of the integrator at time *t* = 0.)

#### Differentiator

Differentiates the (inverted) signal over time.

*The name "differentiator" should not be confused with the "differential amplifier", also shown on this page.*

V_out = - RC ( dV_in over dt )

(where V_in and V_out are functions of time)

#### Comparator

- V_out = \ V_S+ V_1 > V_2 \ V_S- V_1 < V_2.

Từ V₀ = A (V₂ - V₁)

- V_o = 0 khi V₂ = V₁

- V_o > 0 khi V₂ > V₁
V_o = V_ss

- V_o < 0 khi V₂ < V₁
V_o = V_-ss

When two input voltages equal. The output voltage is zero. When the two input voltages different and if one is greater than or less than the other

1. V_o = V_ss khi V₂ > V₁
1. V_o = V_-ss khi V₂ < V₁

#### Instrumentation amplifier

Combines very high input impedance, high common-mode rejection, low DC offset, and other properties used in making very accurate, low-noise measurements

- Is made by adding a inverting buffer to each input of the differential amplifier to increase the input impedance.

#### Schmitt trigger

A comparator with hysteresis

Hysteresis from (-R_1)/(R_2)V_sat to (R_1)/(R_2)V_sat.

#### Gyrator

A gyrator can transform impedances. Here a capacitor is changed into an inductor.

L = R_L R C

#### Zero level detector

Voltage divider reference

- Zener sets reference voltage

#### Negative impedance converter (NIC)

Creates a resistor having a negative value for any signal generator

- In this case, the ratio between the input voltage and the input current (thus the input resistance) is given by:

R_in = - R_3 (R_1)/(R_2)

### Non-linear configurations

#### Rectifier

Behaves like an ideal diode for the load, which is here represented by a generic resistor R_L.

- This basic configuration has some limitations. For more information and to know the configuration that is actually used, see the main article.

#### Peak detector

When the switch is closed, the output goes to zero volts. When the switch is opened for a certain time interval, the capacitor will charge to the maximum input voltage attained during that time interval.

The charging time of the capacitor must be much shorter than the period of the highest appreciable frequency component of the input voltage.

#### Logarithmic output

- The relationship between the input voltage v_in and the output voltage v_out is given by:

v_out = -V_γ ln ( (v_in)/(I_S · R) )

where I_S is the *saturation current*.

- If the operational amplifier is considered ideal, the negative pin is virtually grounded, so the current flowing into the resistor from the source (and thus through the diode to the output, since the op-amp inputs draw no current) is:

(v_in)/(R) = I_R = I_D

where I_D is the current through the diode. As known, the relationship between the current and the voltage for a diode is:

I_D = I_S ( e^fracV_DV_γ - 1 )

This, when the voltage is greater than zero, can be approximated by:

I_D simeq I_S e^V_D over V_γ

Putting these two formulae together and considering that the output voltage V_out is the inverse of the voltage across the diode V_D, the relationship is proven.

Note that this implementation does not consider temperature stability and other non-ideal effects.

#### Exponential output

- The relationship between the input voltage v_in and the output voltage v_out is given by:

v_out = - R I_S e^v_in over V_γ

where I_S is the *saturation current*.

- Considering the operational amplifier ideal, then the negative pin is virtually grounded, so the current through the diode is given by:

I_D = I_S ( e^fracV_DV_γ - 1 )

when the voltage is greater than zero, it can be approximated by:

I_D simeq I_S e^V_D over V_γ

The output voltage is given by:
v_out = -R I_D
