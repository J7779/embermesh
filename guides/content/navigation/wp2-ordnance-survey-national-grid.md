---
id: navigation/wp2-ordnance-survey-national-grid
title: Ordnance Survey National Grid
category: navigation
tags: [ordnance survey national grid, osgb, british national grid, bng, navigation]
summary: The Ordnance Survey National Grid reference system (OSGB), also known as British National Grid (BNG), is a system of geographic grid references, distinct from latitude and longitude, whereby any location in Great Britain can be described in terms of its distance from the origin (0, 0)…
source: https://en.wikipedia.org/w/index.php?title=Ordnance_Survey_National_Grid&oldid=1379996376
source_title: Wikipedia: Ordnance Survey National Grid
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Ordnance Survey National Grid", Wikipedia, The Free Encyclopedia (revision 1379996376), https://en.wikipedia.org/w/index.php?title=Ordnance_Survey_National_Grid&oldid=1379996376 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Ordnance Survey National Grid

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **Ordnance Survey National Grid reference system** (**OSGB**), also known as **British National Grid** (**BNG**), is a system of geographic grid references, distinct from latitude and longitude, whereby any location in Great Britain can be described in terms of its distance from the origin (0, 0), which lies to the west of the Isles of Scilly.

Devised by the Ordnance Survey (OS), the national grid reference system is heavily used in OS survey data and in maps based on those surveys, whether published by the Ordnance Survey or by commercial map producers. Grid references are also commonly quoted in other publications and data sources, such as guide books and government planning documents.

A number of different systems exist that can provide grid references for locations within the British Isles: this article describes the system created solely for Great Britain and its outlying islands (including the Isle of Man). The Irish grid reference system is a similar system created by the Ordnance Survey of Ireland and the Ordnance Survey of Northern Ireland for the island of Ireland. The Irish Transverse Mercator (ITM) coordinate reference system was adopted in 2001 and is now the preferred coordinate reference system across Ireland. ITM is based on the Universal Transverse Mercator coordinate system (UTM), used to provide grid references for worldwide locations, and this is the system commonly used for the Channel Islands. European-wide agencies also use UTM when mapping locations, or may use the Military Grid Reference System (MGRS), or variants of it.

## Grid letters

The first letter of the British National Grid is derived from a larger set of 25 squares of size 500 km by 500 km, labelled A to Z, omitting one letter (I) (refer diagram below), previously used as a military grid. Four of these largest squares contain significant land area within Great Britain: S, T, N and H. The O square contains a tiny area of North Yorkshire, Beast Cliff at, almost all of which lies below mean high tide.
For the second letter, each 500 km square is subdivided into 25 squares of size 100 km by 100 km, each with a letter code from A to Z (again omitting I) starting with A in the north-west corner to Z in the south-east corner. These squares are outlined in light grey on the "100km squares" map, with those containing land lettered. The central (2° W) meridian is shown in red.

## Grid digits

Within each square, *eastings* and *northings* from the south west corner of the square are given numerically. For example, NH0325 means a 1 km square whose south-west corner is 3 km east and 25 km north from the south-west corner of square NH. A location can be indicated to varying resolutions numerically, usually from two digits in each coordinate (for a 1 km square) through to five (for a 1 m square); in each case the first half of the digits is for the first coordinate and the second half for the other. The most common usage is the *six figure grid reference*, employing three digits in each coordinate to determine a 100 m square. For example, the grid reference of the 100 m square containing the summit of Ben Nevis is. (Grid references may be written with or without spaces; e.g., also NN166712.) NN has an easting of 200 km and northing of 700 km, so the OSGB36 National Grid location for Ben Nevis is at 216600, 771200.

## All-numeric grid references

Grid references may also be quoted as a pair of numbers: eastings then northings in metres, measured from the south-west corner of the SV square. Thirteen digits may be required for locations in Orkney and further north. For example, the grid reference for Sullom Voe Oil Terminal in the Shetland Islands may be given as or 439668,1175316.

Another, distinct, form of all-numeric grid reference is an abbreviated alphanumeric reference where the letters are simply omitted, e.g. 166712 for the summit of Ben Nevis. Unlike the numeric references described above, this abbreviated grid reference is incomplete; it gives the location relative to an OS 100×100 km square, but does not specify which square. It is often used informally when the context identifies the OS 2-letter square. For example, within the context of a location known to be on OS Landranger sheet 41 (which extends from NN000500 in the south-west to NN400900 in the north-east) the abbreviated grid reference 166712 is equivalent to NN166712. If working with more than one Landranger sheet, this may also be given as 41/166712.

Alternatively, numbers are sometimes used instead of the two-letter combinations for the 100×100 km squares. The numbering follows a grid index where the tens denote the progress from West to East and the units from South to North. In the north of Scotland, the numbering is modified: the 100 km square to the north of 39 is numbered N30; the square to the north of 49 is N40, etc.

## Compatibility with related systems

The grid is based on the **OSGB36 datum** (Ordnance Survey Great Britain 1936). The datum was introduced after the retriangulation of 1936–1962. It replaced the Cassini Grid which had previously been the standard projection for Ordnance Survey maps.
A datum transformation exists between GRS36 and more recent geocentric frames (see below).

OSGB36 is based on the **Airy ellipsoid**, a reference ellipsoid named after George Biddell Airy. Introduced in 1830, it is a best fit for the Britain region. More modern mapping tends to use the GRS80 or WGS 84 ellipsoid, as used by the Global Positioning System. The Airy ellipsoid assumes the Earth to be about 1 km smaller in diameter than the global/world ellipsoid, and to be slightly less flattened.

The British maps adopt a transverse Mercator projection with an origin (the "true" origin) at 49° N, 2° W (an offshore point in the English Channel which lies between the island of Jersey and the French port of Saint-Malo). Over the Airy ellipsoid a straight line grid, the National Grid, is placed with a new false origin to eliminate negative numbers, creating a 700 km by 1300 km grid. This false origin is located south-west of the Isles of Scilly.

In order to minimize the overall scale error, a factor of 2499/2500 is applied. This creates two secant lines of longitude about 180 km east and west of the central meridian along which the local scale factor equals 1, i.e. map scale is correct. Inside these lines the local scale factor is less than 1, with a minimum of 0.04% too small at the central meridian. Outside these lines the local scale factor is greater than 1, and is about 0.04% too large near the east and west coasts. Grid north and true north are only aligned on the central meridian (400 km easting) of the grid which is 2° W (OSGB36) and approx. 2° 0′ 5″ W (WGS 84).

A geodetic transformation between OSGB36 and other terrestrial reference systems (like ITRF2000, ETRS89, or WGS 84) can become quite tedious if attempted manually. The most common transformation is called the Helmert datum transformation, which results in a typical 7 m error. The definitive transformation from ETRS89 that is published by the Ordnance Survey is called the National Grid Transformation OSTN15. This models the detailed distortions in the 1936–1962 retriangulation, and achieves backwards compatibility in grid coordinates to sub-metre accuracy.

### Datum shift between OSGB 36 and WGS 84

The difference between the coordinates on different datums varies from place to place. The longitude and latitude positions on OSGB 36 are the same as for WGS 84 at a point in the Atlantic Ocean well to the west of Great Britain. In Cornwall, the WGS 84 longitude meridians are about 70 metres east of their OSGB 36 equivalents, this value rising gradually to about 120 m east on the east coast of East Anglia. The WGS 84 latitude parallels are about 70 m south of the OSGB 36 lines in South Cornwall, the difference diminishing to zero in the Scottish Borders, and then increasing to about 50 m north on the north coast of Scotland. The smallest datum shift is on the west coast of Scotland and the greatest in Kent.

### Datum shift between OSGB 36 and ED50

These two datums are not both in general use in any one place, but for a point in the English Channel halfway between Dover and Calais, the ED50 longitude lines are about 20 m east of the OSGB36 equivalents, and the ED50 latitude lines are about 150 m south of the OSGB36 ones.

_(Later sections of the original article were left out of this offline copy for length: Summary parameters of the coordinate system.)_
