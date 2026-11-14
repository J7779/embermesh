---
id: electronics/wp2-high-pass-filter
title: High-pass filter
category: electronics
tags: [high-pass filter, hpf, low-cut filter, bass-cut filter, high-pass, electronics]
summary: A high-pass filter (HPF) is an electronic filter that passes signals with a frequency higher than a certain cutoff frequency and attenuates signals with frequencies lower than the cutoff frequency.
source: https://en.wikipedia.org/w/index.php?title=High-pass_filter&oldid=1369018426
source_title: Wikipedia: High-pass filter
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "High-pass filter", Wikipedia, The Free Encyclopedia (revision 1369018426), https://en.wikipedia.org/w/index.php?title=High-pass_filter&oldid=1369018426 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# High-pass filter

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **high-pass filter** (**HPF**) is an electronic filter that passes signals with a frequency higher than a certain cutoff frequency and attenuates signals with frequencies lower than the cutoff frequency. The amount of attenuation for each frequency depends on the filter design. A high-pass filter is usually modeled as a linear time-invariant system. It is sometimes called a **low-cut filter** or **bass-cut filter** in the context of audio engineering. High-pass filters have many uses, such as blocking DC from circuitry sensitive to non-zero average voltages or radio frequency devices. They can also be used in conjunction with a low-pass filter to produce a band-pass filter.

In the optical domain filters are often characterised by wavelength rather than frequency. **High-pass** and **low-pass** have the opposite meanings, with a "high-pass" filter (more commonly "short-pass") passing only *shorter* wavelengths (higher frequencies), and vice versa for "low-pass" (more commonly "long-pass").

## Description

In electronics, a filter is a two-port electronic circuit which removes frequency components from a signal (time-varying voltage or current) applied to its input port. A high-pass filter attenuates frequency components below a certain frequency, called its cutoff frequency, allowing higher frequency components to pass through. This contrasts with a low-pass filter, which attenuates frequencies higher than a certain frequency, and a bandpass filter, which allows a certain band of frequencies through and attenuates frequencies both higher and lower than the band.

In optics a high pass filter is a transparent or translucent window of colored material that allows light longer than a certain wavelength to pass through and attenuates light of shorter wavelengths. Since light is often measured not by frequency but by wavelength, which is inversely related to frequency, a high pass optical filter, which attenuates light frequencies below a cutoff frequency, is often called a short-pass filter; it attenuates longer wavelengths.

## Continuous-time circuits

### First-order passive

A resistor and either a capacitor or an inductor can be configured as a first-order high-pass filter. The simple first-order capacitive high-pass filter shown in Figure 1 is implemented by placing an input voltage across the series combination of a capacitor and a resistor and using the voltage across the resistor as an output. The transfer function of this linear time-invariant system is:

fracV_rm out(s)V_rm in(s)=(sRC)/(1+sRC).

The product of the resistance and capacitance (*R*×*C*) is the time constant (τ); it is inversely proportional to the cutoff frequency *f*_*c*, that is,

f_c = (1)/(2 π τ) = (1)/(2 π R C),

where *f*_*c* is in hertz, *τ* is in seconds, *R* is in ohms, and *C* is in farads. At the cutoff frequency, the filter's frequency response reaches -3dB referenced to the gain at an infinite frequency.

### First-order active

Figure 2 shows an active electronic implementation of a first-order high-pass filter using an operational amplifier. The transfer function of this linear time-invariant system is:

fracV_rm out(s)V_rm in(s)=(-sR_2C)/(1+sR_1C).

In this case, the filter has a passband gain of −*R*₂/*R*₁ and has a cutoff frequency of

f_c = (1)/(2 π τ) = (1)/(2 π R_1 C),

Because this filter is active, it may have non-unity passband gain. That is, high-frequency signals are inverted and amplified by *R*₂/*R*₁.

All of these first-order high-pass filters are called differentiators, because they perform differentiation for signals whose frequency band is well below the filter's cutoff frequency.

### Higher orders

Filters of higher order have steeper slope in the stopband, such that the slope of n^th-order filters equals 20n dB per decade. Higher order filters can be achieved simply by cascading these first order filters. While impedance matching and loading must be taken into account when chaining passive filters, active filters can be easily chained because the signal is restored by the output of the op amp at each stage. Various filter topologies and network synthesis filters for higher orders exist, which ease design.

## Discrete-time realization

Discrete-time high-pass filters can also be designed. Discrete-time filter design is beyond the scope of this article; however, a simple example comes from the conversion of the continuous-time high-pass filter above to a discrete-time realization. That is, the continuous-time behavior can be discretized.

From the circuit in Figure 1 above, according to Kirchhoff's Laws and the definition of capacitance:

V_out(t) = I(t) R (V); Q_c(t) = C ( V_in(t) - V_out(t) ) (Q); I(t) = (d Q_c)/(d t) (I)

where Q_c(t) is the charge stored in the capacitor at time t. Substituting Equation (Q) into Equation (I) and then Equation (I) into Equation (V) gives:

V_out(t) = overbraceC ( fracd V_indt - fracd V_outdt )^I(t) R = R C ( frac d V_indt - fracd V_outdt )

This equation can be discretized. For simplicity, assume that samples of the input and output are taken at evenly spaced points in time separated by Δ_T time. Let the samples of V_in be represented by the sequence (x_1, x_2, …, x_n), and let V_out be represented by the sequence (y_1, y_2, …, y_n) which correspond to the same points in time. Making these substitutions:

y_i = R C ( fracx_i - x_i-1Δ_T - fracy_i - y_i-1Δ_T )

And rearranging terms gives the recurrence relation

y_i = overbrace(RC)/(RC + Δ_T) y_i-1^Decaying contribution from prior inputs + overbrace(RC)/(RC + Δ_T) ( x_i - x_i-1 )^Contribution from change in input

That is, this discrete-time implementation of a simple continuous-time RC high-pass filter is

y_i = α y_i-1 + α (x_i - x_i-1) where α triangleq (RC)/(RC + Δ_T)

By definition, 0 ≤ α ≤ 1. The expression for parameter α yields the equivalent time constant RC in terms of the sampling period Δ_T and α:

RC = Δ_T ( (α)/(1 - α) ).
Recalling that
f_c=(1)/(2π RC) so RC=(1)/(2π f_c)
then α and f_c are related by:
α = (1)/(2π Δ_T f_c + 1)
and
f_c=(1-α)/(2π α Δ_T).

If α = 0.5, then the RC time constant equal to the sampling period. If α ll 0.5, then RC is significantly smaller than the sampling interval, and RC ≈ α Δ_T.

### Algorithmic implementation

The filter recurrence relation provides a way to determine the output samples in terms of the input samples and the preceding output. The following pseudocode algorithm will simulate the effect of a high-pass filter on a series of digital samples, assuming equally spaced samples:

 // Return RC high-pass filter output samples, given input samples,
 // time interval *dt*, and time constant *RC*
 **function** highpass(*real[1..n]* x, *real* dt, *real* RC)
 **var** *real[1..n]* y
 **var** *real* α := RC / (RC + dt)
 y[1] := x[1]
 **for** i **from** 2 **to** n
 y[i] := α × y[i−1] + α × (x[i] − x[i−1])
 **return** y

The loop which calculates each of the n outputs can be refactored into the equivalent:

 **for** i **from** 2 **to** n
 y[i] := α × (y[i−1] + x[i] − x[i−1])

However, the earlier form shows how the parameter α changes the impact of the prior output `y[i-1]` and current *change* in input `(x[i] - x[i-1])`. In particular,

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Discrete-time realization, Applications.)_
