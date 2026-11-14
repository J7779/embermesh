---
id: electronics/wp2-test-probe
title: Test probe
category: electronics
tags: [test probe, test prods, oscilloscope probes, test lead, electronics]
summary: A test probe is a physical device used to connect electronic test equipment to a device under test (DUT).
source: https://en.wikipedia.org/w/index.php?title=Test_probe&oldid=1320537903
source_title: Wikipedia: Test probe
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Test probe", Wikipedia, The Free Encyclopedia (revision 1320537903), https://en.wikipedia.org/w/index.php?title=Test_probe&oldid=1320537903 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Test probe

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **test probe** is a physical device used to connect electronic test equipment to a device under test (DUT). Test probes range from very simple, robust devices to complex probes that are sophisticated, expensive, and fragile. Specific types include **test prods**, **oscilloscope probes** and current probes. A test probe is often supplied as a **test lead**, which includes the probe, cable and terminating connector.

## Voltage

Voltage probes are used to measure voltages present on the DUT. To achieve high accuracy, the test instrument and its probe must not significantly affect the voltage being measured. This is accomplished by ensuring that the combination of instrument and probe exhibit a sufficiently high impedance that will not load the DUT. For AC measurements, the reactive component of impedance may be more important than the resistive.

### Simple test leads

A typical voltmeter probe consists of a single wire *test lead* that has on one end a connector that fits the voltmeter and on the other end a rigid, tubular plastic section that comprises both a handle and probe body. The handle allows a person to hold and guide the probe without influencing the measurement (by becoming part of the electric circuit) or being exposed to dangerous voltages that might cause electric shock. Within the probe body, the wire is connected to a rigid, pointed metal tip that contacts the DUT. Some probes allow an alligator clip to be attached to the tip, thus enabling the probe to be attached to the DUT so that it need not be held in place.

Test leads are usually made with finely stranded wire to keep them flexible, of wire gauges sufficient to conduct a few amperes of electric current. The insulation is chosen to be both flexible and have a breakdown voltage higher than the voltmeter's maximum input voltage. The many fine strands and the thick insulation make the wire thicker than ordinary hookup wire.

Two probes are used together to measure voltage, current, and two-terminal components such as resistors and capacitors. When making DC measurements it is necessary to know which probe is positive and which is negative, so by convention the probes are colored red for positive and black for negative. Depending upon the accuracy required, they can be used with signal frequencies ranging from DC to a few kilohertz.

When sensitive measurements must be made (e.g., very low voltages, or very low or very high resistances) shields, guards, and techniques such as four-terminal Kelvin sensing (using separate leads to carry the measuring current and to sense the voltage) are used.

#### Tweezer probes

Tweezer probes are a pair of simple probes fixed to a tweezer mechanism, operated with one hand, for measuring voltages or other electronic circuit parameters between closely spaced pins.

#### Pogo pins

Spring probes (a.k.a. "pogo pins") are spring-loaded pins used in electrical test fixtures to contact test points, component leads, and other conductive features of the DUT (Device Under Test). These probes are usually press-fit into probe sockets, to allow their easy replacement on test fixtures which may remain in service for decades, testing many thousands of DUTs in automatic test equipment.

### Oscilloscope probes

Oscilloscopes display the instantaneous waveform of varying electrical quantities, unlike other instruments which give numerical values of relatively stable quantities.

Scope probes fall into two main categories: passive and active.
Passive scope probes contain no active electronic parts, such as transistors, so they require no external power.

Because of the high frequencies often involved, oscilloscopes do not normally use simple wires ("flying leads") to connect to the DUT. Flying leads are likely to pick up interference, so they are not suitable for low-level signals. Furthermore, the inductance of flying leads make them unsuitable for high frequency signals. Instead, a specific *scope probe* is used, which uses a coaxial cable to transmit the signal from the tip of the probe to the oscilloscope. This cable has two main benefits: it protects the signal from external electromagnetic interference, improving accuracy for low-level signals; and it has a lower inductance than flying leads, making the probe more accurate for high-frequency signals.

Although coaxial cable has lower inductance than flying leads, it has higher capacitance: a typical 50 ohm cable has about 90 pF per meter. Consequently, a one-meter high-impedance direct (1×) coaxial probe may load the circuit with a capacitance of about 110 pF and a resistance of 1 megohm.

Oscilloscope probes are characterised by their frequency limit, where the amplitude response has fallen by 3 dB, and/or by their rise time t_r. These are related as (in round figures)

f_3 dB = 0.35/t_r

Thus a 50 MHz probe has a rise time of 7 ns. The response of the combination of an oscilloscope and a probe is given by

t_r (combined) = √(t_r(probe)^2 + t_r(scope)^2)

For example, a 50 MHz probe feeding a 50 MHz scope will give a 35 MHz system. It is therefore advantageous to use a probe with a higher frequency limit to minimize the effect on the overall system response.

#### Passive probes

To minimize loading, attenuator probes (e.g., 10× probes) are used. A typical probe uses a 9 megohm series resistor shunted by a low-value capacitor to make an RC compensated divider with the cable capacitance and scope input. The RC time constants are adjusted to match. For example, the 9 megohm series resistor is shunted by a 12.2 pF capacitor for a time constant of 110 microseconds. The cable capacitance of 90 pF in parallel with the scope input of 20 pF (total capacitance 110 pF) and 1 megohm also gives a time constant of 110 microseconds. In practice, there will be an adjustment so the operator can precisely match the low frequency time constant (called compensating the probe). Matching the time constants makes the attenuation independent of frequency. At low frequencies (where the resistance of *R* is much less than the reactance of *C*), the circuit looks like a resistive divider; at higher frequencies (resistance much greater than reactance), the circuit looks like a capacitive divider.

The result is a frequency compensated probe for modest frequencies that presents a load of about 10 megohms shunted by 12 pF. Although such a probe is an improvement, it does not work when the time scale shrinks to several cable transit times (transit time is typically 5 ns). In that time frame, the cable looks like its characteristic impedance, and there will be reflections from the transmission line mismatch at the scope input and the probe that causes ringing. The modern scope probe uses lossy low capacitance transmission lines and sophisticated frequency shaping networks to make the 10× probe perform well at several hundred megahertz. Consequently, there are other adjustments for completing the compensation.

A directly connected test probe (so called 1× probe) puts the unwanted lead capacitance across the circuit under test. For a typical coaxial cable, loading is of the order of 100pF per meter (the length of a typical test lead).

Attenuator probes minimize capacitive loading with an attenuator, but reduce the magnitude of the signal delivered to the instrument. A 10× attenuator will reduce the capacitive load by a factor of about 10. The attenuator must have an accurate ratio over the whole range of frequencies of interest; the input impedance of the instrument becomes part of the attenuator. A DC attenuator with resistive divider is supplemented with capacitors, so that the frequency response is predictable over the range of interest.

The RC time constant matching method works as long as the transit time of the shielded cable is much less than the time scale of interest. That means that the shielded cable can be viewed as a lumped capacitor rather than an inductor. Transit time on a 1-meter cable is about 5 ns. Consequently, these probes will work to a few megahertz, but after that transmission line effects cause trouble.

At high frequencies, the probe impedance will be low.

_(This section is shortened.)_

## Demodulator probes

To measure or display the modulating waveform of a modulated high-frequency signal—for example, an amplitude-modulated radio signal—a probe fitted with a simple diode demodulator can be used. The probe will output the modulating waveform without the high-frequency carrier.

_(Later sections of the original article were left out of this offline copy for length: the rest of Voltage, Current probes, Near-field probes, Temperature probes, Logic probes.)_
