---
id: radio/neets10-ch3-transmission-lines-1
title: "NEETS Module 10 Ch. 3: Principles of Transmission Lines (1/2)"
category: radio
tags: [radio, transmission line, coax, SWR, impedance, feedline]
summary: "Navy training text on transmission lines: types, losses, characteristic impedance, reflections and standing waves."
source: "https://archive.org/details/NEETSModule10"
source_title: "Navy Electricity and Electronics Training Series (NEETS) Module 10: Introduction to Wave Propagation, Transmission Lines, and Antennas (NAVEDTRA 14182)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Naval Education and Training Professional Development and Technology Center, US Navy. US Government work; 'Approved for public release; distribution is unlimited.' Figures not reproduced; end-of-chapter question answers retained."
retrieved: 2026-11-13
part_of: radio/neets10
order: 31
---

# NEETS Module 10 Ch. 3: Principles of Transmission Lines (1/2)

## Learning Objectives

Upon completion of this chapter, you will be able to:

1. State what a transmission line is and how transmission lines are used.

2. Explain the operating principles of transmission lines.

3. Describe the five types of transmission lines.

4. State the length of a transmission line.

5. Explain the theory of the transmission line.

6. Define the term LUMPED CONSTANTS in relation to a transmission line.

7. Define the term DISTRIBUTED CONSTANTS in relation to a transmission line.

8. Define LEAKAGE CURRENT.

9. Describe how the electromagnetic lines of force around a transmission line are affected by the distributed constants.

10. Define the term CHARACTERISTIC IMPEDANCE and explain how it affects the transfer of energy along a transmission line.

11. State how the energy transfer along a transmission line is affected by characteristic impedance and the infinite line.

12. Identify the cause of and describe the characteristics of reflections on a transmission line.

13. Define the term STANDING WAVES as applied to a transmission line.

14. Describe how standing waves are produced on a transmission line and identify the types of terminations.

15. Describe the types of standing-wave ratios.

### Introduction To Transmission Lines

A TRANSMISSION LINE is a device designed to guide electrical energy from one point to another. It is used, for example, to transfer the output rf energy of a transmitter to an antenna. This energy will not travel through normal electrical wire without great losses. Although the antenna can be connected directly to the transmitter, the antenna is usually located some distance away from the transmitter. On board ship, the transmitter is located inside a radio room and its associated antenna is mounted on a mast. A transmission line is used to connect the transmitter and the antenna.

The transmission line has a single purpose for both the transmitter and the antenna. This purpose is to transfer the energy output of the transmitter to the antenna with the least possible power loss. How well this is done depends on the special physical and electrical characteristics (impedance and resistance) of the transmission line.

### Terminology

All transmission lines have two ends (see figure 3-1). The end of a two-wire transmission line connected to a source is ordinarily called the INPUT END or the GENERATOR END. Other names given to this end are TRANSMITTER END, SENDING END, and SOURCE. The other end of the line is called the OUTPUT END or RECEIVING END. Other names given to the output end are LOAD END and SINK.

*[Figure 3-1: Basic transmission line (figure not reproduced).]*

You can describe a transmission line in terms of its impedance. The ratio of voltage to current (Ein/Iin) at the input end is known as the INPUT IMPEDANCE (Zin). This is the impedance presented to the transmitter by the transmission line and its load, the antenna. The ratio of voltage to current at the output (Eout/Iout) end is known as the OUTPUT IMPEDANCE (Zout). This is the impedance presented to the load by the transmission line and its source. If an infinitely long transmission line could be used, the ratio of voltage to current at any point on that transmission line would be some particular value of impedance. This impedance is known as the CHARACTERISTIC IMPEDANCE.

**Q1.** What connecting link is used to transfer energy from a radio transmitter to its antenna located on the mast of a ship?

**Q2.** What term is used for the end of the transmission line that is connected to a transmitter?

**Q3.** What term is used for the end of the transmission line that is connected to an antenna?

### Types Of Transmission Mediums

The Navy uses many different types of TRANSMISSION MEDIUMS in its electronic applications. Each medium (line or wave guide) has a certain characteristic impedance value, current-carrying capacity, and physical shape and is designed to meet a particular requirement.

The five types of transmission mediums that we will discuss in this chapter include PARALLEL-LINE, TWISTED PAIR, SHIELDED PAIR, COAXIAL LINE, and WAVEGUIDES. The use of a particular line depends, among other things, on the applied frequency, the power-handling capabilities, and the type of installation.

**Note:** In the following paragraphs, we will mention LOSSES several times. We will discuss these losses more thoroughly under "LOSSES IN TRANSMISSION LINES."

Two-Wire Open Line One type of parallel line is the TWO-WIRE OPEN LINE illustrated in figure 3-2. This line consists of two wires that are generally spaced from 2 to 6 inches apart by insulating spacers. This type of line is most often used for power lines, rural telephone lines, and telegraph lines. It is sometimes used as a transmission line between a transmitter and an antenna or between an antenna and a receiver. An advantage of this type of line is its simple construction. The principal disadvantages of this type of line are the high radiation losses and electrical noise pickup because of the lack of shielding. Radiation losses are produced by the changing fields created by the changing current in each conductor.

*[Figure 3-2: Parallel two-wire line (figure not reproduced).]*

Another type of parallel line is the TWO-WIRE RIBBON (TWIN LEAD) illustrated in figure 3-3. This type of transmission line is commonly used to connect a television receiving antenna to a home television set. This line is essentially the same as the two-wire open line except that uniform spacing is assured by embedding the two wires in a low-loss dielectric, usually polyethylene. Since the wires are embedded in the thin ribbon of polyethylene, the dielectric space is partly air and partly polyethylene.

*[Figure 3-3: Two-wire ribbon type line (figure not reproduced).]*

Twisted Pair The TWISTED PAIR transmission line is illustrated in figure 3-4. As the name implies, the line consists of two insulated wires twisted together to form a flexible line without the use of spacers. It is not used for transmitting high frequency because of the high dielectric losses that occur in the rubber insulation. When the line is wet, the losses increase greatly.

*[Figure 3-4: Twisted pair (figure not reproduced).]*

Shielded Pair The SHIELDED PAIR, shown in figure 3-5, consists of parallel conductors separated from each other and surrounded by a solid dielectric. The conductors are contained within a braided copper tubing that acts as an electrical shield. The assembly is covered with a rubber or flexible composition coating that protects the line from moisture and mechanical damage. Outwardly, it looks much like the power cord of a washing machine or refrigerator.

*[Figure 3-5: Shielded pair (figure not reproduced).]*

The principal advantage of the shielded pair is that the conductors are balanced to ground; that is, the capacitance between the wires is uniform throughout the length of the line. This balance is due to the uniform spacing of the grounded shield that surrounds the wires along their entire length. The braided copper shield isolates the conductors from stray magnetic fields.

Coaxial Lines There are two types of COAXIAL LINES, RIGID (AIR) COAXIAL LINE and FLEXIBLE (SOLID) COAXIAL LINE. The physical construction of both types is basically the same; that is, each contains two concentric conductors.

The rigid coaxial line consists of a central, insulated wire (inner conductor) mounted inside a tubular outer conductor. This line is shown in figure 3-6. In some applications, the inner conductor is also tubular. The inner conductor is insulated from the outer conductor by insulating spacers or beads at regular intervals. The spacers are made of Pyrex, polystyrene, or some other material that has good insulating characteristics and low dielectric losses at high frequencies.

*[Figure 3-6: Air coaxial line (figure not reproduced).]*

The chief advantage of the rigid line is its ability to minimize radiation losses. The electric and magnetic fields in a two-wire parallel line extend into space for relatively great distances and radiation losses occur. However, in a coaxial line no electric or magnetic fields extend outside of the outer conductor. The fields are confined to the space between the two conductors, resulting in a perfectly shielded coaxial line. Another advantage is that interference from other lines is reduced. The rigid line has the following disadvantages: (1) it is expensive to construct; (2) it must be kept dry to prevent excessive leakage between the two conductors; and (3) although high-frequency losses are somewhat less than in previously mentioned lines, they are still excessive enough to limit the practical length of the line.

Leakage caused by the condensation of moisture is prevented in some rigid line applications by the use of an inert gas, such as nitrogen, helium, or argon. It is pumped into the dielectric space of the line at a pressure that can vary from 3 to 35 pounds per square inch. The inert gas is used to dry the line when it is first installed and pressure is maintained to ensure that no moisture enters the line. Flexible coaxial lines (figure 3-7) are made with an inner conductor that consists of flexible wire insulated from the outer conductor by a solid, continuous insulating material. The outer conductor is made of metal braid, which gives the line flexibility. Early attempts at gaining flexibility involved using rubber insulators between the two conductors. However, the rubber insulators caused excessive losses at high frequencies.

*[Figure 3-7: Flexible coaxial line (figure not reproduced).]*

Because of the high-frequency losses associated with rubber insulators, polyethylene plastic was developed to replace rubber and eliminate these losses. Polyethylene plastic is a solid substance that remains flexible over a wide range of temperatures. It is unaffected by seawater, gasoline, oil, and most other liquids that may be found aboard ship. The use of polyethylene as an insulator results in greater high-frequency losses than the use of air as an insulator. However, these losses are still lower than the losses associated with most other solid dielectric materials.

Waveguides The WAVEGUIDE is classified as a transmission line. However, the method by which it transmits energy down its length differs from the conventional methods. Waveguides are cylindrical, elliptical, or rectangular (cylindrical and rectangular shapes are shown in figure 3-8). The rectangular waveguide is used more frequently than the cylindrical waveguide.

*[Figure 3-8: Waveguides (figure not reproduced).]*

The term waveguide can be applied to all types of transmission lines in the sense that they are all used to guide energy from one point to another. However, usage has generally limited the term to mean a hollow metal tube or a dielectric transmission line. In this chapter, we use the term waveguide only to mean "hollow metal tube." It is interesting to note that the transmission of electromagnetic energy along a waveguide travels at a velocity somewhat slower than electromagnetic energy traveling through free space.

A waveguide may be classified according to its cross section (rectangular, elliptical, or circular), or according to the material used in its construction (metallic or dielectric). Dielectric waveguides are seldom used because the dielectric losses for all known dielectric materials are too great to transfer the electric and magnetic fields efficiently.

The installation of a complete waveguide transmission system is somewhat more difficult than the installation of other types of transmission lines. The radius of bends in the waveguide must measure greater than two wavelengths at the operating frequency of the equipment to avoid excessive attenuation. The cross section must remain uniform around the bend. These requirements hamper installation in confined spaces. If the waveguide is dented, or if solder is permitted to run inside the joints, the attenuation of the line is greatly increased. Dents and obstructions in the waveguide also reduce its breakdown voltage, thus limiting the waveguide’s power-handling capability because of possible arc over. Great care must be exercised during installation; one or two carelessly made joints can seriously inhibit the advantage of using the waveguide.

We will not consider the waveguide operation in this module, since waveguide theory is discussed in NEETS, Module 11, Microwave Principles.

**Q4.** List the five types of transmission lines in use today.

**Q5.** Name two of the three described uses of a two-wire open line.

**Q6.** What are the two primary disadvantages of a two-wire open line?

**Q7.** What type of transmission line is often used to connect a television set to its antenna?

**Q8.** What is the primary advantage of the shielded pair?

**Q9.** What are the two types of coaxial lines in use today?

**Q10.** What is the chief advantage of the air coaxial line?

**Q11.** List the three disadvantages of the air coaxial line.

**Q12.** List the two common types of waveguides in use today.

### Losses In Transmission Lines

The discussion of transmission lines so far has not directly addressed LINE LOSSES; actually some line losses occur in all lines. Line losses may be any of three types—COPPER, DIELECTRIC, and RADIATION or INDUCTION LOSSES.

**Note:** Transmission lines are sometimes referred to as rf lines. In this text the terms are used interchangeably.

Copper Losses One type of copper loss is I2R LOSS. In rf lines the resistance of the conductors is never equal to zero. Whenever current flows through one of these conductors, some energy is dissipated in the form of heat. This heat loss is a POWER LOSS. With copper braid, which has a resistance higher than solid tubing, this power loss is higher.

Another type of copper loss is due to SKIN EFFECT. When dc flows through a conductor, the movement of electrons through the conductor's cross section is uniform. The situation is somewhat different when ac is applied. The expanding and collapsing fields about each electron encircle other electrons. This phenomenon, called SELF INDUCTION, retards the movement of the encircled electrons. The flux density at the center is so great that electron movement at this point is reduced. As frequency is increased, the opposition to the flow of current in the center of the wire increases. Current in the center of the wire becomes smaller and most of the electron flow is on the wire surface. When the frequency applied is 100 megahertz or higher, the electron movement in the center is so small that the center of the wire could be removed without any noticeable effect on current. You should be able to see that the effective cross-sectional area decreases as the frequency increases. Since resistance is inversely proportional to the cross-sectional area, the resistance will increase as the frequency is increased. Also, since power loss increases as resistance increases, power losses increase with an increase in frequency because of skin effect.

Copper losses can be minimized and conductivity increased in an rf line by plating the line with silver. Since silver is a better conductor than copper, most of the current will flow through the silver layer. The tubing then serves primarily as a mechanical support.

Dielectric Losses DIELECTRIC LOSSES result from the heating effect on the dielectric material between the conductors. Power from the source is used in heating the dielectric. The heat produced is dissipated into the surrounding medium. When there is no potential difference between two conductors, the atoms in the dielectric material between them are normal and the orbits of the electrons are circular. When there is a potential difference between two conductors, the orbits of the electrons change. The excessive negative charge on one conductor repels electrons on the dielectric toward the positive conductor and thus distorts the orbits of the electrons. A change in the path of electrons requires more energy, introducing a power loss.

The atomic structure of rubber is more difficult to distort than the structure of some other dielectric materials. The atoms of materials, such as polyethylene, distort easily. Therefore, polyethylene is often used as a dielectric because less power is consumed when its electron orbits are distorted. Radiation and Induction Losses RADIATION and INDUCTION LOSSES are similar in that both are caused by the fields surrounding the conductors. Induction losses occur when the electromagnetic field about a conductor cuts through any nearby metallic object and a current is induced in that object. As a result, power is dissipated in the object and is lost.

Radiation losses occur because some magnetic lines of force about a conductor do not return to the conductor when the cycle alternates. These lines of force are projected into space as radiation and this results in power losses. That is, power is supplied by the source, but is not available to the load.

**Q13.** What are the three types of line losses associated with transmission lines?

**Q14.** Losses caused by skin effect and the I 2 R (power) loss are classified as what type of loss?

**Q15.** What types of losses cause the dielectric material between the conductors to be heated?

### Length Of A Transmission Line

A transmission line is considered to be electrically short when its physical length is short compared /G87/G82/G3/G68/G3/G84/G88/G68/G85/G87/G72/G85/G16/G90/G68/G89/G72/G79/G72/G81/G74/G87/G75/G3/G11/G20/G18/G23 /G12/G3/G82/G73/G3/G87/G75/G72/G3/G72/G81/G72/G85/G74/G92/G3/G76/G87/G3/G76/G86/G3/G87/G82/G3/G70/G68/G85/G85/G92/G17

**Note:** In this module, for ease of reading, the value of the wavelength will be spelled out in some cases, and in other cases, the numerical value will be used.

A transmission line is electrically long when its physical length is long compared to a quarter-wavelength of the energy it is to carry. You must understand that the terms "short" and "long" are relative ones. For example, a line that has a physical length of 3 meters (approximately 10 feet) is considered quite short electrically if it transmits a radio frequency of 30 kilohertz. On the other hand, the same transmission line is considered electrically long if it transmits a frequency of 30,000 megahertz. To show the difference in physical and electrical lengths of the lines mentioned above, compute the wavelength of the two frequencies, taking the 30-kilohertz example first:

Now, computing the wavelength for the line carrying 30,000 megahertz:

Thus, you can see that a 3-meter line is electrically very short for a frequency of 30 kilohertz. Also, the 3-meter line is electrically very long for a frequency of 30,000 megahertz.

When power is applied to a very short transmission line, practically all of it reaches the load at the output end of the line. This very short transmission line is usually considered to have practically no electrical properties of its own, except for a small amount of resistance.

However, the picture changes considerably when a long line is used. Since most transmission lines are electrically long (because of the distance from transmitter to antenna), the properties of such lines must be considered. Frequently, the voltage necessary to drive a current through a long line is considerably greater than the amount that can be accounted for by the impedance of the load in series with the resistance of the line.

### Transmission Line Theory

The electrical characteristics of a two-wire transmission line depend primarily on the construction of the line. The two-wire line acts like a long capacitor. The change of its capacitive reactance is noticeable as the frequency applied to it is changed. Since the long conductors have a magnetic field about them when electrical energy is being passed through them, they also exhibit the properties of inductance. The values of inductance and capacitance presented depend on the various physical factors that we discussed earlier. For example, the type of line used, the dielectric in the line, and the length of the line must be considered. The effects of the inductive and capacitive reactances of the line depend on the frequency applied. Since no dielectric is perfect, electrons manage to move from one conductor to the other through the dielectric. Each type of two-wire transmission line also has a conductance value. This conductance value represents the value of the current flow that may be expected through the insulation. If the line is uniform (all values equal at each unit length), then one small section of the line may represent several feet. This illustration of a two-wire transmission line will be used throughout the discussion of transmission lines; but, keep in mind that the principles presented apply to all transmission lines. We will explain the theories using LUMPED CONSTANTS and DISTRIBUTED CONSTANTS to further simplify these principles.

### Lumped Constants

A transmission line has the properties of inductance, capacitance, and resistance just as the more conventional circuits have. Usually, however, the constants in conventional circuits are lumped into a single device or component. For example, a coil of wire has the property of inductance. When a certain amount of inductance is needed in a circuit, a coil of the proper dimensions is inserted. The inductance of the circuit is lumped into the one component. Two metal plates separated by a small space, can be used to supply the required capacitance for a circuit. In such a case, most of the capacitance of the circuit is lumped into this one component. Similarly, a fixed resistor can be used to supply a certain value of circuit resistance as a lumped sum. Ideally, a transmission line would also have its constants of inductance, capacitance, and resistance lumped together, as shown in figure 3-9. Unfortunately, this is not the case. Transmission line constants are distributed, as described below.

*[Figure 3-9: Equivalent circuit of a two-wire transmission line (figure not reproduced).]*

### Distributed Constants

Transmission line constants, called distributed constants, are spread along the entire length of the transmission line and cannot be distinguished separately. The amount of inductance, capacitance, and resistance depends on the length of the line, the size of the conducting wires, the spacing between the wires, and the dielectric (air or insulating medium) between the wires. The following paragraphs will be useful to you as you study distributed constants on a transmission line.

Inductance of a Transmission Line When current flows through a wire, magnetic lines of force are set up around the wire. As the current increases and decreases in amplitude, the field around the wire expands and collapses accordingly. The energy produced by the magnetic lines of force collapsing back into the wire tends to keep the current flowing in the same direction. This represents a certain amount of inductance, which is expressed in microhenrys per unit length. Figure 3-10 illustrates the inductance and magnetic fields of a transmission line.

*[Figure 3-10: Distributed inductance (figure not reproduced).]*

Capacitance of a Transmission Line Capacitance also exists between the transmission line wires, as illustrated in figure 3-11. Notice that the two parallel wires act as plates of a capacitor and that the air between them acts as a dielectric. The capacitance between the wires is usually expressed in picofarads per unit length. This electric field between the wires is similar to the field that exists between the two plates of a capacitor.

*[Figure 3-11: Distributed capacitance (figure not reproduced).]*

Resistance of a Transmission Line The transmission line shown in figure 3-12 has electrical resistance along its length. This resistance is usually expressed in ohms per unit length and is shown as existing continuously from one end of the line to the other.

*[Figure 3-12: Distributed resistance (figure not reproduced).]*

**Q16.** What must the physical length of a transmission line be if it will be operated at 15,000,000 Hz? Use the formula:

**Q17.** What are two of the three physical factors that determine the values of capacitance and inductance of a transmission line?

**Q18.** A transmission line is said to have distributed constants of inductance, capacitance, and resistance along the line. What units of measurement are used to express these constants? Leakage Current Since any dielectric, even air, is not a perfect insulator, a small current known as LEAKAGE CURRENT flows between the two wires. In effect, the insulator acts as a resistor, permitting current to pass between the two wires. Figure 3-13 shows this leakage path as resistors in parallel connected between the two lines. This property is called CONDUCTANCE (G) and is the opposite of resistance. Conductance in transmission lines is expressed as the reciprocal of resistance and is usually given in micromhos per unit length.

*[Figure 3-13: Leakage in a transmission line (figure not reproduced).]*

### Electromagnetic Fields About A Transmission Line

The distributed constants of resistance, inductance, and capacitance are basic properties common to all transmission lines and exist whether or not any current flow exists. As soon as current flow and voltage exist in a transmission line, another property becomes quite evident. This is the presence of an electromagnetic field, or lines of force, about the wires of the transmission line. The lines of force themselves are not visible; however, understanding the force that an electron experiences while in the field of these lines is very important to your understanding of energy transmission.

There are two kinds of fields; one is associated with voltage and the other with current. The field associated with voltage is called the ELECTRIC (E) FIELD. It exerts a force on any electric charge placed in it. The field associated with current is called a MAGNETIC (H) FIELD, because it tends to exert a force on any magnetic pole placed in it. Figure 3-14 illustrates the way in which the E fields and H fields tend to orient themselves between conductors of a typical two-wire transmission line. The illustration shows a cross section of the transmission lines. The E field is represented by solid lines and the H field by dotted lines. The arrows indicate the direction of the lines of force. Both fields normally exist together and are spoken of collectively as the electromagnetic field.

*[Figure 3-14: Fields between conductors (figure not reproduced).]*

### Characteristic Impedance Of A Transmission Line

You learned earlier that the maximum (and most efficient) transfer of electrical energy takes place when the source impedance is matched to the load impedance. This fact is very important in the study of transmission lines and antennas. If the characteristic impedance of the transmission line and the load impedance are equal, energy from the transmitter will travel down the transmission line to the antenna with no power loss caused by reflection.

Definition and Symbols Every transmission line possesses a certain CHARACTERISTIC IMPEDANCE, usually designated as Z0. Z0 is the ratio of E to I at every point along the line. If a load equal to the characteristic impedance is placed at the output end of any length of line, the same impedance will appear at the input terminals of the line. The characteristic impedance is the only value of impedance for any given type and size of line that acts in this way. The characteristic impedance determines the amount of current that can flow when a given voltage is applied to an infinitely long line. Characteristic impedance is comparable to the resistance that determines the amount of current that flows in a dc circuit.

In a previous discussion, lumped and distributed constants were explained. Figure 3-15, view A, shows the properties of resistance, inductance, capacitance, and conductance combined in a short section of two-wire transmission line. The illustration shows the evenly distributed capacitance as a single lumped capacitor and the distributed conductance as a lumped leakage path. Lumped values may be used for transmission line calculations if the physical length of the line is very short compared to the wavelength of energy being transmitted. Figure 3-15, view B, shows all four properties lumped together and represented by their conventional symbols.

*[Figure 3-15: Short section of two-wire transmission line and equivalent circuit (figure not reproduced).]*

**Q19.** Describe the leakage current in a transmission line and in what unit it is expressed.

**Q20.** All the power sent down a transmission line from a transmitter can be transferred to an antenna under what optimum conditions?

**Q21.** What symbol is used to designate the characteristic impedance of a line, and what two variables does it compare?

Characteristic Impedance and the Infinite Line Several short sections, as shown in figure 3-15, can be combined to form a large transmission line, as shown in figure 3-16. Current will flow if voltage is applied across points K and L. In fact, any circuit, such as that represented in figure 3-16, view A, has a certain current flow for each value of applied voltage. The ratio of the voltage to the current is the impedance (Z).

Recall that:

*[Figure 3-16: Characteristic impedance (figure not reproduced).]*

The impedance presented to the input terminals of the transmission line is not merely the resistance of the wire in series with the impedance of the load. The effects of series inductance and shunt capacitance of the line itself may overshadow the resistance, and even the load, as far as the input terminals are concerned.

To find the input impedance of a transmission line, determine the impedance of a single section of line. The impedance between points K and L, in view B of figure 3-16, can be calculated by the use of series-parallel impedance formulas, provided the impedance across points M and N is known. But since this section is merely one small part of a longer line, another similar section is connected to points M and N. Again, the impedance across points K and L of the two sections can be calculated, provided the impedance of the third section is known. This process of adding one section to another can be repeated endlessly. The addition of each section produces an impedance across points K and L of a new and lower value. However, after many sections have been added, each successive added section has less and less effect on the impedance across points K and L. If sections are added to the line endlessly, the line is infinitely long, and a certain finite value of impedance across points K and L is finally reached. In this discussion of transmission lines, the effect of conductance (G) is minor compared to that of inductance (L) and capacitance (C), and is frequently neglected. In figure 3-16, view C, G is omitted and the inductance and resistance of each line can be considered as one line.

Let us assume that the sections of view C continue to the right with an infinite number of sections. When an infinite number of sections extends to the right, the impedance appearing across K and L is Z0. If the line is cut at R and S, an infinite number of sections still extends to the right since the line is endless in that direction. Therefore, the impedance now appearing across points R and S is also Z0, as illustrated in view D. You can see that if only the first three sections are taken and a load impedance of Z0 is connected across points R and S, the impedance across the input terminals K and L is still Z0. The line continues to act as an infinite line. This is illustrated in view E.

*[Figure 3-17: , view A, illustrates how the characteristic impedance of an infinite line can be calculated. Resistors are added in series parallel across terminals K and L in eight steps, and the resultant impedances are noted. In step 1 the impedance is infinite; in step 2 the impedance is 110 ohms. In step 3 the impedance becomes 62.1 ohms, a change of 47.9 ohms. In step 4 the impedance is 48.5 ohms, a change of only 13.6 ohms. The resultant changes in impedance from each additional increment become progressively smaller. Eventually, practically no change in impedance results from further additions to the line. The total impedance of the line at this point is said to be at its characteristic impedance; which, in this case, is 37 ohms. This means that an infinite line constructed as indicated in step 8 could be effectively replaced by a 37-ohm resistor. View B shows a 37-ohm resistor placed in the line at various points to replace the infinite line of step 8 in view A. There is no change in total impedance (figure not reproduced).]*

*[Figure 3-17: Termination of a line (figure not reproduced).]*

In figure 3-17, resistors were used to show impedance characteristics for the sake of simplicity. Figuring the actual impedance of a line having reactance is very similar, with inductance taking the place of the series resistors and capacitance taking the place of the shunt resistors. The characteristic impedance of lines in actual use normally lies between 50 and 600 ohms.

When a transmission line is "short” compared to the length of the radio-frequency waves it carries, the opposition presented to the input terminals is determined primarily by the load impedance. A small amount of power is dissipated in overcoming the resistance of the line. However, when the line is "long” and the load is an incorrect impedance, the voltages necessary to drive a given amount of current through the line cannot be accounted for by considering just the impedance of the load in series with the impedance of the line. The line has properties other than resistance that affect input impedance. These properties are inductance in series with the line, capacitance across the line, resistance leakage paths across the line, and certain radiation losses.

**Q22.** What is the range of the characteristic impedance of lines used in actual practice?

### Voltage Change Along A Transmission Line

Let us summarize what we have just discussed. In an electric circuit, energy is stored in electric and magnetic fields. These fields must be brought to the load to transmit that energy. At the load, energy contained in the fields is converted to the desired form of energy.

Transmission of Energy When the load is connected directly to the source of energy, or when the transmission line is short, problems concerning current and voltage can be solved by applying Ohm’s law. When the transmission line becomes long enough so the time difference between a change occurring at the generator and the change appearing at the load becomes appreciable, analysis of the transmission line becomes important. Dc Applied to a Transmission Line In figure 3-18, a battery is connected through a relatively long two-wire transmission line to a load at the far end of the line. At the instant the switch is closed, neither current nor voltage exists on the line. When the switch is closed, point A becomes a positive potential, and point B becomes negative. These points of difference in potential move down the line. However, as the initial points of potential leave points A and B, they are followed by new points of difference in potential which the battery adds at A and B. This is merely saying that the battery maintains a constant potential difference between points A and B. A short time after the switch is closed, the initial points of difference in potential have reached points A’ and B’; the wire sections from points A to A’ and points B to B’ are at the same potential as A and B, respectively. The points of charge are represented by plus (+) and minus (-) signs along the wires. The directions of the currents in the wires are represented by the arrowheads on the line, and the direction of travel is indicated by an arrow below the line. Conventional lines of force represent the electric field that exists between the opposite kinds of charge on the wire sections from A to A’ and B to B’. Crosses (tails of arrows) indicate the magnetic field created by the electric field moving down the line. The moving electric field and the accompanying magnetic field constitute an electromagnetic wave that is moving from the generator (battery) toward the load. This wave travels at approximately the speed of light in free space. The energy reaching the load is equal to that developed at the battery (assuming there are no losses in the transmission line). If the load absorbs all of the energy, the current and voltage will be evenly distributed along the line.

*[Figure 3-18: Dc voltage applied to a line (figure not reproduced).]*

Ac Applied to a Transmission Line When the battery of figure 3-18 is replaced by an ac generator (fig. 3-19), each successive instantaneous value of the generator voltage is propagated down the line at the speed of light. The action is similar to the wave created by the battery except that the applied voltage is sinusoidal instead of constant. Assume that the switch is closed at the moment the generator voltage is passing through zero and that the next half cycle makes point A positive. At the end of one cycle of generator voltage, the current and voltage distribution will be as shown in figure 3-19.

*[Figure 3-19: Ac voltage applied to a line (figure not reproduced).]*

In this illustration the conventional lines of force represent the electric fields. For simplicity, the magnetic fields are not shown. Points of charge are indicated by plus (+) and minus (-) signs, the larger signs indicating points of higher amplitude of both voltage and current. Short arrows indicate direction of current (electron flow). The waveform drawn below the transmission line represents the voltage (E) and current (I) waves. The line is assumed to be infinite in length so there is no reflection. Thus, traveling sinusoidal voltage and current waves continually travel in phase from the generator toward the load, or far end of the line. Waves traveling from the generator to the load are called INCIDENT WAVES. Waves traveling from the load back to the generator are called REFLECTED WAVES and will be explained in later paragraphs.

Dc Applied to an Infinite Line

*[Figure 3-20: shows a battery connected to a circuit that is the equivalent of a transmission line. In this line the series resistance and shunt conductance are not shown. In the following discussion the line will be considered to have no losses (figure not reproduced).]*

*[Figure 3-20: Dc applied to an equivalent transmission line (figure not reproduced).]*

As the switch is closed, the battery voltage is applied to the input terminals of the line. Now, C1 has no charge and appears, effectively, as a short circuit across points A and B. The full battery voltage appears across inductor L1. Inductor L1 opposes the change of current (0 now) and limits the rate of charge of C1.

Capacitor C2 cannot begin to charge until after C1 has charged. No current can flow beyond points A and B until C1 has acquired some charge. As the voltage across C1 increases, current through L2 and C2 charges C2. This action continues down the line and charges each capacitor, in turn, to the battery voltage. Thus a voltage wave is traveling along the line. Beyond the wavefront, the line is uncharged. Since the line is infinitely long, there will always be more capacitors to be charged, and current will not stop flowing. Thus current will flow indefinitely in the line.

Notice that current flows to charge the capacitors along the line. The flow of current is not advanced along the line until a voltage is developed across each preceding capacitor. In this manner voltage and current move down the line together in phase.

Ac Applied to an Infinite Line An rf line displays similar characteristics when an ac voltage is applied to its sending end or input terminals. In figure 3-21, view A, an ac voltage is applied to the line represented by the circuit shown.

*[Figure 3-21: Ac applied to an equivalent transmission line (figure not reproduced).]*

In view B the generator voltage starts from zero (T1) and produces the voltage shown. As soon as a small voltage change is produced, it starts its journey down the line while the generator continues to produce new voltages along a sine curve. At T2 the generator voltage is 70 volts. The voltages still move along the line until, at T3, the first small change arrives at point W, and the voltage at that point starts increasing. At T5, the same voltage arrives at point X on the line. Finally, at T7, the first small change arrives at the receiving end of the line. Meanwhile, all the changes in the sine wave produced by the generator pass each point in turn. The amount of time required for the changes to travel the length of the line is the same as that required for a dc voltage to travel the same distance.

At T7, the voltage at the various points on the line is as follows:

At the generator: -100 V At point W: 0 V At point X: +100 V At point Y: 0 V If these voltages are plotted along the length of the line, the resulting curve is like the one shown in figure 3-22, view A. Note that such a curve of instantaneous voltages resembles a sine wave. The changes in voltage that occur between T7 and T8 are as follows:

At the generator: Rise from -100 V to -70 V At point W: Drop from 0 V to -70 V At point X: Drop from +100 V to +70 V At point Y: Rise from 0 V to + 70 V

*[Figure 3-22: Instantaneous voltages along a transmission line (figure not reproduced).]*

A plot of these new voltages produces the solid curve shown in figure 3-22, view B. For reference, the curve from T7 is drawn as a dotted line. The solid curve has exactly the same shape as the dotted curve, but has moved to the right by the distance X. Another plot at T9 would show a new curve similar to the one at T8, but moved to the right by the distance Y.

By analyzing the points along the graph just discussed, you should be able to see that the actions associated with voltage changes along an rf line are as follows:

1. All instantaneous voltages of the sine wave produced by the generator travel down the line in the order they are produced.

2. At any point, a sine wave can be obtained if all the instantaneous voltages passing the point are plotted. An oscilloscope can be used to plot these values of instantaneous voltages against time.

3. The instantaneous voltages (oscilloscope displays) are the same in all cases except that a phase difference exists in the displays seen at different points along the line. The phase changes continually with respect to the generator until the change is 360 degrees over a certain length of line.

4. All parts of a sine wave pass every point along the line. A plot of the readings of an ac meter (which reads the effective value of the voltage over a given time) taken at different points along the line shows that the voltage is constant at all points. This is shown in view C of figure 3-22.

5. Since the line is terminated with a resistance equal to Z 0, the energy arriving at the end of the line is absorbed by the resistance.

### Velocity Of Wave Propagation

If a voltage is initially applied to the sending end of a line, that same voltage will appear later some distance from the sending end. This is true regardless of any change in voltage, whether the change is a jump from zero to some value or a drop from some value to zero. The voltage change will be conducted down the line at a constant rate.

Recall that the inductance of a line delays the charging of the line capacitance. The velocity of propagation is therefore related to the values of L and C. If the inductance and capacitance of the rf line are known, the time required for any waveform to travel the length of the line can be determined. To see how this works, observe the following relationship:

Q = IT This formula shows that the total charge or quantity is equal to the current multiplied by the time the current flows. Also:

Q = CE This formula shows that the total charge on a capacitor is equal to the capacitance multiplied by the voltage across the capacitor.

If the switch in figure 3-23 is closed for a given time, the quantity (Q) of electricity leaving the battery can be computed by using the equation Q = IT. The electricity leaves the battery and goes into the line, where a charge is built up on the capacitors. The amount of this charge is computed by using the equation Q = CE.

*[Figure 3-23: Dc applied to an equivalent transmission line (figure not reproduced).]*

Since none of the charge is lost, the total charge leaving the battery during T is equal to the total charge on the line. Therefore:

Q = IT = CE As each capacitor accumulates a charge equal to CE, the voltage across each inductor must change. As C1 in figure 3-23 charges to a voltage of E, point A rises to a potential of E volts while point B is still at zero volts. This makes E appear across L2. As C2 charges, point B rises to a potential of E volts as did point A. At this time, point B is at E volts and point C rises. Thus, we have a continuing action of voltage moving down the infinite line.

In an inductor, these circuit components are related, as shown in the formula This shows that the voltage across the inductor is directly proportional to inductance and the change in current, but inversely proportional to a change in time. Since current and time start from zero, the change in time (DT) and the change in current (DI) are equal to the final time (T) and final current (I). For this case the equation becomes:

ET = LI If voltage E is applied for time (T) across the inductor (L), the final current (I) will flow. The following equations show how the three terms (T, L, and C) are related:

For convenience, you can find T in terms of L and C in the following manner. Multiply the left and right member of each equation as follows:

This final equation is used for finding the time required for a voltage change to travel a unit length, since L and C are given in terms of unit length. The velocity of the waves may be found by: Where: D is the physical length of a unit This is the rate at which the wave travels over a unit length. The units of L and C are henrys and farads, respectively. T is in seconds per unit length and V is in unit lengths per second.

### Determining Characteristic Impedance

As previously discussed, an infinite transmission line exhibits a definite input impedance. This impedance is the CHARACTERISTIC IMPEDANCE and is independent of line length. The exact value of this impedance is the ratio of the input voltage to the input current. If the line is infinite or is terminated in a resistance equal to the characteristic impedance, voltage and current waves traveling the line are in phase. To determine the characteristic impedance or voltage-to-current ratio, use the following procedure: Take the square root:

Example:

A problem using this equation will illustrate how to determine the characteristics of a transmission line. Assume that the line shown in figure 3-23 is 1000 feet long. A 100-foot (approximately 30.5 meter) section is measured to determine L and C. The section is found to have an inductance of 0.25 millihenries and a capacitance of 1000 picofarads. Find the characteristic impedance of the line and the velocity of the wave on the line.

If any other unit length had been considered, the values of L and C would be different, but their ratio would remain the same as would the characteristic impedance.

### Reflections On A Transmission Line

Transmission line characteristics are based on an infinite line. A line cannot always be terminated in its characteristic impedance since it is sometimes operated as an OPEN-ENDED line and other times as a SHORT-CIRCUIT at the receiving end. If the line is open-ended, it has a terminating impedance that is infinitely large. If a line is not terminated in characteristic impedance, it is said to be finite. When a line is not terminated in Z0, the incident energy is not absorbed but is returned along the only path available—the transmission line. Thus, the behavior of a finite line may be quite different from that of the infinite line.
