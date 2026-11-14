---
id: electronics/wp2-ground-loop-electricity
title: Ground loop (electricity)
category: electronics
tags: [ground loop, earth loop, electronics]
summary: In an electrical system, a ground loop or earth loop occurs when two points of a circuit are intended to have the same ground reference potential but instead have a different potential between them.
source: https://en.wikipedia.org/w/index.php?title=Ground_loop_(electricity)&oldid=1383945498
source_title: Wikipedia: Ground loop (electricity)
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Ground loop (electricity)", Wikipedia, The Free Encyclopedia (revision 1383945498), https://en.wikipedia.org/w/index.php?title=Ground_loop_(electricity)&oldid=1383945498 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Ground loop (electricity)

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In an electrical system, a **ground loop** or **earth loop** occurs when two points of a circuit are intended to have the same ground reference potential but instead have a different potential between them. This is typically caused when enough current is flowing in the connection between the two ground points to produce a voltage drop and cause the two points to be at different potentials. Current may be produced in a ground loop by electromagnetic induction.

Ground loops are a major cause of noise, hum, and interference in audio, video, and computer systems. Wiring practices that protect against ground loops include ensuring that all vulnerable signal circuits are referenced to one point as ground. The use of differential signaling can provide rejection of ground-induced interference. The removal of ground connections to equipment in an effort to eliminate ground loops will also eliminate the protection the safety ground connection is intended to provide.

## Description

A ground loop is caused by the interconnection of electrical devices that results in multiple paths to ground, thereby forming closed conductive loops through the ground connections. A common example is two electrical devices, each connected to a mains power outlet by a three-conductor cable and plug containing a safety ground. When signal cables are connected between both devices, the shield of the signal cable is typically connected to the grounded chassis of both devices. This forms a closed loop through the ground conductors of the power cords, which are connected through the building wiring.

In the vicinity of electric power wiring, there will always be stray magnetic fields, particularly from utility lines oscillating at line frequency (50 or 60 hertz). These ambient magnetic fields passing through the ground loop will induce a current in the loop by electromagnetic induction. The ground loop acts as a single-turn secondary winding of a transformer, the primary being the summation of all current-carrying conductors nearby. The amount of current induced will depend on the magnitude and proximity of nearby currents. The presence of high-power equipment, such as industrial motors or transformers can increase the interference. Since the conductors comprising the ground loop usually have very low resistance, often below one ohm, even weak magnetic fields can induce significant currents.

Since the ground conductor of the signal cable linking the two devices is part of the signal path of the cable, the alternating ground current flowing through the cable can introduce electrical interference in the signal. The induced alternating current flowing through the resistance of the cable ground conductor will cause a small AC voltage drop across the cable ground. This is added to the signal applied to the input of the next stage. In audio equipment, the line frequency interference may be heard as a hum in the speakers. In a video system it may cause distortion or synchronization problems. In computer data connections, it can cause slowdowns or failures of data transfer.

Ground loops can also exist within the internal circuits of electronic equipment, as design flaws.

The addition of signal interconnection cables to a system where equipment enclosures are already required to be bonded to ground can create ground loops. Proper design of such a system will satisfy both safety grounding requirements and signal integrity. For this reason, in some large professional installations such as recording studios, it is sometimes the practice to provide two completely separate ground connections to equipment bays. One is the normal safety ground that connects to exposed metalwork, the other is a technical ground for cable screens and the like.

## Representative circuit

The circuit diagram illustrates a simple ground loop. Circuit 1 (left) and circuit 2 (right) share a common path to ground of resistance scriptstyle R_G. Ideally, this ground conductor would have no resistance (scriptstyle R_G = 0), yielding no voltage drop across it (scriptstyle V_G = 0), keeping the connection point between the circuits at a constant ground potential. In that case, the output of circuit 2 is simply scriptstyle V_out = V_2.

However, if this ground conductor has some resistance (scriptstyle R_G > 0), then it forms a voltage divider with scriptstyle R_1. As a result, if a current (scriptstyle I_1) is flowing through scriptstyle R_G from circuit 1, then a voltage drop across scriptstyle R_G of scriptstyle V_G = I_1 R_G occurs, causing the shared ground connection to no longer be at the actual ground potential. This voltage across the ground conductor is applied to circuit 2 and added to its output:V_out = V_2 - V_G = V_2 - (R_G)/(R_G + R_1)V_1.

Thus, the two circuits are no longer isolated, and circuit 1 can introduce interference into the output of circuit 2. If circuit 2 is an audio system and circuit 1 has large AC currents flowing in it, the interference may be heard as a 50 or 60 Hz hum in the speakers. Also, both circuits have voltage scriptstyle V_G on their grounded parts that may be exposed to contact, possibly presenting a shock hazard. This is true even if circuit 2 is turned off.

Although ground loops occur most often in the ground conductors of electrical equipment, similar loops can occur wherever two or more circuits share a common current path. If enough current flows, similar problems occur in these conditions.

## Common ground loops

A common type of ground loop is due to faulty interconnections between electronic components, such as laboratory or recording studio equipment, or home component audio, video, and computer systems. This can create inadvertent closed loops in the ground wiring circuit, which can allow stray line frequency AC current to be induced and flow through the ground conductors of signal cables. The voltage drops in the ground system caused by these currents are added to the signal path, introducing noise and hum into the output. The loops can include the building's utility wiring ground system when more than one component is grounded through the protective earth (third wire) in their power cords.

### Ground currents on signal cables

The symptoms of a ground loop, ground noise and hum in electrical equipment are caused by current flowing in the ground or *shield* conductor of a cable. Fig. 1 shows a signal cable *S* linking two electronic components, including the typical line driver and receiver amplifiers *(triangles)*. The cable has a ground or shield conductor which is connected to the chassis ground of each component. The driver amplifier in component 1 *(left)* applies signal *V*₁ between the signal and ground conductors of the cable. At the destination end *(right)*, the signal and ground conductors are connected to a differential amplifier. This produces the signal input to component 2 by subtracting the shield voltage from the signal voltage to eliminate common-mode noise picked up by the cable
V_2 = V_S2 - V_G2

If a current *I* from a separate source is flowing through the ground conductor, the resistance *R* of the conductor will create a voltage drop along the cable ground of *IR*, so the destination end of the ground conductor will be at a different potential than the source end
V_G2 = V_G1 - IR
Since the differential amplifier has high impedance, little current flows in the signal wire, therefore there is no voltage drop across it: V_S2 = V_S1 The ground voltage appears to be in series with the signal voltage *V*₁ and adds to it
V_2 = V_S1 - (V_G1 - IR)
V_2 = V_1 + IR

If *I* is an AC current, this can result in noise added to the signal path in component 2.

### Sources of ground current

The diagrams in this section show a typical ground loop caused by a signal cable *S* connecting two grounded electronic components *C1* and *C2*. The loop consists of the signal cable's ground conductor, which is connected through the components' metal chassis to the ground wires *P* in their power cords, which are plugged into outlet grounds that are connected through the building's utility ground wire system *G*.

Such loops in the ground path can cause currents in signal cable grounds by two main mechanisms:

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Common ground loops, In low-frequency audio and instrumentation systems, In analog video systems, In digital and RF systems, Internal ground loops in equipment, In circuit design.)_
