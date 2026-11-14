---
id: radio/neets10-ch2-radio-wave-propagation-1
title: "NEETS Module 10 Ch. 2: Radio Wave Propagation (1/2)"
category: radio
tags: [radio, propagation, ionosphere, MUF, fading, ducting]
summary: "Navy training text on how radio waves travel: ground and sky waves, the ionosphere, MUF/LUF/FOT, fading and tropospheric effects."
source: "https://archive.org/details/NEETSModule10"
source_title: "Navy Electricity and Electronics Training Series (NEETS) Module 10: Introduction to Wave Propagation, Transmission Lines, and Antennas (NAVEDTRA 14182)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Naval Education and Training Professional Development and Technology Center, US Navy. US Government work; 'Approved for public release; distribution is unlimited.' Figures not reproduced; end-of-chapter question answers retained."
retrieved: 2026-11-13
part_of: radio/neets10
order: 21
---

# NEETS Module 10 Ch. 2: Radio Wave Propagation (1/2)

## Learning Objectives

Upon completion of this unit, you should be able to:

1. State what the electromagnetic field is and what components make up the electromagnetic field.

2. State the difference between the induction field and the radiation field.

3. State what radio waves are.

4. List the components of a radio wave and define the terms cycle, frequency, harmonics, period, wavelength, and velocity as applied to radio wave propagation.

5. Compute the wavelength of radio waves.

6. State how radio waves are polarized, vertically and horizontally.

7. State what reflection, refraction, and diffraction are as applied to radio waves.

8. State what influence the Earth's atmosphere has on radio waves and list the different layers of the Earth's atmosphere.

9. Identify a ground wave, a sky wave, and state the effects of the ionosphere on the sky wave.

10. Identify the structure of the ionosphere.

11. Define density of layer, frequency, angle of incidence, skip distance, and skip zone.

12. Describe propagation paths.

13. Describe fading, multipath fading, and selective fading. Describe propagation paths.

14. State how transmission losses affect radio wave propagation.

15. State how electromagnetic interference, man-made/natural interference, and ionospheric disturbances affect radio wave propagation. State how transmission losses affect radio wave propagation.

16. Identify variations in the ionosphere.

17. Identify the maximum, optimum, and lowest usable frequencies of radio waves.

18. State what temperature inversion is, how frequency predictions are made, and how weather affects frequency.

19. State what tropospheric scatter is and how it affects radio wave propagation.

### Electromagnetic Fields

The way energy is propagated into free space is a source of great dispute among people concerned with it. Although many theories have been proposed, the following theory adequately explains the phenomena and has been widely accepted. There are two basic fields associated with every antenna; an INDUCTION FIELD and a RADIATION FIELD. The field associated with the energy stored in the antenna is the induction field. This field is said to provide no part in the transmission of electromagnetic energy through free space. However, without the presence of the induction field, there would be no energy radiated.

### Induction Field

*[Figure 2-1: , a low-frequency generator connected to an antenna, will help you understand how the induction field is produced. Let's follow the generator through one cycle of operation (figure not reproduced).]*

*[Figure 2-1: Induction field about an antenna (figure not reproduced).]*

Initially, you can consider that the generator output is zero and that no fields exist about the antenna, as shown in view A. Now assume that the generator produces a slight potential and has the instantaneous polarity shown in view B. Because of this slight potential, the antenna capacitance acts as a short, allowing a large flow of current (I) through the antenna in the direction shown. This current flow, in turn, produces a large magnetic field about the antenna. Since the flow of current at each end of the antenna is minimum, the corresponding magnetic fields at each end of the antenna are also minimum. As time passes, charges, which oppose antenna current and produce an electrostatic field (E field), collect at each end of the antenna. Eventually, the antenna capacitance becomes fully charged and stops current flow through the antenna. Under this condition, the electrostatic field is maximum, and the magnetic field (H field) is fully collapsed, as shown in view C.

As the generator potential decreases back to zero, the potential of the antenna begins to discharge. During the discharging process, the electrostatic field collapses and the direction of current flow reverses, as shown in view D. When the current again begins to flow, an associated magnetic field is generated. Eventually, the electrostatic field completely collapses, the generator potential reverses, and current is maximum, as shown in view E. As charges collect at each end of the antenna, an electrostatic field is produced and current flow decreases. This causes the magnetic field to begin collapsing. The collapsing magnetic field produces more current flow, a greater accumulation of charge, and a greater electrostatic field. The antenna gradually reaches the condition shown in view F, where current is zero and the collected charges are maximum.

As the generator potential again decreases toward zero, the antenna begins to discharge and the electrostatic field begins to collapse. When the generator potential reaches zero, discharge current is maximum and the associated magnetic field is maximum. A brief time later, generator potential reverses, and the condition shown in view B recurs.

**Note:** The electric field (E field) and the electrostatic field (E field) are the same. They will be used interchangeably throughout this text.

The graph shown in figure 2-2 shows the relationship between the magnetic (H) field and the electric (E) field plotted against time. Note that the two fields are 90 degrees out of phase with each other. If you compare the graph in figure 2-2 with figure 2-1, you will notice that the two fields around the antenna are displaced 90 degrees from each other in space. (The H field exists in a plane perpendicular to the antenna. The E field exists in a plane parallel with the antenna, as shown in figure 2-1.)

*[Figure 2-2: Phase relationship of induction field components (figure not reproduced).]*

All the energy supplied to the induction field is returned to the antenna by the collapsing E and H fields. No energy from the induction field is radiated from the antenna. Therefore, the induction field is considered a local field and plays no part in the transmission of electromagnetic energy. The induction field represents only the stored energy in the antenna and is responsible only for the resonant effects that the antenna reflects to the generator.

### Radiation Fields

The E and H fields that are set up in the transfer of energy through space are known collectively as the radiation field. This radiation field is responsible for electromagnetic radiation from the antenna. The radiation field decreases as the distance from the antenna is increased. Because the decrease is linear, the radiation field reaches great distances from the antenna.

Let's look at a half-wave antenna to illustrate how this radiation actually takes place. Simply stated, a half-wave antenna is one that has an electrical length equal to half the wavelength of the signal being transmitted. Assume, for example, that a transmitter is operating at 30 megahertz. If a half-wave antenna is used with the transmitter, the antenna's electrical length would have to be at least 16 feet long. (The formula used to compute the electrical length of an antenna will be explained in chapter 4.) When power is delivered to the half-wave antenna, both an induction field and a radiation field are set up by the fluctuating energy. At the antenna, the intensities of these fields are proportional to the amount of power delivered to the antenna from a source such as a transmitter. At a short distance from the antenna and beyond, only the radiation field exists. This radiation field is made up of an electric component and a magnetic component at right angles to each other in space and varying together in intensity. With a high-frequency generator (a transmitter) connected to the antenna, the induction field is produced as described in the previous section. However, the generator potential reverses before the electrostatic field has had time to collapse completely. The reversed generator potential neutralizes the remaining antenna charges, leaving a resultant E field in space.

*[Figure 2-3: is a simple picture of an E field detaching itself from an antenna. (The H field will not be considered, although it is present.) In view A the voltage is maximum and the electric field has maximum intensity. The lines of force begin at the end of the antenna that is positively charged and extend to the end of the antenna that is negatively charged. Note that the outer E lines are stretched away from the inner lines. This is because of the repelling force that takes place between lines of force in the same direction (figure not reproduced).]*

As the voltage drops (view B), the separated charges come together, and the ends of the lines move toward the center of the antenna. But, since lines of force in the same direction repel each other, the centers of the lines are still being held out.

*[Figure 2-3: Radiation from an antenna (figure not reproduced).]*

As the voltage approaches zero (view B), some of the lines collapse back into the antenna. At the same time, the ends of other lines begin to come together to form a complete loop. Notice the direction of these lines of force next to the antenna in view C. At this point the voltage on the antenna is zero. As the charge starts to build up in the opposite direction (view D), electric lines of force again begin at the positive end of the antenna and stretch to the negative end of the antenna. These lines of force, being in the same direction as the sides of the closed loops next to the antenna, repel the closed loops and force them out into space at the speed of light. As these loops travel through space, they generate a magnetic field in phase with them.

Since each successive E field is generated with a polarity that is opposite the preceding E field (that is, the lines of force are opposite), an oscillating electric field is produced along the path of travel. When an electric field oscillates, a magnetic field having an intensity that varies directly with that of the E field is produced. The variations in magnetic field intensity, in turn, produce another E field. Thus, the two varying fields sustain each other, resulting in electromagnetic wave propagation.

During this radiation process, the E and H fields are in phase in time but physically displaced 90 degrees in space. Thus, the varying magnetic field produces a varying electric field; and the varying electric field, in turn, sustains the varying magnetic field. Each field supports the other, and neither can be propagated by itself. Figure 2-4 shows a comparison between the induction field and the radiation field.

*[Figure 2-4: E and H components of induction and radiation fields (figure not reproduced).]*

**Q1.** Which two composite fields (composed of E and H fields) are associated with every antenna?

**Q2.** What composite field (composed of E and H fields) is found stored in the antenna?

**Q3.** What composite field (composed of E and H fields) is propagated into free space?

### Radio Waves

An energy wave generated by a transmitter is called a RADIO WAVE. The radio wave radiated into space by the transmitting antenna is a very complex form of energy containing both electric and magnetic fields. Because of this combination of fields, radio waves are also referred to as ELECTROMAGNETIC

### Radiation.

This discussion will explain the Earth's atmosphere and its effect on radio waves. All the principles of wave motion that were discussed in chapter 1 also apply to radio waves.

**Note:** The term radio wave is not limited to communications equipment alone. The term applies to all equipment that generate signals in the form of electromagnetic energy.

### Components Of Radio Waves

The basic shape of the wave generated by a transmitter is that of a sine wave. The wave radiated out into space, however, may or may not retain the characteristics of the sine wave.

A sine wave can be one cycle or many cycles. Recall from chapter 1 that the number of cycles of a sine wave that are completed in 1 second is known as the frequency of the sine wave. For example, 60 cycles of ordinary house current occur each second, so house current is said to have a frequency of 60 cycles per second or 60 hertz.

The frequencies falling between 3000 hertz (3 kHz) and 300,000,000,000 hertz (300 GHz) are called RADIO FREQUENCIES (abbreviated rf) since they are commonly used in radio communications. This part of the radio frequency spectrum is divided into bands, each band being 10 times higher in frequency than the one immediately below it. This arrangement serves as a convenient way to remember the range of each band. The rf bands are shown in table 2-1. The usable radio-frequency range is roughly 10 kilohertz to 100 gigahertz.

*[Table 2-1: Radio Frequency Bands (table not reproduced).]*

### Description Abbreviation Frequency

Very low VLF 3 to 30 KHz Low LF 30 to 300 KHz Medium MF 300 to 3000 KHz High HF 3 to 30 MHz Very high VHF 30 to 300 MHz Ultrahigh UHF 300 to 3000 MHz Super high SHF 3 to 30 GHz Extremely high EHF 30 to 300 GHz Any frequency that is a whole number multiple of a smaller basic frequency is known as a HARMONIC of that basic frequency. The basic frequency itself is called the first harmonic or, more commonly, the FUNDAMENTAL FREQUENCY. A frequency that is twice as great as the fundamental frequency is called the second harmonic; a frequency three times as great is the third harmonic; and so on. For example:

First harmonic (Fundamental frequency) 3000 kHz Second harmonic 6000 kHz Third harmonic 9000 kHz The PERIOD of a radio wave is simply the amount of time required for the completion of one full cycle. If a sine wave has a frequency of 2 hertz, each cycle has a duration, or period, of one-half second. If the frequency is 10 hertz, the period of each cycle is one-tenth of a second. Since the frequency of a radio wave is the number of cycles that are completed in one second, you should be able to see that as the frequency of a radio wave increases, its period decreases.

A wavelength is the space occupied by one full cycle of a radio wave at any given instant. Wavelengths are expressed in meters (1 meter is equal to 3.28 feet). You need to have a good understanding of frequency and wavelength to be able to select the proper antenna(s) for use in successful communications. The relationship between frequency, wavelength, and antennas will be discussed in chapter 4 of this module.

The velocity (or speed) of a radio wave radiated into free space by a transmitting antenna is equal to the speed of light—186,000 miles per second or 300,000,000 meters per second. Because of various factors, such as barometric pressure, humidity, molecular content, etc., radio waves travel inside the Earth's atmosphere at a speed slightly less than the speed of light. Normally, in discussions of the velocity of radio waves, the velocity referred to is the speed at which radio waves travel in free space. The frequency of a radio wave has nothing to do with its velocity. A 5-megahertz wave travels through space at the same velocity as a 10-megahertz wave. However, the velocity of radio waves is an important factor in making wavelength-to-frequency conversions, the subject of our next discussion.

**Q4.** What is the term used to describe the basic frequency of a radio wave?

**Q5.** What is the term used to describe a whole number multiple of the basic frequency of a radio wave?

### Wavelength-To-Frequency Conversions

Radio waves are often referred to by their wavelength in meters rather than by frequency. For example, most people have heard commercial radio stations make announcements similar to the following: "Station WXYZ operating on 240 meters..." To tune receiving equipment that is calibrated by frequency to such a station, you must first convert the designated wavelength to its equivalent frequency. As discussed earlier, a radio wave travels 300,000,000 meters a second (speed of light); therefore, a radio wave of 1 hertz would have traveled a distance (or wavelength) of 300,000,000 meters. Obviously then, if the frequency of the wave is increased to 2 hertz, the wavelength will be cut in half to 150,000,000 meters. This illustrates the principle that the HIGHER THE FREQUENCY, the SHORTER

### The Wavelength.

Wavelength-to-frequency conversions of radio waves are really quite simple because wavelength and frequency are reciprocals: Either one divided into the velocity of a radio wave yields the other. Remember, the formula for wavelength is:

The wavelength in meters divided into 300,000,000 yields the frequency of a radio wave in hertz. Likewise, the wavelength divided into 300,000 yields the frequency of a radio wave in kilohertz, and the wavelength divided into 300 yields the frequency in megahertz.

Now, let us apply the formula to determine the frequency to which the receiving equipment must be tuned to receive station WXYZ operating on 240 meters. Radio wave frequencies are normally expressed in kilohertz or megahertz.

To find the frequency in hertz, use the formula:

To find the frequency in kilohertz, use the formula:

To find the frequency in megahertz, use the formula:

**Q6.** It is known that WWV operates on a frequency of 10 megahertz. What is the wavelength of WWV?

**Q7.** A station is known to operate at 60-meters. What is the frequency of the unknown station?

### Polarization

For maximum absorption of energy from the electromagnetic fields, the receiving antenna must be located in the plane of polarization. This places the conductor of the antenna at right angles to the magnetic lines of force moving through the antenna and parallel to the electric lines, causing maximum induction.

Normally, the plane of polarization of a radio wave is the plane in which the E field propagates with respect to the Earth. If the E field component of the radiated wave travels in a plane perpendicular to the Earth's surface (vertical), the radiation is said to be VERTICALLY POLARIZED, as shown in figure 2-5, view A. If the E field propagates in a plane parallel to the Earth's surface (horizontal), the radiation is said to be HORIZONTALLY POLARIZED, as shown in view B.

*[Figure 2-5: Vertical and horizontal polarization (figure not reproduced).]*

The position of the antenna in space is important because it affects the polarization of the electromagnetic wave. When the transmitting antenna is close to the ground, vertically polarized waves cause a greater signal strength along the Earth's surface. On the other hand, antennas high above the ground should be horizontally polarized to get the greatest possible signal strength to the Earth's surface. Vertically and horizontally polarized antennas will be discussed in more detail in chapter 4. The radiated energy from an antenna is in the form of an expanding sphere. Any small section of this sphere is perpendicular to the direction the energy travels and is called a WAVEFRONT. All energy on a wavefront is in phase. Usually all points on the wavefront are at equal distances from the antenna. The farther the wavefront is from the antenna, the less spherical the wave appears. At a considerable distance the wavefront can be considered as a plane surface at a right angle to the direction of propagation. If you know the directions of the E and H components, you can use the "right-hand rule" (see figure 2-6) to determine the direction of wave propagation. This rule states that if the thumb, forefinger, and middle finger of the right hand are extended so they are mutually perpendicular, the middle finger will point in the direction of wave propagation if the thumb points in the direction of the E field and the forefinger points in the direction of the H field. Since both the E and H fields reverse directions simultaneously, propagation of a particular wavefront is always in the same direction (away from the antenna).

*[Figure 2-6: Right-hand rule for propagation (figure not reproduced).]*

**Q8.** If a transmitting antenna is placed close to the ground, how should the antenna be polarized to give the greatest signal strength?

**Q9.** In the right-hand rule for propagation, the thumb points in the direction of the E field and the forefinger points in the direction of the H field. In what direction does the middle finger point?

### Atmospheric Propagation

Within the atmosphere, radio waves can be reflected, refracted, and diffracted like light and heat waves.

Reflection Radio waves may be reflected from various substances or objects they meet during travel between the transmitting and receiving sites. The amount of reflection depends on the reflecting material. Smooth metal surfaces of good electrical conductivity are efficient reflectors of radio waves. The surface of the Earth itself is a fairly good reflector. The radio wave is not reflected from a single point on the reflector but rather from an area on its surface. The size of the area required for reflection to take place depends on the wavelength of the radio wave and the angle at which the wave strikes the reflecting substance. When radio waves are reflected from flat surfaces, a phase shift in the alternations of the wave occurs. Figure 2-7 shows two radio waves being reflected from the Earth's surface. Notice that the positive and negative alternations of radio waves (A) and (B) are in phase with each other in their paths toward the Earth's surface. After reflection takes place, however, the waves are approximately 180 degrees out of phase from their initial relationship. The amount of phase shift that occurs is not constant. It depends on the polarization of the wave and the angle at which the wave strikes the reflecting surface. Radio waves that keep their phase relationships after reflection normally produce a stronger signal at the receiving site. Those that are received out of phase produce a weak or fading signal. The shifting in the phase relationships of reflected radio waves is one of the major reasons for fading. Fading will be discussed in more detail later in this chapter.

*[Figure 2-7: Phase shift of reflected radio waves (figure not reproduced).]*

Refraction Another phenomenon common to most radio waves is the bending of the waves as they move from one medium into another in which the velocity of propagation is different. This bending of the waves is called refraction. For example, suppose you are driving down a smoothly paved road at a constant speed and suddenly one wheel goes off onto the soft shoulder. The car tends to veer off to one side. The change of medium, from hard surface to soft shoulder, causes a change in speed or velocity. The tendency is for the car to change direction. This same principle applies to radio waves as changes occur in the medium through which they are passing. As an example, the radio wave shown in figure 2-8 is traveling through the Earth's atmosphere at a constant speed. As the wave enters the dense layer of electrically charged ions, the part of the wave that enters the new medium first travels faster than the parts of the wave that have not yet entered the new medium. This abrupt increase in velocity of the upper part of the wave causes the wave to bend back toward the Earth. This bending, or change of direction, is always toward the medium that has the lower velocity of propagation.

*[Figure 2-8: Radio wave refraction (figure not reproduced).]*

Radio waves passing through the atmosphere are affected by certain factors, such as temperature, pressure, humidity, and density. These factors can cause the radio waves to be refracted. This effect will be discussed in greater detail later in this chapter.

Diffraction A radio wave that meets an obstacle has a natural tendency to bend around the obstacle as illustrated in figure 2-9. The bending, called diffraction, results in a change of direction of part of the wave energy from the normal line-of-sight path. This change makes it possible to receive energy around the edges of an obstacle as shown in view A or at some distances below the highest point of an obstruction, as shown in view B. Although diffracted rf energy usually is weak, it can still be detected by a suitable receiver. The principal effect of diffraction extends the radio range beyond the visible horizon. In certain cases, by using high power and very low frequencies, radio waves can be made to encircle the Earth by diffraction.

*[Figure 2-9: Diffraction around an object (figure not reproduced).]*

**Q10.** What is one of the major reasons for the fading of radio waves which have been reflected from a surface?

### The Effect Of The Earth'S Atmosphere On Radio Waves

This discussion of electromagnetic wave propagation is concerned mainly with the properties and effects of the medium located between the transmitting antenna and the receiving antenna. While radio waves traveling in free space have little outside influence affecting them, radio waves traveling within the Earth's atmosphere are affected by varying conditions. The influence exerted on radio waves by the Earth's atmosphere adds many new factors to complicate what at first seems to be a relatively simple problem. These complications are because of a lack of uniformity within the Earth's atmosphere. Atmospheric conditions vary with changes in height, geographical location, and even with changes in time (day, night, season, year). A knowledge of the composition of the Earth's atmosphere is extremely important for understanding wave propagation.

The Earth's atmosphere is divided into three separate regions, or layers. They are the TROPOSPHERE, the STRATOSPHERE, and the IONOSPHERE. The layers of the atmosphere are illustrated in figure 2-10.

*[Figure 2-10: Layers of the earth's atmosphere (figure not reproduced).]*

### Troposphere

The troposphere is the portion of the Earth's atmosphere that extends from the surface of the Earth to a height of about 3.7 miles (6 km) at the North Pole or the South Pole and 11.2 miles (18 km) at the equator. Virtually all weather phenomena take place in the troposphere. The temperature in this region decreases rapidly with altitude, clouds form, and there may be much turbulence because of variations in temperature, density, and pressure. These conditions have a great effect on the propagation of radio waves, which will be explained later in this chapter.

### Stratosphere

The stratosphere is located between the troposphere and the ionosphere. The temperature throughout this region is considered to be almost constant and there is little water vapor present. The stratosphere has relatively little effect on radio waves because it is a relatively calm region with little or no temperature changes.

### Ionosphere

The ionosphere extends upward from about 31.1 miles (50 km) to a height of about 250 miles (402 km). It contains four cloud-like layers of electrically charged ions, which enable radio waves to be propagated to great distances around the Earth. This is the most important region of the atmosphere for long distance point-to-point communications. This region will be discussed in detail a little later in this chapter.

**Q11.** What are the three layers of the atmosphere?

**Q12.** Which layer of the atmosphere has relatively little effect on radio waves?

### Radio Wave Transmission

There are two principal ways in which electromagnetic (radio) energy travels from a transmitting antenna to a receiving antenna. One way is by GROUND WAVES and the other is by SKY WAVES. Ground waves are radio waves that travel near the surface of the Earth (surface and space waves). Sky waves are radio waves that are reflected back to Earth from the ionosphere. (See figure 2-11.)

*[Figure 2-11: Ground waves and sky waves (figure not reproduced).]*

Ground Waves The ground wave is actually composed of two separate component waves. These are known as the SURFACE WAVE and the SPACE WAVE (fig. 2-11). The determining factor in whether a ground wave component is classified as a space wave or a surface wave is simple. A surface wave travels along the surface of the Earth. A space wave travels over the surface.

SURFACE WAVE.—The surface wave reaches the receiving site by traveling along the surface of the ground as shown in figure 2-12. A surface wave can follow the contours of the Earth because of the process of diffraction. When a surface wave meets an object and the dimensions of the object do not exceed its wavelength, the wave tends to curve or bend around the object. The smaller the object, the more pronounced the diffractive action will be.

*[Figure 2-12: Surface wave propagation (figure not reproduced).]*

As a surface wave passes over the ground, the wave induces a voltage in the Earth. The induced voltage takes energy away from the surface wave, thereby weakening, or attenuating, the wave as it moves away from the transmitting antenna. To reduce the attenuation, the amount of induced voltage must be reduced. This is done by using vertically polarized waves that minimize the extent to which the electric field of the wave is in contact with the Earth. When a surface wave is horizontally polarized, the electric field of the wave is parallel with the surface of the Earth and, therefore, is constantly in contact with it. The wave is then completely attenuated within a short distance from the transmitting site. On the other hand, when the surface wave is vertically polarized, the electric field is vertical to the Earth and merely dips into and out of the Earth's surface. For this reason, vertical polarization is vastly superior to horizontal polarization for surface wave propagation.

The attenuation that a surface wave undergoes because of induced voltage also depends on the electrical properties of the terrain over which the wave travels. The best type of surface is one that has good electrical conductivity. The better the conductivity, the less the attenuation. Table 2-2 gives the relative conductivity of various surfaces of the Earth.

*[Table 2-2: Surface Conductivity (table not reproduced).]*

### Surface Relative Conductivity

Sea water Good Flat, loamy soil Fair Large bodies of fresh water Fair Rocky terrain Poor Desert Poor Jungle Unusable Another major factor in the attenuation of surface waves is frequency. Recall from earlier discussions on wavelength that the higher the frequency of a radio wave, the shorter its wavelength will be. These high frequencies, with their shorter wavelengths, are not normally diffracted but are absorbed by the Earth at points relatively close to the transmitting site. You can assume, therefore, that as the frequency of a surface wave is increased, the more rapidly the surface wave will be absorbed, or attenuated, by the Earth. Because of this loss by attenuation, the surface wave is impractical for long-distance transmissions at frequencies above 2 megahertz. On the other hand, when the frequency of a surface wave is low enough to have a very long wavelength, the Earth appears to be very small, and diffraction is sufficient for propagation well beyond the horizon. In fact, by lowering the transmitting frequency into the very low frequency (vlf) range and using very high-powered transmitters, the surface wave can be propagated great distances. The Navy's extremely high-powered vlf transmitters are actually capable of transmitting surface wave signals around the Earth and can provide coverage to naval units operating anywhere at sea.

SPACE WAVE.—The space wave follows two distinct paths from the transmitting antenna to the receiving antenna—one through the air directly to the receiving antenna, the other reflected from the ground to the receiving antenna. This is illustrated in figure 2-13. The primary path of the space wave is directly from the transmitting antenna to the receiving antenna. So, the receiving antenna must be located within the radio horizon of the transmitting antenna. Because space waves are refracted slightly, even when propagated through the troposphere, the radio horizon is actually about one-third farther than the line-of-sight or natural horizon.

*[Figure 2-13: Space wave propagation (figure not reproduced).]*

Although space waves suffer little ground attenuation, they nevertheless are susceptible to fading. This is because space waves actually follow two paths of different lengths (direct path and ground reflected path) to the receiving site and, therefore, may arrive in or out of phase. If these two component waves are received in phase, the result is a reinforced or stronger signal. Likewise, if they are received out of phase, they tend to cancel one another, which results in a weak or fading signal.

**Q13.** What is the determining factor in classifying whether a radio wave is a ground wave or a space wave?

**Q14.** What is the best type of surface or terrain to use for radio wave transmission?

**Q15.** What is the primary difference between the radio horizon and the natural horizon?

**Q16.** What three factors must be considered in the transmission of a surface wave to reduce attenuation?

Sky Wave The sky wave, often called the ionospheric wave, is radiated in an upward direction and returned to Earth at some distant location because of refraction from the ionosphere. This form of propagation is relatively unaffected by the Earth's surface and can propagate signals over great distances. Usually the high frequency (hf) band is used for sky wave propagation. The following in-depth study of the ionosphere and its effect on sky waves will help you to better understand the nature of sky wave propagation.

### Structure Of The Ionosphere

As we stated earlier, the ionosphere is the region of the atmosphere that extends from about 30 miles above the surface of the Earth to about 250 miles. It is appropriately named the ionosphere because it consists of several layers of electrically charged gas atoms called ions. The ions are formed by a process called ionization.

Ionization Ionization occurs when high energy ultraviolet light waves from the sun enter the ionospheric region of the atmosphere, strike a gas atom, and literally knock an electron free from its parent atom. A normal atom is electrically neutral since it contains both a positive proton in its nucleus and a negative orbiting electron. When the negative electron is knocked free from the atom, the atom becomes positively charged (called a positive ion) and remains in space along with the free electron, which is negatively charged. This process of upsetting electrical neutrality is known as IONIZATION.

The free negative electrons subsequently absorb part of the ultraviolet energy, which initially freed them from their atoms. As the ultraviolet light wave continues to produce positive ions and negative electrons, its intensity decreases because of the absorption of energy by the free electrons, and an ionized layer is formed. The rate at which ionization occurs depends on the density of atoms in the atmosphere and the intensity of the ultraviolet light wave, which varies with the activity of the sun. Since the atmosphere is bombarded by ultraviolet light waves of different frequencies, several ionized layers are formed at different altitudes. Lower frequency ultraviolet waves penetrate the atmosphere the least; therefore, they produce ionized layers at the higher altitudes. Conversely, ultraviolet waves of higher frequencies penetrate deeper and produce layers at the lower altitudes. An important factor in determining the density of ionized layers is the elevation angle of the sun, which changes frequently. For this reason, the height and thickness of the ionized layers vary, depending on the time of day and even the season of the year.

Recombination Recall that the process of ionization involves ultraviolet light waves knocking electrons free from their atoms. A reverse process called RECOMBINATION occurs when the free electrons and positive ions collide with each other. Since these collisions are inevitable, the positive ions return to their original neutral atom state.

The recombination process also depends on the time of day. Between the hours of early morning and late afternoon, the rate of ionization exceeds the rate of recombination. During this period, the ionized layers reach their greatest density and exert maximum influence on radio waves. During the late afternoon and early evening hours, however, the rate of recombination exceeds the rate of ionization, and the density of the ionized layers begins to decrease. Throughout the night, density continues to decrease, reaching a low point just before sunrise.

Four Distinct Layers The ionosphere is composed of three layers designated D, E, and F, from lowest level to highest level as shown in figure 2-14. The F layer is further divided into two layers designated F1 (the lower layer) and F2 (the higher layer). The presence or absence of these layers in the ionosphere and their height above the Earth varies with the position of the sun. At high noon, radiation in the ionosphere directly above a given point is greatest. At night it is minimum. When the radiation is removed, many of the particles that were ionized recombine. The time interval between these conditions finds the position and number of the ionized layers within the ionosphere changing. Since the position of the sun varies daily, monthly, and yearly, with respect to a specified point on Earth, the exact position and number of layers present are extremely difficult to determine. However, the following general statements can be made:

*[Figure 2-14: Layers of the ionosphere (figure not reproduced).]*

a. The D layer ranges from about 30 to 55 miles. Ionization in the D layer is low because it is the lowest region of the ionosphere. This layer has the ability to refract signals of low frequencies. High frequencies pass right through it and are attenuated. After sunset, the D layer disappears because of the rapid recombination of ions.

b. The E layer limits are from about 55 to 90 miles. This layer is also known as the Kennelly- Heaviside layer, because these two men were the first to propose its existence. The rate of ionic recombination in this layer is rather rapid after sunset and the layer is almost gone by midnight. This layer has the ability to refract signals as high as 20 megahertz. For this reason, it is valuable for communications in ranges up to about 1500 miles.

c. The F layer exists from about 90 to 240 miles. During the daylight hours, the F layer separates into two layers, the F1 and F2 layers. The ionization level in these layers is quite high and varies widely during the day. At noon, this portion of the atmosphere is closest to the sun and the degree of ionization is maximum. Since the atmosphere is rarefied at these heights, recombination occurs slowly after sunset. Therefore, a fairly constant ionized layer is always present. The F layers are responsible for high-frequency, long distance transmission.

**Q17.** What causes ionization to occur in the ionosphere?

**Q18.** How are the four distinct layers of the ionosphere designated?

**Q19.** What is the height of the individual layers of the ionosphere?

### Refraction In The Ionosphere

When a radio wave is transmitted into an ionized layer, refraction, or bending of the wave, occurs. As we discussed earlier, refraction is caused by an abrupt change in the velocity of the upper part of a radio wave as it strikes or enters a new medium. The amount of refraction that occurs depends on three main factors: (1) the density of ionization of the layer, (2) the frequency of the radio wave, and (3) the angle at which the wave enters the layer.

Density of Layer

*[Figure 2-15: illustrates the relationship between radio waves and ionization density. Each ionized layer has a central region of relatively dense ionization, which tapers off in intensity both above and below the maximum region. As a radio wave enters a region of INCREASING ionization, the increase in velocity of the upper part of the wave causes it to be bent back TOWARD the Earth. While the wave is in the highly dense center portion of the layer, however, refraction occurs more slowly because the density of ionization is almost uniform. As the wave enters into the upper part of the layer of DECREASING ionization, the velocity of the upper part of the wave decreases, and the wave is bent AWAY from the (figure not reproduced).]*

Earth.

*[Figure 2-15: Effects of ionospheric density on radio waves (figure not reproduced).]*

If a wave strikes a thin, very highly ionized layer, the wave may be bent back so rapidly that it will appear to have been reflected instead of refracted back to Earth. To reflect a radio wave, the highly ionized layer must be approximately no thicker than one wavelength of the radio wave. Since the ionized layers are often several miles thick, ionospheric reflection is more likely to occur at long wavelengths (low frequencies).

Frequency For any given time, each ionospheric layer has a maximum frequency at which radio waves can be transmitted vertically and refracted back to Earth. This frequency is known as the CRITICAL FREQUENCY. It is a term that you will hear frequently in any discussion of radio wave propagation. Radio waves transmitted at frequencies higher than the critical frequency of a given layer will pass through the layer and be lost in space; but if these same waves enter an upper layer with a higher critical frequency, they will be refracted back to Earth. Radio waves of frequencies lower than the critical frequency will also be refracted back to Earth unless they are absorbed or have been refracted from a lower layer. The lower the frequency of a radio wave, the more rapidly the wave is refracted by a given degree of ionization. Figure 2-16 shows three separate waves of different frequencies entering an ionospheric layer at the same angle. Notice that the 5-megahertz wave is refracted quite sharply. The 20-megahertz wave is refracted less sharply and returned to Earth at a greater distance. The 100-megahertz wave is obviously greater than the critical frequency for that ionized layer and, therefore, is not refracted but is passed into space.

*[Figure 2-16: Frequency versus refraction and distance (figure not reproduced).]*

Angle of Incidence The rate at which a wave of a given frequency is refracted by an ionized layer depends on the angle at which the wave enters the layer. Figure 2-17 shows three radio waves of the same frequency entering a layer at different angles. The angle at which wave A strikes the layer is too nearly vertical for the wave to be refracted to Earth. As the wave enters the layer, it is bent slightly but passes through the layer and is lost. When the wave is reduced to an angle that is less than vertical (wave B), it strikes the layer and is refracted back to Earth. The angle made by wave B is called the CRITICAL ANGLE for that particular frequency. Any wave that leaves the antenna at an angle greater than the critical angle will penetrate the ionospheric layer for that frequency and then be lost in space. Wave C strikes the ionosphere at the smallest angle at which the wave can be refracted and still return to Earth. At any smaller angle, the wave will be refracted but will not return to Earth.

*[Figure 2-17: Different incident angles of radio waves (figure not reproduced).]*

As the frequency of the radio wave is increased, the critical angle must be reduced for refraction to occur. This is illustrated in figure 2-18. The 2-megahertz wave strikes the layer at the critical angle for that frequency and is refracted back to Earth. Although the 5-megahertz wave (broken line) strikes the ionosphere at a lesser angle, it nevertheless penetrates the layer and is lost. As the angle is lowered from the vertical, however, a critical angle for the 5-megahertz wave is reached, and the wave is then refracted to Earth.

*[Figure 2-18: Effects of frequency on the critical angle (figure not reproduced).]*

**Q20.** What factor determines whether a radio wave is reflected or refracted by the ionosphere?

**Q21.** There is a maximum frequency at which vertically transmitted radio waves can be refracted back to Earth. What is this maximum frequency called?

**Q22.** What three main factors determine the amount of refraction in the ionosphere?

Skip Distance/Skip Zone In figure 2-19, note the relationship between the sky wave skip distance, the skip zone, and the ground wave coverage. The SKIP DISTANCE is the distance from the transmitter to the point where the sky wave is first returned to Earth. The size of the skip distance depends on the frequency of the wave, the angle of incidence, and the degree of ionization present.

*[Figure 2-19: Relationship between skip zone, skip distance, and ground wave (figure not reproduced).]*

The SKIP ZONE is a zone of silence between the point where the ground wave becomes too weak for reception and the point where the sky wave is first returned to Earth. The size of the skip zone depends on the extent of the ground wave coverage and the skip distance. When the ground wave coverage is great enough or the skip distance is short enough that no zone of silence occurs, there is no skip zone.

Occasionally, the first sky wave will return to Earth within the range of the ground wave. If the sky wave and ground wave are nearly of equal intensity, the sky wave alternately reinforces and cancels the ground wave, causing severe fading. This is caused by the phase difference between the two waves, a result of the longer path traveled by the sky wave.

### Propagation Paths

The path that a refracted wave follows to the receiver depends on the angle at which the wave strikes the ionosphere. You should remember, however, that the rf energy radiated by a transmitting antenna spreads out with distance. The energy therefore strikes the ionosphere at many different angles rather than a single angle.

After the rf energy of a given frequency enters an ionospheric region, the paths that this energy might follow are many. It may reach the receiving antenna via two or more paths through a single layer. It may also, reach the receiving antenna over a path involving more than one layer, by multiple hops between the ionosphere and Earth, or by any combination of these paths.

*[Figure 2-20: shows how radio waves may reach a receiver via several paths through one layer. The various angles at which rf energy strikes the layer are represented by dark lines and designated as rays 1 through 6 (figure not reproduced).]*

*[Figure 2-20: Ray paths for a fixed frequency with varying angles of incidence (figure not reproduced).]*

When the angle is relatively low with respect to the horizon (ray 1), there is only slight penetration of the layer and the propagation path is long. When the angle of incidence is increased (rays 2 and 3), the rays penetrate deeper into the layer but the range of these rays decreases. When a certain angle is reached (ray 3), the penetration of the layer and rate of refraction are such that the ray is first returned to Earth at a minimal distance from the transmitter. Notice, however, that ray 3 still manages to reach the receiving site on its second refraction (called a hop) from the ionospheric layer.

As the angle is increased still more (rays 4 and 5), the rf energy penetrates the central area of maximum ionization of the layer. These rays are refracted rather slowly and are eventually returned to Earth at great distances. As the angle approaches vertical incidence (ray 6), the ray is not returned at all, but passes on through the layer.
