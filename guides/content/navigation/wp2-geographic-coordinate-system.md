---
id: navigation/wp2-geographic-coordinate-system
title: Geographic coordinate system
category: navigation
tags: [geographic coordinate system, gcs, navigation]
summary: A geographic coordinate system (GCS) is a spherical or geodetic coordinate system for measuring and communicating positions directly on Earth as latitude and longitude.
source: https://en.wikipedia.org/w/index.php?title=Geographic_coordinate_system&oldid=1376521784
source_title: Wikipedia: Geographic coordinate system
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Geographic coordinate system", Wikipedia, The Free Encyclopedia (revision 1376521784), https://en.wikipedia.org/w/index.php?title=Geographic_coordinate_system&oldid=1376521784 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Geographic coordinate system

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **geographic coordinate system** (**GCS**) is a spherical or geodetic coordinate system for measuring and communicating positions directly on Earth as latitude and longitude. It is the simplest, oldest, and most widely used type of the various spatial reference systems that are in use, and forms the basis for most others. Although latitude and longitude form a coordinate pair like a Cartesian coordinate system, geographic coordinate systems are not Cartesian because the measurements are angles and are not on a planar surface.

A full GCS specification, such as those listed in the EPSG and ISO 19111 standards, also includes a choice of geodetic datum (including an Earth ellipsoid), as different datums will yield different latitude and longitude values for the same location.

## Latitude and longitude

The *latitude* φ of a point on Earth's surface is defined in one of three ways, depending on the type of coordinate system. In each case, the latitude is the angle formed by the plane of the equator and a line formed by the point on the surface and a second point on equatorial plane. What varies between the types of coordinate systems is how the point on the equatorial plane is determined:

- In an astronomical coordinate system, the second point is found where the extension of the plumb bob vertical from the surface point intersects the equatorial plane.
- In a geodetic coordinate system, the second point is found where the normal vector from the surface of the ellipsoid at the surface point intersects the equatorial plane.
- In a geocentric coordinate system, the second point is the center of Earth.
The path that joins all points of the same latitude traces a circle on the surface of Earth, as viewed from above the north or south pole, called parallels, as they are parallel to the equator and to each other. The north pole is 90° N; the south pole is 90° S. The 0° parallel of latitude is defined to be the equator, the fundamental plane of a geographic coordinate system. The equator divides the globe into Northern and Southern Hemispheres.

The *longitude* λ of a point on Earth's surface is the angle east or west of a reference meridian to another meridian that passes through that point. All meridians are halves of great ellipses, which converge at the North and South Poles. The meridian of the British Royal Observatory in Greenwich, in southeast London, England, is the international prime meridian, although some organizations—such as the French Institut national de l'information géographique et forestière—continue to use other meridians for internal purposes. The antipodal meridian of Greenwich is both 180°W and 180°E. This is not to be conflated with the International Date Line, which partly overlaps with the 180° meridian but diverges from it in several places for political and convenience reasons, including between far eastern Russia and the far western Aleutian Islands.

The combination of these two components specifies the position of any location on the surface of Earth, without consideration of altitude or depth. The visual grid on a map formed by lines of latitude and longitude is known as a *graticule*. The origin/zero point of this system is located in the Gulf of Guinea about 625 km south of Tema, Ghana, a location often facetiously called Null Island.

## Geodetic datum

In order to use the theoretical definitions of latitude, longitude, and height to precisely measure actual locations on the physical earth, a *geodetic datum* must be used. A *horizonal datum* is used to precisely measure latitude and longitude, while a *vertical datum* is used to measure elevation or altitude. Both types of datum bind a mathematical model of the shape of the earth (usually a reference ellipsoid for a horizontal datum, and a more precise geoid for a vertical datum) to the earth. Traditionally, this binding was created by a network of control points, surveyed locations at which monuments are installed, and were only accurate for a region of the surface of the Earth. Newer datums are based on a global network for satellite measurements (GNSS, VLBI, SLR and DORIS).

This combination of a mathematical model and physical binding ensures that users of the same datum obtain identical coordinates for a given physical point. However, different datums typically produce different coordinates for the same location (sometimes deviating several hundred meters) not due to actual movement, but because the reference system itself is shifted. Because any spatial reference system or map projection is ultimately calculated from latitude and longitude, it is crucial that they clearly state the datum on which they are based. For example, a UTM coordinate based on a WGS84 realisation will be different than a UTM coordinate based on NAD27 for the same location. Transforming coordinates from one datum to another requires a datum transformation method such as a Helmert transformation, although in certain situations a simple translation may be sufficient.

Datums may be global, meaning that they represent the whole Earth, or they may be regional, meaning that they represent an ellipsoid best-fit to only a portion of the Earth. Examples of global datums include the several realizations of WGS 84 (with the 2D datum ensemble EPSG:4326 with 2 meter accuracy as identifier) used for the Global Positioning System, and the several realizations of the International Terrestrial Reference System and Frame (such as ITRF2020 with subcentimeter accuracy), which takes into account continental drift and crustal deformation.

Datums with a regional fit of the ellipsoid that are chosen by a national cartographical organization include the North American Datums, the European ED50, and the British OSGB36. Given a location, the datum provides the latitude φ and longitude λ. In the United Kingdom there are three common latitude, longitude, and height systems in use. WGS 84 differs at Greenwich from the one used on published maps OSGB36 by approximately 112 m. ED50 differs from about 120 m to 180 m.

Points on the Earth's surface move relative to each other due to continental plate motion, subsidence, and diurnal Earth tidal movement caused by the Moon and the Sun. This daily movement can be as much as a meter. Continental movement can be up to 10 cm a year, or 10 m in a century. A weather system high-pressure area can cause a sinking of 5 mm. Scandinavia is rising by 1 cm a year as a result of the melting of the ice sheets of the last ice age, but neighboring Scotland is rising by only 0.2 cm. These changes are insignificant if a regional datum is used, but are statistically significant if a global datum is used.

## Alternative encodings

Like any series of multiple-digit numbers, latitude-longitude pairs can be challenging to communicate and remember. Therefore, alternative schemes have been developed for encoding GCS coordinates into alphanumeric strings or words:

- the Maidenhead Locator System, popular with radio operators.
- the World Geographic Reference System (GEOREF), developed for global military operations, replaced by the current Global Area Reference System (GARS).
- Open Location Code or "Plus Codes", developed by Google and released into the public domain.
- Geohash, a public domain system based on the Morton Z-order curve.
- Mapcode, an open-source system originally developed at TomTom.
- What3words, a proprietary system that encodes GCS coordinates as pseudorandom sets of words by dividing the coordinates into three numbers and looking up words in an indexed dictionary.

These are not distinct coordinate systems, only alternative methods for expressing latitude and longitude measurements.

_(Later sections of the original article were left out of this offline copy for length: Length of a degree.)_
