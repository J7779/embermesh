---
id: security/wp2-hardware-random-number-generator
title: Hardware random number generator
category: security
tags: [hardware random number generator, hrng, true random number generator, trng, non-deterministic random bit generator, security]
summary: In computing, a hardware random number generator (HRNG), true random number generator (TRNG), non-deterministic random bit generator (NRBG), or physical random number generator is a device that generates random numbers from a physical process capable of producing entropy, unlike a…
source: https://en.wikipedia.org/w/index.php?title=Hardware_random_number_generator&oldid=1385037126
source_title: Wikipedia: Hardware random number generator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Hardware random number generator", Wikipedia, The Free Encyclopedia (revision 1385037126), https://en.wikipedia.org/w/index.php?title=Hardware_random_number_generator&oldid=1385037126 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Hardware random number generator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In computing, a **hardware random number generator** (**HRNG**), **true random number generator** (**TRNG**), **non-deterministic random bit generator** (**NRBG**), or **physical random number generator** is a device that generates random numbers from a physical process capable of producing entropy, unlike a pseudorandom number generator (PRNG) that utilizes a deterministic algorithm and non-physical nondeterministic random bit generators that do not include hardware dedicated to generation of entropy.

Many natural phenomena generate low-level, statistically random "noise" signals, including thermal and shot noise, jitter and metastability of electronic circuits, Brownian motion, and atmospheric noise. Researchers also used the photoelectric effect, involving a beam splitter, other quantum phenomena, and even nuclear decay (due to practical considerations the latter, as well as the atmospheric noise, is not viable except for fairly restricted applications or online distribution services). While "classical" (non-quantum) phenomena are not truly random, an unpredictable physical system is usually acceptable as a source of randomness, so the qualifiers "true" and "physical" are used interchangeably.

A hardware random number generator is expected to output near-perfect random numbers ("full entropy"). A physical process usually does not have this property, and a practical TRNG typically includes a few blocks:

- a *noise source* that implements the physical process producing the entropy. Usually this process is analog, so a *digitizer* is used to convert the output of the analog source into a binary representation;
- a *conditioner* (randomness extractor) that improves the quality of the random bits;
- *health tests*. TRNGs are mostly used in cryptographic algorithms that get completely broken if the random numbers have low entropy, so the testing functionality is usually included.

Hardware random number generators generally produce only a limited number of random bits per second. In order to increase the available output data rate, they are often used to generate the "seed" for a faster PRNG. PRNG also helps with the noise source "anonymization" (whitening out the noise source identifying characteristics) and entropy extraction. With a proper PRNG algorithm selected (cryptographically secure pseudorandom number generator, CSPRNG), the combination can satisfy the requirements of Federal Information Processing Standards and Common Criteria standards.

## Uses

Hardware random number generators can be used in any application that needs randomness. However, in many scientific applications, the additional cost and complexity of a TRNG (when compared with pseudo-random number generators) provide no meaningful benefits. TRNGs have additional drawbacks for data science and statistical applications: the impossibility of re-running a series of numbers unless they are stored, and reliance on an analog physical entity can obscure the failure of the source. The TRNGs therefore are primarily used in the applications where their unpredictability and the impossibility to re-run the sequence of numbers are crucial to the success of the implementation: in cryptography and gambling machines.

### Cryptography

The major use for hardware random number generators is in the field of data encryption, for example, to create random cryptographic keys and nonces needed to encrypt and sign data. In addition to randomness, there are at least two additional requirements imposed by the cryptographic applications:

1. forward secrecy guarantees that knowledge of the past output and internal state of the device should not enable the attacker to predict future data;
1. backward secrecy protects the opposite direction: knowledge of the output and internal state in the future should not divulge the preceding data.
A typical way to fulfill these requirements is to use a TRNG to seed a cryptographically secure pseudorandom number generator.

## Physical phenomena with random properties

Multiple different TRNG designs were proposed over time with a large variety of noise sources and digitization techniques ("harvesting"). However, practical considerations (size, power, cost, performance, robustness) dictate the following desirable traits:

- use of a commonly available inexpensive silicon process;
- exclusive use of digital design techniques. This allows an easier system-on-chip integration and enables the use of FPGAs;
- compact and low-power design. This discourages use of analog components (e.g., amplifiers);
- mathematical justification of the entropy collection mechanisms.

Stipčević and Koç in 2014 classified the physical phenomena used to implement TRNG into four groups:

- electrical noise;
- free-running oscillators;
- chaos;
- quantum effects.

### Electrical noise-based RNG

Noise-based RNGs generally follow the same outline: the source of a noise generator is fed into a comparator. If the voltage is above threshold, the comparator output is 1, otherwise 0. The random bit value is latched using a flip-flop. Sources of noise vary and include:

- Johnson–Nyquist noise ("thermal noise");
- Zener noise;
- avalanche breakdown.
The drawbacks of using noise sources for an RNG design are:

- noise levels are hard to control; they vary with environmental changes and device-to-device;
- calibration processes needed to ensure a guaranteed amount of entropy are time-consuming;
- noise levels are typically low, thus the design requires power-hungry amplifiers. The sensitivity of amplifier inputs enables manipulation by an attacker;
- circuitry located nearby generates a lot of non-random noise thus lowering the entropy;
- a proof of randomness is near-impossible as multiple interacting physical processes are involved.

### Chaos-based RNG

The idea of chaos-based noise stems from the use of a complex system that is hard to characterize by observing its behavior over time. For example, lasers can be put into (undesirable in other applications) chaos mode with chaotically fluctuating power, with power detected using a photodiode and sampled by a comparator. The design can be quite small, as all photonics elements can be integrated on-chip. Stipčević & Koç characterize this technique as "most objectionable", mostly due to the fact that chaotic behavior is usually controlled by a differential equation and no new randomness is introduced, thus there is a possibility of the chaos-based TRNG producing a limited subset of possible output strings.

### Free-running oscillators-based RNG

The TRNGs based on a free-running oscillator (FRO) typically utilize one or more ring oscillators (ROs), outputs of which are sampled using yet another clock. Since inverters forming the RO can be thought of as amplifiers with a very large gain, an FRO output exhibits very fast oscillations in phase and frequency domains. Despite issues with randomness proofs and chip-to-chip variability, FRO-based TRNGs are very popular due to their use of standard digital logic.

### Quantum-based RNG

Quantum random number generation technology is well established with 8 commercial **quantum random number generator** (**QRNG**) products offered before 2017.

Herrero-Collantes and Garcia-Escartin list the following stochastic processes as "quantum":

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Physical phenomena with random properties, Performance test.)_
