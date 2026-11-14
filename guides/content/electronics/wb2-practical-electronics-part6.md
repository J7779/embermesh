---
id: electronics/wb2-practical-electronics-part6
title: Practical Electronics (Wikibooks) (part 6 of 7: Amplifiers)
category: electronics
tags: [practical electronics, electronics, components, soldering, resistors, capacitors, transistors, circuits]
summary: Practical Electronics (Wikibooks), part 6 of 7: Amplifiers.
source: https://en.wikibooks.org/w/index.php?title=Practical_Electronics/Amplifiers&oldid=4661722
source_title: Wikibooks: Practical Electronics
license: CC BY-SA 4.0
attribution: Wikibooks contributors, "Practical Electronics/Amplifiers" (revision 4661722, https://en.wikibooks.org/w/index.php?oldid=4661722 ), Wikibooks, The Free Textbook Project. Converted to Markdown for the EmberMesh offline library; navigation templates, images, references and link lists omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-14
part_of: electronics/wb2-practical-electronics
order: 6
---

# Practical Electronics (Wikibooks) (part 6 of 7: Amplifiers)

_Wikibooks textbook pages (Wikibooks, CC BY-SA 4.0), converted to plain Markdown: navigation templates, images, references and link lists were removed._

## Amplifiers

Amplifiers can be specified according to their input and output properties. They have some kind of **gain**, or multiplication factor relating the magnitude of the output signal to the input signal. The gain may be specified as "output voltage/input voltage", "output power/input power" or any other combination of current, voltage and power. In many cases, with input and output in the same units, gain will be unitless; for others this is not necessarily so – for example, a transconductance amplifier has a gain with units of conductance (output current per input voltage).

In most cases an amplifier should be linear, that is the gain should be constant for any combination of input and output signal. If the gain is not linear, e.g. by clipping the output signal at the limits of its capabilities, the output signal is distorted.

### Classification of amplifier stages and systems

There are many alternative classifications that address different aspects of amplifier designs, and they all express some particular perspective relating the design parameters to the objectives of the circuit. Amplifier design is always a compromise of numerous factors, such as cost, power consumption, real-world device imperfections, and a multitude of performance specifications. Below are several different approaches to classification:

#### Input and output variables

Electronic amplifiers use two variables: current and voltage. Either can be used as input, and either as output leading to four types of amplifiers. In idealized form they are represented by each of the four types of dependent source used in linear analysis, as shown in the figure, namely:

| Input | Output | Dependent source | Amplifier type |
|---|---|---|---|
| **I** | **I** | current controlled current source **CCCS** | current amplifier |
| **I** | **V** | current controlled voltage source **CCVS** | transresistance amplifier |
| **V** | **I** | voltage controlled current source **VCCS** | transconductance amplifier |
| **V** | **V** | voltage controlled voltage source **VCVS** | voltage amplifier |

Each type of amplifier in its ideal form has an ideal input and output resistance that is the same as that of the corresponding dependent source:

| Amplifier type | Dependent source | Input impedance | Output impedance |
|---|---|---|---|
| **Current** | **CCCS** | **0** | **∞** |
| **Transresistance** | **CCVS** | **0** | **0** |
| **Transconductance** | **VCCS** | **∞** | **∞** |
| **Voltage** | **VCVS** | **∞** | **0** |

In practice the ideal impedances are only approximated. For any particular circuit, a small-signal analysis often is used to find the impedance actually achieved. A small-signal AC test current *I_x* is applied to the input or output node, all external sources are set to zero, and the corresponding alternating voltage *V_x* across the test current source determines the impedance seen at that node as *R = V_x / I_x*.

Amplifiers designed to attach to a transmission line at input and/or output, especially RF amplifiers, do not fit into this classification approach. Rather than dealing with voltage or current individually, they ideally couple with an input and/or output impedance matched to the transmission line impedance, that is, match *ratios* of voltage to current. Many real RF amplifiers come close to this ideal. Although, for a given appropriate source and load impedance, RF amplifiers can be characterized as amplifying voltage or current, they fundamentally are amplifying power.

#### Common terminal

One set of classifications for amplifiers is based on which device terminal is common to both the input and the output circuit. In the case of bipolar junction transistors, the three classes are common emitter, common base, and common collector. For field-effect transistors, the corresponding configurations are common source, common gate, and common drain; for triode vacuum devices, common cathode, common grid, and common plate.

#### Unilateral or bilateral

When an amplifier has an output that exhibits no feedback to its input side, it is called **unilateral**. One consequence is the amplifier has an input impedance that is independent of the load attached to the amplifier, and an output impedance that is independent of the signal source driving the amplifier.

The opposite case is the **bilateral** amplifier, where feedback connects the output to the input side of the amplifier. Such feedback often is deliberate, for example negative feedback often is used to tailor amplifier behavior. However, at least as often, feedback is both undesirable and unavoidable; introduced, for example, by parasitic elements like inherent, undesirable capacitances in transistors that couple input to output. In any case, a bilateral amplifier has an input impedance that depends upon the load attached to the amplifier, and an output impedance that depends on the source driving the amplifier.

Linear unilateral and bilateral amplifiers can be represented by two-port networks. Most amplifiers are bilateral to some degree, however they may often be modeled as unilateral under certain operating conditions to simplify the analysis (see the common base article for an example).

#### Inverting or non-inverting

Another way to classify amps is the phase relationship of the input signal to the output signal. An **inverting** amplifier produces an output 180 degrees out of phase with the input signal (that is, an inversion or mirror image of the input as seen on an oscilloscope). A **non-inverting** amplifier maintains the phase of the input signal waveforms. An **emitter follower** is a type of non-inverting amplifier, indicating that the signal at the emitter of a transistor is following (that is, matching with unity gain but perhaps an offset) the input signal.

This description can apply to a single stage of an amplifier, or to a complete amplifier system.

#### Function

Other amplifiers may be classified by their function or output characteristics. These functional descriptions usually apply to complete amplifier systems or sub-systems and rarely to individual stages.

- A **servo amplifier** indicates an integrated feedback loop to actively control the output at some desired level. A **DC servo** indicates use at frequencies down to DC levels, where the rapid fluctuations of an audio or RF signal do not occur. These are often used in mechanical actuators, or devices such as DC motors that must maintain a constant speed or torque. An **AC servo** amp can do this for some ac motors.
- A **linear** amplifier responds to different frequency components independently, and does not generate harmonic distortion or intermodulation distortion. A **nonlinear** amplifier does generate distortion.
- A **wideband** amplifier has a precise amplification factor over a wide range of frequencies, and is often used to boost signals for relay in communications systems. A **narrowband** amp is made to amplify only a specific narrow range of frequencies, to the exclusion of other frequencies.
- An **RF** amplifier refers to an amplifier designed for use in the radio frequency range of the electromagnetic spectrum, and is often used to increase the sensitivity of a receiver or the output power of a transmitter.
- An **audio amplifier** is designed for use in reproducing audio frequencies. This category subdivides into small signal amplification, and power amps which are optimised for driving speakers, sometimes with multiple amps grouped together as separate or bridgeable channels to accommodate different audio reproduction requirements.
- A special type of amplifier is widely used in instruments and for signal processing, among many other varied uses. These are known as **operational amplifiers**, (or **op-amps**). This is because this type of amplifier is used in circuits that perform mathematical algorithmic functions, or "operations" on input signals to obtain specific types of output signals. A typical op-amp has differential inputs (one "inverting", one, "non-inverting" relative to the output) and one output. An idealised op-amp has the following characteristics:
 - Infinite input impedance (so as to not load circuitry it is sampling as a control input)
 - Zero output impedance
 - Infinite gain
 - Zero propagation delay

The performance of an op-amp with these characteristics would be entirely defined by the (usually passive) components forming a negative feedback loop around it, that is, *the amplifier itself has no effect on the output*.

Today, op-amps are usually provided as integrated circuits, rather than constructed from discrete components. All real-world op-amps fall short of the idealised specification above – but some modern components have remarkable performance and come close in some respects.

Most amplifiers and many other kinds of analog electronics are built using op-amps. They are so useful that we've dedicated an entire section to them, Practical Electronics/Operating amplifiers.

#### Interstage coupling method

Amplifiers are sometimes classified by the coupling method of the signal at the input, output, or between stages. Different types of these include:

- **R-C coupled amplifier, using a network of resistors and capacitors.** By design these amplifiers cannot amplify DC signals as the capacitors block the DC component of the input signal. RC-coupled amplifiers were used very often in circuits with vacuum tubes or discrete transistors. In the days of the integrated circuit a few more transistors on a chip are much cheaper and smaller than a capacitor.
- **L-C coupled amplifier, using a network of inductors and capacitors.** This kind of amplifier is most often used in selective radio-frequency circuits.
- **Transformer coupled amplifier, using a transformer to match impedances or to decouple parts of the circuits.** Quite often LC-coupled and transformer-coupled amplifiers cannot be distinguished as a transformer is some kind of inductor.
- Direct coupled amplifier, using no impedance and bias matching components. This class of amplifier was very uncommon in the vacuum tube days when the anode (output) voltage was at several 100 V plus and the grid (input) voltage at a few volts minus. So they were only used if the gain was specified down to DC, e.g. in an oscilloscope. In the context of modern electronics developers are encouraged to use directly coupled amplifiers whenever possible.

#### Frequency range

Depending on the frequency range and other properties amplifiers are designed according to different principles.

- Frequency ranges down to DC are only used when this property is needed. DC amplification leads to specific complications that are avoided if possible.
- Depending on the frequency range specified different design principles must be used. Up to the MHz range only "discrete" properties need be considered, e.g. a terminal has an input impedance.
- As soon as any connection within the circuit gets longer than perhaps 1% of the wavelength of the highest specified frequency (e.g. at 100 MHz the wavelength is 3 m, so the critical connection length is maybe 3 cm) design properties radically change. For example a specified length and width of a PCB trace can be used as a selective or impedance-matching entity.
- Above a few 100 MHz it gets difficult to use discrete elements, especially inductors. In most cases PCB traces of very closely defined shapes are used instead.

#### Type of load

- Untuned - (a) audio (b) video
- Tuned (RF amps) - used for amplifying a single radio frequency or band of frequencies

### Implementation

Amplifiers are implemented using **active elements** of different kinds:

- The first active elements were relays. They were used in trans-continental telegraph lines in the 1800s; a weak current was used to switch the voltage of a battery to the outgoing line.
- For transmitting audio, carbon microphones were used as the first ever active element.
- Up to the early 1970s, many amplifiers used vacuum tubes ("valves" in the UK). Today tubes are only generally used for very high-power, high-frequency amplifiers, microwave ovens, CRT TVs, and for specialist audio applications, in which field they have achieved a degree of popularity. Many broadcast transmitters still use vacuum tubes. Valves' imperviousness to electromagnetic pulse damage may have led to their retention in certain defence contexts.
- In the 1950s the transistor began a gradual take over. These days, discrete transistors are still used in many electronics, but are increasingly replaced by ICs
- Beginning in the 1970s more and more transistors were connected on a single chip therefore creating the integrated circuit. a large proportion of the amplifiers commercially available today are based on integrated circuits.

For exotic purposes several other active elements have been used. For example in the early days of the communication satellite parametric amplifiers were used. The core circuit was a diode whose capacity was changed by an RF signal created locally. Under certain conditions this RF signal provided energy that was modulated by the extremely weak satellite signal received at the earth station. The operating principle of a parametric amplifier is somewhat similar to the principle by which children keep their swings in motion: As long as the swing moves you only need to change a "parameter" of the swinging entity, e.g. you must move your center of gravity up and down. In our case the capacity of the diode is changed periodically.

### Power amplifier classes

##### Angle of flow or conduction angle

Power amplifier circuits (output stages) are classified as A, B, AB and C for analog designs, and class D and E for switching designs based upon the conduction angle or 'angle of flow' Θ of the input signal through the amplifying device, that is, the portion of the input signal cycle during which the amplifying device conducts. The image of the conduction angle is derived from amplifying a sinusoidal signal. (If the device is always on, Θ = 360°.) The angle of flow is closely related to the amplifier power efficiency. The various classes are introduced below, followed by more detailed discussion under individual headings later on.

Class A
100% of the input signal is used (conduction angle Θ = 360° or 2π, i.e. the active element works in its linear range all of the time). Where efficiency is not a consideration, most small signal linear amplifiers are designed as Class A, which means that the output devices are always in the conduction region. Class A amplifiers are typically more linear and less complex than other types, but are very inefficient. This type of amplifier is most commonly used in small-signal stages or for low-power applications (such as driving headphones).

Class B
50% of the input signal is used by each output transistor (Θ = 180° or π, i.e. the active element works in its linear range half of the time and is more or less turned off for the other half). In most Class B, there are two output devices (or sets of output devices), each of which conducts alternately (push–pull) for exactly 180 deg (or half cycle) of the input signal; selective RF amplifiers can also be implemented using a single active element.

These amplifiers are subject to *crossover distortion* if the handoff from one active element to the other is not perfect, as when two complementary transistors (i.e. one PNP, one NPN) are connected as two emitter followers with their base and emitter terminals in common, requiring the base voltage to slew across the region where both devices are turned off.

Class AB
Here the two active elements conduct more than half of the time as a means to reduce the cross-over distortions of Class B amplifiers. In the example of the complementary emitter followers a bias network allows for more or less quiescent current thus providing an operating point somewhere between Class A and Class B. Sometimes a figure is added, e.g. AB₁ or AB₂, with higher figures implying a higher quiescent current and therefore more of the properties of Class A.

Class D

These use switching to achieve a very high power efficiency (more than 90% in modern designs). By operating each output device either fully on or off, losses are minimized. The analog output is created by pulse-width modulation, i.e. the active element is switched on for shorter or longer intervals instead of modifying its resistance. There are more complicated switching schemes like sigma-delta modulation, to improve some performance aspects like lower distortions or better efficiency.

Other classes
There are several other amplifier classes, although they are mainly variations of the previous classes. For example, Class H and Class G amplifiers are marked by variation of the supply rails (in discrete steps or in a continuous fashion, respectively) following the input signal. Wasted heat on the output devices can be reduced as excess voltage is reduced or kept to a minimum. The amplifier that is fed with these rails itself can be of any class. These kinds of amplifiers are more complex, and are mainly used for specialized applications, such as very high-power units. Also, Class E and Class F amplifiers are commonly described in literature for radio frequencies applications where efficiency of the traditional classes deviate substantially from their ideal values. These classes use harmonic tuning of their output networks to achieve higher efficiency and can be considered a subset of Class C due to their conduction angle characteristics.

More detail on the various classes is provided below.

#### Class A

Class A amplifying devices operate over the whole of the input cycle. The output signal is normally a scaled-up replica of the input. Class A amplifiers are the usual means of implementing small-signal amplifiers. They are not efficient; **a theoretical maximum of 50% is obtainable with inductive output coupling and only 25% with capacitive coupling**.

In a Class A circuit, the amplifying element is biased so the device is always conducting to some extent, and is operated over the most linear portion of its characteristic curve (known as its transfer characteristic or transconductance curve). Because the device is always conducting, even if there is no input at all, power is drawn from the power supply. Quiescent power consumption is high, hence its inefficiency.

Class A Amplifier

If high output powers are needed from a Class A circuit, the power waste (and the accompanying heat) is substantial. For every watt delivered to the load, the amplifier itself, at best, dissipates another watt. For large powers this means large and expensive power supplies and heat sinking. Class A designs have largely been superseded for audio power amplifiers, though some audiophiles believe that Class A gives the best sound quality, due to it being operated in as linear a manner as possible which provides a small market for expensive high fidelity Class A amps. In addition, some aficionados prefer thermionic valve (or "tube") designs instead of transistors, due to claimed better sound quality.

Class A amplifiers are often used in output stages of op-amps; they are sometimes used as medium-power, low-efficiency, and high-cost audio amplifiers. The average power consumption is unrelated to the output power when amplifying ac. At idle (no input), the power consumption is essentially the same as at high output volume. The result is low efficiency and high heat dissipation.

#### Class B and AB

Class B amplifiers use 2 active elements that each amplify one half of the input wave cycle. They create a little distortion as one device hands over to the other, but their efficiency is much better than Class A. **Class B has a maximum theoretical efficiency of 78.5% (i.e., π/4).** This is because there is zero quiescent current in the amplifying elements.

A single Class B element is rarely found in practice, though it can be used in RF power amplifier where the distortion levels are less important. However Class C is more commonly used for this.

A practical circuit using Class B elements is the complementary pair or "push–pull" arrangement. Here, complementary (or quasi-complementary) devices are used to each amplify the opposite halves of the input signal, which is then recombined at the output. This arrangement gives excellent efficiency, but can suffer from the drawback that there is a small mismatch at the "joins" between the two halves of the signal. This is called crossover distortion. An improvement is to bias the devices so they are not completely off when they're not in use. This approach is called Class AB operation.

In Class AB operation, each device operates the same way as in Class B over half the waveform, but also conducts a small amount on the other half. As a result, the region where both devices simultaneously are nearly off (the "dead zone") is reduced. The result is that when the waveforms from the two devices are combined, the crossover is greatly minimised or eliminated.

Class AB sacrifices some efficiency over class B in favor of linearity, so will always be less efficient (below 78.5%). It is typically much more efficient than class A.

Class B push–pull amplifier

Class B or AB push–pull circuits are the most common design type found in audio power amplifiers. Class AB is widely considered a good compromise for audio amplifiers, with most of the efficiency of Class B, but little of its distortion. Crossover distortion can be reduced further by negative feedback. Class B and AB amplifiers are sometimes used for RF linear amplifiers as well. Class B amplifiers are also favored in battery-operated devices, such as transistor radios.

#### Digital Class B

Class B amplifiers are in widespread use in logic ICs. They are much more efficient than class A, and are mostly only intended to provide logic 1 or 0 output. Some also provide high impedance output at times.

#### Class C

Class C amplifiers conduct less than 50% of the input signal, typically only conducting during input peaks. Distortion is high, but high efficiencies (up to 90%) are possible. Some applications (for example, megaphones) can tolerate the distortion. A much more common application for Class C amplifiers is in RF transmitters, where the distortion can be vastly reduced by using tuned loads on the amplifier stage. The input signal is used to switch the amplifying device on during peaks, and the complete waveform is recreated by tuned circuit.

The Class C amp has two modes of operation: tuned, and untuned. The diagram below shows a waveform from a simple class C circuit without the tuned load. This is called untuned operation, and the analysis of the waveforms shows the massive distortion that appears in the signal. When the proper load (e.g., a pure LC filter) is used, two things happen. The first is that the output's bias level is "clamped", so that the output variation is centered at one-half of the supply voltage. This is why tuned operation is sometimes called a "clamper". This action of elevating bias level allows the waveform to be restored to its proper shape, allowing a complete waveform to be re-established despite having only a one-polarity supply. This is directly related to the second phenomenon: the waveform on the center frequency becomes much less distorted. The distortion that is present is dependent upon the bandwidth of the tuned load, with the center frequency seeing very little distortion, but greater attenuation the farther from the tuned frequency that the signal gets.

The tuned circuit will only resonate at particular frequencies, and so the unwanted frequencies are dramatically suppressed, and the wanted full signal (sine wave) will be extracted by the tuned load (e.g., a high-quality bell will ring at a particular frequency when it is hit periodically with a hammer). Provided the transmitter is not required to operate over a very wide band of frequencies, this arrangement works extremely well. Other residual harmonics can be removed using a filter.

Class C Amplifier

#### Class D

Class D amplifiers are much more efficient than Class AB power amplifiers. So Class D amplifiers use smaller lighter transformers and heatsinks. Power devices in a Class D amplifier are operated in on/off mode. Output stages such as those used in pulse generators are examples of class D amplifiers. The term usually applies to devices intended to reproduce signals with a bandwidth well below the switching frequency.

These amplifiers use pulse width modulation, pulse density modulation (sometimes referred to as pulse frequency modulation) or more advanced form of modulation such as Delta-sigma modulation (for example, in the Analog Devices AD1990 Class-D audio power amplifier).

The input signal is converted to a sequence of pulses whose averaged value is directly proportional to the instantaneous amplitude of the signal. The frequency of the pulses is typically ten or more times the highest frequency of interest in the input signal. The output of such an amplifier contains unwanted spectral components (that is, the pulse frequency and its harmonics) which must be removed by a passive filter. The resulting filtered signal is then an amplified replica of the input.

The main advantage of a class D amplifier is efficiency. Because the output pulses have a fixed amplitude, the switching elements (usually MOSFETs, but valves and bipolar transistors were once used) are switched either on or off, rather than operated in linear mode. This means that very little power is dissipated by the transistors, except during the very short interval between the on and off states. The wasted power is low because the instantaneous power dissipated in the transistor is the product of voltage and current, and one or the other is almost always close to zero.

Class D amplifiers can be controlled by either analog or digital circuits. The digital control introduces additional distortion called *quantization error* caused by its conversion of the input signal to a digital value.

Class D amplifiers have been widely used to control motors, and almost exclusively for small DC motors, but they are now also used as audio amplifiers, with circuitry to convert analogue to a much higher frequency pulse width modulated signal. The relative difficulty of achieving good audio quality means that nearly all are used in applications where quality is not a factor, such as laptops, modestly-priced bookshelf audio systems and "DVD-receivers" in mid-price home theater systems.

High quality Class D audio amplifiers are now starting to appear in the market:

- Tripath have called their revised Class D designs Class T.
- Bang and Olufsen's ICEPower Class D system has been used in the Alpine PDX range and some Pioneer's PRS range and for other manufacturers' equipment.

These revised designs have been said to rival good traditional AB amplifiers in terms of quality.

Before these higher quality designs existed an earlier use of Class D amplifiers and prolific area of application was high-powered, subwoofer amplifiers in cars. Because subwoofers are generally limited to a bandwidth of no higher than 150Hz, the switching speed for the amplifier does not have to be as high as for a full range amplifier. The drawback with Class D designs being used to power subwoofers is that their output filters (typically inductors that convert the pulse width signal back into an analogue waveform) lower the damping factor of the amplifier.

This means that the amplifier cannot prevent the subwoofer's reactive nature from lessening the impact of low bass sounds (as explained in the feedback part of the Class AB section).
Class D amplifiers for driving subwoofers are relatively inexpensive, in comparison to Class AB amplifiers. A 1000 watt Class D subwoofer amplifier that can operate at about 80% to 95% efficiency costs about $250 USD, much less than a Class AB amplifier of this power, which would cost several thousand dollars.

The letter *D* used to designate this amplifier class is simply the next letter after *C*, and does not stand for *digital*. Class D and Class E amplifiers are sometimes mistakenly described as "digital" because the output waveform superficially resembles a pulse-train of digital symbols, but a Class D amplifier merely converts an input waveform into a continuously pulse-width modulated (square wave) analog signal. (A digital waveform would be pulse-code modulated.)

#### Special classes

##### Class E

The class E/F amplifier is a highly efficient switching power amplifier, typically used at such high frequencies that the switching time becomes comparable to the duty time. As said in the class-D amplifier the transistor is connected via a serial-LC-circuit to the load,
and connected via a large L (inductance) to the supply voltage. The supply voltage is connected to ground via a large capacitor to prevent any RF-signals leaking into the supply. The class-E amplifier adds a *C* between the transistor and ground and uses a defined *L₁* to connect to the supply voltage.

Class E Amplifier

The following description ignores DC, which can be added afterwards easily.
The above mentioned *C* (*C1* in the figure) and L are in effect a parallel LC-circuit to ground. When the transistor is on, it pushes through the serial LC-circuit into the load and some current begins to flow to the parallel LC-circuit to ground. Then the serial LC-circuit swings back and compensates the current into the parallel LC-circuit. At this point the current through the transistor is zero and it is switched off. Both LC-circuits are now filled with energy in the C and the L₀. The whole circuit performs a damped oscillation. The damping by the load has been adjusted so that some time later the energy from the Ls is gone into the load, but the energy in both C₀ peaks at the original value, to in turn restore the original voltage, so that the voltage across the transistor is zero again and it can be switched on.

With load, frequency, and duty cycle (0.5) as given parameters and the constraint that the voltage is not only restored, but peaks at the original voltage, the four parameters (L, L₀, C and C₀) are determined. The class F-amplifier takes the finite on resistance into account and tries to make the current touch the bottom at zero. This means the voltage and the current at the transistor are symmetric with respect to time. The Fourier Transform allows an elegant formulation to generate the complicated LC-networks. It says that the first harmonic is passed into the load, all even harmonics are shorted and all higher odd harmonics are open.

##### Class F and the even harmonics

In push–pull amplifiers and in CMOS the even harmonics of both transistors just cancel. Experiment tells that a square wave can be generated by those amplifiers and math tells that square waves do consist of odd harmonics only. In a class D amplifier the output filter blocks all harmonics, that means the harmonics see an open load. So even small currents in the harmonics suffice to generate a voltage square wave. The current is in phase with the voltage applied to filter, but the voltage across the transistors is out of phase. Therefore there is a minimal overlap between current through the transistors and voltage across the transistors. The sharper the edges the lower the overlap.

While class D sees the transistors and the load as two separate modules the class F admits imperfections like the parasitics of the transistor and tries to optimise the global system to have a high impedance at the harmonics. Of course there has to be a finite voltage across the transistor to push the current across the on state resistance. Because the combined current through both transistors is mostly in the first harmonic it looks like a sine. That means that in the middle of the square the maximum of current has to flow, so it may make sense to have a dip in the square or in other words to allow some over swing of the voltage square wave. A class F load network by definition has to transmit below a cut off frequency and to reflect above.

Any frequency lying below the cut off and having its second harmonic above the cut off can be amplified, that is an octave bandwidth. On the other hand an LC series circuit with a large L and a tunable C may be simpler to implement. By reducing the duty cycle below 0.5, the output amplitude can be modulated. The voltage square waveform will degrade, but any overheating is compensated by the lower overall power flowing. Any load mismatch behind the filter can only act on the first harmonic current waveform, clearly only a purely resistive load makes sense, then the lower the resistance the higher the current.

Class F can be driven by sine or by a square wave, for a sine the input can be tuned by an L to increase gain. If class F is implemented with a single transistor the filter is complicated to short the even harmonics. All previous designs use sharp edges to minimise the overlap.
Class E uses a significant amount of second harmonic voltage. The second harmonic can be used to reduce the overlap with edges with finite sharpness. For this to work energy on the second harmonic has to flow from the load into the transistor, and no source for this is visible in the circuit diagram. In reality the impedance is mostly reactive and the only reason for it is that class E is a class F amplifier with a very simplified load network and thus has to deal with imperfections.

In many amateur simulations of class E amplifiers sharp current edges are assumed nullifying the very motivation for class E and measurements near the transit frequency of the transistors show very symmetric curves, which look much similar to class F simulations.
The class E amplifier was invented in 1972 by Nathan O. Sokal and Alan D. Sokal, and details were first published in 1975. Some earlier reports on this operating class have been published in Russian.

##### Class G and H

There is a variety of amplifier designs that couple a class AB output stage with other more efficient techniques to achieve a higher efficiency with low distortion. These designs are common in large audio amplifiers since the heatsinks and power transformers would be prohibitively large (and costly) without the increase in efficiency. The terms "class G" and "class H" are used interchangeably to refer to different designs, varying in definition from one manufacturer or paper to another.

Class G amplifiers (which use "rail switching" to decrease power consumption and increase efficiency) are a more efficient than class AB amplifiers. The class G amplifier has several power rails at different voltages, and switches between rails as the signal output approaches each. Thus the amplifier increases efficiency by reducing the wasted power at the output transistors.

A Class H amplifier takes the idea of Class G one step further creating an infinitely variable supply rail. This is done by modulating the supply rails so that the rails are only a few volts larger than the output signal at any given time. The output stage operates at its maximum efficiency all the time. Switched mode power supplies can be used to create the tracking rails. Significant efficiency gains can be achieved but with the drawback of more complicated supply design and reduced THD performance.

_(Later sections of this page were left out of this offline copy for length: remaining text.)_
