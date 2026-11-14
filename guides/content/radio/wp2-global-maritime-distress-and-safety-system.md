---
id: radio/wp2-global-maritime-distress-and-safety-system
title: Global Maritime Distress and Safety System
category: radio
tags: [global maritime distress and safety system, gmdss, radio]
summary: The Global Maritime Distress and Safety System (GMDSS) is a worldwide system for automated emergency signal communication for ships at sea developed by the United Nations' International Maritime Organization (IMO) as part of the SOLAS Convention.
source: https://en.wikipedia.org/w/index.php?title=Global_Maritime_Distress_and_Safety_System&oldid=1362625517
source_title: Wikipedia: Global Maritime Distress and Safety System
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Global Maritime Distress and Safety System", Wikipedia, The Free Encyclopedia (revision 1362625517), https://en.wikipedia.org/w/index.php?title=Global_Maritime_Distress_and_Safety_System&oldid=1362625517 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Global Maritime Distress and Safety System

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **Global Maritime Distress and Safety System** (**GMDSS**) is a worldwide system for automated emergency signal communication for ships at sea developed by the United Nations' International Maritime Organization (IMO) as part of the SOLAS Convention.

It is a set of safety procedures, types of equipment, and communication protocols used for safety and rescue operations of the distressed ships, boats, and aircraft. It is supplemental to the International Convention on Maritime Search and Rescue (ICMSaR) adopted in 1979 and provides basis for the communication.

GMDSS consists of several systems which are intended to perform the following functions: alerting (including position determination of the ship in distress) ships in the vicinity and ashore authorities, search and rescue coordination, locating (homing), maritime safety information broadcasts, general communications, and bridge-to-bridge communications. Specific radio carriage requirements depend upon the ship's area of operation, rather than its tonnage. The system also provides redundant means of distress alerting, and emergency sources of power.

Recreational vessels do not need to comply with GMDSS radio carriage requirements. Offshore vessels may elect to equip themselves further. Vessels under 300 gross tonnage (GT) are not subject to GMDSS requirements.

## Components of GMDSS

The main types of equipment used in GMDSS are:

### Emergency position-indicating radio beacon (EPIRB)

Cospas-Sarsat is an international satellite-based search and rescue system, established by Canada, France, the United States, and Russia. These four countries jointly helped develop the 406 MHz Emergency Position-Indicating Radio Beacon (EPIRB), an element of the GMDSS designed to operate with Cospas-Sarsat system. These automatic-activating EPIRBs, now required on SOLAS ships, commercial fishing vessels, and all passenger ships, are designed to transmit to alert rescue coordination centers via the satellite system from anywhere in the world. The original COSPAS/SARSAT system used polar orbiting satellites, but in recent years the system has been expanded to also include 4 geostationary satellites and 50 medium orbiting navigation satellites. Newest designs incorporate GPS receivers to transmit highly accurate positions (within about 20 metres) of the distress position. EPIRB manufacturers offer AIS (Automatic Identification System) enabled beacons. 406 MHz EPIRBs transmit an ID which is linked to a database of information about the vessel. Typically a vessel has a unique ID: MMSI.
The serviceability of these items is checked monthly and annually and they have limited battery shelf life, between two and five years using mostly lithium-type batteries.

### NAVTEX

Navtex is an international, automated system for instantly distributing maritime safety information (MSI) which includes navigational warnings, weather forecasts and weather warnings, search and rescue notices and similar information to ships. A small, low-cost and self-contained "smart" printing radio receiver is installed on the bridge, or the place from where the ship is navigated, and checks each incoming message to see if it has been received during an earlier transmission, or if it is of a category of no interest to the ship's master. The frequency of transmission of these messages is 518 kHz in English, while 490 kHz is sometime used to broadcast in a local language.
The messages are coded with a header code identified by the using single letters of the alphabet to represent broadcasting stations, type of messages, and followed by two figures indicating the serial number of the message.
For example:
**FA56** where **F** is the ID of the transmitting station, **A** indicates the message category *navigational warning*, and **56** is the consecutive message number.

### Satellite

As of January 2020, there are two certified providers of GMDSS satellite services: Inmarsat, with several of their commsats in equatorial geosynchronous orbit, and Iridium Communications, with their 66-satellite constellation in low Earth orbit (LEO) that can cover higher latitudes and operate with lower communications latency. The certification of Iridium in 2020 ended a monopoly on the provision of the satellite-based portion of maritime distress services that had previously been held by Inmarsat since the system became operational in 1999.

Satellite systems operated by the Inmarsat company, overseen by the International Mobile Satellite Organization (IMSO) are important elements of the GMDSS. The types of Inmarsat ship earth station terminals recognized by the GMDSS are: Inmarsat C and F77. Inmarsat F77, an updated version of the now redundant Inmarsat A and B, provide ship-to-shore, ship-to-ship and shore-to-ship telephone, telex and high-speed data services, including a distress priority telephone and telex service to and from rescue coordination centers. Fleet 77 fully supports the Global Maritime Distress and Safety System (GMDSS) and includes advanced features such as emergency call prioritisation. Unfortunately Fleet 77 has an end of life scheduled for 1 December 2020. No definite alternatives are currently defined. The Inmarsat C provides ship/shore, shore/ship and ship/ship store-and-forward data and email messaging, the capability for sending preformatted distress messages to a rescue coordination center, and the Inmarsat C SafetyNET service. The Inmarsat C SafetyNET service is a satellite-based worldwide maritime safety information broadcast service of high seas weather warnings, NAVAREA navigational warnings, radionavigation warnings, ice reports and warnings generated by the USCG-conducted International Ice Patrol, and other similar information not provided by NAVTEX. SafetyNET works similarly to NAVTEX in areas outside NAVTEX coverage.

Inmarsat C equipment is relatively small and lightweight, and costs much less than a F77 station. A F77 ship earth station requires a relatively large gyro-stabilized unidirectional antennas; the antenna size of the Inmarsat C is much smaller and is omnidirectional. Inmarsat C being a low power system allows for its operation from the vessels emergency 24volt battery supply under emergency conditions. This coupled to the omni directional antenna arrangements allow for a guaranteed response to a distress alert between 76 degrees North and 76 degrees South (Sea area A3).

Under a cooperative agreement with the National Oceanic and Atmospheric Administration (NOAA), combined meteorological observations and AMVER reports can now be sent to both the USCG AMVER Center, and NOAA, using an Inmarsat C ship earth station, at no charge.

SOLAS now requires that Inmarsat C equipment have an integral satellite navigation receiver, or be externally connected to a satellite navigation receiver. That connection will ensure accurate location information to be sent to a rescue coordination center if a distress alert is ever transmitted.

Also, the new LRIT long-range tracking systems are upgraded via GMDSS Inmarsat C, which are also compliant, along with inbuilt SSAS, or ship security alert system. SSAS provides a means to covertly transmit a security alert distress message to local authorities in the event of a mutiny, pirate attack, or other hostile action towards the vessel or its crew.

### High frequency

A GMDSS system may include high-frequency (HF) radiotelephone and radiotelex (narrow-band direct printing) equipment, with calls initiated by digital selective calling (DSC). Worldwide broadcasts of maritime safety information can also be made on HF narrow-band direct printing channels. All ships trading in Sea area A4 must carry HF DSC and NBDP equipment which can also operate from the ships reserve energy supply (typically a 24 Volt battery supply). This HF provision is necessary as the Inmarsat coverage does not extend to the polar regions.

### Search and rescue transponder (SART)

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Components of GMDSS, GMDSS sea areas, Licensing of operators (USA), Licensing of operators (UK and Europe).)_
