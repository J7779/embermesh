---
id: navigation/wp2-scale-map
title: Scale (map)
category: navigation
tags: [scale, generating globe, nominal scale, principal scale, representative fraction, navigation]
summary: The scale of a map is the ratio of a distance on the map to the corresponding distance on the ground.
source: https://en.wikipedia.org/w/index.php?title=Scale_(map)&oldid=1379906592
source_title: Wikipedia: Scale (map)
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Scale (map)", Wikipedia, The Free Encyclopedia (revision 1379906592), https://en.wikipedia.org/w/index.php?title=Scale_(map)&oldid=1379906592 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Scale (map)

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **scale** of a map is the ratio of a distance on the map to the corresponding distance on the ground. This simple concept is complicated by the curvature of the Earth's surface, which forces scale to vary across a map. Because of this variation, the concept of scale becomes meaningful in two distinct ways.

The first way is the ratio of the size of the **generating globe** to the size of the Earth. The generating globe is a conceptual model to which the Earth is shrunk and from which the map is projected. The ratio of the Earth's size to the generating globe's size is called the **nominal scale** (also called **principal scale** or **representative fraction**). Many maps state the nominal scale and may even display a bar scale (sometimes merely called a "scale") to represent it.

The second distinct concept of scale applies to the variation in scale across a map. It is the ratio of the mapped point's scale to the nominal scale. In this case 'scale' means the **scale factor** (also called **point scale** or **particular scale**).

If the region of the map is small enough to ignore Earth's curvature, such as in a town plan, then a single value can be used as the scale without causing measurement errors. In maps covering larger areas, or the whole Earth, the map's scale may be less useful or even useless in measuring distances. The map projection becomes critical in understanding how scale varies throughout the map. When scale varies noticeably, it can be accounted for as the scale factor. Tissot's indicatrix is often used to illustrate the variation of point scale across a map.

## Large-scale maps with curvature neglected

The region over which the earth can be regarded as flat depends on the accuracy of the survey measurements. If measured only to the nearest metre, then curvature of the earth is undetectable over a meridian distance of about 100 km and over an east-west line of about 80 km (at a latitude of 45 degrees). If surveyed to the nearest 1 mm, then curvature is undetectable over a meridian distance of about 10 km and over an east-west line of about 8 km. Thus a plan of New York City accurate to one metre or a building site plan accurate to one millimetre would both satisfy the above conditions for the neglect of curvature. They can be treated by plane surveying and mapped by scale drawings in which any two points at the same distance on the drawing are at the same distance on the ground. True ground distances are calculated by measuring the distance on the map and then multiplying by the inverse of the scale fraction or, equivalently, simply using dividers to transfer the separation between the points on the map to a bar scale on the map.

## Point scale (or particular scale)

As proved by Gauss’s *Theorema Egregium*, a sphere (or ellipsoid) cannot be projected onto a plane without distortion. This is commonly illustrated by the impossibility of smoothing an orange peel onto a flat surface without tearing and deforming it. The only true representation of a sphere at constant scale is another sphere such as a globe.

Given the limited practical size of globes, we must use maps for detailed mapping. Maps require projections. A projection implies distortion: a constant separation on the map does not correspond to a constant separation on the ground. While a map may display a graphical bar scale, the scale must be used with the understanding that it will be accurate on only some lines of the map. (This is discussed further in the examples in the following sections.)

Let *P* be a point at latitude φ and longitude λ on the sphere (or ellipsoid). Let Q be a neighbouring point and let α be the angle between the element PQ and the meridian at P: this angle is the **azimuth** angle of the element PQ. Let P' and Q' be corresponding points on the projection. The angle between the direction P'Q' and the projection of the meridian is the **bearing** β. In general αneβ. Comment: this precise distinction between azimuth (on the Earth's surface) and bearing (on the map) is not universally observed, many writers using the terms almost interchangeably.

**Definition:** the **point scale** at P is the ratio of the two distances P'Q' and PQ in the limit that Q approaches P. We write this as
 µ(λ, φ, α)=lim_Q→ P(P'Q')/(PQ),
where the notation indicates that the point scale is a function of the position of P and also the direction of the element PQ.

**Definition:** if P and Q lie on the same meridian (α=0), the **meridian scale** is denoted by h(λ, φ).

**Definition:** if P and Q lie on the same parallel (α=π/2), the **parallel scale** is denoted by k(λ, φ).

**Definition:** if the point scale depends only on position, not on direction, we say that it is isotropic and conventionally denote its value in any direction by the parallel scale factor k(λ,φ).

**Definition:** A map projection is said to be conformal if the angle between a pair of lines intersecting at a point P is the same as the angle between the projected lines at the projected point P', for all pairs of lines intersecting at point P. A conformal map has an isotropic scale factor. Conversely isotropic scale factors across the map imply a conformal projection.

Isotropy of scale implies that *small* elements are stretched equally in all directions, that is the shape of a small element is preserved. This is the property of **orthomorphism** (from Greek 'right shape'). The qualification 'small' means that at some given accuracy of measurement no change can be detected in the scale factor over the element. Since conformal projections have an isotropic scale factor they have also been called **orthomorphic projections**. For example, the Mercator projection is conformal since it is constructed to preserve angles and its scale factor is isotropic, a function of latitude only: Mercator *does* preserve shape in small regions.

**Definition:** on a conformal projection with an isotropic scale, points which have the same scale value may be joined to form the **isoscale lines**. These are not plotted on maps for end users but they feature in many of the standard texts. (See Snyder pages 203—206.)

### The representative fraction (RF) or principal scale

There are two conventions used in setting down the equations of any given projection. For example, the equirectangular cylindrical projection may be written as
cartographers: x=aλ y=aφ
mathematicians: x=λ y=φ
Here we shall adopt the first of these conventions (following the usage in the surveys by Snyder). Clearly the above projection equations define positions on a huge cylinder wrapped around the Earth and then unrolled. We say that these coordinates define the **projection map** which must be distinguished logically from the actual **printed** (or viewed) maps. If the definition of point scale in the previous section is in terms of the projection map then we can expect the scale factors to be close to unity. For normal tangent cylindrical projections the scale along the equator is k=1 and in general the scale changes as we move off the equator. Analysis of scale on the projection map is an investigation of the change of k away from its true value of unity.

Actual **printed maps** are produced from the projection map by a *constant* scaling denoted by a ratio such as 1:100M (for whole world maps) or 1:10000 (for such as town plans). To avoid confusion in the use of the word 'scale' this constant
scale fraction is called the **representative fraction** (RF) of the printed map and it is to be identified with the ratio printed on the map. The actual printed map coordinates for the equirectangular cylindrical projection are
printed map: x=(RF)aλ y=(RF)aφ
This convention allows a clear distinction of the intrinsic projection scaling and the reduction scaling.

From this point we ignore the RF and work with the projection map.

### Visualisation of point scale: the Tissot indicatrix

_(This section is shortened.)_

## Ratio symbol

While the colon is often used to express ratios, Unicode can express a symbol specific to ratios, being slightly raised:.

_(Later sections of the original article were left out of this offline copy for length: the rest of Point scale (or particular scale).)_
