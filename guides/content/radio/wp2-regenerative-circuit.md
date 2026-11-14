---
id: radio/wp2-regenerative-circuit
title: Regenerative circuit
category: radio
tags: [regenerative circuit, regeneration, reaction, regenerative comparator, regenerative receivers, radio]
summary: A regenerative circuit is an amplifier circuit that employs positive feedback (also known as regeneration or reaction).
source: https://en.wikipedia.org/w/index.php?title=Regenerative_circuit&oldid=1374383676
source_title: Wikipedia: Regenerative circuit
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Regenerative circuit", Wikipedia, The Free Encyclopedia (revision 1374383676), https://en.wikipedia.org/w/index.php?title=Regenerative_circuit&oldid=1374383676 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Regenerative circuit

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **regenerative circuit** is an amplifier circuit that employs positive feedback (also known as **regeneration** or **reaction**). Some of the output of the amplifying device is applied back to its input to add to the input signal, increasing the amplification. One example is the Schmitt trigger (which is also known as a **regenerative comparator**), but the most common use of the term is in RF amplifiers, and especially **regenerative receivers**, to greatly increase the gain of a single amplifier stage.

The regenerative receiver was invented in 1912 and patented in 1914 by American electrical engineer Edwin Armstrong when he was an undergraduate at Columbia University.

The regenerative receiver was widely used from the mid-1910s through the 1920s, with use declining during the 1930s and becoming uncommon by the early 1940s. Its principal advantage was high sensitivity with little added hardware, achieved by applying positive feedback around an RF detector stage and operating the circuit below the onset of oscillation.

Armstrong's key insight was that radio-frequency energy existed in the detector's plate circuit and could be fed back to the input, contrary to the prevailing belief that only audio frequencies remained after detection. When carefully adjusted, this feedback greatly increased the effective gain of a single active device, though operating required skill.

Regeneration improves selectivity by increasing loop gain near resonance, sharpening the frequency response without altering the intrinsic Q of the tuned circuit itself. The effect is equivalent to compensating circuit losses through feedback, simulating a negative resistance. As noted by Terman, regenerative detectors suffer from excessive selectivity, frequency-dependent critical adjustment, and a tendency toward oscillation that can produce interference and audible whistles if regeneration is increased too far. With the development of radio-frequency amplifiers designed around better tubes, regenerative detectors found relatively little application after the early 1930s.

A receiver circuit that used larger amounts of regeneration in a more complicated way to achieve even higher amplification, the superregenerative receiver, was also invented by Armstrong in 1922. It was never widely used in general commercial receivers, but due to its small parts count it was used in specialized applications. One widespread use during WWII was IFF transceivers, where single tuned circuit completed the entire electronics system. It is still used in a few specialized low data rate applications, such as garage door openers, wireless networking devices, walkie-talkies and toys.

## Regenerative receiver

In a regenerative receiver, a portion of the detector's RF output is fed back to its input through a tuned circuit, providing frequency-selective positive feedback. When adjusted below oscillation, this feedback substantially increases sensitivity and selectivity, allowing RF amplification and detection to be implemented using a single active device.

Regeneration sharpens the receiver's frequency response by increasing loop gain near resonance. The intrinsic Q of the tuned circuit itself is unchanged; instead, feedback compensates for circuit losses, producing behavior mathematically equivalent to reducing resistive loss. As the loop gain approaches unity, the effective bandwidth narrows rapidly. Oscillation begins when losses are fully compensated.

Contemporary measurements showed that regeneration could increase detector gain by orders of magnitude. For example, a type 36 screen-grid tube with a non-regenerative detection gain of about 9 at 7.2 MHz achieved gains exceeding 7,000 under critical regeneration, with higher values possible near oscillation.

Some commercial receivers combined regeneration with other techniques. The Crosley Trirdyn receivers of 1924 used regenerative detection together with reflex amplification.

A major improvement in stability and a small improvement in available gain for reception of CW radiotelegraphy is provided by the use of a separate oscillator, known as a *heterodyne oscillator* or *beat oscillator*. Providing the oscillation separately from the detector allows the regenerative detector to be set for maximum gain and selectivity - which is always in the non-oscillating condition. Interaction between the detector and the beat oscillator can be minimized by operating the beat oscillator at half of the receiver operating frequency, using the second harmonic of the beat oscillator in the detector.

### AM reception

For AM reception, the gain of the loop is adjusted so it is just below the level required for oscillation (a loop gain of just less than one). The result of this is to greatly increase the gain of the amplifier at the bandpass frequency (resonant frequency), while not increasing it at other frequencies. So the incoming radio signal is amplified by a large factor, 10³ - 10⁵, increasing the receiver's sensitivity to weak signals. The high gain also has the effect of reducing the circuit's bandwidth (increasing the Q) by an equal factor, increasing the selectivity of the receiver.

### CW reception (autodyne mode)

For the reception of CW radiotelegraphy (Morse code), the feedback is increased just to the point of oscillation. The tuned circuit is adjusted to provide typically 400 to 1000 Hertz difference between the receiver oscillation frequency and the desired transmitting station's signal frequency. The two frequencies *beat* in the nonlinear amplifier, generating heterodyne or *beat* frequencies. The difference frequency, typically 400 to 1000 Hertz, is in the audio range; so it is heard as a tone in the receiver's speaker whenever the station's signal is present.

Demodulation of a signal in this manner, by use of a single amplifying device as oscillator and mixer simultaneously, is known as *autodyne* reception. The term *autodyne* predates multigrid tubes and is not applied to use of tubes specifically designed for frequency conversion.

### SSB reception

For the reception of single-sideband (SSB) signals, the circuit is also adjusted to oscillate as in CW reception. The tuning is adjusted until the demodulated voice is intelligible.

### Advantages and disadvantages

Regenerative receivers require fewer components than other types of receiver circuit, such as the TRF and superheterodyne. The circuit's advantage was that it got much more amplification (gain) out of the expensive vacuum tubes, thus reducing the number of tubes required and therefore the cost of a receiver. Early vacuum tubes had low gain and tended to oscillate at radio frequencies (RF). TRF receivers often required 5 or 6 tubes; each stage requiring tuning and neutralization, making the receiver cumbersome, power hungry, and hard to adjust. A regenerative receiver, by contrast, could often provide adequate reception with the use of only one tube. In the 1930s the regenerative receiver was replaced by the superheterodyne circuit in commercial receivers due to the superheterodyne's superior performance and the falling cost of tubes. Since the advent of the transistor in 1946, the low cost of active devices has removed most of the advantage of the circuit. However, in recent years the regenerative circuit has seen a modest comeback in receivers for low cost digital radio applications such as garage door openers, keyless locks, RFID readers and some cell phone receivers.

A disadvantage of this receiver, especially in designs that couple the detector tuned circuit to the antenna, is that the regeneration (feedback) level must be adjusted when the receiver is tuned to a different frequency. The antenna impedance varies with frequency, changing the loading of the input tuned circuit by the antenna, requiring the regeneration to be adjusted. In addition, the Q of the detector tuned circuit components vary with frequency, requiring adjustment of the regeneration control.

A disadvantage of the single active device regenerative detector in autodyne operation is that the local oscillation causes the operating point to move significantly away from the ideal operating point, resulting in the detection gain being reduced.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Regenerative receiver.)_
