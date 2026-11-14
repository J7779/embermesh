---
id: radio/line-of-sight-propagation
title: Line-of-sight propagation
category: radio
tags: [radio, propagation, VHF, UHF, radio horizon]
summary: "Why VHF/UHF range is set by the radio horizon, and how antenna height extends it."
source: "https://en.wikipedia.org/w/index.php?title=Line-of-sight_propagation&oldid=1339192660"
source_title: Line-of-sight propagation (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Line-of-sight propagation' (rev 1339192660), Wikipedia, https://en.wikipedia.org/wiki/Line-of-sight_propagation. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# Line-of-sight propagation

Line-of-sight propagation is a characteristic of electromagnetic radiation or acoustic wave propagation which means waves can only travel in a direct visual path from the source to the receiver without obstacles. Electromagnetic transmission includes light emissions traveling in a straight line. The rays or waves may be diffracted, refracted, reflected, or absorbed by the atmosphere and obstructions with material and generally cannot travel over the horizon or behind obstacles.

In contrast to line-of-sight propagation, at low frequency (below approximately 3 MHz) due to diffraction, radio waves can travel as ground waves, which follow the contour of the Earth. This enables AM radio stations to transmit beyond the horizon. Additionally, frequencies in the shortwave bands between approximately 1 and 30 MHz, can be refracted back to Earth by the ionosphere, called skywave or "skip" propagation, thus giving radio transmissions in this range a potentially global reach.

However, at frequencies above 30 MHz (VHF and higher) and in lower levels of the atmosphere, neither of these effects are significant. Thus, any obstruction between the transmitting antenna (transmitter) and the receiving antenna (receiver) will block the signal, just like the light that the eye may sense. Therefore, since the ability to visually see a transmitting antenna (disregarding the limitations of the eye's resolution) roughly corresponds to the ability to receive a radio signal from it, the propagation characteristic at these frequencies is called "line-of-sight". The farthest possible point of propagation is referred to as the "radio horizon".

In practice, the propagation characteristics of these radio waves vary substantially depending on the exact frequency and the strength of the transmitted signal (a function of both the transmitter and the antenna characteristics). Broadcast FM radio, at comparatively low frequencies of around 100 MHz, are less affected by the presence of buildings and forests.

## Impairments to line-of-sight propagation

Low-powered microwave transmitters can be foiled by tree branches, or even heavy rain or snow. The presence of objects not in the direct line-of-sight can cause diffraction effects that disrupt radio transmissions. For the best propagation, a volume known as the first Fresnel zone should be free of obstructions.

Reflected radiation from the surface of the surrounding ground or salt water can also either cancel out or enhance the direct signal. This effect can be reduced by raising either or both antennas further from the ground: The reduction in loss achieved is known as *height gain*.

See also Non-line-of-sight propagation for more on impairments in propagation.

It is important to take into account the curvature of the Earth for calculation of line-of-sight paths from maps, when a direct visual fix cannot be made. Designs for microwave formerly used 4⁄3 Earth radius to compute clearances along the path.

## Mobile telephones

Although the frequencies used by mobile phones (cell phones) are in the line-of-sight range, they still function in cities. This is made possible by a combination of the following effects:

- 1⁄*r*^4 propagation over the rooftop landscape
- diffraction into the "street canyon" below
- multipath reflection along the street
- diffraction through windows, and attenuated passage through walls, into the building
- reflection, diffraction, and attenuated passage through internal walls, floors and ceilings within the building

The combination of all these effects makes the mobile phone propagation environment highly complex, with multipath effects and extensive Rayleigh fading. For mobile phone services, these problems are tackled using:

- rooftop or hilltop positioning of base stations
- many base stations (usually called "cell sites"). A phone can typically see at least three, and usually as many as six at any given time.
- "sectorized" antennas at the base stations. Instead of one antenna with omnidirectional coverage, the station may use as few as 3 (rural areas with few customers) or as many as 32 separate antennas, each covering a portion of the circular coverage. This allows the base station to use a directional antenna that is pointing at the user, which improves the signal-to-noise ratio. If the user moves (perhaps by walking or driving) from one antenna sector to another, the base station automatically selects the proper antenna.
- rapid handoff between base stations (roaming)
- the radio link used by the phones is a digital link with extensive error correction and detection in the digital protocol
- sufficient operation of mobile phone in tunnels when supported by split cable antennas
- local repeaters inside complex vehicles or buildings

A Faraday cage is composed of a conductor that completely surrounds an area on all sides, top, and bottom. Electromagnetic radiation is blocked where the wavelength is longer than any gaps. For example, mobile telephone signals are blocked in windowless metal enclosures that approximate a Faraday cage, such as elevator cabins, and parts of trains, cars, and ships. The same problem can affect signals in buildings with extensive steel reinforcement.

## Radio horizon

The *radio horizon* is the locus of points at which direct rays from an antenna are tangential to the surface of the Earth. If the Earth were a perfect sphere without an atmosphere, the radio horizon would be a circle.

The radio horizon of the transmitting and receiving antennas can be added together to increase the effective communication range.

Radio wave propagation is affected by atmospheric conditions, ionospheric absorption, and the presence of obstructions, for example mountains or trees. Simple formulas that include the effect of the atmosphere give the range as:

`horizon_(mi) ≈ 1.23⋅sqrt(height_(feet))`

`horizon_(km) ≈ 3.57⋅sqrt(height_(metres))`

The simple formulas give a best-case approximation of the maximum propagation distance, but are not sufficient to estimate the quality of service at any location.

### Earth bulge

In telecommunications, Earth bulge refers to the effect of earth's curvature on radio propagation. It is a consequence of a circular segment of earth profile that blocks off long-distance communications. Since the vacuum line of sight passes at varying heights over the Earth, the propagating radio wave encounters slightly different propagation conditions over the path.

### Vacuum distance to horizon

Assuming a perfect sphere with no terrain irregularity, the distance to the horizon from a high altitude transmitter (i.e., line of sight) can readily be calculated.

Let *R* be the radius of the Earth and *h* be the altitude of a telecommunication station. The line of sight distance *d* of this station is given by the Pythagorean theorem;

`d^2 = (R + h)^2 − R^2 = 2⋅R⋅h + h^2`

The altitude of the station *h* is much smaller than the radius of the Earth *R.* Therefore, `h^2` can be neglected compared with `2⋅R⋅h`.

Thus:

`d ≈ sqrt(2⋅R⋅h)`

If the height *h* is given in metres, and distance *d* in kilometres,

`d ≈ 3.57⋅sqrt(h)`

If the height *h* is given in feet, and the distance *d* in statute miles,

`d ≈ 1.23⋅sqrt(h)`

In the case, when there are two stations involve, e.g. a transmit station on ground with a station height *h* and a receive station in the air with a station height *H*, the line of sight distance can be calculated as follows:

```
 `d ≈ sqrt(2R) (sqrt(h) + sqrt(H))`
```

### Atmospheric refraction

The usual effect of the declining pressure of the atmosphere with height (vertical pressure variation) is to bend (refract) radio waves down towards the surface of the Earth. This results in an effective Earth radius, increased by a factor around 4⁄3. This *k*-factor can change from its average value depending on weather.

#### Refracted distance to horizon

The previous vacuum distance analysis does not consider the effect of atmosphere on the propagation path of RF signals. In fact, RF signals do not propagate in straight lines: Because of the refractive effects of atmospheric layers, the propagation paths are somewhat curved. Thus, the maximum service range of the station is not equal to the line of sight vacuum distance. Usually, a factor *k* is used in the equation above, modified to be

`d ≈ sqrt(2⋅k⋅R⋅h)`

*k* > 1 means geometrically reduced bulge and a longer service range. On the other hand, *k* < 1 means a shorter service range.

Under normal weather conditions, *k* is usually chosen to be 4⁄3. That means that the maximum service range increases by 15%.

`d ≈ 4.12⋅sqrt(h)`

for *h* in metres and *d* in kilometres; or

`d ≈ 1.41⋅sqrt(h)`

for *h* in feet and *d* in miles.

But in stormy weather, *k* may decrease to cause fading in transmission. (In extreme cases *k* can be less than 1.) That is equivalent to a hypothetical decrease in Earth radius and an increase of Earth bulge.

For example, in normal weather conditions, the service range of a station at an altitude of 1500 m with respect to receivers at sea level can be found as,

`d ≈ 4.12⋅sqrt(1500) = 160km.`
