---
id: electronics/wp2-transformer
title: Transformer
category: electronics
tags: [transformer, electronics]
summary: In electrical engineering, a transformer is a passive component that transfers electrical energy from one electrical circuit to another circuit, or multiple circuits.
source: https://en.wikipedia.org/w/index.php?title=Transformer&oldid=1384501652
source_title: Wikipedia: Transformer
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Transformer", Wikipedia, The Free Encyclopedia (revision 1384501652), https://en.wikipedia.org/w/index.php?title=Transformer&oldid=1384501652 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Transformer

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In electrical engineering, a **transformer** is a passive component that transfers electrical energy from one electrical circuit to another circuit, or multiple circuits. A varying current in any coil of the transformer produces a varying magnetic flux in the transformer's core, which induces a varying electromotive force (EMF) across any other coils wound around the same core. Electrical energy can be transferred between separate coils without a metallic (conductive) connection between the two circuits. Faraday's law of induction, discovered in 1831, describes the induced voltage effect in any coil due to a changing magnetic flux encircled by the coil.

Transformers are used to change AC voltage levels, such transformers being termed *step-up* or *step-down* type to increase or decrease voltage level, respectively. Transformers can also be used to provide galvanic isolation between circuits as well as to couple stages of signal-processing circuits. Since the invention of the first constant-potential transformer in 1885, transformers have become essential for the transmission, distribution, and utilization of alternating current electric power. A wide range of transformer designs is encountered in electronic and electric power applications. Transformers range in size from RF transformers less than a cubic centimeter in volume, to units weighing hundreds of tons used to interconnect the power grid.

## Principles

### Ideal transformer

An ideal transformer is linear, lossless, and perfectly coupled. Perfect coupling implies infinitely high core magnetic permeability and winding inductance and zero net magnetomotive force (i.e. *i*_*p**n*_*p* − *i*_*s**n*_*s* = 0).

A varying current in the transformer's primary winding creates a varying magnetic flux in the transformer core, which is also encircled by the secondary winding. This varying flux at the secondary winding induces a varying electromotive force or voltage in the secondary winding. This electromagnetic induction phenomenon is the basis of transformer action and, in accordance with Lenz's law, the secondary current so produced creates a flux equal and opposite to that produced by the primary winding.

The windings are wound around a core of infinitely high magnetic permeability so that all of the magnetic flux passes through both the primary and secondary windings. With a voltage source connected to the primary winding and a load connected to the secondary winding, the transformer currents flow in the indicated directions and the core magnetomotive force cancels to zero.

According to Faraday's law, since the same magnetic flux passes through both the primary and secondary windings in an ideal transformer, a voltage is induced in each winding proportional to its number of turns. The transformer winding voltage ratio is equal to the winding turns ratio.

An ideal transformer is a reasonable approximation for a typical commercial transformer, with voltage ratio and winding turns ratio both being inversely proportional to the corresponding current ratio.

The load impedance *referred* to the primary circuit is equal to the turns ratio squared times the secondary circuit load impedance.

### Real transformer

#### Deviations from ideal transformer

The ideal transformer model neglects many basic linear aspects of real transformers, including unavoidable losses and inefficiencies.

(a) Core losses, collectively called magnetizing current losses, consisting of

- Hysteresis losses due to nonlinear magnetic effects in the transformer core, and
- Eddy current losses due to joule heating in the core that are proportional to the square of the transformer's applied voltage.

(b) Unlike the ideal model, the windings in a real transformer have non-zero resistances and inductances associated with:

- Joule losses due to resistance in the primary and secondary windings
- Leakage flux that escapes from the core and passes through one winding only, resulting in primary and secondary reactive impedance.

(c) similar to an inductor, parasitic capacitance and self-resonance phenomenon due to the electric field distribution. Three kinds of parasitic capacitance are usually considered and the closed-loop equations are provided

- Capacitance between adjacent turns in any one layer;
- Capacitance between adjacent layers;
- Capacitance between the core and the layer(s) adjacent to the core;

Inclusion of capacitance into the transformer model is complicated, and is rarely attempted; the 'real' transformer model's equivalent circuit shown below does not include parasitic capacitance. However, the capacitance effect can be measured by comparing open-circuit inductance, i.e., the inductance of a primary winding when the secondary circuit is open, to a short-circuit inductance when the secondary winding is shorted.

#### Leakage flux

The ideal transformer model assumes that all flux generated by the primary winding links all the turns of every winding, including itself. In practice, some flux traverses paths that take it outside the windings. Such flux is termed *leakage flux*, and results in leakage inductance in series with the mutually coupled transformer windings. Leakage flux results in energy being alternately stored in and discharged from the magnetic fields with each cycle of the power supply. It is not directly a power loss, but results in inferior voltage regulation, causing the secondary voltage not to be directly proportional to the primary voltage, particularly under heavy load. Transformers are therefore normally designed to have very low leakage inductance.

In some applications, increased leakage is desired, and long magnetic paths, air gaps, or magnetic bypass shunts may deliberately be introduced in a transformer design to limit the short-circuit current it will supply. Leaky transformers may be used to supply loads that exhibit negative resistance, such as electric arcs, mercury- and sodium- vapor lamps and neon signs or for safely handling loads that become periodically short-circuited such as electric arc welders.

Air gaps are also used to keep a transformer from saturating, especially audio-frequency transformers in circuits that have a DC component flowing in the windings. A saturable reactor exploits saturation of the core to control alternating current.

Knowledge of leakage inductance is also useful when transformers are operated in parallel. It can be shown that if the percent impedance and associated winding leakage reactance-to-resistance (*X*/*R*) ratio of two transformers were
the same, the transformers would share the load power in proportion to their respective ratings. However, the impedance tolerances of commercial transformers are significant. Also, the impedance and X/R ratio of different capacity transformers tend to vary.

#### Equivalent circuit

Referring to the diagram, a practical transformer's physical behavior may be represented by an equivalent circuit model, which can incorporate an ideal transformer.

Winding joule losses and leakage reactance are represented by the following series loop impedances of the model:

- Primary winding: *R*_P, *X*_P
- Secondary winding: *R*_S, *X*_S.
In normal course of circuit equivalence transformation, *R*_S and *X*_S are in practice usually referred to the primary side by multiplying these impedances by the turns ratio squared, (*N*_P/*N*_S)^ 2 = *a*².

Core loss and reactance is represented by the following shunt leg impedances of the model:

- Core or iron losses: *R*_C
- Magnetizing reactance: *X*_M.
*R*_C and *X*_M are collectively termed the *magnetizing branch* of the model.

Core losses are caused mostly by hysteresis and eddy current effects in the core and are proportional to the square of the core flux for operation at a given frequency. The finite permeability core requires a magnetizing current *I*_M to maintain mutual flux in the core. Magnetizing current is in phase with the flux, the relationship between the two being non-linear due to saturation effects. However, all impedances of the equivalent circuit shown are by definition linear and such non-linearity effects are not typically reflected in transformer equivalent circuits. With sinusoidal supply, core flux lags the induced EMF by 90°. With open-circuited secondary winding, magnetizing branch current *I*₀ equals transformer no-load current.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Principles, Construction, Classification parameters, Applications.)_
