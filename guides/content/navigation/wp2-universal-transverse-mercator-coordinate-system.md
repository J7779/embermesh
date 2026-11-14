---
id: navigation/wp2-universal-transverse-mercator-coordinate-system
title: Universal Transverse Mercator coordinate system
category: navigation
tags: [universal transverse mercator coordinate system, universal transverse mercator, utm, navigation]
summary: Universal Transverse Mercator (UTM) is a system for specifying locations on the Earth, based on transverse Mercator projections.
source: https://en.wikipedia.org/w/index.php?title=Universal_Transverse_Mercator_coordinate_system&oldid=1381101134
source_title: Wikipedia: Universal Transverse Mercator coordinate system
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Universal Transverse Mercator coordinate system", Wikipedia, The Free Encyclopedia (revision 1381101134), https://en.wikipedia.org/w/index.php?title=Universal_Transverse_Mercator_coordinate_system&oldid=1381101134 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Universal Transverse Mercator coordinate system

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Universal Transverse Mercator** (**UTM**) is a system for specifying locations on the Earth, based on *transverse Mercator* projections. It covers the part of the Earth between 84° N and 80° S, dividing it into 60 zones, each adopting a separate projection and offset by a false origin. Thus, specifying a location in UTM requires specifying the zone and the hemisphere (north or south), not only the *x* and *y* plane coordinates within the zone and hemisphere.

Each UTM zone is a meridional strip that extends from 84° N to 80° S and spans 6 degrees of longitude, with a few exceptions. By using such narrow zones, and setting a scale factor of 0.9996 for the corresponding central meridians, UTM limits linear distortion to about 1 part in 1,000 within the bounds of each zone. UTM coordinates are used in many nations and regions for topographic mapping and for pinpointing locations.

## Definitions

### UTM zone

The UTM system divides the Earth into 60 zones, each 6° of longitude in width. Zone 1 covers longitude 180° to 174° W; zone numbering increases eastward to zone 60, which covers longitude 174°E to 180°. The polar regions south of 80°S and north of 84°N are excluded, and instead covered by the universal polar stereographic (UPS) coordinate system.

Each of the 60 zones uses a transverse Mercator projection that can map a region of large north-south extent with low distortion. By using narrow zones of 6° of longitude (up to 668 km) in width, and reducing the scale factor along the central meridian to 0.9996 (a reduction of 1:2500), the amount of distortion is held below 1 part in 1,000 inside each zone. Distortion of scale increases to 1.0010 at the zone boundaries along the equator.

In each zone the scale factor of the central meridian reduces the diameter of the transverse cylinder to produce a secant projection with two standard lines, or lines of true scale, about 180 km on each side of, and about parallel to, the central meridian (Arc cos 0.9996 = 1.62° at the Equator). The scale is less than 1 inside the standard lines and greater than 1 outside them, but the overall distortion is minimized.

### Exceptions

The UTM zones are uniform across the globe, except in two areas. On the southwest coast of Norway, zone 32 is extended 3° further west, and zone 31 is correspondingly shrunk to cover only open water. Also, in the region around Svalbard, the zones 32, 34 and 36 are not used, while zones 31 (9° wide), 33 (12° wide), 35 (12° wide), and 37 (9° wide) are extended to cover the gaps.

### Overlapping grids

Distortion of scale increases in each UTM zone as the boundaries between the UTM zones are approached. However, it is often convenient or necessary to measure a series of locations on a single grid when some are located in two adjacent zones. Around the boundaries of large scale maps (1:100,000 or larger) coordinates for both adjoining UTM zones are usually printed within a minimum distance of 40 km on either side of a zone boundary. Ideally, the coordinates of each position should be measured on the grid for the zone in which they are located, but because the scale factor is still relatively small near zone boundaries, it is possible to overlap measurements into an adjoining zone for some distance when necessary.

### Latitude bands

Latitude bands are not a part of UTM, but rather a part of the military grid reference system (MGRS). They are however sometimes included in UTM notation. Including latitude bands in UTM notation can lead to ambiguous coordinates—as the letter "S" either refers to the southern hemisphere or a latitude band in the northern hemisphere—and should therefore be avoided.

## Locating a position using UTM coordinates

A position on the Earth is given by the UTM zone number and hemisphere designator and the easting and northing planar coordinate pair in that zone.

The natural point of origin of each UTM zone is the intersection of the equator and the zone's central meridian. To avoid dealing with negative numbers, a false Easting of -500000 meters is added to the central meridian. Thus a point that has an easting of 400000 meters is about 100 km west of the central meridian. For most such points, the true distance would be slightly more than 100 km as measured on the surface of the Earth because of the distortion of the projection. UTM eastings range from about 166000 meters to 834000 meters at the equator.

In the Northern Hemisphere, positions are measured northward from zero at the equator. The maximum "northing" value is about 9300000 meters at latitude 84 degrees North, the north end of the UTM zones. In the Southern Hemisphere, the equator is set at a false northing of 10000000 meters. Northings decrease southward from these 10000000 meters to about 1100000 meters at 80 degrees South, the south end of the UTM zones. Therefore, no point has a negative northing value.

For example, the CN Tower is at, which is in UTM zone 17, and the grid position is 630084 m east, 4833438 m north. Two points in Zone 17 have these coordinates, one in the Northern Hemisphere and one in the south; the non-ambiguous format is to specify the full zone and hemisphere designator, that is, "17N 630084 m 4833438 m".

### Simplified formulae

These formulae are truncated version of Transverse Mercator: flattening series, which were originally derived by Johann Heinrich Louis Krüger in 1912. They are accurate to around a millimeter within 3000 km of the central meridian. Concise commentaries for their derivation have also been given.

The WGS 84 spatial reference system describes Earth as an oblate spheroid along north-south axis with an equatorial radius of a=6378.137 km and an inverse flattening of 1/f=298.257 223 563. Taking a point of latitude φ and of longitude λ and computing its UTM coordinates as well as point scale factor k and meridian convergence γ using a reference meridian of longitude λ_0. By convention, in the Northern Hemisphere N_0=0 km and in the Southern Hemisphere N_0=10000 km. By convention also k_0=0.9996 and E_0=500 km.

In the following formulas, the distances are in kilometers and angles are in radians. First, here are some preliminary values:

n =(f)/(2-f), A =(a)/(1+n)(1+ (n^2)/(4) +(n^4)/(64)), \
α_1 =1/2n-2/3n^2+5/16n^3, α_2 =13/48n^2-3/5n^3, α_3 =61/240n^3,; [12pt] β_1 =1/2n-2/3n^2+37/96n^3, β_2 =1/48n^2+1/15n^3, β_3 =17/480n^3,; [12pt] δ_1 =2n-2/3n^2-2n^3, δ_2 =7/3n^2-8/5n^3, δ_3 =56/15n^3.

#### From latitude, longitude (φ, λ) to UTM coordinates (E, N)

First we compute some intermediate values:

t=sinh(tanh^-1(sinφ)-(2sqrt n)/(1+n)tanh^-1((2sqrt n)/(1+n) sinφ)),

xi'=tan^-1((t)/(cos(λ-λ_0))), η'=tanh^-1((sin(λ-λ_0))/(√(1+t^2))),

σ=1+Σ_j=1^3 2jα_jcos(2jxi')cosh(2jη'), τ=Σ_j=1^3 2j α_j sin(2jxi') sinh(2jη').

The final formulae are:

E=E_0+k_0 A(η'+Σ_j=1^3 α_jcos(2jxi')sinh(2jη')),

N=N_0+k_0 A(xi'+Σ_j=1^3 α_jsin(2jxi')cosh(2jη')),

k=(k_0 A)/(a)sqrt\1+((1-n)/(1+n) tanφ)^2\ (σ^2+τ^2)/(t^2+cos^2(λ-λ_0)),

γ=tan^-1((τ√(1+t^2)+σ ttan(λ-λ_0))/(σ√(1+t^2)-τ ttan(λ-λ_0))).

where E is Easting, N is Northing, k is the Scale Factor, and γ is the Grid Convergence.

#### From UTM coordinates (E, N, Zone, Hemi) to latitude, longitude (φ, λ)

Note: Hemi = +1 for Northern, Hemi = −1 for Southern

First let's compute some intermediate values:

xi=(N-N_0)/(k_0 A), η=(E-E_0)/(k_0 A),

xi'=xi-Σ_j=1^3β_jsin(2jxi)cosh(2jη), η'=η-Σ_j=1^3β_jcos(2jxi)sinh(2jη),

σ'=1-Σ_j=1^32jβ_jcos(2jxi)cosh(2jη), τ'=Σ_j=1^32jβ_jsin(2jxi)sinh(2jη),

chi=sin^-1((sinxi')/(coshη')).

The final formulae are:

φ=chi+Σ_j=1^3δ_jsin(2jchi),

λ_0=Zone× 6^° - 183^°

λ=λ_0+tan^-1((sinhη')/(cosxi')),

k=(k_0 A)/(a)sqrt\1+((1-n)/(1+n)tanφ)^2\(cos^2xi'+sinh^2η')/(σ'^2+τ'^2),

γ=Hemi×tan^-1((τ'+σ'tanxi'tanhη')/(σ'-τ'tanxi'tanhη')).

_(Later sections of the original article were left out of this offline copy for length: Map distortion within UTM zones.)_
