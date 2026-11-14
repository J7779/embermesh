---
id: radio/fm24-18-ch3-antennas-1
title: "FM 24-18 Ch. 3: Antennas (1/3)"
category: radio
tags: [radio, antenna, field expedient, dipole, long wire, repair, ground, counterpoise]
summary: "Antenna characteristics, types, and field repair and expedient antennas you can build from wire."
source: "https://archive.org/details/milmanual-fm-24-18-tactical-single-channel-radio-communications-techni"
source_title: "FM 24-18 Tactical Single-Channel Radio Communications Techniques (Department of the Army, 1987)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Headquarters, Department of the Army, FM 24-18, 30 September 1987. US Government work; distribution statement: 'Approved for public release; distribution is unlimited.' Figures and image-only tables not reproduced."
retrieved: 2026-11-13
part_of: radio/fm24-18
order: 31
---

# FM 24-18 Ch. 3: Antennas (1/3)

## SAFETY WARNING

Be extremely careful when putting up, taking down, or moving antennas located near high voltage or commercial power lines. Antenna contact with these can and may result in electrocution or sever injury to personnel holding the antenna or the connecting guy wires and cables.

## Section I. Requirement and Function

### 3-1. Necessity

All radios, whether transmitting or receiving, require some sort of antenna. Single-channel radios normally send and receive radio signals on one antenna. This is called one-way-reversible (OWR) or simplex operation. During duplex (DX) operation two antennas are used, one for transmitting and the other for receiving. In either case, the transmitter generates a radio signal. A transmission line delivers the signal from the transmitter to the antenna. The transmitting antenna sends the radio signal into space toward the receiving antenna. The receiving antenna intercepts the signal and sends it through a transmission line to the receiver. The receiver processes the radio signal so that it can either be heard or used to operate a recording device such as a teletypewriter (fig 3-1).

*[Figure 3-1: Simple radio communications network (figure not reproduced).]*

### 3-2. Function

The function of an antenna depends on whether it is transmitting or receiving. A transmitting antenna transforms the output RF energy produced by a radio transmitter (RF output power) into an electromagnetic field that is radiated through space. In other words, the transmitting antenna converts energy from one form to another form. The receiving antenna reverses this process. It transforms the electromagnetic field into RF energy which is delivered to a radio receiver.

### 3-3. Gain

The gain of an antenna depends mainly on its design. Transmitting antennas are designed for high efficiency in radiating energy, and receiving antennas are designed for high efficiency in picking up energy. On many radio circuits, transmission is required between a transmitter and only one receiving station. In this case, energy may be radiated in one direction because it is useful only in that direction. Directional receiving antennas increase the energy pickup or gain in the favored direction, and reduce the reception of unwanted noise and signals from other directions. The general requirements for transmitting and receiving antennas are that they have small energy losses and that they be efficient as radiators and receptors.

## Section II. Characteristics

### 3-4. Electromagnetic Radiation

#### Radiation Fields

When RF power is delivered to an antenna, two fields are set up: one is an induction field, which is associated with the stored energy; the other is a radiation field. At the antenna, the intensities of these fields are large and are proportional to the amount of RF power delivered to the antenna. At a short distance from the antenna and beyond, only the radiation field remains. This radiation field is composed of an electric component and a magnetic component (fig 3-2).

*[Figure 3-2: Components of electromagnetic waves (figure not reproduced).]*

The electric and magnetic fields (components) radiated from an antenna form the electromagnetic field. The electromagnetic field is responsible for the transmission and reception of electromagnetic energy through free space. A radio wave is a moving electromagnetic field that has velocity in the direction of travel, and with components of electric intensity and magnetic intensity arranged at right angles to each other.

#### Radiation Patterns

The radio signals radiated by an antenna form an electromagnetic field having a definite pattern, depending on the type of antenna used. This radiation pattern is used to show the directional characteristics of an antenna. A vertical antenna theoretically radiates energy equally in all directions (omnidirectional); a horizontal antenna is mainly bidirectional. There are also unidirectional antennas. These antennas theoretically radiate energy in one direction. In practice, however, the patterns usually are distorted by nearby obstructions or terrain features.

The full- or solid-radiation pattern is represented as a three-dimensional figure that looks somewhat like a doughnut with a transmitting antenna in the center (fig 3-3). The upper pattern in the figure is that of a quarter-wave vertical antenna; the center pattern is that of a half-wave horizontal antenna, located one-half wavelength above the ground. The bottom pattern is that of a vertical half-rhombic antenna.

*[Figure 3-3: Solid radiation patterns from quarter-wave, half-wave, and vertical half-rhombic antennas (figure not reproduced).]*

### 3-5. Polarization

The polarization of a radiated wave is determined by the direction of the lines of force making up the electric field. If the lines of electric force are at right angles to the surface of the Earth, the wave is said to be vertically polarized (fig 3-4). If the lines of electric force are parallel to the surface of the Earth, the wave is said to be horizontally polarized (fig 3-5). When a single-wire antenna is used to extract (receive) energy from a passing radio wave, maximum pickup results if the antenna is oriented so that it lies in the same direction as the electric field component. Thus, a vertical antenna is used for efficient reception of vertically polarized waves and a horizontal antenna is used for the reception of horizontally polarized waves. In some cases, the field rotates as the waves travel through space. Under these conditions, both horizontal and vertical components of the field exist and the wave is said to have elliptical polarization.

*[Figure 3-4: Vertically polarized signal (figure not reproduced).]*

#### Polarization Requirements for Various Frequencies

At medium and low frequencies, ground-wave transmission is used extensively and it is necessary to use vertical polarization. Vertical lines of force are perpendicular to the ground, and the radio wave can travel a considerable distance along the ground surface with a minimum amount of loss. Because the Earth acts as a relatively good conductor at low frequencies, horizontal lines of electric force are shorted out and the useful range with the horizontal polarization is limited.

At high frequencies, with sky wave transmission, it makes little difference whether horizontal or vertical polarization is used. The sky-wave, after being reflected by the ionosphere, arrives at the receiving antenna elliptically polarized. Therefore, the transmitting and receiving antennas can be mounted either horizontally or vertically. Horizontal antennas are preferred however, since they can be made to radiate effectively at high angles and have inherent directional properties.

*[Figure 3-5: Horizontally polarized signal (figure not reproduced).]*

For frequencies in the very-high or ultra-high range, either horizontal or vertical polarization is satisfactory. Since the radio wave travels directly from the transmitting antenna to the receiving antenna, the original polarization produced at the transmitting antenna is maintained as the wave travels to the receiving antenna. Therefore, if a horizontal antenna is used for transmitting, a horizontal antenna must be used for receiving.

Satellites and satellite terminals use circular polarization. Circular polarization describes a wave whose plane of polarization rotates through 360° as it progresses forward. The rotation can be clockwise or counterclockwise (see fig 3-6). Circular polarization occurs when equal magnitudes of vertically and horizontally polarized waves are combined with a phase difference of 90°. Depending on their phase relationship, this causes rotation either in one direction or the other (see app L).

*[Figure 3-6: Circular polarized wave (figure not reproduced).]*

#### Advantages of Vertical Polarization

Simple vertical half-wave and quarter-wave antennas can be used to provide omnidirectional (in all directions) communications. This is desirable in communicating with a moving vehicle. Its disadvantage is that it radiates equally to the enemy and friendly forces.

When antenna heights are limited to 3.05 meters (10 ft) or less over land, as in a vehicular installation, vertical polarization provides a stronger received signal at frequencies up to about 50 MHz. From about 50 MHz to 100 MHz, there is only a slight improvement over horizontal polarization with antennas at the same height. Above 100 MHz, the difference in signal strength between vertical and horizontal polarization is small. However, when antennas are located near dense forests, horizontally polarized waves suffer lower losses than vertically polarized waves. Vertically polarized radiation is somewhat less affected by reflections from aircraft flying over the transmission path. With horizontal polarization, such reflections cause variations in received signal strength. An example is the picture flutter in a television set when an aircraft interferes with the transmission path. This factor is important in areas where aircraft traffic is heavy.

When vertical polarization is used, less interference is produced or picked up from strong VHF and UHF transmissions (television and FM broadcasts) because they use horizontal polarization. This factor is important when an antenna must be located in an urban area that has television or FM broadcast stations.

#### Advantages of Horizontal Polarization

A simple horizontal half-wave antenna is bidirectional. This characteristic is useful in minimizing interference from certain directions.

Horizontal antennas are less likely to pick up man-made interference, which ordinarily is vertically polarized. When antennas are located near dense forests, horizontally polarized waves suffer lower losses than vertically polarized waves, especially above 100 MHz.

Small changes in antenna location do not cause large variations in the field intensity of horizontally polarized waves when an antenna is located among trees or buildings. When vertical polarization is used, a change of only a few feet in the antenna location may have a significant effect on the received signal strength.

### 3-6. Directionality

Vertical receiving antennas accept radio signals equally from all horizontal directions, just as vertical transmitting antennas radiate equally in all horizontal directions. Because of this characteristic, other stations operating on the same or nearby frequencies may interfere with the desired signal and make reception difficult or impossible. However, reception of a desired signal can be improved by using directional antennas.

Horizontal half-wave antennas accept radio signals from all directions, with the strongest reception being received in a line perpendicular to the antenna (that is, broadside); and, the weakest reception being received from the direction of the ends of the antenna. Interfering signals can be eliminated or reduced by changing the antenna installation so that either end of the antenna points directly at the interfering station.

Communications over a radio circuit is satisfactory when the received signal is strong enough to override undesired signals and noise. The receiver must be within range of the transmitter. Increasing the transmitting power between two radio stations increases communications effectiveness. Also, changing the types of transmission (for example, changing from radiotelephone to CW), changing to a frequency that is not readily absorbed, or using a directional antenna aids in communications effectiveness.

Directional transmitting antennas concentrate radiation in a given direction and minimize radiation in other directions. A directional antenna may also be used to lessen interception by the enemy and interference with friendly stations.

### 3-7. Ground Effects

Since all practical antennas are erected over the Earth and not out in free space, except for those on satellites, the presence of the ground will alter the free space radiation patterns of antennas. The ground will also have an effect on some of the electrical characteristics of an antenna. It has the greatest effect on those antennas that must be mounted relatively close to the ground in terms of wavelength. For example, medium- and high-frequency antennas, elevated above the ground by only a fraction of a wavelength, will have radiation patterns that are quite different from the free-space patterns.

#### Grounded Antenna Theory

The ground is a good conductor for medium and low frequencies and acts as a large mirror for the radiated energy. This results in the ground reflecting a large amount of energy that is radiated downward from an antenna mounted over it. Using this characteristic of the ground, an antenna only a quarter-wavelength long can be made into the equivalent of a half-wave antenna. A quarter-wave antenna erected vertically, with its lower end connected electrically to the ground (fig 3-7), behaves like a half-wave antenna. Under these conditions, the ground takes the place of the missing quarter-wavelength, and the reflections supply that part of the radiated energy that normally would be supplied by the lower half of an ungrounded half-wave antenna.

#### Types of Grounds

When grounded antennas are used, it is especially important that the ground has as high a conductivity as possible. This reduces ground losses and provides the best possible reflecting surface for the down-going radiated energy from the antenna. At low and medium frequencies, the ground acts as a sufficiently good conductor. Therefore, the ground connection must be made in such a way as to introduce the least possible amount of resistance to ground. At higher frequencies, artificial grounds constructed of large metal surfaces are common.

The ground connections take many forms, depending on the type of installation and the loss that can be tolerated. In many simple field installations, the ground connection is made by means of one or more metal rods driven into the soil. Where more satisfactory arrangements cannot be made, ground leads can be connected to existing devices which are grounded. Metal structures or underground pipe systems are commonly used as ground connections. In an emergency, a ground connection can be made by forcing one or more bayonets into the soil.

When an antenna must be erected over soil with low conductivity, treat the soil to reduce its resistance. The soil should be treated with substances that are highly conductive when in solution. Some of these substances, listed in order of preference, are sodium chloride (common salt), calcium chloride, copper sulphate (blue vitriol), magnesium sulphate (Epsom salt, and potassium nitrate (saltpeter). The amount required depends on the type of soil and its moisture content.

**Warning:** When these substances are used, it is important that they do not get into nearby drinking water supplies.

*[Figure 3-7: Quarter-wave antenna connected to ground (figure not reproduced).]*

For simple installations, a single ground rod can be fabricated in the field from pipe or conduit. It is important that a low resistance connection be made between the ground wire and the ground rod. The rod should be cleaned thoroughly by scraping and sandpapering at the point where the connection is to be made, and a clean ground clamp should be installed. A ground wire can then be soldered or joined to the clamp. This joint should be covered with tape to prevent an increase in resistance because of oxidation.

#### Counterpoise

When an actual ground connection cannot be used because of the high resistance of the soil or because a large buried ground system is not practical, a counterpoise may be used to replace the usual direct ground connection. The counterpoise (fig 3-8) consists of a device made of wire which is erected a short distance above the ground and insulated from it. The size of the counterpoise should be at least equal to or larger than the size of the antenna. When the antenna is mounted vertically, the counterpoise should be made into a simple geometric pattern. Perfect symmetry is not required. The counterpoise appears to the antenna as an artificial ground that helps to produce the required radiation pattern.

*[Figure 3-8: Wire counterpoise (figure not reproduced).]*

In some VHF antenna installations on vehicles, the metal roof of the vehicle (or shelter) is used as a counterpoise for the antenna.

Small counterpoises of metal mesh are sometimes used with special VHF antennas that must be located a considerable distance above the ground.

#### Ground Screen

A ground screen consists of a fairly large area of metal mesh or screen that is laid on the surface of the ground under the antenna. There are two specific advantages in using ground screens. First, the ground screen reduces ground absorption losses that occur when an antenna is erected over ground with poor conductivity. Second, the height of the antenna can be set accurately. As a result of this, the radiation resistance of the antenna can be determined more accurately (See TM 11-666, para 61).

### 3-8. Antenna Length

The length of an antenna must be considered in two ways. It has both a physical and an electrical length, and the two are never the same. The reduced velocity of the wave on the antenna and a capacitive effect (known as end effect) make the antenna seem longer electrically than it is physically. The contributing factors are the ratio of the diameter of the antenna to its length and the capacitive effect of terminal equipment (insulators, clamps, etc.) used to support the antenna. To calculate the physical length of an antenna, use a correction of 0.95 for frequencies between 3.0 and 50.0 MHz. The figures given below are for a half-wave antenna.

Length (meters) = 150 X 0.95 / Frequency in MHz = 142.5 / Frequency in MHz Length (feet) = 492 X 0.95 / Frequency in Mhz = 468 / Frequency in MHz The length of a long-wire antenna (one wavelength or longer) for harmonic operation is calculated by using the following formula.

Length (meters) = 150(N-0.05) / Freq MHz Length (feet) = 492 (N-0.05) / Freq MHz Where N = number of half-wave lengths in the total length of the antenna.

For example, if the number of half-wavelengths is 3 and the frequency in MHz is 7, then:

Length (meters) =150(N-0.05) / Freq MHz = 150(3-.05) / 7 = 150 x 2.95 / 7 = 442.50 / 7 = 63.2 meters

### 3-9. Antenna Orientation

#### Azimuth

If the azimuth of the radio path is not provided, the azimuth should be determined by the best available means. The accuracy required in determining the azimuth of the path is dependent upon the radiation pattern of the directional antenna. If the antenna beam width is very wide (for example, 90° angle between half-power points, fig 3-9), an error of 10° in azimuth is of little consequence. In transportable operation, the rhombic and V antennas may have such a narrow beam as to require great accuracy in azimuth determination. The antenna should be erected for the correct azimuth. Great accuracy is not required in erecting broad-beam antennas. Unless a line of known azimuth is available at the site, the direction of the path is best determined by a magnetic compass. Figure 3-10 is a map of magnetic declination, showing the variation of the compass needle from the true north. When the compass is held so that the needle points to the direction indicated for the location on the map, all directions indicated by the compass will be true.

*[Figure 3-9: Beam width measured on relative field strength and relative power patterns (figure not reproduced).]*

*[Figure 3-10: Magnetic declination over the world (figure not reproduced).]*

#### Improvement of Marginal Communications

Under certain situations, it may not be feasible to orient directional antennas to the correct azimuth of the desired radio path. As a result, marginal communications may suffer. To improve marginal communications, follow the procedure presented below.

Check, tighten, and tape cable couplings and connections.

- Retune all transmitters and receivers in the circuit.
- Check to see that antennas are adjusted for the proper operating frequency.
- Try changing the heights of antennas.
- Try moving the antenna a short distance away and in different locations from its original location.
- Separate transmitters from receiving equipment, if feasible.
- Transmission and Reception of Strong Signals.

After an adequate site has been selected and the proper antenna orientation obtained, the signal level at the receiver will be proportional to the strength of the transmitted signal.

If a high-gain antenna is used, a stronger signal can be obtained. Losses between the antenna and the equipment can be reduced by using a high quality transmission line, as short as possible, and properly matched at both ends.

**Warning:** Excessive signal strength may result in enemy intercept and interference or in your interfering with adjacent frequencies.
