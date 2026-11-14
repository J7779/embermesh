---
id: radio/wp2-impedance-matching
title: Impedance matching
category: radio
tags: [impedance matching, radio]
summary: In electrical engineering, impedance matching is the practice of designing or adjusting the input impedance or output impedance of an electrical device for a desired value.
source: https://en.wikipedia.org/w/index.php?title=Impedance_matching&oldid=1383210113
source_title: Wikipedia: Impedance matching
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Impedance matching", Wikipedia, The Free Encyclopedia (revision 1383210113), https://en.wikipedia.org/w/index.php?title=Impedance_matching&oldid=1383210113 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Impedance matching

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electrical engineering, **impedance matching** is the practice of designing or adjusting the input impedance or output impedance of an electrical device for a desired value. Often, the desired value is selected to maximize power transfer or minimize signal reflection. For example, impedance matching is typically used to improve power transfer from a radio transmitter via the interconnecting transmission line to the antenna. Signals on a transmission line will be transmitted without reflections if the transmission line is terminated with a matching impedance.

Techniques of impedance matching include transformers, adjustable networks of lumped resistance, capacitance and inductance, or properly proportioned transmission lines. Practical impedance-matching devices will generally provide best results over a specified frequency band.

The concept of impedance matching is widespread in electrical engineering, but is relevant in other applications in which a form of energy, not necessarily electrical, is transferred between a source and a load, such as in acoustics or optics.

## Theory

Impedance characterizes the ratio of energy components in a given domain. For constant signals, this impedance can also be constant. For varying signals, it usually changes with frequency. The energy involved can be electrical, mechanical, acoustic, magnetic, electromagnetic, or thermal. The concept of electrical impedance is perhaps the most commonly known. Electrical impedance, like electrical resistance, is measured in ohms. In general, impedance (symbol: *Z*) has a complex value; this means that loads generally have a resistance component (symbol: *R*) which forms the real part and a reactance component (symbol: *X*) which forms the imaginary part. This impedance characterizes the ratio between the voltage and current components in the medium.

In simple cases (such as low-frequency or direct current power transmission), the reactance may be negligible or zero; the impedance can be considered a pure resistance, expressed as a real number. In the following summary, we will consider the general case when resistance and reactance are both significant, and the special case in which the reactance is negligible.

### Maximum power transfer matching

Complex conjugate matching is used when maximum power transfer is required, namely
Z_load = Z_source^*,
where a superscript * indicates the complex conjugate. A conjugate match is different from a reflection-less match when either the source or load has a reactive component.

If the source has a reactive component, but the load is purely resistive, then matching can be achieved by adding a reactance of the same magnitude but opposite sign to the load. This simple matching network, consisting of a single element, will usually achieve a perfect match at only a single frequency. This is because the added element will either be a capacitor or an inductor, whose impedance in both cases is frequency dependent, and will not, in general, follow the frequency dependence of the source impedance. For wide bandwidth applications, a more complex network must be designed.

#### Phase-shifting matching network

If a lossless reciprocal two-port network matches a load impedance Z_L = R_L + jX_L to a source impedance Z_S = R_S + jX_S with transmission phase shift φ (i.e., \Z_S xrightarrowφ Z_L\), then the transmission matrix of the matching network can be expressed as
A B; C D = a jb; jc d = (1)/(√(R_S R_L)) Re(Z_Se^jφ) jIm(Z_SZ_Le^jφ); jIm(e^jφ) Re(Z_Le^jφ),
where Re(Z) and Im(Z) are real and imaginary part of complex number Z. Z is the complex conjugate of Z.

If a lossless reciprocal two-port network matches a resistive load impedance Z_L = R_L to a resistive source impedance Z_S = R_S with a transmission phase shift of φ (i.e., \R_S xrightarrowφ R_L\), then the transmission matrix (ABCD matrix) of the matching network can be expressed as
a jb; jc d = √((R_S)/(R_L))cosφ jsqrtR_SR_Lsinφ; j(1)/(√(R_SR_L))sinφ √((R_L)/(R_S))cosφ.

If both source and load are equal resistive impedance (i.e., R_S = R_L = R_0), then the ABCD parameters can be written as
a jb; jc d = cosφ jR_0sinφ; j(1)/(R_0)sinφ cosφ,
which is a transmission line with characteristic impedance R_0 and electrical length θ = β l = φ.

## Power transfer

Whenever a source of power *with a fixed output impedance* such as an electric signal source, a radio transmitter or a mechanical sound (e.g., a loudspeaker) operates into a load, the maximum possible power is delivered to the load when the impedance of the load (load impedance or input impedance) is equal to the *complex conjugate* of the impedance of the source (that is, its internal impedance or output impedance). For two impedances to be complex conjugates their resistances must be equal, and their reactances must be equal in magnitude but of opposite signs. In low-frequency or DC systems (or systems with purely resistive sources and loads) the reactances are zero, or small enough to be ignored. In this case, maximum power transfer occurs when the resistance of the load is equal to the resistance of the source (see maximum power theorem for a mathematical proof).

Impedance matching is not always necessary. For example, if delivering a high voltage (to reduce signal degradation or to reduce power consumption) is more important than maximizing power transfer, then *impedance bridging* or *voltage bridging* is often used.

In older audio systems (reliant on transformers and passive filter networks, and based on the telephone system), the source and load resistances were matched at 600 ohms. One reason for this was to maximize power transfer, as there were no amplifiers available that could restore lost signal. Another reason was to ensure correct operation of the hybrid transformers used at central exchange equipment to separate outgoing from incoming speech, so these could be amplified or fed to a four-wire circuit. Most modern audio circuits, on the other hand, use active amplification and filtering and can use voltage-bridging connections for greatest accuracy. Strictly speaking, impedance matching only applies when both source and load devices are linear; however, matching may be obtained between nonlinear devices within certain operating ranges.

## Power factor correction

Power factor correction devices are intended to cancel the reactive and nonlinear characteristics of a load at the end of a power line. This causes the load seen by the power line to be purely resistive. For a given true power required by a load this minimizes the true current supplied through the power lines, and minimizes power wasted in the resistance of those power lines. For example, a maximum power point tracker is used to extract the maximum power from a solar panel and efficiently transfer it to batteries, the power grid or other loads.
The maximum power theorem applies to its "upstream" connection to the solar panel, so it emulates a load resistance equal to the solar panel source resistance. However, the maximum power theorem does not apply to its "downstream" connection. That connection is an impedance bridging connection; it emulates a high-voltage, low-resistance source to maximize efficiency.

On the power grid the overall load is usually inductive. Consequently, power factor correction is most commonly achieved with banks of capacitors. It is only necessary for correction to be achieved at one single frequency, the frequency of the supply. Complex networks are only required when a band of frequencies must be matched and this is the reason why simple capacitors are all that is usually required for power factor correction.

_(Later sections of the original article were left out of this offline copy for length: Impedance-matching devices, Transmission lines, Electrical examples, Non-electrical examples.)_
