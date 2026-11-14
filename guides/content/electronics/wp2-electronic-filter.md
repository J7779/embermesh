---
id: electronics/wp2-electronic-filter
title: Electronic filter
category: electronics
tags: [electronic filter, electronic filters, electronics]
summary: Electronic filters are a type of signal processing filter in the form of electrical circuits.
source: https://en.wikipedia.org/w/index.php?title=Electronic_filter&oldid=1367750379
source_title: Wikipedia: Electronic filter
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Electronic filter", Wikipedia, The Free Encyclopedia (revision 1367750379), https://en.wikipedia.org/w/index.php?title=Electronic_filter&oldid=1367750379 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Electronic filter

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Electronic filters** are a type of signal processing filter in the form of electrical circuits. This article covers those filters consisting of lumped electronic components, as opposed to distributed-element filters. That is, using components and interconnections that, in analysis, can be considered to exist at a single point. These components can be in discrete packages or part of an integrated circuit.

Electronic filters remove unwanted frequency components from the applied signal, enhance wanted ones, or both. They can be:

- passive or active
- analog or digital
- high-pass, low-pass, band-pass, band-stop (band-rejection; notch), or all-pass.
- discrete-time (sampled) or continuous-time
- linear or non-linear
- infinite impulse response (IIR type) or finite impulse response (FIR type)

The most common types of electronic filters are linear filters, regardless of other aspects of their design. See the article on linear filters for details on their design and analysis.

## Classification by technology

### Passive filters

Passive implementations of linear filters are based on combinations of resistors (R), inductors (L) and capacitors (C). These types are collectively known as *passive filters*, because they do not depend upon an external power supply and they do not contain active components such as transistors.

Inductors block high-frequency signals and conduct low-frequency signals, while capacitors do the reverse. A filter in which the signal passes through an inductor, or in which a capacitor provides a path to ground, presents less attenuation to low-frequency signals than high-frequency signals and is therefore a *low-pass filter*. If the signal passes through a capacitor, or has a path to ground through an inductor, then the filter presents less attenuation to high-frequency signals than low-frequency signals and therefore is a *high-pass filter*. Resistors on their own have no frequency-selective properties, but are added to inductors and capacitors to determine the *time-constants* of the circuit, and therefore the frequencies to which it responds.

The inductors and capacitors are the reactive elements of the filter. The number of elements determines the order of the filter. In this context, an LC tuned circuit being used in a band-pass or band-stop filter is considered a single element even though it consists of two components.

At high frequencies (above about 100 megahertz), sometimes the inductors consist of single loops or strips of sheet metal, and the capacitors consist of adjacent strips of metal. These inductive or capacitive pieces of metal are called stubs.

#### Single element types

The simplest passive filters, RC and RL filters, include only one reactive element, except for the hybrid LC filter, which is characterized by inductance and capacitance integrated in one element.

#### L filter

An L filter consists of two reactive elements, one in series and one in parallel.

#### T and π filters

Three-element filters can have a 'T' or 'π' topology and in either geometries, a low-pass, high-pass, band-pass, or band-stop characteristic is possible. The components can be chosen symmetric or not, depending on the required frequency characteristics. The high-pass T filter in the illustration, has a very low impedance at high frequencies, and a very high impedance at low frequencies. That means that it can be inserted in a transmission line, resulting in the high frequencies being passed and low frequencies being reflected. Likewise, for the illustrated low-pass π filter, the circuit can be connected to a transmission line, transmitting low frequencies and reflecting high frequencies. Using m-derived filter sections with correct termination impedances, the input impedance can be reasonably constant in the pass band.

#### Multiple-element types

Multiple-element filtration are usually constructed as a ladder network. These can be seen as a continuation of the L, T and π designs of filters. More elements are needed when it is desired to improve some parameter of the filter such as stop-band rejection or slope of transition from pass-band to stop-band.

#### Reflectionless filter

Reflectionless filters are passive structures that in principle have identically zero reflection coefficient at all frequencies. Such structures may be synthesized with lumped inductors, capacitors, and resistors, or by transmission lines and resistors.

### Active filters

Active filters are implemented using a combination of passive and active (amplifying) components, and require an outside power source. Operational amplifiers are frequently used in active filter designs. These can have high Q factor, and can achieve resonance without the use of inductors. However, their upper frequency limit is limited by the bandwidth of the amplifiers.

### Other filter technologies

There are many filter technologies other than lumped component electronics. These include digital filters, crystal filters, mechanical filters, surface acoustic wave (SAW) filters, thin-film bulk acoustic resonator (TFBAR, FBAR) based filters, garnet filters, and atomic filters (used in atomic clocks).

## The transfer function

*see also Filter (signal processing) for further analysis*
The transfer function H(s) of a filter is the ratio of the output signal Y(s) to that of the input signal X(s) as a function of the complex frequency s:

H(s)=(Y(s))/(X(s)).

The transfer function of all linear time-invariant filters, when constructed of lumped components (as opposed to distributed components such as transmission lines), will be the ratio of two polynomials in s, i.e. a rational function of s. The order of the transfer function will be the highest power of s encountered in either the numerator or the denominator.

## Classification by topology

Electronic filters can be classified by the technology used to implement them.
Filters using passive filter and active filter technology can be further classified by the particular electronic filter topology used to implement them.

Any given filter transfer function may be implemented in any electronic filter topology.

Some common circuit topologies are:

- Cauer topology – passive
- Sallen–Key topology – active
- Multiple feedback topology – active
- State variable topology – active
- Biquadratic topology – active

_(Later sections of the original article were left out of this offline copy for length: Classification by design methodology.)_
