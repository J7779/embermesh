---
id: electronics/residual-current-device
title: Residual-current device
category: electronics
tags: [electronics, safety, RCD, GFCI, shock]
summary: "RCDs/GFCIs: devices that cut power when current leaks to earth, preventing electrocution."
source: "https://en.wikipedia.org/w/index.php?title=Residual-current_device&oldid=1384618415"
source_title: Residual-current device (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Residual-current device' (rev 1384618415), Wikipedia, https://en.wikipedia.org/wiki/Residual-current_device. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# Residual-current device

A residual-current device (RCD), residual-current circuit breaker (RCCB) or ground fault circuit interrupter (GFCI) is an electrical safety device, more specifically a form of earth-leakage circuit breaker, that interrupts an electrical circuit when the current passing through line and neutral conductors of a circuit is not equal (the term *residual* relating to the imbalance), therefore indicating current leaking to ground, or to an unintended path that bypasses the protective device. The device's purpose is to reduce the severity of injury caused by an electric shock. This type of circuit interrupter cannot protect a person who touches both circuit conductors at the same time, since it then cannot distinguish normal current from that passing through a person.

A residual-current circuit breaker with integrated overcurrent protection (RCBO) combines RCD protection with additional overcurrent protection into the same device.

These devices are designed to quickly interrupt the protected circuit when it detects that the electric current is unbalanced between the supply and return conductors of the circuit. Any difference between the currents in these conductors indicates leakage current, which presents a shock hazard. Alternating 60 Hz current above 20 mA (0.020 amperes) through the human body is potentially sufficient to cause cardiac arrest or serious harm if it persists for more than a small fraction of a second. RCDs are designed to disconnect the conducting wires ("trip") quickly enough to potentially prevent serious injury to humans, and to prevent damage to electrical devices.

## Purpose and operation

RCDs are designed to disconnect the circuit if there is a leakage current. In their first implementation in the 1950s, power companies used them to prevent electricity theft where consumers grounded returning circuits rather than connecting them to neutral to inhibit electrical meters from registering their power consumption.

The most common modern application is as a safety device to detect small leakage currents (typically 5–30 mA) and disconnecting quickly enough (<30 milliseconds) to prevent device damage or electrocution. They are an essential part of the automatic disconnection of supply (ADS), i.e. to switch off when a fault develops, rather than rely on human intervention, one of the essential tenets of modern electrical practice.

To reduce the risk of electrocution, RCDs should operate within 25–40 milliseconds with any leakage currents (through a person) of greater than 30 mA, before electric shock can drive the heart into ventricular fibrillation, the most common cause of death through electric shock. By contrast, conventional circuit breakers or fuses only break the circuit when the total current is excessive (which may be thousands of times the leakage current an RCD responds to). A small leakage current, such as through a person, can be a very serious fault, but does not increase the total current enough for a fuse or overload circuit breaker to isolate the circuit.

RCDs operate by measuring the current balance between two conductors using a differential current transformer. This measures the difference between current flowing through line and neutral. If these do not sum to zero, there is a leakage of current to somewhere else (to Earth/ground or to another circuit), and the device will open its contacts. Operation does not require a fault current to return through the earth wire in the installation; the trip will operate just as well if the return path is through plumbing or contact with the ground or anything else. Automatic disconnection and a measure of shock protection is therefore still provided even if the earth wiring of the installation is damaged or incomplete.

RCDs are testable and resettable devices—a test button safely creates a small leakage condition, and another button, or switch, resets the conductors after a fault condition has been cleared. Some RCDs disconnect both the line and neutral conductors upon a fault (double pole), while a single pole RCD only disconnects the line conductor. If the fault has left the neutral wire "floating" or not at its expected ground potential for any reason, then a single-pole RCD will leave this conductor still connected to the circuit when it detects the fault.

For an RCD used with three-phase power, all three line conductors and the neutral (if fitted) must pass through the current transformer.

## Application

Electrical plugs with incorporated RCD are sometimes installed on appliances that might be considered to pose a particular safety hazard, for example long extension leads, which might be used outdoors, or garden equipment or hair dryers, which may be used near a bath or basin. Occasionally an in-line RCD may be used to serve a similar function to one in a plug. By putting the RCD in the extension lead, protection is provided at whatever outlet is used even if the building has old wiring, such as knob and tube, or wiring that does not contain a grounding conductor. The in-line RCD can also have a lower tripping threshold than the building to further improve safety for a specific electrical device.

In North America, GFI receptacles can be used in cases where there is no grounding conductor, but they must be labeled as "no equipment ground". This is referenced in the National Electric Code section 406 (D) 2; however, codes change and someone should always consult a licensed professional and their local building and safety departments. An ungrounded GFI receptacle will trip using the built-in "test" button, but will not trip using a GFI test plug, because the plug tests by passing a small current from line to the non-existent ground. It is worth noting that despite this, only one GFCI receptacle at the beginning of each circuit is necessary to protect downstream receptacles. There does not appear to be a risk from using multiple GFI receptacles on the same circuit, though it is considered redundant.

In Europe, RCDs can fit on the same DIN rail as the miniature circuit breakers; much like in miniature circuit breakers, the busbar arrangements in consumer units and distribution boards provides protection for anything downstream.

## RCBO

A pure RCD will detect imbalance in the currents of the supply and return conductors of a circuit. But it cannot protect against overload or short circuit like a fuse or a miniature circuit breaker (MCB) does (except for the special case of a short circuit from line to ground, not line to neutral).

However, an RCD and an MCB often come integrated in the same device, thus being able to detect both supply imbalance and overload current. Such a device is called an *RCBO*, for *residual-current circuit breaker with overcurrent protection*, in Europe and Australia, and a *GFCI breaker*, for *ground fault circuit interrupter*, in the United States and Canada.

## Typical design

The diagram depicts the internal mechanism of a residual-current device (RCD). The device is designed to be wired in-line in an appliance power cord. It is rated to carry a maximal current of 13 A and is designed to trip on a leakage current of 30 mA. This is an active RCD; that is, it latches electrically and therefore trips on power failure, a useful feature for equipment that could be dangerous on unexpected re-energisation. Some early RCDs were entirely electromechanical and relied on finely balanced sprung over-centre mechanisms driven directly from the current transformer. As these are hard to manufacture to the required accuracy and prone to drift in sensitivity both from pivot wear and lubricant dry-out, the electronically amplified type with a more robust solenoid part as illustrated are now dominant.

In the internal mechanism of an RCD, the incoming supply and the neutral conductors are connected to the terminals at (1), and the outgoing load conductors are connected to the terminals at (2). The earth conductor (not shown) is connected through from supply to load uninterrupted. When the reset button (3) is pressed, the contacts ((4) and another, hidden behind (5)) close, allowing current to pass. The solenoid (5) keeps the contacts closed when the reset button is released.

The sense coil (6) is a differential current transformer which surrounds (but is not electrically connected to) the line and neutral conductors. In normal operation, all the current flows in and out of the line and neutral conductors. The amount of current in the two conductors is equal and opposite and cancel each other out.

Any fault to earth (for example caused by a person touching a live component in the attached appliance) causes some of the current to take a different path, with some of the neutral current diverted, which means that there is then an imbalance in the current between the line and neutral conductors (single-phase), or, more generally a nonzero sum of currents from among various conductors (for example, three phase conductors and one neutral conductor), within the RCD.

This difference causes a magnetic flux in the toroidal sense coil (6), which, if sufficiently large, activates the relay (5), causing the switch to activate forcing the contacts (4) apart and thus cutting off the electricity supply to the appliance. In some designs a power failure may also cause the switch contacts to open, causing the safe trip-on-power-failure behaviour mentioned above.

The test button (8) allows the correct operation of the device to be verified by passing a small current through the orange test wire (9). This simulates a fault by creating a deliberate imbalance in the sense coil. If the RCD does not trip when this button is pressed, then the device must be replaced.

### RCD with integral overcurrent protection (RCBO)

Residual-current and over-current protection may be combined in one device. Such a device is termed an RCBO (residual-current circuit breaker with over-current protection). In the US and Canada such devices (in the form of circuit breakers) are known by the term *GFCI circuit breaker* (lacking an American-specific acronym for this combination and seemingly taking for granted the presence of over-current protection in any circuit breaker type device). They are effectively a combination of a RCD and a MCB.

As well as requiring both line and neutral inputs and outputs (or, full three-phase), some RCDs/GFCIs require a functional earth (FE) connection. This serves to provide both EMC immunity and to reliably operate the device if the input-side neutral connection is lost but line and earth remain.

For reasons of space, many devices, especially in DIN rail format, use flying leads rather than screw terminals, especially for the neutral input and FE connections. Additionally, because of the small form factor, the output cables of some models (Eaton/MEM) are used to form the primary winding of the RCD part, and the outgoing circuit cables must be led through a specially dimensioned terminal tunnel with the current transformer part around it. This can lead to incorrect failed trip results when testing with meter probes from the screw heads of the terminals, rather than from the final circuit wiring.

Having one RCD feeding another is generally unnecessary, provided they have been wired properly. One exception is the case of a TT earthing system, where the earth loop impedance may be high, meaning that a ground fault might not cause sufficient current to trip an ordinary circuit breaker or fuse. In this case a special 100 mA (or greater) trip current time-delayed RCD is installed, covering the whole installation, and then more sensitive RCDs should be installed downstream of it for sockets and other circuits that are considered high-risk.

### RCD with additional arc fault protection circuitry

In addition to ground fault circuit interrupters (GFCIs), arc-fault circuit interrupters (AFCI) are important as they offer added protection from potentially hazardous arc faults resulting from damage in branch circuit wiring as well as extensions to branches such as appliances and cord sets. By detecting arc faults and responding by interrupting power, AFCIs help reduce the likelihood of the home's electrical system being an ignition source of a fire. Dual function AFCI/GFCI devices offer both electrical fire prevention and shock prevention in one device making them a solution for many rooms in the home.

## Characteristics

### Differences in disconnection actions

Major differences exist regarding the manner in which an RCD unit will act to disconnect the power to a circuit or appliance.

There are four situations in which different types of RCD units are used:

- At the consumer power distribution level, usually in conjunction with an RCBO resettable circuit breaker
- Built into a wall socket
- Plugged into a wall socket, which may be part of a power-extension cable
- Built into the cord of a portable appliance, such as those intended to be used in outdoor or wet areas

The first three of those situations relate largely to usage as part of a power-distribution system and are almost always of the *passive* or *latched* variety, whereas the fourth relates solely to specific appliances and are always of the *active* or *non-latching* variety. *Active* means prevention of any *re-activation* of the power supply after any inadvertent form of power outage, as soon as the mains supply becomes re-established; *latch* relates to a *switch* inside the unit housing the RCD that remains as set following any form of power outage, but has to be reset manually after the detection of an error condition.

In the fourth situation, it would be deemed to be highly undesirable, and probably very unsafe, for a connected appliance to automatically resume operation after a power disconnection, without having the operator in attendance – as such, manual reactivation of the RCD is necessary.

The difference between the modes of operation of the essentially two different types of RCD functionality is that the operation for power distribution purposes requires the internal latch to remain set within the RCD after any form of power disconnection caused by either the user turning the power off, or after any power outage; such arrangements are particularly applicable for connections to refrigerators and freezers.

Situation two is mostly installed just as described above, but some wall socket RCDs are available to fit the fourth situation, often by operating a switch on the fascia panel.

RCDs for the first and third situation are most commonly rated at 30 mA and 40 ms. For the fourth situation, there is generally a greater choice of ratings available – generally all lower than the other forms, but lower values often result in more nuisance tripping. Sometimes users apply protection in addition to one of the other forms, when they wish to override those with a lower rating. It may be wise to have a selection of type four RCDs available, because connections made under damp conditions or using lengthy power cables are more prone to trip-out when any of the lower ratings of RCD are used; ratings as low as 10 mA are available.

### Number of poles and pole terminology

The number of poles represents the number of conductors that are interrupted when a fault condition occurs. RCDs used on single-phase AC supplies (two current paths), such as domestic power, are usually one- or two-pole designs, also known as single- and double-pole. A single-pole RCD interrupts only the energized conductor, while a double-pole RCD interrupts both the energized and return conductors. (In a single-pole RCD, the return conductor is usually anticipated to be at ground potential at all times and therefore safe on its own.)

RCDs with three or more poles can be used on three-phase AC supplies (three current paths) or to disconnect the neutral conductor as well, with four-pole RCDs used to interrupt three-phase and neutral supplies. Specially designed RCDs can also be used with both AC and DC power distribution systems.

The following terms are sometimes used to describe the manner in which conductors are connected and disconnected by an RCD:

- Single-pole or one-pole – the RCD will disconnect the energized wire only.
- Double-pole or two-pole – the RCD will disconnect both the energized and return wires.
- 1+N and 1P+N – non-standard terms used in the context of RCBOs, at times used differently by different manufacturers. Typically these terms may signify that the return (neutral) conductor is an isolating pole only, without a protective element (an unprotected but switched neutral), that the RCBO provides a conducting path and connectors for the return (neutral) conductor but this path remains uninterrupted when a fault occurs (sometimes known as "solid neutral"), or that both conductors are disconnected for some faults (such as RCD detected leakage) but only one conductor is disconnected for other faults (such as overload).

### Sensitivity

RCD sensitivity is expressed as the rated residual operating current, noted *IΔn*. Preferred values have been defined by the IEC, thus making it possible to divide RCDs into three groups according to their *IΔn* value:

- High sensitivity: 5, 10 and 30 mA (for direct-contact or life injury protection)
- Medium sensitivity: 100, 300, 500 and 1000 mA (for fire protection)
- Low sensitivity: 3, 10 and 30 A (typically for protection of machines)

The 5 mA sensitivity is typical for GFCI outlets.

### Trip time

Trip times are how quickly the device reacts to a fault. Devices must meet specific trip times when leakage current is sensed, with times ranging from tens to hundreds of milliseconds, as defined in national and international standards. The amount of time may vary depending upon the amount of current sensed, potentially having to react quicker to larger amounts, and to not react at all if the amount is sufficiently below the device rating.

In contrast to *general* types, *S-type* (aka *time delayed*) RCDs include a time delay – a minimum amount of time during which the device will not react – and typically higher maximum reaction time limits. This time delay allows for *discrimination* (aka *selectivity*) which is the principle that where two RCDs are installed in series, in the event of a fault it is desirable for the downstream device (which would typically cover a smaller portion of the installation) to trigger rather than the upstream in order to minimise how much of the electrical installation is disconnected. The upstream device should only trigger if the downstream device fails to react or if the fault is located between the two devices.

*Programmable* devices may be available for industrial use that can be adjusted to a range of sensitivity values and possibly response times.

### Type (types of leakage current detected)

IEC Standard 60755 (*General requirements for residual current operated protective devices*) defines the following types of RCD depending on the waveforms and frequency of the fault current:

**Type AC**

These trip on alternating sinusoidal residual current, suddenly applied or smoothly increasing.

**Type A**

These trip on both alternating sinusoidal residual current and residual pulsating direct current, suddenly applied or smoothly increasing.

**Type F**

These trip in the same conditions as Type A, and additionally:

- For composite residual currents, whether suddenly applied or slowly rising, intended for circuit supplied between line and neutral, or line and earthed middle conductor;
- For residual pulsating direct currents superimposed on smooth direct current.

**Type B**

These trip in the same conditions as Type F, and additionally:

- For residual sinusoidal alternating currents up to 1 kHz;
- For residual alternating currents superimposed on a smooth direct current;
- For residual pulsating direct currents superimposed on a smooth direct current;
- For residual pulsating rectified direct current which results from two or more phases;
- For residual smooth direct currents, whether suddenly applied or slowly increased, independent of polarity.

The BEAMA RCD Handbook notes that types F and B have been introduced because some designs of types AC and A can be disabled if a DC current is present that saturates the core of the detector.

### Directionality

RCDs may be uni-directional or bi-directional. Bi-directional devices have recently been introduced to address the problem of traditional uni-directional devices being unsuitable for certain configurations of home generation systems (PV).

### Surge current resistance

The surge current refers to the peak current an RCD is designed to withstand using a test impulse of specified characteristics. The IEC 61008 and IEC 61009 standards require that RCDs withstand a 200 A "ring wave" impulse. The standards also require RCDs classified as "selective" to withstand a 3000 A impulse surge current of specified waveform.

## Testing of correct operation

RCDs can and should be tested with a built-in test button to confirm basic functionality on a regular basis. If the switch mechanism is not operated for a long period then they can become liable to getting stuck. This is not generally a problem for an overcurrent circuit breaker because the force from the amount of current involved with those when they trip can be sufficient to break them free if stuck; however, an RCD is designed to trip on a very small amount of current which can exert far too weak a force to break a stuck switch free, thus failing to operate the safety device. By operating the test button regularly it can be seen whether or not a device is getting stuck. If so then manually operating the switch a few times may free it up temporarily and replacement can be considered. More thorough testing performed by a suitably competent person as part of a periodic test of an electrical installation might include checking what amount of current is required to make each device trip, and how quickly they trip, to check that they are performing within specification.

## Limitations

A residual-current circuit breaker cannot remove all risk of electric shock or fire. In particular, an RCD alone will not detect overload conditions, phase-to-neutral short circuits or phase-to-phase short circuits (see three-phase electric power). Over-current protection (fuses or circuit breakers) must be provided. Circuit breakers that combine the functions of an RCD with overcurrent protection respond to both types of fault. These are known as RCBOs and are available in 2-, 3- and 4-pole configurations. RCBOs will typically have separate circuits for detecting current imbalance and for overload current but use a common interrupting mechanism. Some RCBOs have separate levers for residual-current and over-current protection or use a separate indicator for ground faults.

An RCD helps to protect against electric shock when current flows through a person from a phase (line / hot) to earth. It cannot protect against electric shock when current flows through a person from phase to neutral or from phase to phase, for example where a finger touches both line and neutral contacts in a light fitting; a device cannot differentiate between current flow through an intended load from flow through a person, though the RCD may still trip if the person is in contact with the ground (earth), as some current may still pass through the person's finger and body to earth.

Whole installations on a single RCD, common in older installations in the UK, are prone to "nuisance" trips that can cause secondary safety problems with loss of lighting and defrosting of food. Frequently the trips are caused by deteriorating insulation on heater elements, such as water heaters and cooker elements or rings. Although regarded as a nuisance, the fault is with the deteriorated element and not the RCD: replacement of the offending element will resolve the problem, but replacing the RCD will not.

RCDs are not selective, for example when a ground fault occurs on a circuit protected by a 30 mA IΔn RCD in series with a 300 mA IΔn RCD either or both may trip. Special time-delayed types are available to provide selectivity in such installations.

In the case of RCDs that need a power supply, a dangerous condition can arise if the neutral wire is broken or switched off on the supply side of the RCD, while the corresponding line conductor remains uninterrupted. The tripping circuit needs power to work and does not trip when the power supply fails. Connected equipment will not work without a neutral, but the RCD cannot protect people from contact with the energized wire. For this reason circuit breakers must be installed in a way that ensures that the neutral wire cannot be switched off unless the line conductor is also switched off at the same time. Where there is a requirement for switching off the neutral wire, two-pole breakers (or four-pole for 3-phase) must be used. To provide some protection with an interrupted neutral, some RCDs and RCBOs are equipped with an auxiliary connection wire that must be connected to the earth busbar of the distribution board. This either enables the device to detect the missing neutral of the supply, causing the device to trip, or provides an alternative supply path for the tripping circuitry, enabling it to continue to function normally in the absence of the supply neutral.

Related to this, a single-pole RCD/RCBO interrupts the energized conductor only, while a double-pole device interrupts both the energized and return conductors. Usually this is a standard and safe practice, since the return conductor is held at ground potential anyway. However, because of its design, a single-pole RCD will not isolate or disconnect all relevant wires in certain uncommon situations, for example where the return conductor is not being held, as expected, at ground potential, or where current leakage occurs between the return and earth conductors. In these cases, a double-pole RCD will offer protection, since the return conductor would also be disconnected.

## History and nomenclature

The world's first high-sensitivity earth leakage protection system (i.e. a system capable of protecting people from the hazards of direct contact between a line conductor and earth), was a second-harmonic magnetic amplifier core-balance system, known as the magamp, developed in South Africa by Henri Rubin. Electrical hazards were of great concern in South African gold mines, and Rubin, an engineer at the company C.J. Fuchs Electrical Industries of Alberton Johannesburg, initially developed a cold-cathode system in 1955 which operated at 525 V and had a tripping sensitivity of 250 mA. Prior to this, core balance earth leakage protection systems operated at sensitivities of about 10 A.

The cold cathode system was installed in a number of gold mines and worked reliably. However, Rubin began working on a completely novel system with greatly improved sensitivity, and by early 1956, he had produced a prototype second-harmonic magnetic amplifier-type core balance system (South African Patent No. 2268/56 and Australian Patent No. 218360). The prototype magamp was rated at 220 V, 60 A and had an internally adjustable tripping sensitivity of 12.5–17.5 mA. Very rapid tripping times were achieved through a novel design, and this combined with the high sensitivity was well within the safe current–time envelope for ventricular fibrillation determined by Charles Dalziel of the University of California, Berkeley, United States, who had estimated electrical shock hazards in humans. This system, with its associated circuit breaker, included overcurrent and short-circuit protection. In addition, the original prototype was able to trip at a lower sensitivity in the presence of an interrupted neutral, thus protecting against an important cause of electrical fire.

Following the accidental electrocution of a woman in a domestic accident at the Stilfontein gold mining village near Johannesburg, a few hundred F.W.J. 20 mA magamp earth leakage protection units were installed in the homes of the mining village during 1957 and 1958. F.W.J. Electrical Industries, which later changed its name to FW Electrical Industries, continued to manufacture 20 mA single phase and three phase magamp units.

At the time that he worked on the magamp, Rubin also considered using transistors in this application, but concluded that the early transistors then available were too unreliable. However, with the advent of improved transistors, the company that he worked for and other companies later produced transistorized versions of earth leakage protection.

In 1961, Dalziel, working with Rucker Manufacturing Co., developed a transistorized device for earth leakage protection which became known as a ground fault circuit interrupter (GFCI), sometimes colloquially shortened to ground fault interrupter (GFI). This name for high-sensitivity earth leakage protection is still in common use in the United States.

In the early 1970s most North American GFCI devices were of the circuit breaker type. GFCIs built into the outlet receptacle became commonplace beginning in the 1980s. The circuit breaker type, installed into a distribution panel, suffered from accidental trips mainly caused by poor or inconsistent insulation on the wiring. False trips were frequent when insulation problems were compounded by long circuit lengths. So much current leaked along the length of the conductors' insulation that the breaker might trip with the slightest increase of current imbalance. The migration to outlet-receptacle–based protection in North American installations reduced the accidental trips and provided obvious verification that wet areas were under electrical-code–required protection. European installations continue to use primarily RCDs installed at the distribution board, which provides protection in case of damage to fixed wiring. In Europe socket-based RCDs are primarily used for retrofitting.

## Regulation and adoption

Regulations differ widely from country to country. A single RCD installed for an entire electrical installation provides protection against shock hazards to all circuits; however, any fault may cut all power to the premises. A solution is to create groups of circuits, each with an RCD, or to use an RCBO for each individual circuit.

### Australia

In Australia, residual current devices have been mandatory on power circuits since 1991 and on light circuits since 2000. In Queensland specifically, residual power devices have been compulsory for all new homes since 1992.

A minimum of two RCDs is required per domestic installation. All socket outlets and lighting circuits are to be distributed over circuit RCDs. A maximum of three subcircuits only, may be connected to a single RCD. In Australia, the RCD testing procedure must meet a set standard – this is the AS/NZS 3760:2010 in-service safety inspection and testing of electrical equipment.

### Austria

Austria regulated residual current devices in the ÖVE E8001-1/A1:2013-11-01 norm (most recent revision). It has been required in private housing since 1980. The maximum activation time must not exceed 0.4 seconds. It needs to be installed on all circuits with power plugs with a maximum leakage current of 30 mA and a maximum rated current of 16 A.

Additional requirements are placed on circuits in wet areas, construction sites and commercial buildings.

### Belgium

Belgian domestic installations are required to be equipped with a 300 mA residual current device that protects all circuits. Furthermore, at least one 30 mA residual current device is required that protects all circuits in "wet rooms" (e.g. bathroom, kitchen) as well as circuits that power certain "wet" appliances (washing machine, tumble dryer, dishwasher). Electrical underfloor heating is required to be protected by a 100 mA RCD. These RCDs must be of type A.

### Brazil

Since NBR 5410 (1997) residual current devices and grounding are required for new construction or repair in wet areas, outdoor areas, interior outlets used for external appliances, or in areas where water is more probable like bathrooms and kitchens.

### Denmark

Denmark requires 30 mA RCDs on all circuits that are rated for less than 20 A (circuits at greater rating are mostly used for distribution). RCDs became mandatory in 1975 for new buildings, and then for all buildings in 2008.

### France

According to the NF C 15-100 regulation (1911 -> 2002), a general RCD not exceeding 100 to 300 mA at the origin of the installation is mandatory. Moreover, all circuits must also include 30 mA protections in the user's distribution board, with each RCD protecting up to 8 circuit breakers, usually on the same DIN rail (electric panels of 1 to 4 DIN rails are the norm for residential). Before 1991, this 30 mA protection was mandatory only in rooms where there is water, high power or sensitive equipment (bathrooms, kitchens, IT...). The type of RCD required (A, AC, F) depends upon the type of the equipment that will be connected and the maximum power of the socket outlet. Minimal distances between electrical devices and water or the floor are described and mandatory.

### Germany

Since 1 May 1984, RCDs are mandatory for all rooms with a bath tub or a shower. Since June 2007, Germany requires the use of RCDs with a trip current of no more than 30 mA on sockets rated up to 32 A which are for general use. (DIN *Verband der Elektrotechnik, Elektronik und Informationstechnik* (VDE) 0100-410 Nr. 411.3.3). It is not allowed to use type "AC" RCDs since 1987, to be used to protect humans against electrical shocks. It must be Type "A" or type "B".

### India

According to Regulation 36 of the Electricity Regulations 1990

a) For a place of public entertainment, protection against earth leakage current must be provided by a residual current device of sensitivity not exceeding 10 mA.

b) For a place where the floor is likely to be wet or where the wall or enclosure is of low electrical resistance, protection against earth leakage current must be provided by a residual current device of sensitivity not exceeding 10 mA.

c) For an installation where hand-held equipment, apparatus or appliance is likely to be used, protection against earth leakage current must be provided by a residual current device of sensitivity not exceeding 30 mA.

d) For an installation other than the installation in (a), (b) and (c), protection against earth leakage current must be provided by a residual current device of sensitivity not exceeding 100 mA.

### Italy

The Italian law (n. 46 March 1990) prescribes RCDs with no more than 30 mA residual current (informally called "salvavita"—life saver, after early BTicino models, or differential circuit breaker for the mode of operation) for all domestic installations to protect all the lines. The law was recently updated to mandate at least two separate RCDs for separate domestic circuits. Short-circuit and overload protection has been compulsory since 1968.

### Malaysia

In the latest guidelines for electrical wiring in residential buildings (2008) handbook, the overall residential wiring need to be protected by a residual current device of sensitivity not exceeding 100 mA. Additionally, all power sockets need to be protected by a residual current device of sensitivity not exceeding 30 mA and all equipment in wet places (water heater, water pump) need to be protected by a residual current device of sensitivity not exceeding 10 mA.

### New Zealand

From January 2003, all new circuits originating at the switchboard supplying lighting or socket outlets (power points) in domestic buildings must have RCD protection. Residential facilities (such as boarding houses, hospitals, hotels and motels) will also require RCD protection for all new circuits originating at the switchboard supplying socket outlets. These RCDs will normally be located at the switchboard. They will provide protection for all electrical wiring and appliances plugged into the new circuits.

### North America

In North America socket-outlets located in places where an easy path to ground exists—such as wet areas and rooms with uncovered concrete floors—must be protected by a GFCI. The US *National Electrical Code* has required devices in certain locations to be protected by GFCIs since the 1960s. Beginning with underwater swimming pool lights (1968) successive editions of the code have expanded the areas where GFCIs are required to include: construction sites (1974), bathrooms and outdoor areas (1975), garages (1978), areas near hot tubs or spas (1981), hotel bathrooms (1984), kitchen counter sockets (1987), crawl spaces and unfinished basements (1990), near wet bar sinks (1993), near laundry sinks (2005), in laundry rooms (2014) and in kitchens, including areas away from water sources (2023).

GFCIs are commonly available as an integral part of a socket or a circuit breaker installed in the distribution panelboard. GFCI sockets invariably have rectangular faces and accept so-called Decora face plates, and can be mixed with regular outlets or switches in a multi-gang box with standard cover plates. In both Canada and the US older two-wire, ungrounded NEMA 1 sockets may be replaced with NEMA 5 sockets protected by a GFCI (integral with the socket or with the corresponding circuit breaker) in lieu of rewiring the entire circuit with a grounding conductor. In such cases the sockets must be labeled "no equipment ground" and "GFCI protected"; GFCI manufacturers typically provide tags for the appropriate installation description.

GFCIs approved for protection against electric shock trip at 5 mA within 25 ms. A GFCI device which protects equipment (not people) is allowed to trip as high as 30 mA of current; this is known as an Equipment Protective Device (EPD). RCDs with trip currents as high as 500 mA are sometimes deployed in environments (such as computing centers) where a lower threshold would carry an unacceptable risk of accidental trips. These high-current RCDs serve for equipment and fire protection instead of protection against the risks of electrical shocks.

In the United States the American Boat and Yacht Council requires both GFCIs for outlets and Equipment Leakage Circuit Interrupters (ELCI) for the entire boat. The difference is GFCIs trip on 5 mA of current whereas ELCIs trip on 30 mA after up to 100 ms. The greater values are intended to provide protection while minimizing nuisance trips.

### Norway

In Norway, it has been required in all new homes since 2002, and on all new sockets since 2006. This applies to 32 A sockets and below. The RCD must trigger after a maximum 0.4 seconds for 230 V circuits, or 0.2 seconds for 400 V circuits.

### South Africa

South Africa mandated the use of Earth Leakage Protection devices in residential environments (e.g. houses, flats, hotels, etc.) from October 1974, with regulations being refined in 1975 and 1976. Devices need to be installed in new premises and when repairs are carried out. Protection is required for power outlets and lighting, with the exception of emergency lighting that should not be interrupted. The standard device used in South Africa is indeed a hybrid of ELPD and RCCB.

### Switzerland

According to the NIBT regulation, the use of RCD type AC is forbidden (since 2010).

### Taiwan

Taiwan requires circuits of receptacles in washrooms, balconies, and receptacles in kitchen no more than 1.8 metres from the sink the use of earth leakage circuit breakers. This requirement also apply to circuit of water heater in washrooms and circuits that involves devices in water, lights on metal frames, public drinking fountains and so on. In principle, ELCBs should be installed on branch circuits, with trip current no more than 30 mA within 0.1 second according to Taiwanese law.

### Turkey

Turkey requires the use of RCDs with no more than 30 mA and 300 mA in all new homes since 2004. This rule was introduced in RG-16/06/2004-25494.

### United Kingdom

The current (18th) edition of the IET Electrical Wiring Regulations requires that all socket outlets in most installations have RCD protection, though there are exemptions. Non armoured cables buried in walls must also be RCD protected (again with some specific exemptions). Provision of RCD protection for circuits present in bathrooms and shower rooms reduces the requirement for supplementary bonding in those locations. Two RCDs may be used to cover the installation, with upstairs and downstairs lighting and power circuits spread across both RCDs. When one RCD trips, power is maintained to at least one lighting and power circuit. Other arrangements, such as the use of RCBOs, may be employed to meet the regulations. The new requirements for RCDs do not affect most existing installations unless they are rewired, the distribution board is changed, a new circuit is installed, or alterations are made such as additional socket outlets or new cables buried in walls.

RCDs used for shock protection must be of the 'immediate' operation type (not time-delayed) and must have a residual current sensitivity of no greater than 30 mA.

If spurious tripping would cause a greater problem than the risk of the electrical accident the RCD is supposed to prevent (examples might be a supply to a critical factory process, or to life support equipment), RCDs may be omitted, providing affected circuits are clearly labelled and the balance of risks considered; this may include the provision of alternative safety measures.

The previous edition of the regulations required use of RCDs for socket outlets that were liable to be used by outdoor appliances. Normal practice in domestic installations was to use a single RCD to cover all the circuits requiring RCD protection (typically sockets and showers) but to have some circuits (typically lighting) not RCD protected. This was to avoid a potentially dangerous loss of lighting should the RCD trip. Protection arrangements for other circuits varied. To implement this arrangement it was common to install a consumer unit incorporating an RCD in what is known as a split load configuration, where one group of circuit breakers is supplied direct from the main switch (or time delay RCD in the case of a TT earth) and a second group of circuits is supplied via the RCD. This arrangement had the recognised problems that cumulative earth leakage currents from the normal operation of many items of equipment could cause spurious tripping of the RCD, and that tripping of the RCD would disconnect power from all the protected circuits.
