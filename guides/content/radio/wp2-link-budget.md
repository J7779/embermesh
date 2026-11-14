---
id: radio/wp2-link-budget
title: Link budget
category: radio
tags: [link budget, radio]
summary: A link budget is an accounting of all of the power gains and losses that a communication signal experiences in a telecommunication system, from a transmitter, through a communication channel such as radio waves, cables, waveguides, or optical fibers, to the receiver.
source: https://en.wikipedia.org/w/index.php?title=Link_budget&oldid=1379736720
source_title: Wikipedia: Link budget
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Link budget", Wikipedia, The Free Encyclopedia (revision 1379736720), https://en.wikipedia.org/w/index.php?title=Link_budget&oldid=1379736720 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Link budget

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **link budget** is an accounting of all of the power gains and losses that a communication signal experiences in a telecommunication system, from a transmitter, through a communication channel such as radio waves, cables, waveguides, or optical fibers, to the receiver. It is an equation giving the received power from the transmitter power, after the attenuation of the transmitted signal due to propagation, as well as the antenna gains and feedline and other losses, and amplification of the signal in the receiver or any repeaters it passes through. A link budget is a design aid, calculated during the design of a communication system to determine the received power, to ensure that the information is received intelligibly with an adequate signal-to-noise ratio. In most real-world systems, the losses must be estimated to some degree, and may vary. A link margin is therefore specified as a safety margin between the received power and minimum power required by the receiver to accurately detect the signal. The link margin is chosen based on the anticipated severity of a communications dropout and can be reduced by the use of mitigating techniques such as antenna diversity or multiple-input and multiple-output (MIMO).

A simple link budget equation looks like this:

Received power (dBm) = transmitted power (dBm) + gains (dB) − losses (dB)

Power levels are expressed in (dBm), Power gains and losses are expressed in decibels (dB), which is a logarithmic measurement, so adding decibels is equivalent to multiplying the actual power ratios.

## In radio systems

A link budget equation including the key effects for a wireless radio transmission system, expressed logarithmically, might look like:

P_RX = P_TX + G_TX - L_TX - L_FS - L_M + G_RX - L_RX
where:
P_RX, received power (dBm)
P_TX, transmitter output power (dBm)
G_TX, transmitter antenna gain (dBi)
L_TX, transmitter losses (coax, connectors...) (dB)
L_FS, path loss (dB)
L_M, miscellaneous losses (fading margin, body loss, polarization mismatch, other losses, ...) (dB)
G_RX, receiver antenna gain (dBi)
L_RX, receiver losses (coax, connectors, ...) (dB)

The path loss is the loss due to propagation between the transmitting and receiving antennas and is usually the most significant contributor to the losses, and also the largest unknown. When transmitting through free space, it can be expressed in a dimensionless form by normalizing the distance to the wavelength:

L_FS(dB) = 20log_10(4πdistance over wavelength) (where distance and wavelength are in the same units)

When substituted into the link budget equation above, the result is the logarithmic form of the Friis transmission equation.

In some cases, it is convenient to consider the loss due to distance and wavelength separately, but in that case, it is important to keep track of which units are being used, as each choice involves a differing constant offset. Some examples are provided below.

L_FS (dB) ≈ 32.45 dB + 20 log10[frequency (MHz)] + 20 log10[distance (km)]
L_FS (dB) ≈ −27.55 dB + 20 log10[frequency (MHz)] + 20 log10[distance (m)]
L_FS (dB) ≈ 36.6 dB + 20 log10[frequency (MHz)] + 20 log10[distance (miles)]

These alternative forms can be derived by substituting wavelength with the ratio of propagation velocity (*c*, approximately 3×10^8 m/s) divided by frequency, and by inserting the proper conversion factors between km or miles and meters, and between MHz and Hz.

The gain of both the transmitting and receiving antennas is affected by the antenna's directivity. For example, antennas can be isotropic, omnidirectional, directional, or sectorial, depending on the way in which the antenna power is oriented.

- Isotropic antennas radiate power equally in all directions.
- Omnidirectional antennas distribute the power equally in every direction of a plane, so the radiation pattern has the shape of a sphere squeezed between two parallel flat surfaces. They are widely used in many applications, for instance in WiFi Access Points.
- Directional antennas concentrate the power in a specific direction, called the bore sight, and are widely used in point to point applications, like wireless bridges and satellite communications.
- Sector antennas concentrate the power in a wider region, typically embracing 45º, 60º, 90º or 120º. They are routinely deployed in Cellular towers.

### Line-of-sight vs non-line-of-sight transmission

For a line-of-sight (LOS) radio system, the path loss can be closely modeled by a single path through free space using the Friis transmission equation. This models the decrease in signal power as it spreads over an increasing area as it propagates, proportional to the square of the distance (geometric spreading) and the square of the frequency. This is a best case scenario, and additional losses are incurred in most radio links.

In non-line-of-sight (NLOS) links, diffraction and reflection losses are the most important since the direct path is not available. Building obstructions such as walls and ceilings cause propagation losses indoors to be significantly higher. This occurs because of a combination of attenuation by walls and ceilings, and blockage due to equipment, furniture, and even people.

- For example, a "2 by 4" wood stud wall with drywall on both sides results in about 6 dB loss per wall at 2.4 GHz.
- Older buildings may have even greater internal losses than new buildings due to materials and line of sight issues.

Experience has shown that in dense office environments, line-of-sight propagation holds only for about the first 3 meters. Beyond 3 meters propagation losses indoors can increase at up to 30 dB per 30 meters. This is a good rule-of-thumb, in that it is conservative (it overstates path loss in most cases). Actual propagation losses may vary significantly depending on building construction and layout.

The attenuation of the signal is highly dependent on the frequency of the signal.

### Further losses

In practical situations (deep space telecommunications, weak signal DXing etc.) other sources of signal loss must also be accounted for, including:

- The transmitting and receiving antennas may be partially cross-polarized.
- The cabling between the radios and antennas may introduce significant additional loss.
- Either antenna may have an impedance mismatch.
- Fresnel zone losses due to a partially obstructed line-of-sight path.
- Doppler shift induced signal power losses in the receiver.
- Atmospheric attenuation by gases, rain, fog and clouds.
- Fading due to variations of the channel.
- Multipath losses.
- Antenna misalignment.

### Earth–Moon–Earth communications

Link budgets are important in Earth–Moon–Earth communication. As the albedo of the Moon is very low (maximally 12% but usually closer to 7%), and the path loss over the 770,000 kilometre return distance is extreme (around 250 to 310 dB depending on VHF-UHF band used, modulation format and Doppler shift effects), high power (more than 100 watts) and high-gain antennas (more than 20 dB) must be used.

- In practice, this limits the use of this technique to the spectrum at VHF and above.
- The Moon must be above the horizon in order for EME communications to be possible.

### Voyager program

The Voyager program spacecraft have the highest known path loss (308 dB as of 2002) and lowest link budgets of any telecommunication circuit. The Deep Space Network has been able to maintain the link at a higher than expected bitrate through a series of improvements, such as increasing the antenna size from 64 m to 70 m for a 1.2 dB gain, and upgrading to low noise electronics for a 0.5 dB gain in 2000–2001. During the Neptune flyby, in addition to the 70-m antenna, two 34-m antennas and twenty-seven 25-m antennas were used to increase the gain by 5.6 dB, providing additional link margin to be used for a 4× increase in bitrate.

## In waveguides and cables

Guided media such as coaxial and twisted pair electrical cable and radio frequency waveguides have losses that are exponential with distance.

The path loss will be in terms of dB per unit distance. This means that there is always a crossover distance beyond which the loss in a guided medium will exceed that of a line-of-sight path of the same length.

_(Later sections of the original article were left out of this offline copy for length: In optical communications.)_
