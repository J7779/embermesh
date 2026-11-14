---
id: electronics/wp2-potentiometer
title: Potentiometer
category: electronics
tags: [potentiometer, variable resistor, rheostat, electronics]
summary: A potentiometer is a three-terminal resistor with a sliding or rotating contact that forms an adjustable voltage divider.
source: https://en.wikipedia.org/w/index.php?title=Potentiometer&oldid=1385148998
source_title: Wikipedia: Potentiometer
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Potentiometer", Wikipedia, The Free Encyclopedia (revision 1385148998), https://en.wikipedia.org/w/index.php?title=Potentiometer&oldid=1385148998 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Potentiometer

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **potentiometer** is a three-terminal resistor with a sliding or rotating contact that forms an adjustable voltage divider. In motion control systems, potentiometers are frequently used as position sensors to provide analog feedback to a controller, allowing for precise tracking of mechanical movement. If only two terminals are used, one end and the wiper, it acts as a **variable resistor** or **rheostat**.

The measuring instrument called a potentiometer is essentially a voltage divider used for measuring electric potential (voltage); the component is an implementation of the same principle, hence its name.

Potentiometers are commonly used to control electrical devices such as volume controls on audio equipment. It is also used in speed control of fans. Potentiometers operated by a mechanism can be used as position transducers, for example, in a joystick. Potentiometers are rarely used to directly control significant power (more than a watt), since the power dissipated in the potentiometer would be comparable to the power in the controlled load.

## Construction

Potentiometers consist of: a resistive element; a sliding contact (wiper) that moves along the element, making good electrical contact with one part of it; electrical terminals at each end of the element; a mechanism that moves the wiper from one end to the other; and a housing containing the element and wiper.

Many inexpensive potentiometers are constructed with a resistive element (B in cutaway drawing) formed into an arc of a circle usually a little less than a full turn and a wiper (C) sliding on this element when rotated, making electrical contact. The resistive element can be flat or angled. Each end of the resistive element is connected to a terminal (E, G) on the case. The wiper is connected to a third terminal (F), usually between the other two. On panel potentiometers, the wiper is usually the center terminal of three. For single-turn potentiometers, this wiper typically travels just under one revolution around the contact. The only point of ingress for contamination is the narrow space between the shaft and the housing it rotates in.

Another type is the linear slider potentiometer, which has a wiper which slides along a linear element instead of rotating. Contamination can potentially enter anywhere along the slot the slider moves in, making effective sealing more difficult and compromising long-term reliability. An advantage of the slider potentiometer is that the slider position gives a visual indication of its setting. While the setting of a rotary potentiometer can be seen by the position of a marking on the knob, an array of sliders can give a visual impression of settings as in a graphic equalizer or faders on a mixing console.

The resistive element of inexpensive potentiometers is often made of graphite. Other materials used include resistance wire, carbon particles in plastic, and a ceramic/metal mixture called cermet.

Conductive track potentiometers use conductive polymer resistor pastes that contain hard-wearing resins and polymers, solvents, and lubricant, in addition to the carbon that provides the conductive properties.

Multiturn potentiometers are also operated by rotating a shaft, but by several turns rather than less than a full turn. Some multiturn potentiometers have a linear resistive element with a sliding contact moved by a lead screw; others have a helical resistive element and a wiper that turns through 10, 20, or more complete revolutions, moving along the helix as it rotates. Multiturn potentiometers, both user-accessible and preset, allow finer adjustments; rotation through the same angle changes the setting by typically a tenth as much as for a simple rotary potentiometer.

A string potentiometer is a multi-turn potentiometer operated by an attached reel of wire turning against a spring, allowing it to convert linear position to a variable resistance.

User-accessible rotary potentiometers can be fitted with a switch which operates usually at the anti-clockwise extreme of rotation. Before digital electronics became the norm such a component was used to allow radio and television receivers and other equipment to be switched on at minimum volume with an audible click, then the volume increased by turning the same knob. In other applications, such as domestic light dimmers, the normal usage pattern is best satisfied if the potentiometer remains set at its current position, so the switch is operated by a push action, alternately on and off, by axial presses of the knob.

Multiple resistance elements can be ganged together with their sliding contacts on the same shaft, for example in stereo audio amplifiers for volume control. Blending or balance controls have resistive elements in opposite directions so that each channel is at equal level in the middle and one or the other channel is at maximum resistance at opposite extremes of rotation. These can have tapers that each only cover half of the track so that both channels are at full volume in the middle and only one or the other channel is affected at other positions in rotation.

Other potentiometers are enclosed within the equipment and are intended to only be adjusted when calibrating the equipment during manufacture or repair, and not otherwise touched. They are usually physically much smaller than user-accessible potentiometers, and may need to be operated by a screwdriver rather than having a knob. They are usually called "trimmer", "trim[ming]", or "preset" potentiometers (or pots), or the genericized brand name "trimpot".

### Resistance–position relationship: "taper"

The relationship between slider position and resistance, known as the "taper" or "law", can be controlled during manufacture by changing the composition or thickness of the resistance coating along the resistance element. Although in principle any taper is possible, two types are widely manufactured: linear and logarithmic (aka "audio taper") potentiometers.

A letter code may be used to identify which taper is used, but the letter code definitions are not standardized. Potentiometers made in Asia and the US are usually marked with an "A" for logarithmic taper or a "B" for linear taper; "C" for the rarely seen reverse logarithmic taper. Others, particularly those from Europe, may be marked with an "A" for linear taper, a "C" or "B" for logarithmic taper, or an "F" for reverse logarithmic taper. The code used also varies between different manufacturers. When a percentage is referenced with a non-linear taper, it relates to the resistance value at the midpoint of the shaft rotation. A 10% log taper would therefore measure 10% of the total resistance at the midpoint of the rotation; i.e. 10% log taper on a 10 kOhm potentiometer would yield 1 kOhm at the midpoint. The higher the percentage, the steeper the log curve.

#### Linear taper potentiometer

A *linear taper potentiometer* (*linear* describes the electrical characteristic of the device, not the geometry of the resistive element) has a resistive element of constant cross-section, resulting in a device where the resistance between the contact (wiper) and one end terminal is proportional to the distance between them. Linear taper potentiometers are used when the division ratio of the potentiometer must be proportional to the angle of shaft rotation (or slider position), for example, controls used for adjusting the centering of the display on an analog cathode-ray oscilloscope. Precision potentiometers have an accurate relationship between resistance and slider position.

#### Logarithmic potentiometer

A *logarithmic taper potentiometer* is a potentiometer that has a bias built into the resistive element, meaning the center position of the potentiometer is not one half of the total value of the potentiometer. The resistive element is designed to follow a logarithmic taper, aka a mathematical exponent or "squared" profile.
A logarithmic taper potentiometer is constructed with a resistive element that either "tapers" in from one end to the other, or is made from a material whose resistivity varies from one end to the other. This results in a device where output voltage is a logarithmic function of the slider position.

_(This section is shortened.)_

## Failure

Ageing may cause intermittent contact between the resistive track and the wiper as it is rotated. In volume control use this causes crackling.

_(Later sections of the original article were left out of this offline copy for length: the rest of Construction, Rheostat, Digital potentiometer, Membrane potentiometers, Applications, Theory of operation.)_
