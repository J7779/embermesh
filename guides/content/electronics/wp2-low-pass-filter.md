---
id: electronics/wp2-low-pass-filter
title: Low-pass filter
category: electronics
tags: [low-pass filter, high-cut filter, treble-cut filter, high-pass, low-pass, electronics]
summary: A low-pass filter is a filter that passes signals with a frequency lower than a selected cutoff frequency and attenuates signals with frequencies higher than the cutoff frequency.
source: https://en.wikipedia.org/w/index.php?title=Low-pass_filter&oldid=1384696058
source_title: Wikipedia: Low-pass filter
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Low-pass filter", Wikipedia, The Free Encyclopedia (revision 1384696058), https://en.wikipedia.org/w/index.php?title=Low-pass_filter&oldid=1384696058 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Low-pass filter

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **low-pass filter** is a filter that passes signals with a frequency lower than a selected cutoff frequency and attenuates signals with frequencies higher than the cutoff frequency. The exact frequency response of the filter depends on the filter design. The filter is sometimes called a **high-cut filter**, or **treble-cut filter** in audio applications. A low-pass filter is the complement of a high-pass filter.

In optics, **high-pass** and **low-pass** may have different meanings, depending on whether referring to the frequency or wavelength of light, since these variables are inversely related. High-pass frequency filters would act as low-pass wavelength filters, and vice versa. For this reason, it is a good practice to refer to wavelength filters as *short-pass* and *long-pass* to avoid confusion, which would correspond to *high-pass* and *low-pass* frequencies.

Low-pass filters exist in many different forms, including electronic circuits such as a *hiss filter* used in audio, anti-aliasing filters for conditioning signals before analog-to-digital conversion, digital filters for smoothing sets of data, acoustic barriers, blurring of images, and so on. The moving average operation used in fields such as finance is a particular kind of low-pass filter and can be analyzed with the same signal processing techniques as are used for other low-pass filters. Low-pass filters provide a smoother form of a signal, removing the short-term fluctuations and leaving the longer-term trend.

Filter designers will often use the low-pass form as a prototype filter. That is a filter with unity bandwidth and impedance. The desired filter is obtained from the prototype by scaling for the desired bandwidth and impedance and transforming into the desired bandform (that is, low-pass, high-pass, band-pass or band-stop).

## Examples

Examples of low-pass filters occur in acoustics, optics and electronics.

A stiff physical barrier tends to reflect higher sound frequencies, acting as an acoustic low-pass filter for transmitting sound. When music is playing in another room, the low notes are easily heard, while the high notes are attenuated.

An optical filter with the same function can correctly be called a low-pass filter, but conventionally is called a *longpass* filter (low frequency is long wavelength), to avoid confusion.

In an electronic low-pass RC filter for voltage signals, high frequencies in the input signal are attenuated, but the filter has little attenuation below the cutoff frequency determined by its RC time constant. For current signals, a similar circuit, using a resistor and capacitor in parallel, works in a similar manner. (See current divider discussed in more detail below.)

Electronic low-pass filters are used on inputs to subwoofers and other types of loudspeakers, to block high pitches that they cannot efficiently reproduce. Radio transmitters use low-pass filters to block harmonic emissions that might interfere with other communications. The tone knob on many electric guitars is a low-pass filter used to reduce the amount of treble in the sound. An integrator is another time constant low-pass filter.

Telephone lines fitted with DSL splitters use low-pass filters to separate DSL from POTS signals (and high-pass vice versa), which share the same pair of wires (*transmission channel*).

Low-pass filters also play a significant role in the sculpting of sound created by analogue and virtual analogue synthesisers. *See subtractive synthesis.*

A low-pass filter is used as an anti-aliasing filter before sampling and for reconstruction in digital-to-analog conversion.

## Ideal and real filters

An ideal low-pass filter completely eliminates all frequencies above the cutoff frequency while passing those below unchanged; its frequency response is a rectangular function and is a brick-wall filter. The transition region present in practical filters does not exist in an ideal filter. An ideal low-pass filter can be realized mathematically (theoretically) by multiplying a signal by the rectangular function in the frequency domain or, equivalently, convolution with its impulse response, a sinc function, in the time domain.

However, the ideal filter is impossible to realize without also having signals of infinite extent in time, and so generally needs to be approximated for real ongoing signals, because the sinc function's support region extends to all past and future times. The filter would therefore need to have infinite delay, or knowledge of the infinite future and past, to perform the convolution. It is effectively realizable for pre-recorded digital signals by assuming extensions of zero into the past and future, or, more typically, by making the signal repetitive and using Fourier analysis.

Real filters for real-time applications approximate the ideal filter by truncating and windowing the infinite impulse response to make a finite impulse response; applying that filter requires delaying the signal for a moderate period of time, allowing the computation to "see" a little bit into the future. This delay is manifested as phase shift. Greater accuracy in approximation requires a longer delay.

Truncating an ideal low-pass filter result in ringing artifacts via the Gibbs phenomenon, which can be reduced or worsened by the choice of windowing function. Design and choice of real filters involves understanding and minimizing these artifacts. For example, simple truncation of the sinc function will create severe ringing artifacts, which can be reduced using window functions that drop off more smoothly at the edges.

## Time response

The time response of a low-pass filter is found by solving the response to the simple low-pass RC filter.

Using Kirchhoff's laws we arrive at the differential equation
v_out(t) = v_in(t) - RC (dv_out)/(dt).
This first-order differential equation is also the standard form of a leaky integrator, with the time constant τ = *RC*.

### Step input response example

If we let v_in(t) be a step function of magnitude V_i, then the differential equation has the solution
v_out(t) = V_i (1 - e^-ω_0 t),
where ω_0 = 1/RC is the cutoff frequency of the filter.

## Frequency response

The most common way to characterize the frequency response of a circuit is to find its Laplace transform transfer function, H(s) = V_out(s)/V_in(s). Taking the Laplace transform of our differential equation and solving for H(s), we get
H(s) = (V_out(s))/(V_in(s)) = (ω_0)/(s + ω_0).

## Difference equation through discrete time sampling

A discrete difference equation is easily obtained by sampling the step input response above at regular intervals of nT, where n = 0, 1, …, and T is the time between samples. Taking the difference between two consecutive samples, we have
v_out(nT) - v_outbig((n - 1)Tbig) = V_i (1 - e^-ω_0 nT) - V_i (1 - e^-ω_0 [(n - 1)T]).

Solving for v_out(nT) gives
v_out(nT) = β v_outbig((n - 1)Tbig) + (1 - β) V_i,
where β = e^-ω_0 T.

Using the notation V_n = v_out(nT) and v_n = v_in(nT), and substituting our sampled value, v_n = V_i, we get the difference equation
V_n = β V_n-1 + (1 - β)v_n.

### Error analysis

Comparing the reconstructed output signal from the difference equation, V_n = β V_n-1 + (1 - β)v_n, to the step input response, v_out(t) = V_i (1 - e^-ω_0 t), we find that there is an exact reconstruction (0% error). This is the reconstructed output for a time-invariant input. However, if the input is *time-variant*, such as v_in(t) = V_i sin(ω t), this model approximates the input signal as a series of step functions with duration T, producing an error in the reconstructed output signal. The error produced from *time-variant* inputs is difficult to quantify but decreases as T → 0.

_(Later sections of the original article were left out of this offline copy for length: Discrete-time realization, Continuous-time realization, Electronic low-pass filters.)_
