---
id: navigation/wp2-equation-of-time
title: Equation of time
category: navigation
tags: [equation of time, navigation]
summary: The equation of time describes the discrepancy between two kinds of solar time.
source: https://en.wikipedia.org/w/index.php?title=Equation_of_time&oldid=1381509323
source_title: Wikipedia: Equation of time
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Equation of time", Wikipedia, The Free Encyclopedia (revision 1381509323), https://en.wikipedia.org/w/index.php?title=Equation_of_time&oldid=1381509323 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Equation of time

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **equation of time** describes the discrepancy between two kinds of solar time. The two times that differ are the apparent solar time, which directly tracks the diurnal motion of the Sun, and mean solar time, which tracks a theoretical mean Sun with uniform motion along the celestial equator. Apparent solar time can be obtained by measurement of the current position (hour angle) of the Sun, as indicated (with limited accuracy) by a sundial. *Mean* solar time, for the same place, would be the time indicated by a steady clock set so that over the year its differences from apparent solar time would have a mean of zero.

The equation of time is the east or west component of the analemma, a curve representing the angular offset of the Sun from its mean position on the celestial sphere as viewed from Earth. The equation of time values for each day of the year, compiled by astronomical observatories, were widely listed in almanacs and ephemerides.

The equation of time can be approximated by a sum of two sine waves:

Δ t_ey = -7.659sin(D) + 9.863sin (2D + 3.5932 ) [minutes]

where: D = 6.240 040 77 + 0.017 201 97(365.25(y-2000) + d)

where d represents the number of days since 1 January of the current year, y.

## Concept

During a year the equation of time varies as shown on the graph; its change from one year to the next is slight. Apparent time, and the sundial, can be ahead (fast) by as much as 16 min 33 s (around 3 November), or behind (slow) by as much as 14 min 6 s (around 11 February). The equation of time has zeros near 15 April, 13 June, 1 September, and 25 December. Ignoring very slow changes in the Earth's orbit and rotation, these events are repeated at the same times every tropical year. However, due to the non-integral number of days in a year, these dates can vary by a day or so from year to year. As an example of the inexactness of the dates, according to the U.S. Naval Observatory's *Multiyear Interactive Computer Almanac* the equation of time was zero at 02:00 UT1 on 16 April 2011.

The graph of the equation of time is closely approximated by the sum of two sine curves, one with a period of a year and one with a period of half a year. The curves reflect two astronomical effects, each causing a different non-uniformity in the apparent daily motion of the Sun relative to the stars:

- the obliquity of the ecliptic (the plane of the Earth's annual orbital motion around the Sun), which is inclined by about 23.44 degrees relative to the plane of the Earth's equator; and
- the eccentricity of the Earth's orbit around the Sun, which is about 0.0167.

The equation of time vanishes only for a planet with zero axial tilt and zero orbital eccentricity. Two examples of planets with large equations of time are Mars and Uranus. On Mars the difference between sundial time and clock time can be as much as 50 minutes, due to the considerably greater eccentricity of its orbit. The planet Uranus, which has an extremely large axial tilt, has an equation of time that makes its days start and finish several hours earlier or later depending on where it is in its orbit.

## Notation

The United States Naval Observatory states "the Equation of Time is the difference *apparent solar time* minus *mean solar time*", i.e. if the sun is ahead of the clock the sign is positive, and if the clock is ahead of the sun the sign is negative. The equation of time is shown in the upper graph above for a period of slightly more than a year. The lower graph (which covers exactly one calendar year) has the same absolute values but the sign is reversed as it shows how far the clock is ahead of the sun. Publications may use either format: in the English-speaking world, the former usage is the more common, but is not always followed. Anyone who makes use of a published table or graph should first check its sign usage. Often, there is a note or caption which explains it. Otherwise, the usage can be determined by knowing that, during the first three months of each year, the clock is ahead of the sundial. The mnemonic "NYSS" (pronounced "nice"), for "new year, sundial slow", can be useful. Some published tables avoid the ambiguity by not using signs, but by showing phrases such as "sundial fast" or "sundial slow" instead.

## Major components

### Eccentricity of the Earth's orbit

The Earth revolves around the Sun. As seen from Earth, the Sun appears to revolve once around the Earth through the background stars in one year. If the Earth orbited the Sun with a constant speed, in a circular orbit in a plane perpendicular to the Earth's axis, then the Sun would culminate every day at exactly the same time, and be a perfect time keeper (except for the very small effect of the slowing rotation of the Earth). But the orbit of the Earth is an ellipse not centered on the Sun, and its speed varies between 30.287 and 29.291 km/s, according to Kepler's laws of planetary motion, and its angular speed also varies, and thus the Sun appears to move faster (relative to the background stars) at perihelion (currently around 3 January) and slower at aphelion a half year later.

At these extreme points, this effect varies the apparent solar day by 7.9 s/day from its mean. Consequently, the smaller daily differences on other days in speed are cumulative until these points, reflecting how the planet accelerates and decelerates compared to the mean.

As a result, the eccentricity of the Earth's orbit contributes a periodic variation which is (in the first-order approximation) a sine wave with:

- amplitude: 7.66 minutes
- period: one year
- zero points: perihelion (at the beginning of January) and aphelion (beginning of July)
- extreme values: early April (negative) and early October (positive)

This component of the EoT is represented by aforementioned factor *a*:
a = -7.659sin(6.240 040 77 + 0.017 201 97(365(y-2000) + d))

### Obliquity of the ecliptic

Even if the Earth's orbit were circular, the perceived motion of the Sun along our celestial equator would still not be uniform. This is a consequence of the tilt of the Earth's rotational axis with respect to the plane of its orbit, or equivalently, the tilt of the ecliptic (the path the Sun appears to take in the celestial sphere) with respect to the celestial equator. The projection of this motion onto our celestial equator, along which "clock time" is measured, is a maximum at the solstices, when the yearly movement of the Sun is parallel to the equator (causing amplification of perceived speed) and yields mainly a change in right ascension. It is a minimum at the equinoxes, when the Sun's apparent motion is more sloped and yields more change in declination, leaving less for the component in right ascension, which is the only component that affects the duration of the solar day. A practical illustration of obliquity is that the daily shift of the shadow cast by the Sun in a sundial even on the equator is smaller close to the solstices and greater close to the equinoxes. If this effect operated alone, then days would be up to 24 hours and 20.3 seconds long (measured solar noon to solar noon) near the solstices, and as much as 20.3 seconds shorter than 24 hours near the equinoxes.

In the figure on the right, we can see the monthly variation of the apparent slope of the plane of the ecliptic at solar midday as seen from Earth. This variation is due to the apparent precession of the rotating Earth through the year, as seen from the Sun at solar midday.

In terms of the equation of time, the inclination of the ecliptic results in the contribution of a sine wave variation with:

- amplitude: 9.87 minutes
- period: 1/2 year
- zero points: equinoxes and solstices
- extreme values: beginning of February and August (negative) and beginning of May and November (positive).

This component of the EoT is represented by the aforementioned factor "b":

b = 9.863sin ( 2 (6.240 040 77 + 0.017 201 97 (365(y-2000)+ d)) + 3.5932 )

_(Later sections of the original article were left out of this offline copy for length: Secular effects, Practical use, Calculation.)_
