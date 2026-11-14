---
id: radio/neets10-ch4-antennas-2
title: "NEETS Module 10 Ch. 4: Antennas (2/3)"
category: radio
tags: [radio, antenna, dipole, Marconi, arrays, radiation pattern]
summary: "Navy training text on antennas: radiation, polarization, half-wave and quarter-wave antennas, arrays and special antennas. (continued)"
source: "https://archive.org/details/NEETSModule10"
source_title: "Navy Electricity and Electronics Training Series (NEETS) Module 10: Introduction to Wave Propagation, Transmission Lines, and Antennas (NAVEDTRA 14182)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Naval Education and Training Professional Development and Technology Center, US Navy. US Government work; 'Approved for public release; distribution is unlimited.' Figures not reproduced; end-of-chapter question answers retained."
retrieved: 2026-11-13
part_of: radio/neets10
order: 42
---

# NEETS Module 10 Ch. 4: Antennas (2/3)

## Quarter-Wave Antennas

As you have studied in the previous sections, a 1/2 wavelength antenna is the shortest antenna that can be used in free space. If we cut a half-wave antenna in half and then ground one end, we will have a grounded quarter-wave antenna. This antenna will resonate at the same frequency as the ungrounded half-wave antenna. Such an antenna is referred to as a QUARTER-WAVE or Marconi antenna. Quarter-wave antennas are widely used in the military. Most mobile transmitting and receiving antennas (fig. 4-17) are quarter-wave antennas.

*[Figure 4-17: Mobile antennas (figure not reproduced).]*

As stated above, a grounded quarter-wave antenna will resonate at the same frequency as an ungrounded half-wave antenna. This is because ground has high conductivity and acts as an electrical mirror image. This characteristic provides the missing half of the antenna, as shown in the bottom part of figure 4-18. In other words, the grounded quarter-wave antenna acts as if another quarter-wave were actually down in the earth.

*[Figure 4-18: Grounded quarter-wave antenna image (figure not reproduced).]*

Characteristics of Quarter-Wave Antennas The grounded end of the quarter-wave antenna has a low input impedance and has low voltage and high current at the input end, as shown in figure 4-18. The ungrounded end has a high impedance, which causes high voltage and low current. The directional characteristics of a grounded quarter-wave antenna are the same as those of a half-wave antenna in free space.

As explained earlier, ground losses affect radiation patterns and cause high signal losses for some frequencies. Such losses may be greatly reduced if a perfectly conducting ground is provided in the vicinity of the antenna. This is the purpose of a GROUND SCREEN (figure 4-19, view A) and COUNTERPOISE view B.

*[Figure 4-19: Groundscreen and counterpoise (figure not reproduced).]*

The ground screen in view A is composed of a series of conductors buried 1 or 2 feet (0.3 to 0.6 meter) below the surface of the earth and arranged in a radial pattern. These conductors reduce losses in the ground in the immediate vicinity of the antenna. Such a radial system of conductors is usually 1/2 wavelength in diameter.

A counterpoise view B is used when easy access to the base of the antenna is necessary. It is also used when the earth is not a good conducting surface, such as ground that is sandy or solid rock. The counterpoise serves the same purpose as the ground screen but it is usually elevated above the earth. No specific dimensions are necessary in the construction of a counterpoise nor is the number of wires particularly critical. A practical counterpoise may be assembled from a large screen of chicken wire or some similar material. This screen may be placed on the ground, but better results are obtained if it is placed a few feet above the ground.

**Q20.** Since the radiation pattern of a dipole is similar to that of a doublet, what will happen to the pattern if the length of the doublet is increased?

**Q21.** What is the simplest method of feeding power to the half-wave antenna?

**Q22.** What is the radiation pattern of a quarter-wave antenna?

**Q23.** Describe the physical arrangement of a ground screen.

### Folded Dipole

The use of parasitic elements and various stacking arrangements causes a reduction in the radiation resistance of a center-fed, half-wave antenna. Under these conditions obtaining a proper impedance match between the radiator and the transmission line is often difficult. A convenient method of overcoming these difficulties is to use a FOLDED DIPOLE in place of the center-fed radiator. (See views A and B of figure 4-20).

*[Figure 4-20: Folded-dipole antennas (figure not reproduced).]*

A FOLDED DIPOLE is an ordinary half-wave antenna that has one or more additional conductors connected across its ends. Additional conductors are mounted parallel to the dipole elements at a distance equal to a very small fraction of a wavelength. Spacing of several inches is common. The feed-point impedance can be further increased by using three or four properly spaced parallel conductors. Standard feed-line SPREADERS are used to maintain this spacing when required. In any folded dipole, the increase of impedance is the square of the number of conductors used in the radiator. Thus, a three-wire dipole has nine times (32) the feed-point impedance of a simple center-fed dipole. A second method of stepping up the impedance of a folded dipole is to use two conductors with different radii, as shown in view B.

The directional characteristics of a folded dipole are the same as those of a simple dipole. However, the reactance of a folded dipole varies much more slowly as the frequency is varied from resonance. Because of this the folded dipole can be used over a much wider frequency range than is possible with a simple dipole.

**Q24.** What is the difference in the amount of impedance between a three-wire dipole and a simple center-fed dipole?

**Q25.** Which has a wider frequency range, a simple dipole or a folded dipole?

### Array Antennas

An array antenna is a special arrangement of basic antenna components involving new factors and concepts. Before you begin studying about arrays, you need to study some new terminology.

### Definition Of Terms

An array antenna is made up of more than one ELEMENT, but the basic element is generally the dipole. Sometimes the basic element is made longer or shorter than a half-wave, but the deviation usually is not great.

A DRIVEN element is similar to the dipole you have been studying and is connected directly to the transmission line. It obtains its power directly from the transmitter or, as a receiving antenna, it delivers the received energy directly to the receiver. A PARASITIC ELEMENT is located near the driven element from which it gets its power. It is placed close enough to the driven element to permit coupling. A parasitic element is sometimes placed so it will produce maximum radiation (during transmission) from its associated driver. When it operates to reinforce energy coming from the driver toward itself, the parasitic element is referred to as a DIRECTOR. If a parasitic element is placed so it causes maximum energy radiation in a direction away from itself and toward the driven element, that parasitic element is called a REFLECTOR.

If all of the elements in an array are driven, the array is referred to as a DRIVEN ARRAY (sometimes as a CONNECTED ARRAY). If one or more elements are parasitic, the entire system usually is considered to be a PARASITIC ARRAY.

MULTIELEMENT ARRAYS frequently are classified according to their directivity. A BIDIRECTIONAL ARRAY radiates in opposite directions along the line of maximum radiation. A UNIDIRECTIONAL ARRAY radiates in only one general direction.

Arrays can be described with respect to their radiation patterns and the types of elements of which they are made. However, you will find it useful to identify them by the physical placement of the elements and the direction of radiation with respect to these elements. Generally speaking, the term BROADSIDE ARRAY designates an array in which the direction of maximum radiation is perpendicular to the plane containing these elements. In actual practice, this term is confined to those arrays in which the elements themselves are also broadside, or parallel, with respect to each other. A COLLINEAR ARRAY is one in which all the elements lie in a straight line with no radiation at the ends of the array. The direction of maximum radiation is perpendicular to the axis of the elements. An END-FIRE ARRAY is one in which the principal direction of radiation is along the plane of the array and perpendicular to the elements. Radiation is from the end of the array, which is the reason this arrangement is referred to as an end-fire array.

Sometimes a system uses the characteristics of more than one of the three types mentioned. For instance, some of the elements may be collinear while others may be parallel. Such an arrangement is often referred to as a COMBINATION ARRAY or an ARRAY OF ARRAYS. Since maximum radiation occurs at right angles to the plane of the array, the term broadside array is also used. The FRONT-TO-BACK RATIO is the ratio of the energy radiated in the principal direction compared to the energy radiated in the opposite direction for a given antenna.

### Phasing

Various reflected and refracted components of the propagated wave create effects of reinforcement and cancellation. At certain distant points from the transmitter, some of the wave components meet in space. Reception at these points is either impaired or improved. If the different components arrive at a given point in the same phase, they add, making a stronger signal available. If they arrive out of phase, they cancel, reducing the signal strength.

Radiation Pattern Effects similar to those described in the preceding paragraph can be produced at the transmitting point itself. Consider the antennas shown in figure 4-21, views A and B. View A shows an unobstructed view of the radiation pattern of a single dipole. In view B two dipoles, shown as points 1 and 2, are perpendicular to the plane of the page. They are spaced 1/4 wavelength apart at the operating frequency. The radiation pattern from either antenna 1 or 2, operating alone, would be uniform in all directions in this plane, as shown in view A. Suppose that current is being fed to both antennas from the same transmitter in such a way that the current fed to antenna 2 lags the current in antenna 1 by 90 degrees. Energy radiating from antenna 1 toward receiving location X will reach antenna 2 after 1/4 cycle of operation. The energy from both antennas will add, and propagation toward X will be strong.

*[Figure 4-21: Phasing of antenna in free space (figure not reproduced).]*

Radiation from antenna 2 toward receiving location Y will reach antenna 1 after 1/4 cycle. The energy in antenna 1 was 1/4 cycle behind that of antenna 2 to begin with; therefore, the radiation from antenna 1 toward receiving point Y will be exactly 180 degrees out of phase with that of antenna 2. As a result, the radiation fields will cancel and there will be no radiation toward Y.

At receiving points away from the line of radiation, phase differences occur between 0 and 180 degrees, producing varying amounts of energy in that direction. The overall effect is shown by the radiation pattern shown in view B. The physical phase relationship caused by the 1/4-wavelength spacing between the two elements, as well as the phase of the currents in the elements, has acted to change the radiation pattern of the individual antennas.

Stub Phasing In the case just discussed, the currents fed to the two antennas from the same transmitter were 90 degrees out of phase. Sections of transmission line, called STUBS, are frequently used for this purpose. These stubs can be adjusted to produce any desired phase relationship between connected elements. When two collinear half-wave elements are connected directly so their currents are in the same phase, the effect is the same as that of a full-wave antenna, as shown in figure 4-22, view A. The current in the first 1/2 wavelength is exactly 180 degrees out of phase with that in the second 1/2 wavelength. This is the opposite of the desired condition. In the illustration, arrows are used to indicate the direction of current flow in the antenna. (Using arrows is a convenient means of determining the phase on more complicated arrays.)

*[Figure 4-22: Phasing of connected elements (figure not reproduced).]*

When the two elements are connected by a shorted 1/4-wavelength stub, as shown in view B, current travels down one side of the stub and up the other. It travels a distance of a 1/2 wavelength in the stub itself. As a result, the current moves through 1/2 cycle of change. When the current reaches the second element, it is in the desired phase. Since the current on one side of the stub is equal and opposite to the current on the other side, the fields produced here cancel and no radiation is transmitted from the stub itself.

### Directivity

The DIRECTIVITY of an antenna or an array can be determined by looking at its radiation pattern. In an array propagating a given amount of energy, more radiation takes place in certain directions than in others. The elements in the array can be altered in such a way that they change the pattern and distribute it more uniformly in all directions. The elements can be considered as a group of antennas fed from a common source and facing different directions. On the other hand, the elements could be arranged so that the radiation would be focused in a single direction. With no increase in power from the transmitter, the amount of radiation in a given direction would be greater. Since the input power has no increase, this increased directivity is achieved at the expense of gain in other directions.

Directivity and Interference In many applications, sharp directivity is desirable although no need exists for added gain. Examine the physical disposition of the units shown in figure 4-23. Transmitters 1 and 2 are sending information to receivers 1 and 2, respectively, along the paths shown by the solid arrows. The distance between transmitter 1 and receiver 1 or between transmitter 2 and receiver 2 is short and does not require high-power transmission. The antennas of the transmitters propagate well in all directions. However, receiver 1 picks up some of the signals from transmitter 2, and receiver 2 picks up some of the signals from transmitter 1, as shown by the broken arrows. This effect is emphasized if the receiving antennas intercept energy equally well in all directions.

*[Figure 4-23: Directivity and interference (figure not reproduced).]*

The use of highly directional arrays as radiators from the transmitters tends to solve the problem. The signals are beamed along the paths of the solid arrows and provide very low radiation along the paths of the broken arrows. Further improvement along these lines is obtained by the use of narrowly directed arrays as receiving antennas. The effect of this arrangement is to select the desired signal while discriminating against all other signals. This same approach can be used to overcome other types of radiated interference. In such cases, preventing radiation in certain directions is more important than producing greater gain in other directions.

Look at the differences between the field patterns of the single-element antenna and the array, as illustrated in figure 4-24. View A shows the relative field-strength pattern for a horizontally polarized single antenna. View B shows the horizontal-radiation pattern for an array. The antenna in view A radiates fairly efficiently in the desired direction toward receiving point X. It radiates equally as efficiently toward Y, although no radiation is desired in this direction. The antenna in view B radiates strongly to point X, but very little in the direction of point Y, which results in more satisfactory operation.

*[Figure 4-24: Single antenna versus array (figure not reproduced).]*

Major and Minor Lobes The pattern shown in figure 4-24, view B, has radiation concentrated in two lobes. The radiation intensity in one lobe is considerably stronger than in the other. The lobe toward point X is called a MAJOR LOBE; the other is a MINOR LOBE. Since the complex radiation patterns associated with arrays frequently contain several lobes of varying intensity, you should learn to use appropriate terminology. In general, major lobes are those in which the greatest amount of radiation occurs. Minor lobes are those in which the radiation intensity is least.

**Q26.** What is the purpose of antenna stubs?

**Q27.** What is the primary difference between the major and minor lobes of a radiation pattern?

### Directional Arrays

You have already learned about radiation patterns and directivity of radiation. These topics are important to you because using an antenna with an improper radiation pattern or with the wrong directivity will decrease the overall performance of the system. In the following paragraphs, we discuss in more detail the various types of directional antenna arrays mentioned briefly in the "definition of terms" paragraph above.

Collinear Array The pattern radiated by the collinear array is similar to that produced by a single dipole. The addition of the second radiator, however, tends to intensify the pattern. Compare the radiation pattern of the dipole (view A of figure 4-25) and the two-element antenna in view B. You will see that each pattern consists of two major lobes in opposite directions along the same axis, QQ1. There is little or no radiation along the PP1 axis. QQ1 represents the line of maximum propagation. You can see that radiation is stronger with an added element. The pattern in view B is sharper, or more directive, than that in view A. This means that the gain along the line of maximum energy propagation is increased and the beam width is decreased. As more elements are added, the effect is heightened, as shown in view C. Unimportant minor lobes are generated as more elements are added.

*[Figure 4-25: Single half-wave antenna versus two half-wave antennas in phase (figure not reproduced).]*

More than four elements are seldom used because accumulated losses cause the elements farther from the point of feeding to have less current than the nearer ones. This introduces an unbalanced condition in the system and impairs its efficiency. Space limitations often are another reason for restricting the number of elements. Since this type of array is in a single line, rather than in a vertically stacked arrangement, the use of too many elements results in an antenna several wavelengths long. RADIATION PATTERN.—The characteristic radiation pattern of a given array is obtained at the frequency or band of frequencies at which the system is resonant. The gain and directivity characteristics are lost when the antenna is not used at or near this frequency and the array tunes too sharply. A collinear antenna is more effective than an end-fire array when used off its tuned frequency. This feature is considered when transmission or reception is to be over a wide frequency band. When more than two elements are used, this advantage largely disappears.

LENGTH AND PHASING.—Although the 1/2 wavelength is the basis for the collinear element, you will find that greater lengths are often used. Effective arrays of this type have been constructed in which the elements are 0.7 and even 0.8 wavelength long. This type of array provides efficient operation at more than one frequency or over a wider frequency range. Whatever length is decided upon, all of the elements in a particular array should closely adhere to that length. If elements of different lengths are combined, current phasing and distribution are changed, throwing the system out of balance and seriously affecting the radiation pattern.

**Q28.** What is the maximum number of elements ordinarily used in a collinear array?

**Q29.** Why is the number of elements used in a collinear array limited?

**Q30.** How can the frequency range of a collinear array be increased?

**Q31.** How is directivity of a collinear array affected when the number of elements is increased? SPACING.—The lower relative efficiency of collinear arrays of many elements, compared with other multi-element arrays, relates directly to spacing and mutual impedance effects. Mutual impedance is an important factor to be considered when any two elements are parallel and are spaced so that considerable coupling is between them. There is very little mutual impedance between collinear sections. Where impedance does exist, it is caused by the coupling between the ends of adjacent elements. Placing the ends of elements close together is frequently necessary because of construction problems, especially where long lengths of wire are involved.

The effects of spacing and the advantages of proper spacing can be demonstrated by some practical examples. A collinear array consisting of two half-wave elements with 1/4-wavelength spacing between centers has a gain of 1.8 dB. If the ends of these same dipoles are separated so that the distance from center to center is 3/4 wavelengths and they are driven from the same source, the gain increases to approximately 2.9 dB.

A three-dipole array with negligible spacing between elements gives a gain of 3.3 dB. In other words, when two elements are used with wider spacing, the gain obtained is approximately equal to the gain obtainable from three elements with close spacing. The spacing of this array permits simpler construction, since only two dipoles are used. It also allows the antenna to occupy less space. Construction problems usually dictate small-array spacing.

Broadside Arrays A broadside array is shown in figure 4-26, view A. Physically, it looks somewhat like a ladder. When the array and the elements in it are polarized horizontally, it looks like an upright ladder. When the array is polarized vertically, it looks like a ladder lying on one side (view B). View C is an illustration of the radiation pattern of a broadside array. Horizontally polarized arrays using more than two elements are not common. This is because the requirement that the bottom of the array be a significant distance above the earth presents construction problems. Compared with collinear arrays, broadside arrays tune sharply, but lose efficiency rapidly when not operated on the frequencies for which they are designed.

*[Figure 4-26: Typical broadside array (figure not reproduced).]*

RADIATION PATTERN.—Figure 4-27 shows an end view of two parallel half-wave antennas (A and B) operating in the same phase and located 1/2 wavelength apart. At a point (P) far removed from the antennas, the antennas appear as a single point. Energy radiating toward P from antenna A starts out in phase with the energy radiating from antenna B in the same direction. Propagation from each antenna travels over the same distance to point P, arriving there in phase. The antennas reinforce each other in this direction, making a strong signal available at P. Field strength measured at P is greater than it would be if the total power supplied to both antennas had been fed to a single dipole. Radiation toward point P1 is built up in the same manner.

*[Figure 4-27: Parallel elements in phase (figure not reproduced).]*

Next consider a wavefront traveling toward point Q from antenna B. By the time it reaches antenna A, 1/2 wavelength away, 1/2 cycle has elapsed. Therefore energy from antenna B meets the energy from antenna A 180 degrees out of phase. As a result, the energy moving toward point Q from the two sources cancels. In a like manner, radiation from antenna A traveling toward point Q1 meets and cancels the radiation in the same direction from antenna B. As a result, little propagation takes place in either direction along the QQ1 axis. Most of the energy is concentrated in both directions along the PP1 axis. When both antenna elements are fed from the same source, the result is the basic broadside array. When more than two elements are used in a broadside arrangement, they are all parallel and in the same plane, as shown in figure 4-26, view B. Current phase, indicated by the arrows, must be the same for all elements. The radiation pattern shown in figure 4-26, view C, is always bi-directional. This pattern is sharper than the one shown in figure 4-27 because of the additional two elements. Directivity and gain depend on the number of elements and the spacing between them.

GAIN AND DIRECTIVITY.—The physical disposition of dipoles operated broadside to each other allows for much greater coupling between them than can occur between collinear elements. Moving the parallel antenna elements closer together or farther apart affects the actual impedance of the entire array and the overall radiation resistance as well. As the spacing between broadside elements increases, the effect on the radiation pattern is a sharpening of the major lobes. When the array consists of only two dipoles spaced exactly 1/2 wavelength apart, no minor lobes are generated at all. Increasing the distance between the elements beyond that point, however, tends to throw off the phase relationship between the original current in one element and the current induced in it by the other element. The result is that, although the major lobes are sharpened, minor lobes are introduced, even with two elements. These, however, are not large enough to be of concern.

If you add the same number of elements to both a broadside array and a collinear array, the gain of the broadside array will be greater. Reduced radiation resistance resulting from the efficient coupling between dipoles accounts for most of this gain. However, certain practical factors limit the number of elements that may be used. The construction problem increases with the number of elements, especially when they are polarized horizontally.

**Q32.** What is the primary cause of broadside arrays losing efficiency when not operating at their designed frequency?

**Q33.** When more than two elements are used in a broadside array, how are the elements arranged?

**Q34.** As the spacing between elements in a broadside array increases, what is the effect on the major lobes?

End-Fire Arrays An end-fire array looks similar to a broadside array. The ladder-like appearance is characteristic of both (fig. 4-28, view A). The currents in the elements of the end-fire array, however, are usually 180 degrees out of phase with each other as indicated by the arrows. The construction of the end-fire array is like that of a ladder lying on its side (elements horizontal). The dipoles in an end-fire array are closer together (1/8-wavelength to 1/4 -wavelength spacing) than they are for a broadside array.

*[Figure 4-28: Typical end-fire array (figure not reproduced).]*

Closer spacing between elements permits compactness of construction. For this reason an end-fire array is preferred to other arrays when high gain or sharp directivity is desired in a confined space. However, the close coupling creates certain disadvantages. Radiation resistance is extremely low, sometimes as low as 10 ohms, making antenna losses greater. The end-fire array is confined to a single frequency. With changes in climatic or atmospheric conditions, the danger of detuning exists. RADIATION PATTERN.—The radiation pattern for a pair of parallel half-wave elements fed 180 degrees out of phase is shown in figure 4-29, view A. The elements shown are spaced 1/2 wavelength apart. In practice, smaller spacings are used. Radiation from elements L and M traveling toward point P begins 180 degrees out of phase. Moving the same distance over approximately parallel paths, the respective wavefronts from these elements remain 180 degrees out of phase. In other words, maximum cancellation takes place in the direction of P. The same condition is true for the opposite direction (toward P1). The P to P1 axis is the line of least radiation for the end-fire array.

*[Figure 4-29: Parallel elements 180 degrees out of phase (figure not reproduced).]*

Consider what happens along the QQ1 axis. Energy radiating from element M toward Q reaches element L in about 1/2 cycle (180 degrees) after it leaves its source. Since element L was fed 180 degrees out of phase with element M, the wavefronts are now in the same phase and are both moving toward Q reinforcing each other. Similar reinforcement occurs along the same axis toward Q1. This simultaneous movement towards Q and Q1 develops a bi-directional pattern. This is not always true in end-fire operation. Another application of the end-fire principle is one in which the elements are spaced 1/4 wavelength apart and phased 90 degrees from each other to produce a unidirectional pattern. In figure 4-29, view A, elements A and B are perpendicular to the plane represented by the page; therefore, only the ends of the antennas appear. In view B the antennas are rotated a quarter of a circle in space around the QQ1 axis so that they are seen in the plane of the elements themselves. Therefore, the PP1 axis, now perpendicular to the page, is not seen as a line. The RR1 axis, now seen as a line, is perpendicular to the PP1 axis as well as to the QQ1 axis. The end-fire array is directional in this plane also, although not quite as sharply. The reason for the greater broadness of the lobes can be seen by following the path of energy radiating from the midpoint of element B toward point S in view B. This energy passes the A element at one end after traveling slightly more than the perpendicular distance between the dipoles. Energy, therefore, does not combine in exact phase toward point S. Although maximum radiation cannot take place in this direction, energy from the two sources combines closely enough in phase to produce considerable reinforcement. A similar situation exists for wavefronts traveling toward T. However, the wider angle from Q to T produces a greater phase difference and results in a decrease in the strength of the combined wave.

Directivity occurs from either one or both ends of the end-fire array, along the axis of the array, as shown by the broken arrows in figure 4-28, view A; hence, the term end-fire is used. The major lobe or lobes occur along the axis of the array. The pattern is sharper in the plane that is at right angles to the plane containing the elements (figure 4-29, view A). If the elements are not exact half-wave dipoles, operation is not significantly affected. However, because of the required balance of phase relationships and critical feeding, the array must be symmetrical. Folded dipoles, such as the one shown in figure 4-20, view A, are used frequently because the impedance at their terminals is higher. This is an effective way of avoiding excessive antenna losses. Another expedient to reduce losses is the use of tubular elements of wide diameter.

GAIN AND DIRECTIVITY.—In end-fire arrays, directivity increases with the addition of more elements and with spacings approaching the optimum. The directive pattern for a two-element, bi-directional system is illustrated in figure 4-29. View A shows radiation along the array axis in a plane perpendicular to the dipoles, and view B shows radiation along the array axis in the plane of the elements. These patterns were developed with a 180-degree phase difference between the elements. Additional elements introduce small, minor lobes.

With a 90-degree phase difference in the energy fed to a pair of end-fire elements spaced approximately 1/4 wavelength apart, unidirectional radiation can be obtained. The pattern perpendicular to the plane of the two elements is shown in figure 4-30, view A. The pattern shown in view B, taken in the same plane, is for a six-element array with 90-degree phasing between adjacent elements. Since both patterns show relative gain only, the increase in gain produced by the six-element array is not evident. End-fire arrays are the only unidirectional arrays wholly made up of driven elements.

*[Figure 4-30: Unidirectional end-fire arrays (figure not reproduced).]*

**Q35.** What are some disadvantages of the end-fire array?

**Q36.** Where does the major lobe in the end-fire array occur?

**Q37.** To maintain the required balance of phase relationships and critical feeding, how must the end-fire array be constructed?

Parasitic Arrays If a small light bulb were placed in the center of a large room, the illumination would be very poor. However, if a reflector were placed behind the bulb, the space in front of the reflector would be brighter and the space behind the reflector would be dimmer. The light rays would be concentrated. Also, if a lens were placed in front of the bulb, the light would be even more concentrated and a very bright spot would appear on the wall in front of the lens. A flashlight is a practical combination of the small bulb, the reflector, and the lens. The energy from an antenna can be reflected and concentrated in a similar manner. Although we do not usually discuss the gain of a flashlight, we can continue the comparison of an antenna and a flashlight to explain the meaning of antenna gain. Suppose the spot on the wall in front of the flashlight becomes 10 times brighter than it was when only the open bulb was used. The lens and reflector have then produced a 10-fold gain in light. For antennas, the simple half-wave antenna corresponds to the open bulb in the flashlight. Suppose an antenna system concentrates the radio waves so that at a particular point the field strength is 10 times more than it would be at the same distance from a half-wave antenna. The antenna system is then said to have a gain of 10.

Parasitic arrays represent another method of achieving high antenna gains. A parasitic array consists of one or more parasitic elements placed in parallel with each other and, in most cases, at the same line-of-sight level. The parasitic element is fed inductively by radiated energy coming from the driven element connected to the transmitter. It is in NO way connected directly to the driven element. When the parasitic element is placed so that it radiates away from the driven element, the element is a director. When the parasitic element is placed so that it radiates toward the driven element, the parasitic element is a reflector.

The directivity pattern resulting from the action of parasitic elements depends on two factors. These are (1) the tuning, determined by the length of the parasitic element; and (2) the spacing between the parasitic and driven elements. To a lesser degree, it also depends on the diameter of the parasitic element, since diameter has an effect on tuning.

OPERATION.—When a parasitic element is placed a fraction of a wavelength away from the driven element and is of approximately resonant length, it will re-radiate the energy it intercepts. The parasitic element is effectively a tuned circuit coupled to the driven element, much as the two windings of a transformer are coupled together. The radiated energy from the driven element causes a voltage to be developed in the parasitic element, which, in turn, sets up a magnetic field. This magnetic field extends over to the driven element, which then has a voltage induced in it. The magnitude and phase of the induced voltage depend on the length of the parasitic element and the spacing between the elements. In actual practice the length and spacing are arranged so that the phase and magnitude of the induced voltage cause a unidirectional, horizontal-radiation pattern and an increase in gain.

In the parasitic array in figure 4-31, view A, the parasitic and driven elements are spaced 1/4 wavelength apart. The radiated signal coming from the driven element strikes the parasitic element after 1/4 cycle. The voltage developed in the parasitic element is 180 degrees out of phase with that of the driven element. This is because of the distance traveled (90 degrees) and because the induced current lags the inducing flux by 90 degrees (90 + 90 = 180 degrees). The magnetic field set up by the parasitic element induces a voltage in the driven element 1/4 cycle later because the spacing between the elements is 1/4 wavelength. This induced voltage is in phase with that in the driven element and causes an increase in radiation in the direction indicated in figure 4-31, view A. Since the direction of the radiated energy is stronger in the direction away from the parasitic element (toward the driven element), the parasitic element is called a reflector. The radiation pattern as it would appear if you were looking down on the antenna is shown in view B. The pattern as it would look if viewed from the ends of the elements is shown in view C.

*[Figure 4-31: Patterns obtained using a reflector with proper spacing (figure not reproduced).]*

Because the voltage induced in the reflector is 180 degrees out of phase with the signal produced at the driven element, a reduction in signal strength exists behind the reflector. Since the magnitude of an induced voltage never quite equals that of the inducing voltage, even in very closely coupled circuits, the energy behind the reflector (minor lobe) is not reduced to 0.

The spacing between the reflector and the driven element can be reduced to about 15 percent of a wavelength. The parasitic element must be made electrically inductive before it will act as a reflector. If this element is made about 5 percent longer than 1/2 wavelength, it will act as a reflector when the spacing is 15 percent of a wavelength.

Changing the spacing and length can change the radiation pattern so that maximum radiation is on the same side of the driven element as the parasitic element. In this instance the parasitic element is called a director.

Combining a reflector and a director with the driven element causes a decrease in back radiation and an increase in directivity. This combination results in the two main advantages of a parasitic array— unidirectivity and increased gain. If the parasitic array is rotated, it can pick up or transmit in different directions because of the reduction of transmitted energy in all but the desired direction. An antenna of this type is called a ROTARY ARRAY. Size for size, both the gain and directivity of parasitic arrays are greater than those of driven arrays. The disadvantage of parasitic arrays is that their adjustment is critical and they do not operate over a wide frequency range.

GAIN AND DIRECTIVITY.—Changing the spacing between either the director or the reflector and the driven element results in a change in the radiation pattern. More gain and directivity are obtained by changing the length of the parasitic elements.

The FRONT-TO-BACK RATIO of an array is the proportion of energy radiated in the principal direction of radiation to the energy radiated in the opposite direction. A high front-to-back ratio is desirable because this means that a minimum amount of energy is radiated in the undesired direction. Since completely suppressing all such radiation is impossible, an infinite ratio cannot be achieved. In actual practice, however, rather high values can be attained. Usually the length and spacing of the parasitic elements are adjusted so that a maximum front-to-back ratio is obtained, rather than maximum gain in the desired direction.

**Q38.** What two factors determine the directivity pattern of the parasitic array?

**Q39.** What two main advantages of a parasitic array can be obtained by combining a reflector and a director with the driven element?

**Q40.** The parasitic array can be rotated to receive or transmit in different directions. What is the name given to such an antenna?

**Q41.** What are the disadvantages of the parasitic array?

Multielement Parasitic Array A MULTIELEMENT PARASITIC array is one that contains two or more parasitic elements with the driven element. If the array contains two parasitic elements (a reflector and a director) in addition to the driven element, it is usually known as a THREE-ELEMENT ARRAY. If three parasitic elements are used, the array is known as a FOUR-ELEMENT ARRAY, and so on. Generally speaking, if more parasitic elements are added to a three-element array, each added element is a director. The field behind a reflector is so small that additional reflectors would have little effect on the overall radiation pattern. In radar, from one to five directors are used.

CONSTRUCTION.—The parasitic elements of a multi-element parasitic array usually are positioned as shown in figure 4-32, views A and B. Proper spacings and lengths are determined experimentally. A folded dipole (view B) is often used as the driven element to obtain greater values of radiation resistance.

*[Figure 4-32: Yagi antenna (figure not reproduced).]*

YAGI ANTENNAS.—An example of a multielement parasitic array is the YAGI ANTENNA (figure 4-32, views A and B). The spacings between the elements are not uniform. The radiation from the different elements arrives in phase in the forward direction, but out of phase by various amounts in the other directions.

The director and the reflector in the Yagi antenna are usually welded to a conducting rod or tube at their centers. This support does not interfere with the operation of the antenna. Since the driven element is center-fed, it is not welded to the supporting rod. The center impedance can be increased by using a folded dipole as the driven element.

The Yagi antenna shown in figure 4-32, view A, has three directors. In general, the greater number of parasitic elements used, the greater the gain. However, a greater number of such elements causes the array to have a narrower frequency response as well as a narrower beamwidth. Therefore, proper adjustment of the antenna is critical. The gain does not increase directly with the number of elements used. For example, a three-element Yagi array has a relative power gain of 5 dB. Adding another director results in a 2 dB increase. Additional directors have less and less effect.

A typical Yagi array used for receiving and transmitting energy is shown with a support frame in figure 4-33. This antenna is used by the military services. It operates at frequencies of from 12 to 50 megahertz and consists of two separate arrays (one high-frequency and one low-frequency antenna array) mounted on one frame. The various elements are indicated in the figure. The high-frequency (hf) array consists of one reflector, one driven element, and two directors; the low-frequency (lf) array has the same arrangement with one less director. The lengths of the elements in the high-frequency array are shorter than those in the low-frequency array. The physical lengths of the elements in the individual arrays are equal, but the electrical lengths can be varied by means of the tuning stubs at the center of the elements. The array can be rotated in any desired direction by a remotely controlled, electrically driven, antenna rotator.

*[Figure 4-33: A typical parasitic array used for transmitting and receiving (figure not reproduced).]*

**Q42.** What is the advantage of adding parasitic elements to a Yagi array?

**Q43.** The Yagi antenna is an example of what type of array?

### Special Antennas

In this section we will cover some special communications and radar antennas. Some of these antennas we touch on briefly since they are covered thoroughly in other courses.

Previously discussed antennas operate with standing waves of current and voltage along the wires. This section deals principally with antenna systems in which the current is practically uniform in all parts of the antenna. In its basic form, such an antenna consists of a single wire grounded at the far end through a resistor. The resistor has a value equal to the characteristic impedance of the antenna. This termination, just as in the case of an ordinary transmission line, eliminates standing waves. The current, therefore, decreases uniformly along the wire as the terminated end is approached. This decrease is caused by the loss of energy through radiation. The energy remaining at the end of the antenna is dissipated in the terminating resistor. For such an antenna to be a good radiator, its length must be fairly long. Also, the wire must not be too close to the ground. The return path through the ground will cause cancellation of the radiation. If the wire is sufficiently long, it will be practically nonresonant over a wide range of operating frequencies.

### Long-Wire Antenna

A LONG-WIRE ANTENNA is an antenna that is a wavelength or longer at the operating frequency. In general, the gain achieved with long-wire antennas is not as great as the gain obtained from the multielement arrays studied in the previous section. But the long-wire antenna has advantages of its own. The construction of long-wire antennas is simple, both electrically and mechanically, with no particularly critical dimensions or adjustments. The long-wire antenna will work well and give satisfactory gain and directivity over a frequency range up to twice the value for which it was cut. In addition, it will accept power and radiate it efficiently on any frequency for which its overall length is not less than approximately 1/2 wavelength. Another factor is that long-wire antennas have directional patterns that are sharp in both the horizontal and vertical planes. Also, they tend to concentrate the radiation at the low vertical angles. Another type of long-wire antenna is the BEVERAGE ANTENNA, also called a WAVE ANTENNA. It is a horizontal, long-wire antenna designed especially for the reception and transmission of low-frequency, vertically polarized ground waves. It consists of a single wire, two or more wavelengths long, supported 3 to 6 meters above the ground, and terminated in its characteristic impedance, as shown in figure 4-34.

*[Figure 4-34: Beverage antenna (figure not reproduced).]*

**Q44.** To radiate power efficiently, a long-wire antenna must have what minimum overall length?

**Q45.** What is another name for the Beverage antenna?
