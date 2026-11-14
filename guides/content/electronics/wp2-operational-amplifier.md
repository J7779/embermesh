---
id: electronics/wp2-operational-amplifier
title: Operational amplifier
category: electronics
tags: [operational amplifier, op amp, op-amp, opamp, voltage-feedback opamp, electronics]
summary: An operational amplifier (often op amp, op-amp, or opamp) is a DC-coupled electronic amplifier with a differential input, a (usually) single-ended output voltage, and an extremely high gain.
source: https://en.wikipedia.org/w/index.php?title=Operational_amplifier&oldid=1378476597
source_title: Wikipedia: Operational amplifier
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Operational amplifier", Wikipedia, The Free Encyclopedia (revision 1378476597), https://en.wikipedia.org/w/index.php?title=Operational_amplifier&oldid=1378476597 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Operational amplifier

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **operational amplifier** (often **op amp**, **op-amp**, or **opamp**) is a DC-coupled electronic amplifier with a differential input, a (usually) single-ended output voltage, and an extremely high gain. Its name comes from its original use of performing mathematical operations in analog computers. The **voltage-feedback opamp** (**VFOA** or **VFA**, the focus of this article) amplifies the voltage difference between its two inputs, while the less common *current-feedback op amp* (CFOA) amplifies the current between its two inputs.

By using negative feedback, the characteristics of an op amp application circuit (e.g., its gain, input and output impedance, bandwidth, and functionality) can be determined by external components and have little dependence on temperature coefficients or engineering tolerance in the op amp itself. This flexibility has made the op amp a popular building block in analog circuits.

Today, op amps are used widely in consumer, industrial, and scientific electronics. Many standard integrated circuit op amps cost only a few cents; however, some integrated or hybrid operational amplifiers with special performance specifications may cost over. Op amps may be packaged as components or used as elements of more complex integrated circuits.

The op amp is one type of differential amplifier. Other differential amplifier types include the fully differential amplifier (an op amp with a differential rather than single-ended output), the instrumentation amplifier (usually built from three op amps), the isolation amplifier (with galvanic isolation between input and output), and negative-feedback amplifier (usually built from one or more op amps and a resistive feedback network).

## Operation

The amplifier's differential inputs consist of a non-inverting input (+) with voltage *V*₊ and an inverting input (−) with voltage *V*_−; ideally the op amp amplifies only the difference in voltage between the two, which is called the *differential input voltage*. The output voltage of the op amp *V*_out is given by the equation
V_out = A_OL (V_+ - V_-),
where *A*_OL is the open-loop gain of the amplifier (the term "open-loop" refers to the absence of an external feedback loop from the output to the input).

### Open-loop amplifier

The magnitude of *A*_OL is typically very large (100,000 or more for integrated circuit op amps, corresponding to +100 dB). Thus, even small microvolts of difference between *V*₊ and *V*_− may drive the amplifier into clipping or saturation. The magnitude of *A*_OL is not well controlled by the manufacturing process, and so it is impractical to use an open-loop amplifier as a stand-alone differential amplifier.

Without negative feedback, and optionally positive feedback for regeneration, an *open-loop* op amp acts as a comparator, although comparator ICs are better suited. If the inverting input of an ideal op amp is held at ground (0 V), and the input voltage *V*_in applied to the non-inverting input is positive, the output will be maximum positive; if *V*_in is negative, the output will be maximum negative.

### Closed-loop amplifier

If predictable operation is desired, negative feedback is used by applying a portion of the output voltage to the inverting input. The *closed-loop* feedback greatly reduces the gain of the circuit. When negative feedback is used, the circuit's overall gain and response are determined primarily by the feedback network, rather than by the op-amp characteristics. If the feedback network is made of components with values small relative to the op amp's input impedance, the value of the op amp's open-loop response *A*_OL does not seriously affect the circuit's performance. In this context, high input impedance at the input terminals and low output impedance at the output terminal(s) are particularly useful features of an op amp.

The response of the op-amp circuit with its input, output, and feedback circuits to an input is characterized mathematically by a transfer function; designing an op-amp circuit to have a desired transfer function is in the realm of electrical engineering. The transfer functions are important in most applications of op amps, such as in analog computers.

In the non-inverting amplifier on the right, the presence of negative feedback via the voltage divider *R*_f, *R*_g determines the *closed-loop gain*. Equilibrium will be established when *V*_out is just sufficient to pull the inverting input to the same voltage as *V*_in. The voltage gain of the entire circuit is thus 1 + *R*_f / *R*_g. As a simple example, if and, *V*_out will be 2 V, exactly the amount required to keep *V*_− at 1 V. Because of the feedback provided by the *R*_f, *R*_g network, this is a *closed-loop* circuit.

Another way to analyze this circuit proceeds by making the following (usually valid) assumptions:

1. When an op amp operates in linear (i.e., not saturated) mode, the difference in voltage between the non-inverting (+) and inverting (−) pins is negligibly small.
1. The input impedance of the (+) and (−) pins is much larger than other resistances in the circuit.

The input signal *V*_in appears at both (+) and (−) pins per assumption 1, resulting in a current i through *R*_g equal to *V*_in / *R*_g:
i = (V_in)/(R_g).

Because Kirchhoff's current law states that the same current must leave a node as enters it, and because the impedance into the (−) pin is near infinity per assumption 2, we can assume practically all of the same current i flows through *R*_f, creating an output voltage
V_out = V_in + iR_f = V_in + ((V_in)/(R_g) R_f) = V_in + (V_inR_f)/(R_g) = V_in (1 + (R_f)/(R_g)).

By combining terms, we determine the closed-loop gain *A*_CL:
A_CL = (V_out)/(V_in) = 1 + (R_f)/(R_g).

## Characteristics

### Ideal op amps

An ideal op amp is usually considered to have the following characteristics:

- Arbitrarily high open-loop gain
- Infinite input impedance *R*_in, and thus zero input current
- Zero input offset voltage
- Unbounded output voltage range
- Unrestricted bandwidth with zero phase shift and infinite slew rate
- Zero output impedance *R*_out, and thus ability to source or sink unbounded output current
- Zero noise
- No effect of common-mode voltages, as described by common-mode rejection ratio (CMRR)
- No effect of supply variations on the output, i.e., perfect rejection of power supply variation.

These ideals can be summarized by the two *golden rules*:

1. In a negative feedback configuration, the output does whatever is necessary to make the voltage difference between the inputs zero.
1. The inputs draw zero current.

The first rule only applies in the usual case where the op amp is used in a negative feedback design, where there is a signal path of some sort feeding back from the output to the inverting input. These rules are commonly used as a good first approximation for analyzing or designing op-amp circuits.

None of these ideals can be perfectly realized. A real op amp may be modeled with non-infinite or non-zero parameters using equivalent resistors and capacitors in the op-amp model. The designer can then include these effects in the overall performance of the final circuit. Some parameters may turn out to have negligible effect on the final design, while others represent actual limitations of the final performance.

### Real op amps

Real op amps differ from the ideal model in various aspects.

_(Later sections of the original article were left out of this offline copy for length: Classification.)_
