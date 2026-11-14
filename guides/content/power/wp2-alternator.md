---
id: power/wp2-alternator
title: Alternator
category: power
tags: [alternator, synchronous generator, energy]
summary: An alternator (or synchronous generator) is an electrical generator that converts mechanical energy to electrical energy in the form of alternating current (AC).
source: https://en.wikipedia.org/w/index.php?title=Alternator&oldid=1382742931
source_title: Wikipedia: Alternator
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Alternator", Wikipedia, The Free Encyclopedia (revision 1382742931), https://en.wikipedia.org/w/index.php?title=Alternator&oldid=1382742931 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Alternator

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **alternator** (or **synchronous generator**) is an electrical generator that converts mechanical energy to electrical energy in the form of alternating current (AC). For reasons of cost and simplicity, most alternators use a rotating magnetic field with a stationary armature. The rotor coil is externally excited by a small direct current (DC), while the AC is generated in the stationary coils. Occasionally, a linear alternator or a rotating armature with a stationary magnetic field is used. Although any AC electrical generator might be called an alternator, usually the term refers to small rotating machines driven by automotive and other internal combustion engines. The term usually applies those types with externally excited rotor coils. An alternator that uses a permanent magnet for its magnetic field is called a magneto.

Alternators in power stations driven by steam turbines are called turbo-alternators. Large 50 or 60 Hz three-phase alternators in power plants generate most of the world's electric power, which is distributed by electric power grids.

## Principle of operation

A conductor moving relative to a magnetic field develops an electromotive force (EMF) in it (Faraday's Law). This EMF reverses its polarity when it moves under magnetic poles of opposite polarity. Typically, a rotating magnet, called the rotor, turns within a stationary set of conductors, called the stator, wound in coils on an iron core. The field cuts across the conductors, generating an induced EMF (electromotive force), as the mechanical input causes the rotor to turn.

The rotating magnetic field induces an AC voltage in the stator windings. Since the currents in the stator windings vary in step with the position of the rotor, an alternator is a synchronous generator.

The rotor's magnetic field may be produced by permanent magnets or by a field coil electromagnet. Automotive alternators use a rotor winding, which allows control of the alternator's generated voltage by varying the current in the rotor field winding. Permanent magnet machines avoid the loss due to magnetizing current in the rotor, but are restricted in size due to the cost of the magnet material. Since the permanent magnet field is constant, the terminal voltage varies directly with the speed of the generator. Brushless AC generators are usually larger than those used in automotive applications.

An automatic voltage control device controls the field current to keep the output voltage constant. If the output voltage from the stationary armature coils drops due to an increase in demand, more current is fed into the rotating field coils through the voltage regulator (VR). This increases the magnetic field around the field coils, which induces a greater voltage in the armature coils. Thus, the output voltage is brought back up to its original value.

Alternators used in central power stations also control the field current to regulate reactive power and to help stabilize the power system against the effects of momentary faults. Often, there are three sets of stator windings, physically offset so that the rotating magnetic field produces a three-phase current, displaced by one-third of a period with respect to each other.

## Synchronous speeds

One cycle of alternating current is produced each time a pair of field poles passes over a point on the stationary winding. The relation between speed and frequency is N = 120f/P, where f is the frequency in Hz (cycles per second). P is the number of poles (2, 4, 6, ...), and N is the rotational speed in revolutions per minute (r/min). Old descriptions of alternating current systems sometimes give the frequency in terms of alternations per minute, counting each half-cycle as one *alternation*; so 12,000 alternations per minute corresponds to 100 Hz.

An alternator's output frequency depends on the number of poles and the rotational speed. The speed corresponding to a particular frequency is called the *synchronous speed*. This table gives some examples:

| Poles | Rotation speed (r/min), giving... 50 Hz | Rotation speed (r/min), giving... 60 Hz | Rotation speed (r/min), giving... 400 Hz |
|---|---|---|---|
| 2 | 3,000 | 3,600 | 24,000 |
| 4 | 1,500 | 1,800 | 12,000 |
| 6 | 1,000 | 1,200 | 8,000 |
| 8 | 750 | 900 | 6,000 |
| 10 | 600 | 720 | 4,800 |
| 12 | 500 | 600 | 4,000 |
| 14 | 428.6 | 514.3 | 3,429 |
| 16 | 375 | 450 | 3,000 |
| 18 | 333.3 | 400 | 2,667 |
| 20 | 300 | 360 | 2,400 |
| 40 | 150 | 180 | 1,200 |

## Classifications

Alternators may be classified by the method of excitation, number of phases, the type of rotation, cooling method, and their application.

### By excitation

There are two main ways to produce the magnetic field used in the alternators: by using permanent magnets, which create their persistent magnetic field, or by using field coils. The alternators that use permanent magnets are specifically called magnetos. In other alternators, wound field coils form an electromagnet to produce the rotating magnetic field. A device that uses permanent magnets to produce alternating current is called a permanent magnet alternator (PMA). A permanent magnet generator (PMG) may produce either alternating current or direct current if it has a commutator.

#### Direct-connected direct-current (DC) generator

This method of excitation consists of a smaller direct-current (DC) generator fixed on the same shaft as the alternator. The DC generator generates a small amount of electricity, just enough to *excite* the field coils of the connected alternator to generate electricity. A variation of this system is a type of alternator that uses direct current from a battery for initial excitation upon start-up, after which the alternator becomes self-excited.

#### Direct-connected alternating-current (AC) generator

This method of excitation consists of a smaller alternating-current (AC) generator fixed on the same shaft as the alternator. The AC stator generates a small amount of field coil excitation current, which is induced in the rotor and rectified to DC by a bridge rectifier built in to the windings where it excites the field coils of the larger connected alternator to generate electricity. This system has the advantage of not requiring brushes, which increases service life, although with a slightly lower overall efficiency. A variation of this system is a type of alternator that uses direct current from a battery for initial excitation upon start-up, after which the alternator becomes self-excited.

#### Transformation and rectification

This method depends on residual magnetism retained in the iron core to generate a weak magnetic field, which would allow a weak voltage to be generated. This voltage is used to excite the field coils so the alternator can generate stronger voltage as part of its *build up* process. After the initial AC voltage buildup, the field is supplied with rectified voltage from the alternator.

#### Brushless alternators

A brushless alternator is composed of two alternators built end-to-end on one shaft. Until 1966, alternators used brushes with rotating field. With the advancement in semiconductor technology, brushless alternators are possible. Smaller brushless alternators may look like one unit, but the two parts are readily identifiable in the large versions. The main alternator is the larger of the two sections, and the smaller one is the exciter. The exciter has stationary field coils and a rotating armature (power coils). The main alternator uses the opposite configuration with a rotating field and stationary armature. A bridge rectifier, called the rotating rectifier assembly, is mounted on the rotor. Neither brushes nor slip rings are used, which reduces the number of wearing parts. The main alternator has a rotating field and a stationary armature (power generation windings).

Varying the amount of current through the stationary exciter field coils varies the 3-phase output from the exciter. This output is rectified by a rotating rectifier assembly mounted on the rotor, and the resultant DC supplies the rotating field of the main alternator and hence alternator output. The result is that a small DC exciter current indirectly controls the output of the main alternator.

### By number of phases

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Classifications, Specific applications.)_
