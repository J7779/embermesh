---
id: radio/neets10-ch3-transmission-lines-2
title: "NEETS Module 10 Ch. 3: Principles of Transmission Lines (2/2)"
category: radio
tags: [radio, transmission line, coax, SWR, impedance, feedline]
summary: "Navy training text on transmission lines: types, losses, characteristic impedance, reflections and standing waves. (continued)"
source: "https://archive.org/details/NEETSModule10"
source_title: "Navy Electricity and Electronics Training Series (NEETS) Module 10: Introduction to Wave Propagation, Transmission Lines, and Antennas (NAVEDTRA 14182)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Naval Education and Training Professional Development and Technology Center, US Navy. US Government work; 'Approved for public release; distribution is unlimited.' Figures not reproduced; end-of-chapter question answers retained."
retrieved: 2026-11-13
part_of: radio/neets10
order: 32
---

# NEETS Module 10 Ch. 3: Principles of Transmission Lines (2/2)

## Reflection Of Dc Voltage From An Open Circuit

The equivalent circuit of an open-ended transmission line is shown in figure 3-24, view A. Again, losses are to be considered as negligible, and L is lumped in one branch. Assume that (1) the battery in this circuit has an internal impedance equal to the characteristic impedance of the transmission line (Zi = Z0); (2) the capacitors in the line are not charged before the battery is connected; and (3) since the line is open-ended, the terminating impedance is infinitely large.

*[Figure 3-24: Reflection from an open-ended line (figure not reproduced).]*

When the battery is connected to the sending end as shown, a negative voltage moves down the line. This voltage charges each capacitor, in turn, through the preceding inductor. Since Zi equals Z 0, one-half the applied voltage will appear across the internal battery impedance, Zi, and one-half across the impedance of the line, Z0. Each capacitor is then charged to E/2 (view B). When the last capacitor in the line is charged, there is no voltage across the last inductor and current flow through the last inductor stops. With no current flow to maintain it, the magnetic field in the last inductor collapses and forces current to continue to flow in the same direction into the last capacitor. Because the direction of current has not changed, the capacitor charges in the same direction, thereby increasing the charge in the capacitor. Since the energy in the magnetic field equals the energy in the capacitor, the energy transfer to the capacitor doubles the voltage across the capacitor. The last capacitor is now charged to E volts and the current in the last inductor drops to zero.

At this point, the same process takes place with the next to the last inductor and capacitor. When the magnetic field about the inductor collapses, current continues to flow into the next to the last capacitor, charging it to E volts. This action continues backward down the line until the first capacitor has been fully charged to the applied voltage. This change of voltage, moving backward down the line, can be thought of in the following manner. The voltage, arriving at the end of the line, finds no place to go and returns to the sending end with the same polarity (view C). Such action is called REFLECTION.

When a reflection of voltage occurs on an open-ended line, the polarity is unchanged. The voltage change moves back to the source, charging each capacitor in turn until the first capacitor is charged to the source voltage and the action stops (view D). As each capacitor is charged, current in each inductor drops to zero, effectively reflecting the current with the opposite polarity (view C). Reflected current of opposite polarity cancels the original current at each point, and the current drops to zero at that point. When the last capacitor is charged, the current from the source stops flowing (view D). Important facts to remember in the reflection of dc voltages in open-ended lines are:

- Voltage is reflected from an open end without change in polarity, amplitude, or shape.
- Current is reflected from an open end with opposite polarity and without change in amplitude or shape.

### Reflection Of Dc Voltage From A Short Circuit

A SHORT-CIRCUITED line affects voltage change differently from the way an open-circuited line affects it. The voltage across a perfect short circuit must be zero; therefore, no power can be absorbed in the short, and the energy is reflected toward the generator.

The initial circuit is shown in figure 3-25, view A. The initial voltage and current waves (view B) are the same as those given for an infinite line. In a short-circuited line the voltage change arrives at the last inductor in the same manner as the waves on an open-ended line. In this case, however, there is no capacitor to charge. The current through the final inductor produces a voltage with the polarity shown in view C. When the field collapses, the inductor acts as a battery and forces current through the capacitor in the opposite direction, causing it to discharge (view D). Since the amount of energy stored in the magnetic field is the same as that in the capacitor, the capacitor discharges to zero.

*[Figure 3-25: Reflection from a short-circuited line (figure not reproduced).]*

Now there is no voltage to maintain the current through the next to the last inductor. Therefore, this inductor discharges the next to the last capacitor.

As each capacitor is discharged to zero, the next inductor effectively becomes a new source of voltage. The amplitude of each of these voltages is equal to E/2, but the polarity is the opposite of the battery at the input end of the line. The collapsing field around each inductor, in turn, produces a voltage that forces the current to continue flowing in the same direction, adding to the current from the source to make it 2I. This action continues until all the capacitors are discharged (view E).

Reflected waves from a short-circuited transmission line are characterized as follows:

- The reflected voltage has the opposite polarity but the same amplitude as the incident wave.
- The reflected current has the same polarity and the same amplitude as the incident current.

### Reflection Of Ac Voltage From An Open Circuit

In most cases where rf lines are used, the voltages applied to the sending end are ac voltages. The action at the receiving end of the line is exactly the same for ac as for dc. In the open-ended line, shown in figure 3-26, view A, the generated ac voltage is distributed along the line, shown in view B. This voltage is distributed in such a way that as each instantaneous voltage arrives at the end, it is reflected with the same polarity and amplitude. When ac is used, this reflection is in phase. Each of the reflected voltages travels back along the line until it reaches the generator. If the generator impedance is the same as the line impedance, energy arriving at the generator is absorbed and not reflected again. Now two voltages are on the line.

*[Figure 3-26: Formation of standing waves (figure not reproduced).]*

View B shows how two waves of the same frequency and amplitude moving in opposite directions on the same conductor will combine to form a resultant wave. The small solid line is moving steadily from left to right and is the INCIDENT WAVE (from the source). The broken-line waveform is moving from right to left and is the REFLECTED WAVE. The resultant waveform, the heavy line, is found by algebraically adding instantaneous values of the two waveforms. The resultant waveform has an instantaneous peak amplitude that is equal to the sum of the peak amplitudes of the incident and reflected waves. Since most indicating instruments are unable to separate these voltages, they show the vector sum. An oscilloscope is usually used to study the instantaneous voltages on rf lines.

Since two waves of voltage are moving on the line, you need to know how to distinguish between the two. The voltages moving toward the receiving end are called INCIDENT VOLTAGES, and the whole waveshape is called the INCIDENT WAVE. The wave moving back to the sending end after reflection is called the REFLECTED WAVE. The resultant voltage curve (view B of figure 3-26) shows that the voltage is maximum at the end of the line, a condition that occurs across an open circuit. Another step in investigating the open-circuited rf line is to see how the current waves act. The incident current wave is the solid line in figure 3-26, view C. The voltage is represented by the dotted line. The current is in phase with the voltage while traveling toward the receiving end. At the end of the line, the current is reflected in the opposite polarity; that is, it is shifted 180 degrees in phase, but its amplitude remains the same. The reflected wave of current is shown by dashed lines in view C. The heavy-line curve represents the sum of the two instantaneous currents and is the resultant wave. Notice that current is zero at the end of the line. This is reasonable, since there can be no current flow through an open circuit.

Views B and C of figure 3-26 show the voltage and current distribution along a transmission line at a point about 1/8 after a maximum voltage or current reaches the end of the line. Since the instantaneous values are continuously changing during the generation of a complete cycle, a large number of these pictures are required to show the many different relationships.

*[Figure 3-27: shows the incident and reflected waveshapes at several different times. The diagrams in the left column of figure 3-27 (representing voltage) show the incident wave and its reflection without change in polarity. In figure 3-27, waveform (1), the incident wave and the reflected wave are added algebraically to produce the resultant wave indicated by the heavy line. In waveform (2), a zero point preceding the negative-going cycle of the incident wave is at the end of the line. The reflected wave and incident wave are 180 degrees out of phase at all points. (The reflected wave is the positive cycle that just preceded the negative cycle now approaching the end of the line.) The resultant of the incident and (figure not reproduced).]*

/G85/G72/G73/G79/G72/G70/G87/G72/G71/G3/G90/G68/G89/G72/G86/G3/G76/G86/G3/G93/G72/G85/G82/G3/G68/G87/G3/G68/G79/G79/G3/G83/G82/G76/G81/G87/G86/G3/G68/G79/G82/G81/G74/G3/G87/G75/G72/G3/G79/G76/G81/G72/G17/G3/G44/G81/G3/G90/G68/G89/G72/G73/G82/G85/G80/G3/G11/G22/G12/G15/G3/G87/G75/G72/G3/G90/G68/G89/G72/G86/G3/G75/G68/G89/G72/G3/G80/G82/G89/G72/G71/G3/G20/G18/G27 /G3/G68/G79/G82/G81/G74/G3/G87/G75/G72 line; the incident wave has moved 45 degrees to the right, and the reflected wave has moved 45 degrees to the left. The resultant voltage, shown by the heavy line, has a maximum negative at the end of the line /G68/G81/G71/G3/G68/G3/G80/G68/G91/G76/G80/G88/G80/G3/G83/G82/G86/G76/G87/G76/G89/G72/G3/G20/G18/G21 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G3/G82/G73/G3/G87/G75/G72/G3/G79/G76/G81/G72/G17

*[Figure 3-27: Instantaneous values of incident and reflected waves on an open-ended line (figure not reproduced).]*

In waveform (4), the incident wave is at a maximum negative value at the end of the line. The wave has moved another 45 degrees to the right from the wave in the preceding illustration. The reflected wave has also moved 45 degrees, but to the left. The reflected wave is in phase with the incident wave. The resultant of these two waves, shown by the dark line, again has a negative maximum at the end of the line /G68/G81/G71/G3/G68/G3/G83/G82/G86/G76/G87/G76/G89/G72/G3/G80/G68/G91/G76/G80/G88/G80/G3/G20/G18/G21 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G3/G82/G73/G3/G87/G75/G72/G3/G79/G76/G81/G72/G17/G3/G49/G82/G87/G76/G70/G72/G3/G87/G75/G68/G87/G3/G87/G75/G72/G86/G72/G3/G80/G68/G91/G76/G80/G68/G3/G75/G68/G89/G72/G3a greater amplitude than those in waveform (3).

In waveform (5), the incident wave has moved another 45 degrees to the right and the reflected wave /G23/G24/G3/G71/G72/G74/G85/G72/G72/G86/G3/G87/G82/G3/G87/G75/G72/G3/G79/G72/G73/G87/G17/G3/G55/G75/G72/G3/G85/G72/G86/G88/G79/G87/G68/G81/G87/G3/G68/G74/G68/G76/G81/G3/G76/G86/G3/G80/G68/G91/G76/G80/G88/G80/G3/G81/G72/G74/G68/G87/G76/G89/G72/G3/G68/G87/G3/G87/G75/G72/G3/G72/G81/G71/G3/G68/G81/G71/G3/G83/G82/G86/G76/G87/G76/G89/G72/G3/G80/G68/G91/G76/G80/G88/G80/G3/G20/G18/G21 from the end. The maxima are lower than those in waveform (4). In waveform (6), the incident and /G85/G72/G73/G79/G72/G70/G87/G72/G71/G3/G90/G68/G89/G72/G3/G75/G68/G89/G72/G3/G80/G82/G89/G72/G71/G3/G68/G81/G82/G87/G75/G72/G85/G3/G20/G18/G27 /G17/G3/G55/G75/G72/G3/G87/G90/G82/G3/G90/G68/G89/G72/G86/G3/G68/G74/G68/G76/G81/G3/G68/G85/G72/G3/G20/G27/G19/G3/G71/G72/G74/G85/G72/G72/G86/G3/G82/G88/G87/G3/G82/G73/G3/G83/G75/G68/G86/G72/G15/G3/G74/G76/G89/G76/G81/G74/G3/G68 resultant wave with no amplitude. The incident and reflected waves continue moving in opposite directions, adding to produce the resultant waveshapes shown in waveforms (7) and (8). Notice that the /G80/G68/G91/G76/G80/G88/G80/G3/G89/G82/G79/G87/G68/G74/G72/G3/G76/G81/G3/G72/G68/G70/G75/G3/G85/G72/G86/G88/G79/G87/G68/G81/G87/G3/G90/G68/G89/G72/G3/G76/G86/G3/G68/G87/G3/G87/G75/G72/G3/G72/G81/G71/G3/G68/G81/G71/G3/G20/G18/G21 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G17 Study each part of figure 3-27 carefully and you will get a clear picture of how the resultant waveforms of voltage are produced. You will also see that the resultant voltage wave on an open-ended /G79/G76/G81/G72/G3/G76/G86/G3/G68/G79/G90/G68/G92/G86/G3/G93/G72/G85/G82/G3/G68/G87/G3/G20/G18/G23 /G3/G68/G81/G71/G3/G22/G18/G23 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G3/G82/G73/G3/G87/G75/G72/G3/G87/G85/G68/G81/G86/G80/G76/G86/G86/G76/G82/G81/G3/G79/G76/G81/G72/G17/G3/G54/G76/G81/G70/G72/G3/G87/G75/G72/G3/G93/G72/G85/G82/G3/G68/G81/G71/G3/G80/G68/G91/G76/G80/G88/G80 points are always in the same place, the resultant of the incident and the reflected wave is called a STANDING WAVE of voltage.

The right-hand column in figure 3-27 shows the current waveshapes on the open-ended line. Since the current is reflected out of phase at an open end, the resultant waveshapes differ from those for voltage. The two out-of-phase components always cancel at the end of the transmission line, so the resultant is always zero at that point. If you check all the resultant waveshapes shown in the right-hand column of /G73/G76/G74/G88/G85/G72/G3/G22/G16/G21/G26/G15/G3/G92/G82/G88/G3/G90/G76/G79/G79/G3/G86/G72/G72/G3/G87/G75/G68/G87/G3/G68/G3/G93/G72/G85/G82/G3/G83/G82/G76/G81/G87/G3/G68/G79/G90/G68/G92/G86/G3/G82/G70/G70/G88/G85/G86/G3/G68/G87/G3/G87/G75/G72/G3/G72/G81/G71/G3/G68/G81/G71/G3/G68/G87/G3/G68/G3/G83/G82/G76/G81/G87/G3/G20/G18/G21 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G17 /G48/G68/G91/G76/G80/G88/G80/G3/G89/G82/G79/G87/G68/G74/G72/G86/G3/G82/G70/G70/G88/G85/G3/G20/G18/G23 /G3/G68/G81/G71/G3/G22/G18/G23 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G17 When an ac meter is used to measure the voltages and currents along a line, the polarity is not indicated. If you plot all the current and voltage readings along the length of the line, you will get curves like the ones shown in figure 3-28. Notice that all are positive. These curves are the conventional method of showing current and voltage standing waves on rf lines.

*[Figure 3-28: Conventional picture of standing waves (figure not reproduced).]*

When an rf line is terminated in a short circuit, reflection is complete, but the effect on voltage and current differs from that in an open-ended line. Voltage is reflected in opposite phase, while current is reflected in phase. Again refer to the series of pictures shown in figure 3-27. However, this time the left column represents current, since it shows reflection in phase; and the right column of pictures now represents the voltage changes on the shorted line, since it shows reflection out of phase. The composite diagram in figure 3-29 shows all resultant curves on a full-wavelength section of line over a complete cycle. Notice that the amplitude of the voltage varies between zero and maximum in both directions at the center and at both ends as well but, one-fourth of the distance from each end the voltage is always zero. The resultant waveshape is referred to as a standing wave of voltage. Standing waves, then, are caused by reflections, which occur only when the line is not terminated in its characteristic impedance.

*[Figure 3-29: Composite results of instantaneous waves (figure not reproduced).]*

The voltage at the center and the ends varies at a sinusoidal rate between the limits shown. At the one-fourth the three-fourths points, the voltage is always zero. A continuous series of diagrams such as these is difficult to see with conventional test equipment, which reads the effective or average voltage over several cycles. The curve of amplitude over the length of line for several cycles is shown in figure 3-29, view B. A meter will read zero at the points shown and will show a maximum voltage at the center, no matter how many cycles pass.

As shown in view D, the amplitude varies along the length of the line. In this case it is zero at the end and center but maximum at the one-fourth and three-fourths points. The entire diagram of the open-ended line conditions is shown in view E. The standing waves of voltage and current appear together. Observe that one is maximum when the other is minimum. The current and voltage standing waves are one-quarter cycle, or 90 degrees, out of phase with one another.

### Reflection Of Ac Voltage From A Short Circuit

Reflection is complete when an rf line is terminated in a short circuit, but the effect on voltage and current differs from the effect obtained in an open-ended line. Voltage is reflected in opposite phase, while current is reflected in phase. Again look at the series of diagrams in figure 3-27. The left column represents current, and the right column shows voltage changes on the shorted line. The standard representation of standing waves on a shorted line is shown in figure 3-30; the voltage is a solid line, and /G87/G75/G72/G3/G70/G88/G85/G85/G72/G81/G87/G3/G76/G86/G3/G68/G3/G71/G68/G86/G75/G72/G71/G3/G79/G76/G81/G72/G17/G3/G55/G75/G72/G3/G89/G82/G79/G87/G68/G74/G72/G3/G76/G86/G3/G93/G72/G85/G82/G3/G68/G87/G3/G87/G75/G72/G3/G72/G81/G71/G3/G68/G81/G71/G3/G70/G72/G81/G87/G72/G85/G3/G11/G20/G18/G21 /G12/G3/G68/G81/G71/G3/G80/G68/G91/G76/G80/G88/G80/G3/G68/G87/G3/G87/G75/G72/G3/G20/G18/G23 /G3/G68/G81/G71 /G22/G18/G23 /G3/G83/G82/G76/G81/G87/G86/G15/G3/G90/G75/G76/G79/G72/G3/G87/G75/G72/G3/G70/G88/G85/G85/G72/G81/G87/G3/G76/G86/G3/G80/G68/G91/G76/G80/G88/G80/G3/G68/G87/G3/G87/G75/G72/G3/G72/G81/G71/G3/G68/G81/G71/G3/G70/G72/G81/G87/G72/G85/G3/G68/G81/G71/G3/G80/G76/G81/G76/G80/G88/G80/G3/G68/G87/G3/G87/G75/G72/G3/G20/G18/G23 /G3/G68/G81/G71/G3/G22/G18/G23 /G3/G83/G82/G76/G81/G87/G86/G17

*[Figure 3-30: Standing waves on a shorted line (figure not reproduced).]*

As we discussed voltage and current waves on transmission lines, we pointed out several differences between open and shorted lines. Basic differences also appear in the standing-wave patterns for open and shorted lines. You can see these differences by comparing figure 3-29, view E, and figure 3-30. Notice that the current and voltage standing waves are shifted 90 degrees with respect to the termination. At the open end of a line, voltage is maximum (zero if there are no losses in the line). At a short circuit, current is maximum and voltage is minimum.

**Q23.** Two types of waves are formed on a transmission line. What names are given to these waves?

**Q24.** In figure 3-27, which waveforms on the left have a resultant wave of zero, and what is indicated by these waves?

**Q25.** On an open-ended transmission line, the voltage is always zero at what distance from each end of the line?

### Terminating A Transmission Line

A transmission line is either NONRESONANT or RESONANT. First, let us define the terms nonresonant lines and resonant lines. A nonresonant line is a line that has no standing waves of current and voltage. A resonant line is a line that has standing waves of current and voltage. Nonresonant Lines A nonresonant line is either infinitely long or terminated in its characteristic impedance. Since no reflections occur, all the energy traveling down the line is absorbed by the load which terminates the line. Since no standing waves are present, this type of line is sometimes spoken of as a FLAT line. In addition, because the load impedance of such a line is equal to Z0, no special tuning devices are required to effect a maximum power transfer; hence, the line is also called an UNTUNED line.

Resonant Lines A resonant line has a finite length and is not terminated in its characteristic impedance. Therefore reflections of energy do occur. The load impedance is different from the Z0 of the line; therefore, the input impedance may not be purely resistive but may have reactive components. Tuning devices are used to eliminate the reactance and to bring about maximum power transfer from the source to the line. Therefore, a resonant line is sometimes called a TUNED line. The line also may be used for a resonant or tuned circuit.

A resonant line is sometimes said to be resonant at an applied frequency. This means that at one frequency the line acts as a resonant circuit. It may act either as a high-resistive circuit (parallel resonant) or as a low-resistive circuit (series resonant). The line may be made to act in this manner by either open-or short-circuiting it at the output end and cutting it to some multiple of a quarter-wavelength. At the points of voltage maxima and minima on a short-circuited or open-circuited line, the line impedance is resistive. On a short-circuited line, each point at an odd number of quarter-wavelengths from the receiving end has a high impedance (figure 3-31, view A). If the frequency of the applied voltage to the line is varied, this impedance decreases as the effective length of the line changes. This variation is exactly the same as the change in the impedance of a parallel-resonant circuit when the applied frequency is varied.

*[Figure 3-31: Sending-end impedance of various lengths and terminations (figure not reproduced).]*

At all even numbered quarter-wavelength points from the short circuit, the impedance is extremely low. When the frequency of the voltage applied to the line is varied, the impedance at these points increases just as the impedance of a series-resonant circuit varies when the frequency applied to it is changed. The same is true for an open-ended line (figure 3-31, view B) except that the points of high and low impedance are reversed.

At this point let us review some of the characteristics of resonant circuits so we can see how resonant line sections may be used in place of LC circuits.

A PARALLEL-RESONANT circuit has the following characteristics:

- At resonance the impedance appears as a very high resistance. A loss-free circuit has infinite impedance (an open circuit). Other than at resonance, the impedance decreases rapidly.
- If the circuit is resonant at a point above the generator frequency (the generator frequency is too low), more current flows through the coil than through the capacitor. This happens because XL decreases with a decrease in frequency but XC increases.

A SERIES-RESONANT circuit has these characteristics:

- At resonance the impedance appears as a very low resistance. A loss-free circuit has zero impedance (a short circuit). Other than at resonance the impedance increases rapidly.
- If the circuit is resonant at a point above the generator frequency (the generator frequency is too low), then XC is larger than XL and the circuit acts capacitively.
- If the circuit is resonant at a point below the generator frequency (the generator frequency is too high), then XL is larger than XC and the circuit acts inductively.

Since the impedance a generator sees at the quarter-wave point in a shorted line is that of a parallel-resonant circuit, a shorted quarter-wave- length of line may be used as a parallel-resonant circuit (figure 3-31, view C). An open quarter-wavelength of line may be used as a series-resonant circuit (view D). The Q of such a resonant line is much greater than can be obtained with lumped capacitance and inductance. Impedance for Various Lengths of Open Lines In figure 3-32, the impedance (Z) the generator sees for various lengths of line is shown at the top. The curves above the letters of various heights show the relative value of the impedances presented to the generator for the various line lengths. The circuit symbols indicate the equivalent electrical circuits for the transmission lines at each particular length. The standing waves of voltage and current are shown on each length of line.

*[Figure 3-32: Voltage, current, and impedance on open line (figure not reproduced).]*

/G36/G87/G3/G68/G79/G79/G3/G82/G71/G71/G3/G84/G88/G68/G85/G87/G72/G85/G16/G90/G68/G89/G72/G3/G83/G82/G76/G81/G87/G86/G3/G11/G20/G18/G23 /G15/G3/G22/G18/G23 /G15/G3/G72/G87/G70/G17/G12/G15/G3/G87/G75/G72/G3/G89/G82/G79/G87/G68/G74/G72/G3/G76/G86/G3/G80/G76/G81/G76/G80/G88/G80/G15/G3/G87/G75/G72/G3/G70/G88/G85/G85/G72/G81/G87/G3/G76/G86/G3/G80/G68/G91/G76/G80/G88/G80/G15 and the impedance is minimum. Thus, at all odd quarter-wave points, the open-ended transmission line acts as a series-resonant circuit. The impedance is equivalent to a very low resistance, prevented from being zero only by small circuit losses.

/G36/G87/G3/G68/G79/G79/G3/G72/G89/G72/G81/G3/G84/G88/G68/G85/G87/G72/G85/G16/G90/G68/G89/G72/G3/G83/G82/G76/G81/G87/G86/G3/G11/G20/G18/G21 /G15/G3/G20 /G15/G3/G22/G18/G21 /G15/G3/G72/G87/G70/G17/G12/G15/G3/G87/G75/G72/G3/G89/G82/G79/G87/G68/G74/G72/G3/G76/G86/G3/G80/G68/G91/G76/G80/G88/G80/G15/G3/G87/G75/G72/G3/G70/G88/G85/G85/G72/G81/G87/G3/G76/G86 minimum, and the impedance is maximum. Comparison of the line with an LC resonant circuit shows that at an even number of quarter-wavelengths, an open line acts as a parallel-resonant circuit. The impedance is therefore an extremely high resistance.

In addition, resonant open lines may also act as nearly pure capacitances or inductances. The illustration shows that an open line less than a quarter-wavelength long acts as a capacitance. Also, it acts as an inductance from 1/4 to 1/2 wavelength, as a capacitance from 1/2 to 3/4 wavelength, and as an inductance from 3/4 to 1 wavelength, etc. A number of open transmission lines, with their equivalent circuits, are shown in the illustration.

Impedance of Various Lengths of Shorted Lines Follow figure 3-33 as we study the shorted line. At the odd quarter-wavelength points, the voltage is high, the current is low, and the impedance is high. Since these conditions are similar to those found in a parallel-resonant circuit, the shorted transmission line acts as a parallel-resonant circuit at these lengths.

*[Figure 3-33: Voltage, current, and impedance on shorted line (figure not reproduced).]*

At the even quarter-wave points voltage is minimum, current is maximum, and impedance is minimum. Since these characteristics are similar to those of a series-resonant LC circuit, a shorted transmission line whose length is an even number of quarter-wavelengths acts as a series-resonant circuit. Resonant shorted lines, like open-end lines, also may act as pure capacitances or inductances. The illustration shows that a shorted line less than 1/4 wavelength long acts as an inductance. A shorted line with a length of from 1/4 to 1/2 wavelength acts as a capacitance. From 1/2 to 3/4 wavelength, the line acts as an inductance; and from 3/4 to 1 wavelength, it acts as a capacitance, and so on. The equivalent circuits of shorted lines of various lengths are shown in the illustration. Thus, properly chosen line segments may be used as parallel-resonant, series-resonant, inductive, or capacitive circuits.

### Standing Waves On A Transmission Line

There is a large variety of terminations for rf lines. Each type of termination has a characteristic effect on the standing waves on the line. From the nature of the standing waves, you can determine the type of termination that produces the waves.

### Termination In Z0

Termination in Z0 (characteristic impedance) will cause a constant reading on an ac meter when it is moved along the length of the line. As illustrated in figure 3-34, view A, the curve, provided there are no losses in the line, will be a straight line. If there are losses in the line, the amplitude of the voltage and current will diminish as they move down the line (view B). The losses are due to dc resistance in the line itself.

*[Figure 3-34: Effects of various terminations on standing waves (figure not reproduced).]*

### Termination In An Open Circuit

In an open-circuited rf line (figure 3-34, view C), the voltage is maximum at the end, but the current /G76/G86/G3/G80/G76/G81/G76/G80/G88/G80/G17/G3/G55/G75/G72/G3/G71/G76/G86/G87/G68/G81/G70/G72/G3/G69/G72/G87/G90/G72/G72/G81/G3/G87/G90/G82/G3/G68/G71/G77/G68/G70/G72/G81/G87/G3/G93/G72/G85/G82/G3/G70/G88/G85/G85/G72/G81/G87/G3/G83/G82/G76/G81/G87/G86/G3/G76/G86/G3/G20/G18/G21 /G15/G3/G68/G81/G71/G3/G87/G75/G72/G3/G71/G76/G86/G87/G68/G81/G70/G72/G3/G69/G72/G87/G90/G72/G72/G81 /G68/G79/G87/G72/G85/G81/G68/G87/G72/G3/G93/G72/G85/G82/G3/G70/G88/G85/G85/G72/G81/G87/G3/G83/G82/G76/G81/G87/G86/G3/G76/G86/G3/G20 /G17/G3/G55/G75/G72/G3/G89/G82/G79/G87/G68/G74/G72/G3/G76/G86/G3/G93/G72/G85/G82/G3/G68/G87/G3/G68/G3/G71/G76/G86/G87/G68/G81/G70/G72/G3/G82/G73/G3/G20/G18/G23 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G3/G82/G73/G3/G87/G75/G72/G3/G79/G76/G81/G72/G17/G3/G55/G75/G76/G86 /G76/G86/G3/G87/G85/G88/G72/G3/G68/G87/G3/G68/G81/G92/G3/G73/G85/G72/G84/G88/G72/G81/G70/G92/G17/G3/G36/G3/G89/G82/G79/G87/G68/G74/G72/G3/G83/G72/G68/G78/G3/G82/G70/G70/G88/G85/G86/G3/G68/G87/G3/G87/G75/G72/G3/G72/G81/G71/G3/G82/G73/G3/G87/G75/G72/G3/G79/G76/G81/G72/G15/G3/G68/G87/G3/G20/G18/G21 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G15/G3/G68/G81/G71/G3/G68/G87/G3/G72/G68/G70/G75 /G20/G18/G21 /G3/G87/G75/G72/G85/G72/G68/G73/G87/G72/G85/G17

### Termination In A Short Circuit

On the line terminated in a short circuit, shown in figure 3-34, view D, the voltage is zero at the end /G68/G81/G71/G3/G80/G68/G91/G76/G80/G88/G80/G3/G68/G87/G3/G20/G18/G23 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G17/G3/G55/G75/G72/G3/G70/G88/G85/G85/G72/G81/G87/G3/G76/G86/G3/G80/G68/G91/G76/G80/G88/G80/G3/G68/G87/G3/G87/G75/G72/G3/G72/G81/G71/G15/G3/G93/G72/G85/G82/G3/G68/G87/G3/G20/G18/G23 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G15/G3/G68/G81/G71 /G68/G79/G87/G72/G85/G81/G68/G87/G72/G79/G92/G3/G80/G68/G91/G76/G80/G88/G80/G3/G68/G81/G71/G3/G93/G72/G85/G82/G3/G72/G89/G72/G85/G92/G3/G20/G18/G23 /G3/G87/G75/G72/G85/G72/G68/G73/G87/G72/G85/G17

### Termination In Capacitance

When a line is terminated in capacitance, the capacitor does not absorb energy, but returns all of the energy to the circuit. This means there is 100 percent reflection. The current and voltage relationships are somewhat more involved than in previous types of termination. For this explanation, assume that the capacitive reactance is equal to the Z0 of the line. Current and voltage are in phase when they arrive at the end of the line, but in flowing through the capacitor and the characteristic impedance (Z0) connected in series, they shift in phase relationship. Current and voltage arrive in phase and leave out of phase. This results in the standing-wave configuration shown in figure 3-34, view E. The standing wave of voltage is /G80/G76/G81/G76/G80/G88/G80/G3/G68/G87/G3/G68/G3/G71/G76/G86/G87/G68/G81/G70/G72/G3/G82/G73/G3/G72/G91/G68/G70/G87/G79/G92/G3/G20/G18/G27 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G17/G3/G44/G73/G3/G87/G75/G72/G3/G70/G68/G83/G68/G70/G76/G87/G76/G89/G72/G3/G85/G72/G68/G70/G87/G68/G81/G70/G72/G3/G76/G86/G3/G74/G85/G72/G68/G87/G72/G85/G3/G87/G75/G68/G81/G3/G610 (smaller capacitance), the termination looks more like an open circuit; the voltage minimum moves away from the end. If the capacitive reactance is smaller than Z0, the minimum moves toward the end.

### Termination In Inductance

When the line is terminated in an inductance, both the current and voltage shift in phase as they arrive at the end of the line. When XL is equal to Z0, the resulting standing waves are as shown in figure 3/G16/G22/G23/G15/G3/G89/G76/G72/G90/G3/G41/G17/G3/G55/G75/G72/G3/G70/G88/G85/G85/G72/G81/G87/G3/G80/G76/G81/G76/G80/G88/G80/G3/G76/G86/G3/G79/G82/G70/G68/G87/G72/G71/G3/G20/G18/G27 /G3/G73/G85/G82/G80/G3/G87/G75/G72/G3/G72/G81/G71/G3/G82/G73/G3/G87/G75/G72/G3/G79/G76/G81/G72/G17/G3/G58/G75/G72/G81/G3/G87/G75/G72/G3/G76/G81/G71/G88/G70/G87/G76/G89/G72/G3/G85/G72/G68/G70/G87/G68/G81/G70/G72 is increased, the standing waves appear closer to the end. When the inductive reactance is decreased, the standing waves move away from the end of the line.

### Termination In A Resistance Not Equal To The Characteristic Impedance

(Z0) Whenever the termination is not equal to Z0, reflections occur on the line. For example, if the terminating element contains resistance, it absorbs some energy, but if the resistive element does not equal the Z0 of the line, some of the energy is reflected. The amount of voltage reflected may be found by using the equation:

Where:

ER = the reflected voltage Ei = the incident voltage RR = the terminating resistance Z0= the characteristic impedance of the line If you try different values of RL in the preceding equation, you will find that the reflected voltage is equal to the incident voltage only when RL equals 0 or is infinitely large. When RL equals Z0, no reflected voltage occurs. When RL is greater than Z0, ER is positive, but less than Ei. As RL increases and approaches an infinite value, ER increases and approaches Ei in value. When RL is smaller than Z0, ER has a negative value. This means that the reflected voltage is of opposite polarity to the incident wave at the termination of the line. As RL approaches zero, ER approaches Ei in value. The smaller the value of ER, the smaller is the peak amplitude of the standing waves and the higher are the minimum values.

### Termination In A Resistance Greater Than Z0

When RL is greater than Z0, the end of the line is somewhat like an open circuit; that is, standing waves appear on the line. The voltage maximum appears at the end of the line and also at half-wave intervals back from the end. The current is minimum (not zero) at the end of the line and maximum at the odd quarter-wave points. Since part of the power in the incident wave is consumed by the load resistance, the minimum voltage and current are less than for the standing waves on an open-ended line. Figure 3-34, view G, illustrates the standing waves for this condition.

### Termination In A Resistance Less Than Z0

When RL is less than Z0, the termination appears as a short circuit. The standing waves are shown in figure 3-34, view H. Notice that the line terminates in a current LOOP (peak) and a voltage NODE (minimum). The values of the maximum and minimum voltage and current approach those for a shorted line as the value of RL approaches zero.

A line does not have to be any particular length to produce standing waves; however, it cannot be an infinite line. Voltage and current must be reflected to produce standing waves. For reflection to occur, a line must not be terminated in its characteristic impedance. Reflection occurs on lines terminated in opens, shorts, capacitances, and inductances, because no energy is absorbed by the load. If the line is terminated in a resistance not equal to the characteristic impedance of the line, some energy will be absorbed and the rest will be reflected.

The voltage and current relationships for open-ended and shorted lines are opposite to each other, as shown in figure 3-34, views C and D. The points of maximum and minimum voltage and current are determined from the output end of the line, because reflection always begins at that end.

**Q26.** A nonresonant line is a line that has no standing waves of current and voltage on it and is considered to be flat. Why is this true?

**Q27.** On an open line, the voltage and impedance are maximum at what points on the line?

### Standing-Wave Ratio

The measurement of standing waves on a transmission line yields information about equipment operating conditions. Maximum power is absorbed by the load when ZL = Z0. If a line has no standing waves, the termination for that line is correct and maximum power transfer takes place. You have probably noticed that the variation of standing waves shows how near the rf line is to being terminated in Z0. A wide variation in voltage along the length means a termination far from Z0. A small variation means termination near Z 0. Therefore, the ratio of the maximum to the minimum is a measure of the perfection of the termination of a line. This ratio is called the STANDING-WAVE RATIO (swr) and is always expressed in whole numbers. For example, a ratio of 1:1 describes a line terminated in its characteristic impedance (Z 0).

Voltage Standing-Wave Ratio The ratio of maximum voltage to minimum voltage on a line is called the VOLTAGE STANDING- WAVE RATIO (vswr). Therefore:

The vertical lines in the formula indicate that the enclosed quantities are absolute and that the two values are taken without regard to polarity. Depending on the nature of the standing waves, the numerical value of vswr ranges from a value of 1 (ZL = Z0, no standing waves) to an infinite value for theoretically complete reflection. Since there is always a small loss on a line, the minimum voltage is never zero and the vswr is always some finite value. However, if the vswr is to be a useful quantity, the power losses along the line must be small in comparison to the transmitted power.

Power Standing-Wave Ratio The square of the voltage standing-wave ratio is called the POWER STANDING-WAVE RATIO (pswr). Therefore:

This ratio is useful because the instruments used to detect standing waves react to the square of the voltage. Since power is proportional to the square of the voltage, the ratio of the square of the maximum and minimum voltages is called the power standing-wave ratio. In a sense, the name is misleading because the power along a transmission line does not vary.

Current Standing-Wave Ratio The ratio of maximum to minimum current along a transmission line is called CURRENT STANDING-WAVE RATIO (iswr). Therefore:

This ratio is the same as that for voltages. It can be used where measurements are made with loops that sample the magnetic field along a line. It gives the same results as vswr measurements.

**Q28.** At what point on an open-circuited rf line do voltage peaks occur?

**Q29.** What is the square of the voltage standing-wave ratio called?

**Q30.** What does vswr measure?

### Summary

This chapter has presented information on the characteristics of transmission lines. The information that follows summarizes the important points of this chapter.

TRANSMISSION LINES are devices for guiding electrical energy from one point to another. INPUT IMPEDANCE is the ratio of voltage to current at the input end of a transmission line. OUTPUT IMPEDANCE is the ratio of voltage to current at the output end of the line.

TWO-WIRE OPEN LINES are parallel lines and have uses such as power lines, rural telephone lines, and telegraph lines. This type of line has high radiation losses and is subject to noise pickup. TWIN LEAD has parallel lines and is most often used to connect televisions to their antennas. A TWISTED PAIR consists of two insulated wires twisted together. This line has high insulation loss.

A SHIELDED PAIR has parallel conductors separated by a solid dielectric and surrounded by copper braided tubing. The conductors are balanced to ground.

RIGID COAXIAL LINE contains two concentric conductors insulated from each other by spacers. Some rigid coaxial lines are pressurized with an inert gas to prevent moisture from entering. High-frequency losses are less than with other lines.

FLEXIBLE COAXIAL LINES consist of a flexible inner conductor and a concentric outer conductor of metal braid. The two are separated by a continuous insulating material.

WAVEGUIDES are hollow metal tubes used to transfer energy from one point to another. The energy travels slower in a waveguide than in free space.

COPPER LOSSES can result from power (I 2R) loss, in the form of heat, or skin effect. These losses decrease the conductivity of a line.

DIELECTRIC LOSSES are caused by the heating of the dielectric material between conductors, taking power from the source.

RADIATION and INDUCTION LOSSES are caused by part of the electromagnetic fields of a conductor being dissipated into space or nearby objects.

A transmission line is either electrically LONG or SHORT/G3/G76/G73/G3/G76/G87/G86/G3/G83/G75/G92/G86/G76/G70/G68/G79/G3/G79/G72/G81/G74/G87/G75/G3/G76/G86/G3/G81/G82/G87/G3/G72/G84/G88/G68/G79/G3/G87/G82/G3/G20/G18/G23 for the frequency it is to carry.

LUMPED CONSTANTS are theoretical properties (inductance, resistance, and capacitance) of a transmission line that are lumped into a single component.

DISTRIBUTED CONSTANTS are constants of inductance, capacitance and resistance that are distributed along the transmission line.

LEAKAGE CURRENT flows between the wires of a transmission line through the dielectric. The dielectric acts as a resistor.

An ELECTROMAGNETIC FIELD exists along transmission line when current flows through it. CHARACTERISTIC IMPEDANCE, Z0, is the ratio of E to I at every point along the line. For maximum transfer of electrical power, the characteristic impedance and load impedance must be matched. The VELOCITY at which a wave travels over a given length of transmission line can be found by using the formula:

A transmission line that is not terminated in its characteristic impedance is said to be FINITE. When dc is applied to an OPEN-ENDED line, the voltage is reflected back from the open end without any change in polarity, amplitude, or shape. Current is reflected back with the same amplitude and shape but with opposite polarity.

When dc is applied to a SHORT-CIRCUITED line, the current is reflected back with the same amplitude, and polarity. The voltage is reflected back with the same amplitude but with opposite polarity. When ac is applied to an OPEN-END line, voltage is always reflected back in phase with the incident wave and current is reflected back out of phase.

When ac is applied to a SHORT-CIRCUITED line, voltage is reflected in opposite phase, while current is reflected in phase.

A NONRESONANT line has NO STANDING WAVES of current and voltage and is either infinitely long or terminated in its characteristic impedance.

A RESONANT line has STANDING WAVES of current and voltage and is of finite length and is NOT terminated in its characteristic impedance.

/G50/G81/G3/G68/G81/G3/G82/G83/G72/G81/G16/G72/G81/G71/G72/G71/G3/G85/G72/G86/G82/G81/G68/G81/G87/G3/G79/G76/G81/G72/G15/G3/G68/G81/G71/G3/G68/G87/G3/G68/G79/G79/G3/G82/G71/G71/G3/G20/G18/G23 /G3/G83/G82/G76/G81/G87/G86/G15/G3/G87/G75/G72/G3/G89/G82/G79/G87/G68/G74/G72/G3/G76/G86/G3minimum, the current is /G80/G68/G91/G76/G80/G88/G80/G15/G3/G68/G81/G71/G3/G87/G75/G72/G3/G76/G80/G83/G72/G71/G68/G81/G70/G72/G3/G76/G86/G3/G80/G76/G81/G76/G80/G88/G80/G17/G3/G36/G87/G3/G68/G79/G79/G3/G72/G89/G72/G81/G3/G20/G18/G23 /G3/G83/G82/G76/G81/G87/G86/G15/G3/G87/G75/G72/G3/G89/G82/G79/G87/G68/G74/G72/G3/G76/G86/G3maximum, the current is minimum and the impedance is maximum.

There are a variety of TERMINATIONS for rf lines. Each termination has an effect on the standing waves on the line.

A transmission line can be terminated in its characteristic impedance as an open- or short-circuit, or in capacitance or inductance.

Whenever the termination on a transmission line is NOT EQUAL TO Z 0, there are reflections on the line. The amount of voltage reflected may be found by using the equation:

When the termination on a transmission line EQUALS Z0, there is NO reflected voltage. The measurement of standing waves on a transmission line yields information about operating conditions. If there are NO standing waves, the termination for that line is correct and maximum power transfer takes place.

The STANDING WAVE RATIO is the measurement of maximum voltage (current) to minimum voltage (current) on a transmission line and measures the perfection of the termination of the line. A ratio of 1:1 describes a line terminated in its characteristic impedance.

### Answers To Questions Q1. Through Q30.

**A1.** Transmission line.

**A2.** Input end, generator end, transmitter end, sending end, and source.

**A3.** Output end, receiving end, load end and sink.

**A4.** Parallel two-wire, twisted pair, shielded pair, coaxial line and waveguide.

**A5.** Power lines, rural telephone lines, and telegraph lines.

**A6.** High radiation losses and noise pickup.

**A7.** Twin lead.

**A8.** The conductors are balanced to ground.

**A9.** Air coaxial (rigid) and solid coaxial (flexible).

**A10.** The ability to minimize radiation losses.

**A11.** Expensive to construct, must be kept dry, and high frequency losses limit the practical length of the line.

**A12.** Cylindrical and rectangular.

**A13.** Copper, dielectric, and radiation.

**A14.** Copper losses.

**A15.** Dielectric losses.

**A16.** /G3/G32/G3/G21/G19/G3/G80/G72/G87/G72/G85/G86/G17

**A17.** (1) Type of line used, (2) dielectric in the line, and (3) length of line.

**A18.** Inductance is expressed in microhenrys per unit length, capacitance is expressed in picofarads per unit length, and resistance is expressed in ohms per unit length.

**A19.** The small amount of current that flows through the dielectric between two wires of a transmission line and is expressed in micromhos per unit length.

**A20.** When the characteristic impedance of the transmission line and the load impedance are equal.

**A21.** Z0 and it is the ratio of E to I at every point along the line.

**A22.** Between 50 and 600 ohms.

**A23.** Incident waves from generator to load. Reflected waves from load back to generator.

**A24.** 2 and 6 have zero resultant wave and they indicate that the incident and reflected waves are 180 degrees out of phase at all parts.

**A25.** One-fourth the distance from each end of the line.

**A26.** The load impedance of such a line is equal to Z0.

**A27.** /G40/G89/G72/G81/G3/G84/G88/G68/G85/G87/G72/G85/G16/G90/G68/G89/G72/G3/G83/G82/G76/G81/G87/G86/G3/G11/G20/G18/G21 /G15/G3/G20 /G15/G3/G22/G18/G21 /G15/G3/G72/G87/G70/G17/G12/G17

**A28.** At 1/2 wavelength from the end and at every 1/2 wavelength along the line.

**A29.** Power standing-wave ratio (pswr).

**A30.** The existence of voltage variations on a line.
