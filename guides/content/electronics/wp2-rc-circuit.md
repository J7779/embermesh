---
id: electronics/wp2-rc-circuit
title: RC circuit
category: electronics
tags: [rc circuit, resistor–capacitor circuit, rc filter, rc network, electronics]
summary: A resistor–capacitor circuit (RC circuit), or RC filter or RC network, is an electric circuit composed of resistors and capacitors.
source: https://en.wikipedia.org/w/index.php?title=RC_circuit&oldid=1383276735
source_title: Wikipedia: RC circuit
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "RC circuit", Wikipedia, The Free Encyclopedia (revision 1383276735), https://en.wikipedia.org/w/index.php?title=RC_circuit&oldid=1383276735 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# RC circuit

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **resistor–capacitor circuit** (**RC circuit**), or **RC filter** or **RC network**, is an electric circuit composed of resistors and capacitors. It may be driven by a voltage or current source and these will produce different responses. A first order RC circuit is composed of one resistor and one capacitor and is the simplest type of RC circuit.

RC circuits can be used to filter a signal by blocking certain frequencies and passing others. The two most common RC filters are the high-pass filters and low-pass filters; band-pass filters and band-stop filters usually require RLC filters, though crude ones can be made with RC filters.

## Natural response

The simplest RC circuit consists of a resistor with resistance R and a charged capacitor with capacitance C connected to one another in a single loop, without an external voltage source. The capacitor will discharge its stored energy through the resistor. If *V*(*t*) is taken to be the voltage of the capacitor's top plate relative to its bottom plate in the figure, then the capacitor current–voltage relation says the current *I*(*t*) *exiting* the capacitor's top plate will equal C multiplied by the *negative* time derivative of *V*(*t*). Kirchhoff's current law says this current is the same current entering the top side of the resistor, which per Ohm's law equals *V*(*t*)/*R*. This yields a linear differential equation
overbraceC(-dV(t))/(dt)^capacitor current = overbrace(V(t))/(R)^resistor current,
which can be rearranged according to the standard form for exponential decay:
(dV(t))/(dt) = -1/RCV(t).
This means that the instantaneous rate of voltage decrease at any time is proportional to the voltage at that time. Solving for *V*(*t*) yields an exponential decay curve that asymptotically approaches 0:
V(t) = V_0 · e^-t/RC,
where *V*₀ is the capacitor voltage at time *t* = 0, and e is Euler's number.

The time required for the voltage to fall to *V*₀/*e* is called the RC time constant and is given by
τ = RC.
When using the International System of Units, R is in ohms, and C is in farads, so τ will be in seconds. At any time *N*·τ, the capacitor's charge or voltage will be 1/*e*^N of its starting value. So if the capacitor's charge or voltage is said to start at 100%, then 36.8% remains at 1·τ, 13.5% remains at 2·τ, 5% remains at 3·τ, 1.8% remains at 4·τ, and less than 0.7% remains at 5·τ and later.

The half-life (*t*₁/₂) is the time that it takes for its charge or voltage to be reduced in half:
1/2 = e^-tfract_1/2τ Rightarrow t_1/2 = ln(2) τ ≈ 0.693 τ.
For example, 50% of charge or voltage remains at time 1·*t*₁/₂, then 25% remains at time 2·*t*₁/₂, then 12.5% remains at time 3·*t*₁/₂, and 1/2^N will remain at time *N*·*t*₁/₂.

## Complex impedance

The RC circuit's behavior is well-suited to be analyzed in the Laplace domain, which the rest of this article requires a basic understanding of. The Laplace domain is a frequency domain representation using complex frequency s, which is (in general) a complex number:
s = σ + j ω,
where
j represents the imaginary unit: *j*² = −1,
σ is the exponential decay constant (equal to −1/(*RC*) in this case),
ω is the sinusoidal angular frequency.

When evaluating circuit equations in the Laplace domain, time-dependent circuit elements of capacitance and inductance can be treated like resistors with complex-valued impedance instead of real resistance. While the complex impedance Z_R of a resistor is simply a real value equal to its resistance R, the complex impedance of a capacitor C is instead
Z_C = (1)/(C s).

## Series circuit

### Current

Kirchhoff's current law means that the current in the series circuit is necessarily the same through both elements. Ohm's law says this current is equal to the input voltage V_in divided by the sum of the complex impedance of the capacitor and resistor:
I(s) = (V_in(s) )/(R + 1/Cs); = ( Cs )/( 1 + RCs ) V_in(s).

### Voltage

By viewing the circuit as a voltage divider, the voltage across the capacitor is:
V_C(s) = (1/Cs)/(R + 1/Cs)V_in(s); = (1)/(1 + RCs)V_in(s)

and the voltage across the resistor is:
V_R(s) = (R)/(R + 1/Cs)V_in(s); = (RCs)/(1 + RCs)V_in(s).

### Transfer functions

The transfer function from the input voltage to the voltage across the capacitor is
H_C(s) = ( V_C(s) )/( V_in(s) ) = ( 1 )/( 1 + RCs ).

Similarly, the transfer function from the input to the voltage across the resistor is

H_R(s) = frac V_R(s) V_rm in(s) = ( RCs )/( 1 + RCs ).

#### Poles and zeros

Both transfer functions have a single pole located at
s = -1/RC.

In addition, the transfer function for the voltage across the resistor has a zero located at the origin.

### Frequency-domain considerations

The sinusoidal steady state is a special case of complex frequency that considers the input to consist only of pure sinusoids. Hence, the exponential decay component represented by σ can be ignored in the complex frequency equation s = σ + j ω when only the steady state is of interest. The simple substitution of s Rightarrow j ω into the previous transfer functions will thus provide the sinusoidal gain and phase response of the circuit.

#### Gain

The magnitude of the gains across the two components are
G_C = big| H_C(j ω) big| = |(V_C(j ω))/(V_in(j ω))| = (1)/(√(1 + (ω RC)^2))
and
G_R = big| H_R(j ω) big| = |(V_R(j ω))/(V_in(j ω))| = (ω RC)/(√(1 + (ω RC)^2)),
As the frequency becomes very large (*ω* → ∞), the capacitor acts like a short circuit, so:
G_C → 0 and G_R → 1.

As the frequency becomes very small (*ω* → 0), the capacitor acts like an open circuit, so:
G_C → 1 and G_R → 0.

##### Operation as either a high-pass or a low-pass filter

The behavior at these extreme frequencies show that if the output is taken across the capacitor, high frequencies are attenuated and low frequencies are passed, so such a circuit configuration is a *low-pass filter*. However, if the output is taken across the resistor, then high frequencies are passed and low frequencies are attenuated, so such a configuration is a *high-pass filter*.

##### Cutoff frequency

The range of frequencies that the filter passes is called its bandwidth. The frequency at which the filter attenuates the signal to half its unfiltered power is termed its cutoff frequency. This requires that the gain of the circuit be reduced to
G_C = G_R = (1)/(sqrt 2).

Solving the above equation yields
ω_c = 1/RC or f_c = (1)/(2π RC)
which is the frequency that the filter will attenuate to half its original power.

#### Phase

The phase angles are
φ_C = angle H_C(j ω) = tan^-1(-ω RC)
and
φ_R = angle H_R(j ω) = tan^-1((1)/(ω RC)).

As *ω* → 0:
φ_C → 0 and φ_R → 90^° = (π)/(2) radians.

As *ω* → ∞:
φ_C → -90^° = -(π)/(2) radians and φ_R → 0.

While the output signal's phase shift relative to the input depends on frequency, this is generally less interesting than the gain variations. At DC (0 Hz), the capacitor voltage is in phase with the input signal voltage while the resistor voltage leads it by 90°. As frequency increases, the capacitor voltage comes to have a 90° lag relative to the input signal and the resistor voltage comes to be in-phase with the input signal.

#### Phasor representation

The gain and phase expressions together may be combined into these phasor expressions representing the output:
V_C = G_C V_in e^jφ_C \ V_R = G_R V_in e^jφ_R.

### Impulse response

The impulse response for each voltage is the inverse Laplace transform of the corresponding transfer function. It represents the response of the circuit to an input voltage consisting of an impulse or Dirac delta function.

The impulse response for the capacitor voltage is
h_C(t) = 1/RC e^-t/RC u(t) = (1)/(τ) e^-(t)/(τ) u(t),

where *u*(*t*) is the Heaviside step function and *τ* = *RC* is the time constant.

Similarly, the impulse response for the resistor voltage is
h_R(t) = δ (t) - 1/RC e^-t/RC u(t) = δ (t) - (1)/(τ) e^-(t)/(τ) u(t),

where *δ*(*t*) is the Dirac delta function.

### Time-domain considerations

*This section relies on knowledge of the Laplace transform*.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Series circuit, Parallel circuit.)_
