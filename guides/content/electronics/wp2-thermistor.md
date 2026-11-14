---
id: electronics/wp2-thermistor
title: Thermistor
category: electronics
tags: [thermistor, electronics]
summary: A thermistor is a semiconductor type of resistor in which the resistance is strongly dependent on temperature.
source: https://en.wikipedia.org/w/index.php?title=Thermistor&oldid=1380549254
source_title: Wikipedia: Thermistor
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Thermistor", Wikipedia, The Free Encyclopedia (revision 1380549254), https://en.wikipedia.org/w/index.php?title=Thermistor&oldid=1380549254 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Thermistor

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **thermistor** is a semiconductor type of resistor in which the resistance is strongly dependent on temperature. The word *thermistor* is a portmanteau of *thermal* and *resistor*. The varying resistance with temperature allows these devices to be used as temperature sensors, or to control current as a function of temperature. Some thermistors have decreasing resistance with temperature, while other types have increasing resistance with temperature. This allows them to be used for limiting current to cold circuits, e.g. for inrush current protection, or for limiting current to hot circuits, e.g. to prevent thermal runaway.

Thermistors are categorized based on their conduction models. *Negative-temperature-coefficient* (NTC) thermistors have *less* resistance at *higher* temperatures, while *positive-temperature-coefficient* (PTC) thermistors have *more* resistance at *higher* temperatures.

NTC thermistors are widely used as inrush current limiters and temperature sensors, while PTC thermistors are used as self-resetting overcurrent protectors and self-regulating heating elements. The operational temperature range of a thermistor is dependent on the material and is typically between -100 and 300 °C.

## Types

Depending on materials used, thermistors are classified into two types:

- With *NTC* (negative temperature coefficient) thermistors, resistance decreases as temperature rises; usually because electrons are bumped up by thermal agitation from the valence band to the conduction band. An NTC is commonly used as a temperature sensor, or in series with a circuit as an inrush current limiter.
- With *PTC* (positive temperature coefficient) thermistors, resistance increases as temperature rises; usually because of increased thermal lattice agitations, particularly those of impurities and imperfections. PTC thermistors are commonly installed in series with a circuit, and used to protect against overcurrent conditions, as resettable fuses.

Thermistors are generally produced using powdered metal oxides. With formulas and techniques vastly improving over the past 20 years As of 2020, NTC thermistors can now achieve accuracies over wide temperature ranges such as ±0.1 °C or ±0.2 °C from 0 °C to 70 °C with excellent long-term stability. NTC thermistor elements come in many styles, such as axial-leaded glass-encapsulated (DO-35, DO-34 and DO-41 diodes), glass-coated chips, epoxy-coated with bare or insulated lead wire and surface-mount, as well as thin film versions. The typical operating temperature range of a thermistor is −55 °C to +150 °C, though some glass-body thermistors have a maximal operating temperature of +300 °C.

Thermistors differ from resistance temperature detectors (RTDs) in that the material used in a thermistor is generally a ceramic or polymer, while RTDs use pure metals. The temperature response is also different; RTDs are useful over larger temperature ranges, while thermistors typically achieve a greater precision within a limited temperature range, typically −90 °C to 130 °C.

## Basic operation

Assuming, as a first-order approximation, that the relationship between resistance and temperature is linear, then
Δ R = k Δ T,
where
Δ R, change in resistance,
Δ T, change in temperature,
k, first-order temperature coefficient of resistance.

Depending on type of the thermistor in question the k may be either positive or negative.

If k is positive, the resistance increases with increasing temperature, and the device is called a *positive-temperature-coefficient* (*PTC*) *thermistor*, or *posistor*. There are two types of PTC resistor*switching thermistor* and *silistor*. If k is negative, the resistance decreases with increasing temperature, and the device is called a *negative-temperature-coefficient* (*NTC*) *thermistor*. Resistors that are not thermistors are designed to have a k as close to 0 as possible so that their resistance remains nearly constant over a wide temperature range.

Instead of the temperature coefficient *k*, sometimes the *temperature coefficient of resistance* α_T ("alpha sub T") is used. It is defined as
α_T = (1)/(R(T)) dR/dT.
This α_T coefficient should not be confused with the a parameter below.

## Construction and materials

Thermistors are typically built by using metal oxides. They're typically pressed into a bead, disk, or cylindrical shape and then encapsulated with an impermeable material such as epoxy or glass.

NTC thermistors are manufactured from oxides of the iron group of metals: e.g. chromium (CrO, Cr₂O₃), manganese (e.g. MnO), cobalt (CoO), iron (iron oxides), and nickel (NiO, Ni₂O₃). these oxides form a ceramic body with terminals composed of conductive metals such as silver, nickel, and tin.

PTC thermistors are usually prepared from barium (Ba), strontium, or lead titanates (e.g. PbTiO₃).

Thermistors can also be produced by resonant acoustic mixing of the previously mentioned oxides, followed by a sintering process. This effort reduces production time and can eliminate the calcination step entirely.

## Steinhart–Hart equation

In practical devices, the linear approximation model (above) is accurate only over a limited temperature range. Over wider temperature ranges, a more complex resistance–temperature transfer function provides a more faithful characterization of the performance. The Steinhart–Hart equation is a widely used third-order approximation:
1/T = a + b ln R + c (ln R)^3,
where *a*, *b* and *c* are called the Steinhart–Hart parameters and must be specified for each device. *T* is the absolute temperature, and *R* is the resistance. The equation is not dimensionally correct, since a change in the units of R results in an equation with a different form, containing a (ln R)^2 term. In practice, the equation gives good numerical results for resistances expressed in, for example, ohms (Ω) or kiloohms, but the coefficients a, b, and c must be stated with reference to that particular unit. To give resistance as a function of temperature, the above cubic equation in ln R can be solved, the real root of which is given by
ln R = fracb3 c x^1/3 -x^1/3
where
y = 1/2c (a - 1/T),; x = y + √((b/3c)^3 + y^2).

The error in the Steinhart–Hart equation is generally less than 0.02 °C in the measurement of temperature over a 200 °C range. As an example, typical values for a thermistor with a resistance of 3 kΩ at room temperature (25 °C = 298.15 K, R in Ω) are:
a = 1.40 × 10^-3,; b = 2.37 × 10^-4,; c = 9.90 × 10^-8.

## B or β parameter equation

NTC thermistors can also be characterised with the *B* (or *β*) parameter equation, which is essentially the Steinhart–Hart equation with a = 1/T_0 - (1/B) ln R_0, b = 1/B and c = 0,

1/T = (1)/(T_0) + 1/B ln (R)/(R_0),

where the temperatures and the *B* parameter are in kelvins, and *R*₀ is the resistance of the thermistor at temperature *T*₀ (25 °C = 298.15 K). Solving for *R* yields

R = R_0 e^B(1/T - (1)/(T_0))

or, alternatively,

R = r_∞ e^B/T,

where r_∞ = R_0 e^-B/T_0.

This can be solved for the temperature:

T = (B)/(ln(R / r_∞)) = (T_0*B)/(T_0*ln(R/R_0) + B).

The *B*-parameter equation can also be written as ln R = B/T + ln r_∞. This can be used to convert the function of resistance vs. temperature of a thermistor into a linear function of ln R vs. 1/T. The average slope of this function will then yield an estimate of the value of the *B* parameter.

_(Later sections of the original article were left out of this offline copy for length: Conduction model, Self-heating effects, Applications.)_
