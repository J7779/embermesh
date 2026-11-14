---
id: electronics/wp2-pulse-width-modulation
title: Pulse-width modulation
category: electronics
tags: [pulse-width modulation, pwm, pulse-duration modulation, pdm, pulse-length modulation, electronics]
summary: Pulse-width modulation (PWM), also known as pulse-duration modulation (PDM) or pulse-length modulation (PLM), is any method of representing a signal as a rectangular wave with a varying duty cycle (and for some methods also a varying period).
source: https://en.wikipedia.org/w/index.php?title=Pulse-width_modulation&oldid=1382026850
source_title: Wikipedia: Pulse-width modulation
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Pulse-width modulation", Wikipedia, The Free Encyclopedia (revision 1382026850), https://en.wikipedia.org/w/index.php?title=Pulse-width_modulation&oldid=1382026850 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Pulse-width modulation

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Pulse-width modulation** (**PWM**), also known as **pulse-duration modulation** (**PDM**) or **pulse-length modulation** (**PLM**), is any method of representing a signal as a rectangular wave with a varying duty cycle (and for some methods also a varying period).

PWM is useful for controlling the average power or amplitude delivered by an electrical signal. The average value of voltage (and current) fed to the load is controlled by switching the supply between 0 and 100% at a rate faster than it takes the load to change significantly. The longer the switch is on, the higher the total power supplied to the load. Along with maximum power point tracking (MPPT), it is one of the primary methods of controlling the output of solar panels to that which can be utilized by a battery. PWM is particularly suited for running inertial loads such as motors, which are not as easily affected by this discrete switching. The goal of PWM is to control a load; however, the PWM switching frequency must be selected carefully in order to do so smoothly.

The PWM switching frequency can vary greatly depending on load and application. For example, switching only has to be done several times a minute in an electric stove, 100 or 120 Hz (double of the utility frequency) in a lamp dimmer, between a few kilohertz (kHz) and tens of kHz for a motor drive, and well into the tens or hundreds of kHz in audio amplifiers and computer power supplies. Choosing a switching frequency that is too high for the application may cause premature failure of mechanical control components, despite getting smooth control of the load. Selecting a switching frequency that is too low for the application causes oscillations in the load. The main advantage of PWM is that power loss in the switching devices is very low. When a switch is off, there is practically no current, and when it is on and power is being transferred to the load, there is almost no voltage drop across the switch. Power loss, being the product of voltage and current, is thus in both cases close to zero. PWM also works well with digital controls, which, because of their on/off nature, can easily set the needed duty cycle. PWM has also been used in certain communication systems where its duty cycle has been used to convey information over a communications channel.

## Duty cycle

The term *duty cycle* describes the proportion of 'on' time to the regular interval or 'period' of time; a low duty cycle corresponds to low power, because the power is off for most of the time. Duty cycle is expressed in percent, 100% being fully on. When a digital signal is on half of the time and off the other half of the time, the digital signal has a duty cycle of 50% and resembles a "square" wave. When a digital signal spends more time in the on state than the off state, it has a duty cycle of >50%. When a digital signal spends more time in the off state than the on state, it has a duty cycle of <50%. Here is a pictorial that illustrates these three scenarios:

## Principle

### Periodic pulse wave

If we consider a periodic pulse wave f(t) with period T, low value y_min, a high value y_max and a constant duty cycle D (Figure 1), the average value of the waveform is given by:

y = 1/T∫^T_0f(t) dt

As f(t) is a pulse wave, its value is y_max for 0 < t < D · T and y_min for D · T < t < T. The above expression then becomes:

y = 1/T (∫_0^DT y_max dt + ∫_DT^T y_min dt); = 1/T (D · T · y_max + T(1 - D) y_min); = D· y_max + (1 - D) y_min

This latter expression can be fairly simplified in many cases where y_min = 0 as y = D · y_max. From this, the average value of the signal (y) is directly dependent on the duty cycle D.

However, by varying (i.e. modulating) the duty cycle (and possibly also the period), the following more advanced pulse-width modulated waves allow variation of the average value of the waveform.

### Intersective method PWM

The intersective method is a simple way to generate a PWM output signal (magenta in the above figure) with a fixed period and varying duty cycle by using a comparator to switch the PWM output state when the input waveform (red) intersects with a sawtooth or a triangle waveform (blue).

Depending on the type of sawtooth or triangle waveform (green in the figure below), intersective PWM signals (blue in the figure) can be aligned in three ways:

- Leading edge modulation (top plot) uses a reverse sawtooth wave to generate the PWM. The PWM's leading edge is held at the leading edge of the window and the trailing edge is modulated.
- Trailing edge modulation (middle plot) uses a normal sawtooth wave to generate the PWM. The PWM's trailing edge is fixed and the leading edge is modulated.
- Centered pulses (bottom) uses a triangle waveform to generate the PWM. The pulse center is fixed in the center of the time window and both edges of the pulse are moved to compress or expand the width.

#### Time proportioning

Many digital circuits can generate PWM signals (e.g., many microcontrollers have PWM outputs). They normally use a counter that increments periodically (it is connected directly or indirectly to the clock of the circuit) and is reset at the end of every period of the PWM. When the counter value is greater than the reference value, the PWM output changes state from high to low (or low to high). This technique is referred to as **time proportioning,** particularly as **time-proportioning control** – which *proportion* of a fixed cycle time is spent in the high state.

The incremented and periodically reset counter is the discrete version of the intersecting method's sawtooth. The analog comparator of the intersecting method becomes a simple integer comparison between the current counter value and the digital (possibly digitized) reference value. The duty cycle can only be varied in discrete steps, as a function of the counter resolution. However, a high-resolution counter can provide quite satisfactory performance.

#### Spectrum

The resulting spectra (of the three alignments) are similar. Each contains a DC component, a base sideband containing the modulating signal, and phase-modulated carriers at each harmonic of the frequency of the pulse. The amplitudes of the harmonic groups are restricted by a sin x / x envelope (sinc function) and extend to infinity. The infinite bandwidth is caused by the nonlinear operation of the pulse-width modulator. In consequence, a digital PWM suffers from aliasing distortion that significantly reduces its applicability for modern communication systems. By limiting the bandwidth of the PWM kernel, aliasing effects can be avoided.

On the contrary, delta modulation and delta-sigma modulation are random processes that produce a continuous spectrum without distinct harmonics. While intersective PWM uses a fixed period but a varying duty cycle, the period of delta and delta-sigma modulated PWMs varies in addition to their duty cycle.

### Delta modulation

Delta modulation produces a PWM signal (magenta in the above figure) which changes state whenever its integral (blue) hits the limits (green) surrounding the input (red).

### Asynchronous delta-sigma PWM

Asynchronous (i.e., unclocked) delta-sigma modulation produces a PWM output (blue in bottom plot) which is subtracted from the input signal (green in top plot) to form an error signal (blue in top plot). This error is integrated (magenta in the middle plot). When the integral of the error exceeds the limits (the upper and lower grey lines in the middle plot), the PWM output changes state. By integrating the difference between the error and the input signal, delta-sigma modulation shapes noise of the resulting spectrum is more concentrated in higher frequencies above the input signal's band.

### Space vector modulation

Space vector modulation is a PWM control algorithm for multi-phase AC generation, in which the reference signal is sampled regularly; after each sample, non-zero active switching vectors adjacent to the reference vector and one or more of the zero switching vectors are selected for the appropriate fraction of the sampling period in order to synthesize the reference signal as the average of the used vectors.

### Direct torque control (DTC)

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Principle, PWM sampling theorem, PWM controllers, Applications.)_
