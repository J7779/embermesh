---
id: radio/wp2-atmospheric-noise
title: Atmospheric noise
category: radio
tags: [atmospheric noise, radio]
summary: Atmospheric noise is radio noise, or "static", caused by natural atmospheric processes, primarily lightning discharges in thunderstorms.
source: https://en.wikipedia.org/w/index.php?title=Atmospheric_noise&oldid=1364994476
source_title: Wikipedia: Atmospheric noise
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Atmospheric noise", Wikipedia, The Free Encyclopedia (revision 1364994476), https://en.wikipedia.org/w/index.php?title=Atmospheric_noise&oldid=1364994476 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Atmospheric noise

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Atmospheric noise** is radio noise, or "static", caused by natural atmospheric processes, primarily lightning discharges in thunderstorms. On a worldwide scale, there are about 40 lightning flashes per second, or ≈ 3.5 million lightning discharges per day.

## Lightning

Atmospheric noise is radio noise caused by natural atmospheric processes, primarily lightning discharges in thunderstorms. It is mainly caused by cloud-to-ground flashes as the current is much stronger than that of cloud-to-cloud flashes.
On a worldwide scale, 3.5 million lightning flashes occur daily. That means there are about 40 lightning flashes per second.

The sum of all these lightning flashes results in atmospheric noise. It can be easily heard with any AM radio or SSB receiver tuned to an unused frequency.
The static heard is a combination of white noise (cumulative of distant thunderstorms) and impulse noise (from relatively nearby thunderstorms, if any). The power-sum of this noise varies with seasons and nearness of thunderstorm centers. It can be *seen* as random speckles on an old analog TV set dialed to an empty channel.

Although lightning has a broad-spectrum emission, its noise power increases with decreasing frequency. Therefore, at very low frequency and low frequency, atmospheric noise often dominates, while at high frequency, man-made noise dominates in urban areas.

## Random number generation

Atmospheric noise and variation is also used to generate high quality random numbers. Unlike pseudorandom number generators (PRNGs), which use algorithms and are inherently deterministic, true random number generators (TRNGs) can derive randomness from physical phenomena.

RANDOM.ORG leverages atmospheric noise to generate truly random numbers. It captures atmospheric noise using radios and translates the variations in the amplitude of this noise into a stream of random numbers. Unlike PRNGs, which rely on mathematical formulas, the numbers generated through atmospheric noise are not predetermined.

### Applications

- Cryptography: True random numbers are essential for generating secure encryption keys.
- Statistical sampling: Ensures unbiased sampling in surveys and experiments.
- Simulations: Used in Monte Carlo simulations to model complex systems and processes.

### Predictability

One challenge in using atmospheric noise for random number generation is the deterministic nature of the universe. Some argue that if we could perfectly understand and measure all the factors influencing atmospheric noise, it might become predictable. However, the complexity of weather systems and the difficulty in measuring variables like the position and velocity of every molecule make it practically impossible to predict these numbers. For this reason, even if atmospheric noise is theoretically predictable, it is practically unpredictable, making it useful for generating random numbers.
