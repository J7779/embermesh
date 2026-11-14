---
id: security/wp2-random-number-generation
title: Random number generation
category: security
tags: [random number generation, random number generator, rng, security]
summary: Random number generation is a process by which, often by means of a random number generator (RNG), a sequence of numbers or symbols is generated that cannot be reasonably predicted better than by random chance.
source: https://en.wikipedia.org/w/index.php?title=Random_number_generation&oldid=1385109583
source_title: Wikipedia: Random number generation
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Random number generation", Wikipedia, The Free Encyclopedia (revision 1385109583), https://en.wikipedia.org/w/index.php?title=Random_number_generation&oldid=1385109583 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Random number generation

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Random number generation** is a process by which, often by means of a **random number generator** (**RNG**), a sequence of numbers or symbols is generated that cannot be reasonably predicted better than by random chance. This means that the particular outcome sequence will contain some patterns detectable in hindsight but impossible to foresee. True random number generators can be hardware random number generators (HRNGs), wherein each generation is a function of the current value of a physical environment's attribute that is constantly changing in a manner that is practically impossible to model. This would be in contrast to pseudorandom number generators (PRNGs), which generate pseudorandom numbers that are in fact predetermined—these numbers can be reproduced simply by knowing the initial state of the PRNG and the method it uses to generate numbers. There is also a class of non-physical true random number generators (NPTRNG) that produce true random numbers without an access to a dedicated hardware source, by scavenging entropy that is present in the computer system.

Various applications of randomness have led to the development of different methods for generating random data. Some of these have existed since ancient times, including well-known examples like the rolling of dice, coin flipping, the shuffling of playing cards, the use of yarrow stalks (for divination) in the I Ching, as well as countless other techniques. Because of the mechanical nature of these techniques, generating large quantities of sufficiently random numbers (important in statistics) required much work and time. Thus, results would sometimes be collected and distributed as random number tables.

Several computational methods for pseudorandom number generation exist. All fall short of the goal of true randomness, although they may meet, with varying success, some of the statistical tests for randomness intended to measure how unpredictable their results are (that is, to what degree their patterns are discernible). This generally makes them unusable for applications such as cryptography. However, carefully designed *cryptographically secure pseudorandom number generators* (CSPRNGS) also exist, with special features specifically designed for use in cryptography.

## Practical applications and uses

Random number generators have applications in gambling, statistical sampling, computer simulation, cryptography, completely randomized design, and other areas where producing an unpredictable result is desirable. Generally, in applications having unpredictability as the paramount feature, such as in security applications, hardware generators are generally preferred over pseudorandom algorithms, where feasible.

Pseudorandom number generators are very useful in developing Monte Carlo-method simulations, as debugging is facilitated by the ability to run the same sequence of random numbers again by starting from the same *random seed*. They are also used in cryptography – so long as the *seed* is secret. The sender and receiver can generate the same set of numbers automatically to use as keys.

The generation of pseudorandom numbers is an important and common task in computer programming. While cryptography and certain numerical algorithms require a very high degree of *apparent* randomness, many other operations only need a modest amount of unpredictability. Some simple examples might be presenting a user with a "random quote of the day", or determining which way a computer-controlled adversary might move in a computer game. Weaker forms of *randomness* are used in hash algorithms and in creating amortized searching and sorting algorithms.

Some applications that appear at first sight to be suitable for randomization are in fact not quite so simple. For instance, a system that "randomly" selects music tracks for a background music system must only *appear* random, and may even have ways to control the selection of music: a truly random system would have no restriction on the same item appearing two or three times in succession.

## True vs. pseudo-random numbers

There are two principal methods used to generate random numbers. The first method measures some physical phenomenon that is expected to be random and then compensates for possible biases in the measurement process. Example sources include measuring atmospheric noise, thermal noise, and other external electromagnetic and quantum phenomena. For example, cosmic background radiation or radioactive decay as measured over short timescales represent sources of natural entropy (as a measure of unpredictability or surprise of the number generation process).

The speed at which entropy can be obtained from natural sources is dependent on the underlying physical phenomena being measured. Thus, sources of naturally occurring *true* entropy are said to be blocking – they are rate-limited until enough entropy is harvested to meet the demand. On some Unix-like systems, including older Linux distributions, the pseudo device file `/dev/random` will block until sufficient entropy is harvested from the environment. Due to this blocking behavior, large bulk reads from `/dev/random`, such as filling a hard disk drive with random bits, can often be slow on systems that use this type of entropy source.

The second method uses computational algorithms that can produce long sequences of apparently random results, which are in fact completely determined by a shorter initial value, known as a seed value or key. As a result, the entire seemingly random sequence can be reproduced if the seed value is known. This type of random number generator is often called a pseudorandom number generator. This type of generator typically does not rely on sources of naturally occurring entropy, though it may be periodically seeded by natural sources. This generator type is non-blocking, so they are not rate-limited by an external event, making large bulk reads a possibility.

Standard cryptographic designs take a hybrid approach, using randomness harvested from natural sources to seed cryptographically secure pseudorandom number generators (CSPRNGs). Hardware random number generators generally produce only a limited number of random bits per second. In order to increase the available output data rate, they are often used to generate the "seed" for a faster PRNG. PRNG also helps with the noise source "anonymization" (whitening out the noise source identifying characteristics) and entropy extraction. With a proper PRNG algorithm selected (cryptographically secure pseudorandom number generator, CSPRNG), the combination can satisfy the requirements of Federal Information Processing Standards and Common Criteria standards.

## Generation methods

### Physical methods

The earliest methods for generating random numbers, such as dice, coin flipping and roulette wheels, are still used today, mainly in games and gambling, as they tend to be too slow for most applications in statistics and cryptography.

### Computational methods

For performance and security reasons, most random number generators use PRNGs in their construction (this architecture is actually mandated in the industrial standards like NIST SP 800-90A).

### By humans

Random number generation may also be performed by humans, in the form of collecting various inputs from end users and using them as a randomization source. However, most studies find that human subjects have some degree of non-randomness when attempting to produce a random sequence of e.g., digits or letters. They may alternate too much between choices when compared to a good random generator; thus, this approach is not widely used. However, for the very reason that humans perform poorly in this task, human random number generation can be used as a tool to gain insights into brain functions otherwise not accessible.

## Low-discrepancy sequences as an alternative

Some computations making use of a random number generator can be summarized as the computation of a total or average value, such as the computation of integrals by the Monte Carlo method. For such problems, it may be possible to find a more accurate solution by the use of so-called low-discrepancy sequences, also called quasirandom numbers. Such sequences have a definite pattern that fills in gaps evenly, qualitatively speaking; a truly random sequence may, and usually does, leave larger gaps.

_(Later sections of the original article were left out of this offline copy for length: Post-processing and statistical checks, Other considerations, Activities and demonstrations, Backdoors.)_
