---
id: navigation/wp2-military-grid-reference-system
title: Military Grid Reference System
category: navigation
tags: [military grid reference system, navigation]
summary: The Military Grid Reference System (MGRS) is the geocoordinate standard used by NATO militaries for geo-referencing, position reporting, and situational awareness during land operations.
source: https://en.wikipedia.org/w/index.php?title=Military_Grid_Reference_System&oldid=1379994581
source_title: Wikipedia: Military Grid Reference System
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Military Grid Reference System", Wikipedia, The Free Encyclopedia (revision 1379994581), https://en.wikipedia.org/w/index.php?title=Military_Grid_Reference_System&oldid=1379994581 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Military Grid Reference System

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **Military Grid Reference System** (*MGRS*) is the geocoordinate standard used by NATO militaries for geo-referencing, position reporting, and situational awareness during land operations. An MGRS coordinate does not represent a single point, but rather identifies a square area (which can be as small as 1 m x 1 m) on the Earth's surface. The MGRS is derived from the Universal Transverse Mercator (UTM) and Universal Polar Stereographic (UPS) grid systems and is used as a geocode system for the entire Earth.

An example of an *MGRS coordinate*, or *grid reference*, is [21_24_35.43062452380_N_157_54_57.89102787040_W_&language=en `4Q FJ 1234 6789`], which consists of three parts:

- `4Q` (grid zone designator, GZD)
- `FJ` (the 100,000-meter square identifier)
- `1234 6789` (numerical location; easting is `1234` and northing is `6789`, in this case specifying a location with 10 m resolution)

For machine-readability and database storage, all spaces may be removed.

An MGRS coordinate references a square or polygon on the Earth with a side length of 10 km, 1 km, 100 m, 10 m or 1 m, depending on the precision of the coordinates provided. (Certain squares adjacent to a Grid Zone Junction (GZJ) are clipped at an angle, resulting in a polygonal shape.)

**MGRS precision levels**

| MGRS Coordinate | Precision (side length) |
|---|---|
| `4Q FJ` | 100,000-meter square |
| `4Q FJ 1 6` | 10,000-meter square |
| `4Q FJ 12 67` | 1,000-meter square |
| `4Q FJ 123 678` | 100-meter square |
| `4Q FJ 1234 6789` | 10-meter square |
| `4Q FJ 12345 67890` | 1-meter square |

The number of digits in the numerical location must be even: 0, 2, 4, 6, 8 or 10, depending on the desired precision. When changing precision levels, it is important to truncate rather than round the easting and northing values to ensure the more precise square will remain within the boundaries of the less precise square.

Related to this is the primacy of the southwest corner of the square being the labeling point for the entire square. (In instances where the polygon is not a square and has been clipped by a grid zone junction, the polygon keeps the label of the southwest corner as if it had not been clipped.)

Google Maps recognizes MGRS grid references with one-meter square (10-digit numerical location) with spaces permitted only between the grid zone, the easting, and the northing (e.g., `4QFJ 12345 67890`), and returns a dropped pin representing the centroid of the area referenced.

## Grid zone designation

The first part of an MGRS coordinate is the *grid-zone designation*. The 6° wide UTM zones, numbered 1–60, are intersected by latitude bands that are normally 8° high, lettered C–X (omitting I and O). The northmost latitude band, X, is 12° high. The intersection of a UTM zone and a latitude band is (normally) a 6° × 8° polygon called a *grid zone*, whose *designation* in MGRS is formed by the zone number (one or two digits – the number for zones 1 to 9 is just a single digit, according to the example in DMA TM 8358.1, Section 3-2, Figure 7), followed by the latitude band letter (uppercase). This same notation is used in both UTM and MGRS, i.e. the UTM grid reference system; the article on Universal Transverse Mercator shows many maps of these grid zones, including the irregularities for Svalbard and southwest Norway.

As Figure 1 illustrates, Honolulu is in grid zone 4Q.

## 100,000-meter square identification

The second part of an MGRS coordinate is the *100,000-meter square identification*. Each UTM zone is divided into 100,000 meter squares, so that their corners have UTM-coordinates that are multiples of 100,000 meters. The *identification* consists of a column letter (A–Z, omitting I and O) followed by a row letter (A–V, omitting I and O).

Near the equator, the columns of UTM zone 1 have the letters A–H, the columns of UTM zone 2 have the letters J–R (omitting O), and the columns of UTM zone 3 have the letters S–Z. At zone 4, the column letters start over from A, and so on around the world.

For the row letters, there are actually two alternative lettering schemes within MGRS:

- In the *AA scheme*, also known as *MGRS-New*, which is used for WGS84 and some other modern geodetic datums, the letter for the first row – just north of the equator – is A in odd-numbered zones, and F in even-numbered zones, as shown in figure 1. Note that the westmost square in this row, in zone 1, has identification AA.
- In the alternative *AL scheme*, also known as *MGRS-Old*, which is used for some older geodetic datums, the row letters are shifted 10 steps in the alphabet. This means that the letter for the first row is L in odd-numbered zones and R in even-numbered zones. The westmost square in the first row, in zone 1, has identification AL.

If an MGRS coordinate is complete (with both a grid zone designation and a 100,000 meter square identification), and is valid in one lettering scheme, then it is usually invalid in the other scheme, which will have no such 100,000 meter square in the grid zone. (Latitude band X is the exception to this rule.) Therefore, a position reported in a modern datum usually cannot be misunderstood as using an old datum, and vice versa – provided the datums use different MGRS lettering schemes.

In the map (figure 1), which uses the AA scheme, we see that Honolulu is in grid zone 4Q, and square FJ. To give the position of Honolulu with 100 km resolution, we write 4QFJ.

## Numerical location

The third part of an MGRS coordinate is the *numerical location* within a 100,000 meter square, given as *n* + *n* digits, where *n* is 1, 2, 3, 4, or 5. If 5 + 5 digits is used, the first 5 digits give the easting in meters, measured from the left edge of the square, and the last 5 digits give the northing in meters, measured from the bottom edge of the square. The resolution in this case is 1 meter, so the MGRS coordinate would represent a 1-meter square, where the easting and northing are measured to its southwest corner. If a resolution of 10 meters is enough, the final digit of the easting and northing can be dropped, so that only 4 + 4 digits are used, representing a 10-meter square. If a 100-meter resolution is enough, 3 + 3 digits suffice; if a 1 km resolution is enough, 2 + 2 digits suffice; if 10 km resolution is enough, 1 + 1 digits suffice.
10 meter resolution (4 + 4 digits) is sufficient for many purposes, and is the NATO standard for specifying coordinates.

If we zoom in on Hawaii (figure 2), we see that the square that contains Honolulu, if we use 10 km resolution, would be written 4QFJ15.

If the grid zone or 100,000-meter square are clear from context, they can be dropped, and only the numerical location is specified. For example:

- If every position being located is within the same grid zone, only the 100,000-meter square and numerical location are specified.
- If every position being located is within the same grid zone and 100,000-meter square, only the numerical location is specified.
- However, even if every position being located is within a small area, but the area overlaps multiple 100,000-meter squares or grid zones, the entire grid reference is required.

One always reads map coordinates from west to east first (easting), then from south to north (northing). Common mnemonics include "in the house, up the stairs", "left-to-right, bottom-to-top" and "Read Right Up".

### Truncation, not rounding

As mentioned above, when converting UTM coordinates to an MGRS grid reference, or when abbreviating an MGRS grid reference to lower precision, coordinates are *truncated*, and not rounded up (which would indicate the next adjacent grid square instead of the surrounding square).

This was once controversial as the oldest specification, TM8358.1, indicated rounding, and early versions of GEOTRANS followed suit.
However, truncation has been applied in GEOTRANS since version 3.0, as well as in NGA Military Map Reading 201 (page 5) and in the U.S. Army Field Manual 3-25.26.

Likewise, the civilian/continental version of MGRS, USNG, specifies truncation.

_(Later sections of the original article were left out of this offline copy for length: Squares that cross a latitude band boundary, Polar regions.)_
