---
id: radio/neets10-ch4-antennas-1
title: "NEETS Module 10 Ch. 4: Antennas (1/3)"
category: radio
tags: [radio, antenna, dipole, Marconi, arrays, radiation pattern]
summary: "Navy training text on antennas: radiation, polarization, half-wave and quarter-wave antennas, arrays and special antennas."
source: "https://archive.org/details/NEETSModule10"
source_title: "Navy Electricity and Electronics Training Series (NEETS) Module 10: Introduction to Wave Propagation, Transmission Lines, and Antennas (NAVEDTRA 14182)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Naval Education and Training Professional Development and Technology Center, US Navy. US Government work; 'Approved for public release; distribution is unlimited.' Figures not reproduced; end-of-chapter question answers retained."
retrieved: 2026-11-13
part_of: radio/neets10
order: 41
---

# NEETS Module 10 Ch. 4: Antennas (1/3)

## Learning Objectives

Upon completion of this chapter you will be able to:

1. State the basic principles of antenna radiation and list the parts of an antenna.

2. Explain current and voltage distribution on an antenna.

3. Describe how electromagnetic energy is radiated from an antenna.

4. Explain polarization, gain, and radiation resistance characteristics of an antenna.

5. Describe the theory of operation of half- wave and quarter-wave antennas.

6. List the various array antennas.

7. Describe the directional array antennas presented and explain the basic operation of each.

8. Identify various special antennas presented, such as long-wire, V, rhombic, turnstile, ground-plane, and corner-reflector; describe the operation of each.

9. List safety precautions when working aloft and around antennas.

### Introduction

If you had been around in the early days of electronics, you would have considered an ANTENNA (AERIAL) to be little more than a piece of wire strung between two trees or upright poles. In those days, technicians assumed that longer antennas automatically provided better reception than shorter antennas. They also believed that a mysterious MEDIUM filled all space, and that an antenna used this medium to send and receive its energy. These two assumptions have since been discarded. Modern antennas have evolved to the point that highly directional, specially designed antennas are used to relay worldwide communications in space through the use of satellites and Earth station antennas (fig. 4-1). Present transmission theories are based on the assumption that space itself is the only medium necessary to propagate (transmit) radio energy.

*[Figure 4-1: Satellite/earth station communications system (figure not reproduced).]*

A tremendous amount of knowledge and information has been gained about the design of antennas and radio-wave propagation. Still, many old-time technicians will tell you that when it comes to designing the length of an antenna, the best procedure is to perform all calculations and try out the antenna. If it doesn't work right, use a cut-and-try method until it does. Fortunately, enough information has been collected over the last few decades that it is now possible to predict the behavior of antennas. This chapter will discuss and explain the basic design and operation of antennas.

### Principles Of Antenna Radiation

After an rf signal has been generated in a transmitter, some means must be used to radiate this signal through space to a receiver. The device that does this job is the antenna. The transmitter signal energy is sent into space by a TRANSMITTING ANTENNA; the rf signal is then picked up from space by a

### Receiving Antenna.

The rf energy is transmitted into space in the form of an electromagnetic field. As the traveling electromagnetic field arrives at the receiving antenna, a voltage is induced into the antenna (a conductor). The rf voltages induced into the receiving antenna are then passed into the receiver and converted back into the transmitted rf information.

The design of the antenna system is very important in a transmitting station. The antenna must be able to radiate efficiently so the power supplied by the transmitter is not wasted. An efficient transmitting antenna must have exact dimensions. The dimensions are determined by the transmitting frequencies. The dimensions of the receiving antenna are not critical for relatively low radio frequencies. However, as the frequency of the signal being received increases, the design and installation of the receiving antenna become more critical. An example of this is a television receiving antenna. If you raise it a few more inches from the ground or give a slight turn in direction, you can change a snowy blur into a clear picture. The conventional antenna is a conductor, or system of conductors, that radiates or intercepts electromagnetic wave energy. An ideal antenna has a definite length and a uniform diameter, and is completely isolated in space. However, this ideal antenna is not realistic. Many factors make the design of an antenna for a communications system a more complex problem than you would expect. These factors include the height of the radiator above the earth, the conductivity of the earth below it, and the shape and dimensions of the antenna. All of these factors affect the radiated-field pattern of the antenna in space. Another problem in antenna design is that the radiation pattern of the antenna must be directed between certain angles in a horizontal or vertical plane, or both.

Most practical transmitting antennas are divided into two basic classifications, HERTZ (half-wave) ANTENNAS and MARCONI (quarter-wave) ANTENNAS. Hertz antennas are generally installed some distance above the ground and are positioned to radiate either vertically or horizontally. Marconi antennas operate with one end grounded and are mounted perpendicular to the Earth or to a surface acting as a ground. Hertz antennas are generally used for frequencies above 2 megahertz. Marconi antennas are used for frequencies below 2 megahertz and may be used at higher frequencies in certain applications. A complete antenna system consists of three parts: (1) The COUPLING DEVICE, (2) the FEEDER, and (3) the ANTENNA, as shown in figure 4-2. The coupling device (coupling coil) connects the transmitter to the feeder. The feeder is a transmission line that carries energy to the antenna. The antenna radiates this energy into space.

*[Figure 4-2: Typical antenna system (figure not reproduced).]*

The factors that determine the type, size, and shape of the antenna are (1) the frequency of operation of the transmitter, (2) the amount of power to be radiated, and (3) the general direction of the receiving set. Typical antennas are shown in figure 4-3.

*[Figure 4-3: Typical antennas (figure not reproduced).]*

### Current And Voltage Distribution On An Antenna

A current flowing in a wire whose length is properly related to the rf produces an electro magnetic field. This field is radiated from the wire and is set free in space. We will discuss how these waves are set free later in this chapter. Remember, the principles of radiation of electromagnetic energy are based on two laws:

1. A MOVING ELECTRIC FIELD CREATES A MAGNETIC (H) FIELD.

2. A MOVING MAGNETIC FIELD CREATES AN ELECTRIC (E) FIELD.

In space, these two fields will be in phase and perpendicular to each other at any given time. Although a conductor is usually considered present when a moving electric or magnetic field is mentioned, the laws that govern these fields say nothing about a conductor. Therefore, these laws hold true whether a conductor is present or not.

*[Figure 4-4: shows the current and voltage distribution on a half-wave (Hertz) antenna. In view A, a piece of wire is cut in half and attached to the terminals of a high-frequency ac generator. The frequency of the generator is set so that each half of the wire is 1/4 wavelength of the output. The result is a common type of antenna known as a DIPOLE (figure not reproduced).]*

*[Figure 4-4: Current and voltage distribution on an antenna (figure not reproduced).]*

At a given time the right side of the generator is positive and the left side negative. Remember that like charges repel. Because of this, electrons will flow away from the negative terminal as far as possible, but will be attracted to the positive terminal. View B shows the direction and distribution of electron flow. The distribution curve shows that most current flows in the center and none flows at the ends. The current distribution over the antenna will always be the same no matter how much or how little current is flowing. However, current at any given point on the antenna will vary directly with the amount of voltage developed by the generator.

One-quarter cycle after electrons have begun to flow, the generator will develop its maximum voltage and the current will decrease to 0. At that time the condition shown in view C will exist. No current will be flowing, but a maximum number of electrons will be at the left end of the line and a minimum number at the right end. The charge distribution view C along the wire will vary as the voltage of the generator varies. Therefore, you may draw the following conclusions:

1. A current flows in the antenna with an amplitude that varies with the generator voltage.

2. A sinusoidal distribution of charge exists on the antenna. Every 1/2 cycle, the charges reverse polarity.

3. The sinusoidal variation in charge magnitude lags the sinusoidal variation in current by 1/4 cycle.

**Q1.** What are the two basic classifications of antennas?

**Q2.** What are the three parts of a complete antenna system?

**Q3.** What three factors determine the type, size, and shape of an antenna?

### Radiation Of Electromagnetic Energy

The electromagnetic radiation from an antenna is made up of two components, the E field and the H field. We discussed these fields in chapters 1 and 2. The two fields occur 90 degrees out of phase with each other. These fields add and produce a single electromagnetic field. The total energy in the radiated wave remains constant in space except for some absorption of energy by the Earth. However, as the wave advances, the energy spreads out over a greater area and, at any given point, decreases as the distance increases.

Various factors in the antenna circuit affect the radiation of these waves. In figure 4-5, for example, if an alternating current is applied at the A end of the length of wire from A to B, the wave will travel along the wire until it reaches the B end. Since the B end is free, an open circuit exists and the wave cannot travel farther. This is a point of high impedance. The wave bounces back (reflects) from this point of high impedance and travels toward the starting point, where it is again reflected. The energy of the wave would be gradually dissipated by the resistance of the wire of this back-and-forth motion (oscillation); however, each time it reaches the starting point, the wave is reinforced by an amount sufficient to replace the energy lost. This results in continuous oscillations of energy along the wire and a high voltage at the A end of the wire. These oscillations are applied to the antenna at a rate equal to the frequency of the rf voltage.

*[Figure 4-5: Antenna and rf source (figure not reproduced).]*

These impulses must be properly timed to sustain oscillations in the antenna. The rate at which the waves travel along the wire is constant at approximately 300,000,000 meters per second. The length of the antenna must be such that a wave will travel from one end to the other and back again during the period of 1 cycle of the rf voltage. Remember, the distance a wave travels during the period of 1 cycle is known as the wavelength and is found by dividing the rate of travel by the frequency. Look at the current and voltage (charge) distribution on the antenna in figure 4-6. A maximum movement of electrons is in the center of the antenna at all times; therefore, the center of the antenna is at a low impedance. This condition is called a STANDING WAVE of current. The points of high current and high voltage are known as current and voltage LOOPS. The points of minimum current and minimum voltage are known as current and voltage NODES. View A shows a current loop and current nodes. View B shows voltage loops and a voltage node. View C shows the resultant voltage and current loops and nodes. The presence of standing waves describes the condition of resonance in an antenna. At resonance the waves travel back and forth in the antenna reinforcing each other and the electromagnetic waves are transmitted into space at maximum radiation. When the antenna is not at resonance, the waves tend to cancel each other and lose energy in the form of heat.

*[Figure 4-6: Standing waves of voltage and current on an antenna (figure not reproduced).]*

**Q4.** If a wave travels exactly the length of an antenna from one end to the other and back during the period of 1 cycle, what is the length of the antenna?

**Q5.** What is the term used to identify the points of high current and high voltage on an antenna?

**Q6.** What is the term used to identify the points of minimum current and minimum voltage on an antenna?

### Antenna Characteristics

You can define an antenna as a conductor or group of conductors used either for radiating electromagnetic energy into space or for collecting it from space. Electrical energy from the transmitter is converted into electromagnetic energy by the antenna and radiated into space. On the receiving end, electromagnetic energy is converted into electrical energy by the antenna and is fed into the receiver. Fortunately, separate antennas seldom are required for both transmitting and receiving rf energy. Any antenna can transfer energy from space to its input receiver with the same efficiency that it transfers energy from the transmitter into space. Of course, this is assuming that the same frequency is used in both cases. This property of interchangeability of the same antenna for transmitting and receiving is known as antenna RECIPROCITY. Antenna reciprocity is possible because antenna characteristics are essentially the same for sending and receiving electromagnetic energy.

### Reciprocity Of Antennas

In general, the various properties of an antenna apply equally, regardless of whether you use the antenna for transmitting or receiving. The more efficient a certain antenna is for transmitting, the more efficient it will be for receiving on the same frequency. Likewise, the directive properties of a given antenna also will be the same whether it is used for transmitting or receiving.

Assume, for example, that a certain antenna used with a transmitter radiates a maximum amount of energy at right angles to the axis of the antenna, as shown in figure 4-7, view A. Note the minimum amount of radiation along the axis of the antenna. Now, if this same antenna were used as a receiving antenna, as shown in view B, it would receive best in the same directions in which it produced maximum radiation; that is, at right angles to the axis of the antenna.

*[Figure 4-7: Reciprocity of antennas (figure not reproduced).]*

### Antenna Gain

Another characteristic of a given antenna that remains the same whether the antenna is used for transmitting or receiving is GAIN. Some antennas are highly directional that is, more energy is propagated in certain directions than in others. The ratio between the amount of energy propagated in these directions compared to the energy that would be propagated if the antenna were not directional is known as its gain. When a transmitting antenna with a certain gain is used as a receiving antenna, it will also have the same gain for receiving.

### Polarization

Let's review polarization briefly. In chapter 2 you learned that the radiation field is composed of electric and magnetic lines of force. These lines of force are always at right angles to each other. Their intensities rise and fall together, reaching their maximums 90 degrees apart. The electric field determines the direction of polarization of the wave. In a vertically polarized wave, the electric lines of force lie in a vertical direction. In a horizontally polarized wave, the electric lines of force lie in a horizontal direction. Circular polarization has the electric lines of force rotating through 360 degrees with every cycle of rf energy.

The electric field was chosen as the reference field because the intensity of the wave is usually measured in terms of the electric field intensity (volts, millivolts, or microvolts per meter). When a single-wire antenna is used to extract energy from a passing radio wave, maximum pickup will result when the antenna is oriented in the same direction as the electric field. Thus a vertical antenna is used for the efficient reception of vertically polarized waves, and a horizontal antenna is used for the reception of horizontally polarized waves. In some cases the orientation of the electric field does not remain constant. Instead, the field rotates as the wave travels through space. Under these conditions both horizontal and vertical components of the field exist and the wave is said to have an elliptical polarization.

**Q7.** The various properties of a transmitting antenna can apply equally to the same antenna when it is used as a receiving antenna. What term is used for this property?

**Q8.** The direction of what field is used to designate the polarization of a wave?

**Q9.** If a wave's electric lines of force rotate through 360 degrees with every cycle of rf energy, what is the polarization of this wave?

Polarization Requirements for Various Frequencies Ground-wave transmission is widely used at medium and low frequencies. Horizontal polarization cannot be used at these frequencies because the electric lines of force are parallel to and touch the earth. Since the earth acts as a fairly good conductor at low frequencies, it would short out the horizontal electric lines of force and prevent the radio wave from traveling very far. Vertical electric lines of force, on the other hand, are bothered very little by the earth. Therefore vertical polarization is used for ground-wave transmission, allowing the radio wave to travel a considerable distance along the ground surface with minimum attenuation.

Sky-wave transmission is used at high frequencies. Either horizontal or vertical polarization can be used with sky-wave transmission because the sky wave arrives at the receiving antenna elliptically polarized. This is the result of the wave traveling obliquely through the Earth's magnetic field and striking the ionosphere. The radio wave is given a twisting motion as it strikes the ionosphere. Its orientation continues to change because of the unstable nature of the ionosphere. The relative amplitudes and phase differences between the horizontal and vertical components of the received wave also change. Therefore, the transmitting and receiving antennas can be mounted either horizontally or vertically. Although either horizontally or vertically polarized antennas can be used for high frequencies, horizontally polarized antennas have certain advantages and are therefore preferred. One advantage is that vertically polarized interference signals, such as those produced by automobile ignition systems and electrical appliances, are minimized by horizontal polarization. Also, less absorption of radiated energy by buildings or wiring occurs when these antennas are used. Another advantage is that support structures for these antennas are of more convenient size than those for vertically polarized antennas. For frequencies in the vhf or uhf range, either horizontal or vertical polarization is satisfactory. These radio waves travel directly from the transmitting antenna to the receiving antenna without entering the ionosphere. The original polarization produced at the transmitting antenna is maintained throughout the entire travel of the wave to the receiver. Therefore, if a horizontally polarized antenna is used for transmitting, a horizontally polarized antenna must be used for receiving. The requirements would be the same for a vertical transmitting and receiving antenna system.

For satellite communications, parallel frequencies can be used without interference by using polarized radiation. The system setup is shown in figure 4-8. One pair of satellite antennas is vertically polarized and another pair is horizontally polarized. Either vertically or horizontally polarized transmissions are received by the respective antenna and retransmitted in the same polarization. For example, transmissions may be made in the 3.7 to 3.74 GHz range on the vertical polarization path and in the 3.72 to 3.76 GHz range on the horizontal polarization path without adjacent frequency (co-channel) interference.

*[Figure 4-8: Satellite transmissions using polarized radiation (figure not reproduced).]*

Advantages of Vertical Polarization Simple vertical antennas can be used to provide OMNIDIRECTIONAL (all directions) communication. This is an advantage when communications must take place from a moving vehicle. In some overland communications, such as in vehicular installations, antenna heights are limited to 3 meters (10 feet) or less. In such instances vertical polarization results in a stronger receiver signal than does horizontal polarization at frequencies up to about 50 megahertz. From approximately 50 to 100 megahertz, vertical polarization results in a slightly stronger signal than does horizontal polarization with antennas at the same height. Above 100 megahertz, the difference in signal strength is negligible. For transmission over bodies of water, vertical polarization is much better than horizontal polarization for antennas at the lower heights. As the frequency increases, the minimum antenna height decreases. At 30 megahertz, vertical polarization is better for antenna heights below about 91 meters (300 feet); at 85 megahertz, antenna heights below 15 meters (50 feet); and still lower heights at the high frequencies. Therefore, at ordinary antenna mast heights of 12 meters (40 feet), vertical polarization is advantageous for frequencies less than about 100 megahertz.

Radiation is somewhat less affected by reflections from aircraft flying over the transmission path when vertical polarization is used instead of horizontal polarization. With horizontal polarization, such reflections cause variations in received signal strength. This factor is important in locations where aircraft traffic is heavy.

When vertical polarization is used, less interference is produced or picked up because of strong vhf and uhf broadcast transmissions (television and fm). This is because vhf and uhf transmissions use horizontal polarization. This factor is important when an antenna must be located in an urban area having several television and fm broadcast stations.

Advantages of Horizontal Polarization A simple horizontal antenna is bi-directional. This characteristic is useful when you desire to minimize interference from certain directions. Horizontal antennas are less likely to pick up man-made interference, which ordinarily is vertically polarized.

When antennas are located near dense forests or among buildings, horizontally polarized waves suffer lower losses than vertically polarized waves, especially above 100 megahertz. Small changes in antenna locations do not cause large variations in the field intensity of horizontally polarized waves. When vertical polarization is used, a change of only a few meters in the antenna location may have a considerable effect on the received signal strength. This is the result of interference patterns that produce standing waves in space when spurious reflections from trees or buildings occur.

When simple antennas are used, the transmission line, which is usually vertical, is less affected by a horizontally mounted antenna. When the antenna is mounted at right angles to the transmission line and horizontal polarization is used, the line is kept out of the direct field of the antenna. As a result, the radiation pattern and electrical characteristics of the antenna are practically unaffected by the presence of the vertical transmission line.

**Q10.** What type of polarization should be used at medium and low frequencies?

**Q11.** What is an advantage of using horizontal polarization at high frequencies?

**Q12.** What type of polarization should be used if an antenna is mounted on a moving vehicle at frequencies below 50 megahertz?

### Radiation Resistance

Radiated energy is the useful part of the transmitter's signal. However, it represents as much of a loss to the antenna as the energy lost in heating the antenna wire. In either case, the dissipated power is equal to I2R. In the case of heat losses, the R is real resistance. In the case of radiation, R is an assumed resistance; if this resistance were actually present, it would dissipate the same amount of power that the antenna takes to radiate the energy. This assumed resistance is referred to as the RADIATION

### Resistance.

Radiation resistance varies at different points on the antenna. This resistance is always measured at a current loop. For the antenna in free space, that is, entirely removed from any objects that might affect its operation, the radiation resistance is 73 ohms. A practical antenna located over a ground plane may have any value of radiation resistance from 0 to approximately 100 ohms. The exact value of radiation resistance depends on the height of the antenna above the ground. For most half-wave wire antennas, the radiation resistance is about 65 ohms. It will usually vary between 55 and 600 ohms for antennas constructed of rod or tubing. The actual value of radiation resistance, so long as it is 50 ohms or more, has little effect on the radiation efficiency of the antenna. This is because the ohmic resistance is about 1 ohm for conductors of large diameter. The ohmic resistance does not become important until the radiation resistance drops to a value less than 10 ohms. This may be the case when several antennas are coupled together.

### Radiation Types And Patterns

The energy radiated from an antenna forms a field having a definite RADIATION PATTERN. A radiation pattern is a plot of the radiated energy from an antenna. This energy is measured at various angles at a constant distance from the antenna. The shape of this pattern depends on the type of antenna used. In this section, we will introduce the basic types of radiation (isotropic and anisotropic) and their radiation patterns.

Isotropic Radiation Some antenna sources radiate energy equally in all directions. Radiation of this type is known as ISOTROPIC RADIATION. We all know the Sun radiates energy in all directions. The energy radiated from the Sun measured at any fixed distance and from any angle will be approximately the same. Assume that a measuring device is moved around the Sun and stopped at the points indicated in figure 4-9 to make a measurement of the amount of radiation. At any point around the circle, the distance from the measuring device to the Sun is the same. The measured radiation will also be the same. The Sun is therefore considered an isotropic radiator.

*[Figure 4-9: Isotropic radiator (figure not reproduced).]*

To plot this pattern, we will assume that the radiation is measured on a scale of 0 to 10 units and that the measured amount of radiation is 7 units at all points. We will then plot our measurements on two different types of graphs, rectangular- and polar-coordinate graphs. The RECTANGULAR- COORDINATE GRAPH of the measured radiation, shown in view A of figure 4-10, is a straight line plotted against positions along the circle. View B shows the POLAR-COORDINATE GRAPH for the same isotropic source.

*[Figure 4-10: Comparison of rectangular- and polar-coordinate graphs for an isotropic source (figure not reproduced).]*

In the rectangular-coordinate graph, points are located by projection from a pair of stationary, perpendicular axes. In the polar-coordinate graph, points are located by projection along a rotating axis (radius) to an intersection with one of several concentric, equally-spaced circles. The horizontal axis on the rectangular-coordinate graph corresponds to the circles on the polar-coordinate graph. The vertical axis on the rectangular-coordinate graph corresponds to the rotating axis (radius) on the polar-coordinate graph.

Rectangular-Coordinate Pattern Look at view A of figure 4-10. The numbered positions around the circle are laid out on the HORIZONTAL AXIS of the graph from 0 to 7 units. The measured radiation is laid out on the VERTICAL AXIS of the graph from 0 to 10 units. The units on both axes are chosen so the pattern occupies a convenient part of the graph.

The horizontal and vertical axes are at a right angle to each other. The point where the axes cross each other is known as the ORIGIN. In this case, the origin is 0 on both axes. Now, assume that a radiation value of 7 units view B is measured at position 2. From position 2 on the horizontal axis, a dotted line is projected upwards that runs parallel to the vertical axis. From position 7 on the vertical axis, a line is projected to the right that runs parallel to the horizontal axis. The point where the two lines cross (INTERCEPT) represents a value of 7 radiation units at position 2. This is the only point on the graph that can represent this value.

As you can see from the figure, the lines used to plot the point form a rectangle. For this reason, this type of plot is called a rectangular-coordinate graph. A new rectangle is formed for each different point plotted. In this example, the points plotted lie in a straight line extending from 7 units on the vertical scale to the projection of position 7 on the horizontal scale. This is the characteristic pattern in rectangular coordinates of an isotropic source of radiation.

Polar-Coordinate Pattern The polar-coordinate graph has proved to be of great use in studying radiation patterns. Compare views A and B of figure 4-10. Note the great difference in the shape of the radiation pattern when it is transferred from the rectangular-coordinate graph in view A to the polar-coordinate graph in view B. The scale of radiation values used in both graphs is identical, and the measurements taken are both the same. However, the shape of the pattern is drastically different.

Look at view B of figure 4-10 and assume that the center of the concentric circles is the Sun. Assume that a radius is drawn from the Sun (center of the circle) to position 0 of the circle. When you move to position 1, the radius moves to position 1; when you move to position 2, the radius also moves to position 2, and so on.

The positions where a measurement was taken are marked as 0 through 7 on the graph. Note how the position of the radius indicates the actual direction from the source at which the measurement was taken. This is a distinct advantage over the rectangular-coordinate graph in which the position is indicated along a straight-line axis and has no physical relation to the actual direction of measurement. Now that we have a way to indicate the direction of measurement, we must devise a way to indicate the magnitude of the radiation.

Notice that the rotating axis is always drawn from the center of the graph to some position on the edge of the graph. As the axis moves toward the edge of the graph, it passes through a set of equally-spaced, concentric circles. In this example view B, they are numbered successively from 1 to 10 from the center out. These circles are used to indicate the magnitude of the radiation. The advantages of the polar-coordinate graph are immediately evident. The source, which is at the center of the observation circles, is also at the center of the graph. By looking at a polar-coordinate plot of a radiation pattern, you can immediately see the direction and strength of radiation put out by the source. Therefore, the polar-coordinate graph is more useful than the rectangular-coordinate graph in plotting radiation patterns.

Anisotropic Radiation Most radiators emit (radiate) stronger radiation in one direction than in another. A radiator such as this is referred to as ANISOTROPIC. An example of an anisotropic radiator is an ordinary flashlight. The beam of the flashlight lights only a portion of the space surrounding it. If a circle is drawn with the flashlight as the center, as shown in view B of figure 4-11, the radiated light can be measured at different positions around the circle. Again, as with the isotropic radiator, all positions are the same distance from the center, but at different angles. However, in this illustration the radiated light is measured at 16 different positions on the circle.

*[Figure 4-11: Anisotropic radiator (figure not reproduced).]*

Directly behind the flashlight (position 0) the radiation measured is minimum. Accordingly, a 0 value is assigned to this position in the rectangular-coordinate graph (fig. 4-11, view A). This radiation remains at minimum until position 4 is reached. Between positions 4 and 6, the measuring device enters the flashlight beam. You can see this transition from darkness to brightness easily in view B. Radiation is fairly constant between positions 6 and 10. Maximum brightness occurs at position 8, which is directly in the path of the flashlight beam. From positions 10 to 12, the measuring device leaves the flashlight beam and the radiation measurement falls off sharply. At position 13 the radiation is again at 0 and stays at this value back to position 0.

Radiation from a light source and radiation from an antenna are both forms of electromagnetic waves. Therefore, the measurement of radiation of an antenna follows the same basic procedure as that just described for the Sun and the flashlight. Before proceeding further with the study of antenna patterns, you should be sure you understand the methods used to graph the measured radiation (magnitude of the radiation). Study the rectangular- and polar-coordinate systems of plotting presented in the following section.

**Q13.** What is the radiation resistance of a half-wave antenna in free space?

**Q14.** A radiating source that radiates energy stronger in one direction than another is known as what type of radiator?

**Q15.** A radiating source that radiates energy equally in all directions is known as what type of radiator?

**Q16.** A flashlight is an example of what type of radiator?

In figure 4-11, view A, the radiation pattern of the flashlight is graphed in rectangular coordinates. The illustration of the flashlight beam in view B clearly indicates the shape of the flashlight beam. This is not evident in the radiation pattern plotted on the rectangular-coordinate graph. Now look at figure 4-12. The radiation pattern shown in this figure looks very much like the actual flashlight beam. The pattern in figure 4-12 is plotted using the same values as those of figure 4-11, view A, but is drawn using polar coordinates.

*[Figure 4-12: Polar-coordinate graph for anisotropic radiator (figure not reproduced).]*

The positions marked off on the two polar-coordinate graphs in figures 4-10 and 4-12 were selected and numbered arbitrarily. However, a standard method allows the positions around a source to be marked off so that one radiation pattern can easily be compared with another. This method is based on the fact that a circle has a radius of 360 degrees. The radius extending vertically from the center (position 0 in figure 4-10) is designated 0 degrees. At position 4 the radius is at a right angle to the 0-degree radius. Accordingly, the radius at position 4 is marked 90 degrees, position 8 is 180 degrees, position 12 is 270 degrees, and position 16 is 360 degrees. The various radii drawn on the graph are all marked according to the angle each radius makes with the reference radius at 0 degrees.

The radiation pattern in figure 4-12 is obtained by using the same procedure that was used for (figure 4-10, view B). The radiation measured at positions 1, 2, 3, and 4 is 0. Position 5 measures approximately 1 unit. This is marked on the graph and the rotating radius moves to position 6. At this position a reading of 5.5 units is taken. As before, this point is marked on the graph. The procedure is repeated around the circle and a reading is obtained from positions 6 through 11. At position 12 no radiation is indicated, and this continues on to position 16.

The polar-coordinate graph now shows a definite area enclosed by the radiation pattern. This pattern indicates the general direction of radiation from the source. The enclosed area is called a LOBE. Outside of this area, minimum radiation is emitted in any direction. For example, at position 2 the radiation is 0. Such a point is called a NULL. In real situations, some radiation is usually transmitted in all directions. Therefore, a null is used to indicate directions of minimum radiation. The pattern of figure 4-12 shows one lobe and one continuous null.

### Antenna Loading

You will sometimes want to use one antenna system for transmitting and receiving on several different frequencies. Since the antenna must always be in resonance with the applied frequency, you may need to either physically or electrically lengthen or shorten the antenna.

Except for trailing-wire antennas used in aircraft installations (which may be lengthened or shortened), physically lengthening the antenna is not very practical. But you can achieve the same result by changing the electrical length of the antenna. To change the electrical length, you can insert either an inductor or a capacitor in series with the antenna. This is shown in figure 4-13, views A and B. Changing the electrical length by this method is known as LUMPED-IMPEDANCE TUNING, or LOADING. The electrical length of any antenna wire can be increased or decreased by loading. If the antenna is too short for the wavelength being used, it is resonant at a higher frequency than that at which it is being excited. Therefore, it offers a capacitive reactance at the excitation frequency. This capacitive reactance can be compensated for by introducing a lumped-inductive reactance, as shown in view A. Similarly, if the antenna is too long for the transmitting frequency, it offers an inductive reactance. Inductive reactance can be compensated for by introducing a lumped-capacitive reactance, as shown in view B. An antenna without loading is represented in view C.

*[Figure 4-13: Electrically equal antenna (figure not reproduced).]*

### Basic Antennas

Before you look at the various types of antennas, consider the relationship between the wavelength at which the antenna is being operated and the actual length of the antenna. An antenna does not necessarily radiate or receive more energy when it is made longer. Specific dimensions must be used for efficient antenna operation.

Nearly all antennas have been developed from two basic types, the Hertz and the Marconi. The basic Hertz antenna is 1/2 wavelength long at the operating frequency and is insulated from ground. It is often called a DIPOLE or a DOUBLET. The basic Marconi antenna is 1/4 wavelength long and is either grounded at one end or connected to a network of wires called a COUNTERPOISE. The ground or counterpoise provides the equivalent of an additional 1/4 wavelength, which is required for the antenna to resonate.

### Half-Wave Antennas

A half-wave antenna (referred to as a dipole, Hertz, or doublet) consists of two lengths of wire rod, or tubing, each 1/4 wavelength long at a certain frequency. It is the basic unit from which many complex antennas are constructed. The half-wave antenna operates independently of ground; therefore, it may be installed far above the surface of the Earth or other absorbing bodies. For a dipole, the current is maximum at the center and minimum at the ends. Voltage is minimum at the center and maximum at the ends, as was shown in figure 4-6.

Radiation Patterns In the following discussion, the term DIPOLE is used to mean the basic half-wave antenna. The term DOUBLET is used to indicate an antenna that is very short compared with the wavelength of the operating frequency. Physically, it has the same shape as the dipole.

RADIATION PATTERN OF A DOUBLET.—The doublet is the simplest form of a practical antenna. Its radiation pattern can be plotted like the radiation pattern of the flashlight (fig. 4-12). Figure 4-14 shows the development of vertical and horizontal patterns for a doublet. This in NOT a picture of the radiation, but three-dimensional views of the pattern itself. In three views the pattern resembles a doughnut. From the dimensions in these views, two types of polar-coordinate patterns can be drawn, horizontal and vertical. The HORIZONTAL PATTERN view A is derived from the solid pattern view C by slicing it horizontally. This produces view B, which is converted to the polar coordinates seen in view A. The horizontal pattern illustrates that the radiation is constant in any direction along the horizontal plane.

*[Figure 4-14: Development of vertical and horizontal patterns (figure not reproduced).]*

A VERTICAL PATTERN view E is obtained from the drawing of the vertical plane view D of the radiation pattern view C. The radiation pattern view C is sliced in half along a vertical plane through the antenna. This produces the vertical plane pattern in view D. Note how the vertical plane in view D of the radiation pattern differs from the horizontal plane in view B. The vertical pattern view E exhibits two lobes and two nulls. The difference between the two patterns is caused by two facts: (1) no radiation is emitted from the ends of the doublet; and (2) maximum radiation comes from the doublet in a direction perpendicular to the antenna axis. This type of radiation pattern is both NONDIRECTIONAL (in a horizontal plane) and DIRECTIONAL (in a vertical plane).

From a practical viewpoint, the doublet antenna can be mounted either vertically or horizontally. The doublet shown in figure 4-14 is mounted vertically, and the radiated energy spreads out about the antenna in every direction in the horizontal plane. Since ordinarily the horizontal plane is the useful plane, this arrangement is termed NONDIRECTIONAL. The directional characteristics of the antenna in other planes is ignored. If the doublet were mounted horizontally, it would have the effect of turning the pattern on edge, reversing the patterns given in figure 4-14. The antenna would then be directional in the horizontal plane. The terms "directional" and "nondirectional" are used for convenience in describing specific radiation patterns. A complete description always involves a figure in three dimensions, as in the radiation pattern of figure 4-14.

**Q17.** What terms are often used to describe basic half-wave antennas?

**Q18.** If a basic half-wave antenna is mounted vertically, what type of radiation pattern will be produced?

**Q19.** In which plane will the half-wave antenna be operating if it is mounted horizontally? RADIATION PATTERN OF A DIPOLE.—The radiation pattern of a dipole (fig. 4-15) is similar to that of the doublet (fig. 4-14). Increasing the length of the doublet to 1/2 wavelength has the effect of flattening out the radiation pattern. The radiation pattern in the horizontal plane of a dipole is a larger circle than that of the doublet. The vertical-radiation pattern lobes are no longer circular. They are flattened out and the radiation intensity is greater.

*[Figure 4-15: Radiation pattern of a dipole (figure not reproduced).]*

Methods of Feeding Energy to an Antenna Voltage and current distribution for the half-wave antenna (shown in figure 4-16) is the same as that for the antenna discussed earlier in this chapter. A point closely related to the voltage and current distribution on an antenna is the method of feeding the transmitter output to the antenna. The simplest method of feeding energy to the half-wave antenna is to connect one end through a capacitor to the final output stage of the transmitter. This method is often called the END-FEED or VOLTAGE-FEED method. In this method the antenna is fed at a point of high voltage (the end).

*[Figure 4-16: Standing waves of current and voltage (figure not reproduced).]*

Energy may also be fed to the half-wave antenna by dividing the antenna at its center and connecting the transmission line from the final transmitter output stage to the two center ends of the halved antenna. Since the antenna is now being fed at the center (a point of low voltage and high current), this type of feed is known as the CENTER-FEED or CURRENT-FEED method. The point of feed is important in determining the type of transmission line to be used.
