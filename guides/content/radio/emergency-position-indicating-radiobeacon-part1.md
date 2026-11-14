---
id: radio/emergency-position-indicating-radiobeacon-part1
title: Emergency position-indicating radio beacon (part 1 of 3)
category: radio
tags: [radio, EPIRB, PLB, distress beacon, Cospas-Sarsat]
summary: "Distress beacons (EPIRB, PLB, ELT) and the Cospas-Sarsat satellite rescue system."
source: "https://en.wikipedia.org/w/index.php?title=Emergency_position-indicating_radio_beacon&oldid=1361140319"
source_title: Emergency position-indicating radio beacon (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Emergency position-indicating radio beacon' (rev 1361140319), Wikipedia, https://en.wikipedia.org/wiki/Emergency_position-indicating_radio_beacon. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
part_of: radio/emergency-position-indicating-radiobeacon
order: 1
---

# Emergency position-indicating radio beacon (part 1 of 3)

An emergency position-indicating radio beacon (EPIRB) is a type of emergency locator beacon for commercial and recreational boats; it is a portable, battery-powered radio transmitter used in emergencies to locate boaters in distress and in need of immediate rescue. In the event of an emergency, such as a ship sinking or medical emergency onboard, the transmitter is activated and transmits a continuous 406 MHz distress radio signal, which is used by search-and-rescue teams to quickly locate the emergency and render aid.

The distress signal is detected by satellites operated by an international consortium of rescue services, COSPAS-SARSAT, which can detect emergency beacons anywhere on Earth transmitting on the distress frequency of 406 MHz. The satellites calculate the position or utilize the GPS coordinates of the beacon and quickly pass the information to the appropriate local first responder organization, which performs the search and rescue. As the search and rescue team approach the search areas, they use direction finding (DF) equipment to locate the beacon using the 121.5 MHz homing signal, or in newer EPIRBs, the automatic identification system (AIS) location signal. The basic purpose of this system is to help rescuers find survivors within the so-called "golden day" (the first 24 hours following a traumatic event) during which the majority of survivors can usually be saved.

The feature distinguishing a modern EPIRB, often called GPIRB, from other types of emergency beacon is that it contains a GPS receiver and broadcasts its position, usually accurate within 100 m (330 ft), to facilitate location. Previous emergency beacons without a GPS can only be localized to within 2 km (1.2 mi) by the COSPAS satellites and rescuers relied heavily upon the 121.5 MHz homing signal to pin-point the beacons location as they arrived on scene.

The standard frequency of a modern EPIRB is 406 MHz. It is an internationally regulated mobile radiocommunication service that aids search-and-rescue operations to detect and locate distressed watercraft, aircraft, and people.

The first form of these beacons was the 121.5 MHz ELT, which was designed as an automatic locator beacon for crashed military aircraft. These beacons were first used in the 1950s by the U.S. military and were mandated for use on many types of commercial and general aviation aircraft beginning in the early 1970s. The frequency and signal format used by the ELT beacons was not designed for satellite detection, which resulted in a system with poor location detection abilities and long delays in detection of activated beacons. The satellite detection network was built after the ELT beacons were already in general use, with the first satellite not being launched until 1982, and even then, the satellites only provided detection, with location accuracy being roughly 20 km (12 mi). The technology was later expanded to cover use on vessels at sea (EPIRB), individual persons (PLB), and starting in 2016, maritime survivor locating devices (MSLD). All have migrated from using 121.500 MHz as their primary frequency to using 406 MHz, which was designed for satellite detection and location, however most models still broadcast a secondary signal on 121.5 MHz as well, as this helps rescue teams pinpoint the location of survivors once in their vicinity with more accuracy (within 2km) than the 406 MHz frequency allows on its own.

Since the inception of COSPAS-SARSAT in 1982, distress radio beacons have assisted in the rescue of over 50,000 people in more than 7,000 distress situations. In 2010 alone, the system provided information used to rescue 2,388 persons in 641 distress situations.

## Types of emergency locator beacons

The several types of emergency locator beacons are distinguished by the environment for which they were designed to be used:

- ELT (emergency locator transmitters) are carried on aircraft and are activated in the event of a crash.
  - Activated by G-switch (crash sensor) or manually by cockpit remote switch or ON switch on ELT.
- EPIRB (emergency position-indicating radio beacons) are carried on ships and boats, and signal maritime distress.
  - Activated by water when the beacon is out of the bracket or manually by the ON switch on the EPIRB.
- SEPIRB (submarine emergency position-indicating radio beacons) are EPIRBs designed only for use on submarines.
- SSAS (ship security alert systems) are used to indicate possible piracy or terrorism attacks discreetly on sea-going vessels.
  - Activated by discreet switch/button in the ship's bridge or cabin, or manually on the SSAS.
- PLB (personal locator beacons) are carried by individuals and intended to indicate a person in distress who is away from normal emergency services; e.g., 9-1-1. They are also used for crew-saving applications in shipping and lifeboats at terrestrial systems. In New South Wales, some police stations and the NSW National Parks & Wildlife Service provide personal locator beacons to hikers for no charge.
  - Activated manually by deploying antenna and pressing the ON button/switch.

Distress alerts transmitted from ELTs, EPIRBs, SSAS, and PLBs are received and processed by the International Cospas-Sarsat Programme, the international satellite system for search and rescue (SAR). These beacons transmit a 406 MHz distress signal every 50 seconds, varying over a span of 2.5 seconds to avoid multiple beacons always transmitting at the same time.

When manually activated, or automatically activated upon immersion or impact, such beacons send out a distress signal. The signals are monitored worldwide and the location of the distress is detected by non-geostationary satellites using the Doppler effect for trilateration, and in more recent EPIRBs, also by GPS.

Loosely related devices, including search and rescue transponders (SART), AIS-SART, avalanche transceivers, and RECCO do not operate on 406 MHz, thus are covered in separate articles.

## International COSPAS-SARSAT programme

Cospas-Sarsat is an international organization that has been a model of international cooperation, even during the Cold War. SARSAT means search-and-rescue satellite-aided tracking. COSPAS (*КОСПАС*) is an acronym for the Russian words "*COsmicheskayaSistema Poiska Avariynyh Sudov*" (Космическая Система Поиска Аварийных Судов), which translates to "space system for the search of vessels in distress". A consortium of USSR, the U.S., Canada, and France formed the organization in 1982. Since then, 29 other countries have joined.

The satellites used in the system include:

- LEOSAR or Low Earth Orbiting Search and Rescue Satellites
  - The LEOSAR system calculates the location of distress events using Doppler processing techniques. Doppler processing is based upon the principle that the frequency of the distress beacon, as "heard" by the satellite instrument, is affected by the relative velocity of the satellite with respect to the beacon. By monitoring the change of the beacon frequency of the received beacon signal and knowing the exact position of the satellite, the LUT is able to calculate the location of the beacon.
- GEOSAR or Geosynchronous Earth Orbiting Search and Rescue Satellites
  - As a GEOSAR satellite remains fixed relative to the Earth rotating with the earth around the equator, GEOSAR satellites utilize the GPS provided by the EPIRB, PLB, or ELT to provide rescuers with beacon position information.
- MEOSAR or Mid-Earth Orbiting Search and Rescue Satellites
  - The newest of the Cospas Sarsat satellites, detect EPIRB, PLB, and ELT distress signals in almost real-time (i.e within 5 minutes) including the beacons location with or without GPS.
  - The new MEOSAR system also provides the framework along with the Galileo Global Navigation Satellite System (GNSS) for EPIRBs, PLBs, and EPIRBs to utilize the new Return Link Service or RLS that provides a confirmation message from Search and Rescue back to the beacon to let the survivors know their distress message was confirmed.

Cospas-Sarsat defines standards for beacons, auxiliary equipment to be mounted on conforming weather and communication satellites, ground stations, and communications methods. The satellites communicate the beacon data to their ground stations, which forward it to main control centers of each nation that can initiate a rescue effort.

Cospas Sarsat Monitoring include:

- Local User Terminals (LUTs)
- Mission Control Centers (MCC)
- Rescue Coordination Center (RCC)

## Detection and location

A transmission is typically detected and processed in this manner:

1. The transmitter is activated, either automatically in a crash or after sinking, or manually by survivors of an emergency situation.
2. At least one satellite picks up the beacon's transmission.
3. The satellites transfer the beacon's signal to their respective ground control stations.
4. The ground stations process the signals and forward the data, including approximate location, to a national authority.
5. The national authority forwards the data to a rescue authority
6. The rescue authority uses its own receiving equipment afterwards to locate the beacon and commence its own rescue or recovery operations.

Once the satellite data is received, less than a minute is needed to forward them to any signatory nation. The primary means of detection and location is by the COSPAS-SARSAT satellites. However, additional means of location are frequently used. For example, the FAA requires that all pilots monitor 121.500 MHz whenever possible, and the USCG has a network of direction finder sites along the coastlines. The National Oceanic and Atmospheric Administration maintains a near-real-time map that shows SARSAT U.S. Rescues.

Several systems are in use, with beacons of varying expense, different types of satellites, and varying performance. Carrying even the oldest systems provides an immense improvement in safety over carrying none.

The types of satellites in the network are:

- LEOSAR
  - Support Doppler detection and reception of encoded position
  - Receivers are payloads on various Low Earth Orbit satellites
- MEOSAR
  - Medium Earth Orbiting Search and Rescue
  - Receivers are payloads on the U.S. GPS satellites, on the Russian GLONASS satellites, and on the European GALILEO satellites.
- GEOSAR
  - Supports only reception of encoded position
  - Receivers are payloads on various geosynchronous satellites, including some of the U.S. GOES weather satellites (including GOES-16).

When one of the COSPAS-SARSAT satellites detects a beacon, the detection is passed to one of the program's roughly 30 Mission Control Centers, such as USMCC (in Suitland, Maryland), where the detected location and beacon details are used to determine to which rescue coordination centre (for example, the U.S. Coast Guard's PACAREA RCC, in Alameda, California) to pass the alert.

### Beacon operation

#### GPS-based, registered

The 406-MHz beacons with GPS track with a precision of 100 m in the 70% of the world closest to the equator, and send a serial number so the responsible authority can look up phone numbers to notify the registrant (e.g., next-of-kin) in four minutes.

The GPS system permits stationary, wide-view geosynchronous communications satellites to enhance the Doppler position received by low Earth orbit satellites. EPIRB beacons with built-in GPS are usually called GPIRBs, for GPS position-indicating radio beacon or global position-indicating radio beacon.

However, rescue cannot begin until a Doppler track is available. The COSPAS-SARSAT specifications say that a beacon location is not considered "resolved" unless at least two Doppler tracks match or a Doppler track confirms an encoded (GPS) track. One or more GPS tracks are not sufficient.

#### High-precision registered

An intermediate technology 406-MHz beacon (now mostly obsolete in favor of GPS-enabled units) has worldwide coverage, locates within 2 km (12.5 km^2 search area), notifies kin and rescuers in 2 hours maximum (46 min average), and has a serial number to look up phone numbers, etc. This can take up to two hours because it has to use moving weather satellites to locate the beacon. To help locate the beacon, the beacon's frequency is controlled to 2 parts per billion, and its power is five watts.

Both of the above types of beacons usually include an auxiliary 25-milliwatt beacon at 121.5 MHz to guide rescue aircraft.

#### Traditional ELT, unregistered

The oldest, cheapest beacons are aircraft ELTs that send an anonymous warble on the aviation band distress frequency at 121.5 MHz. The frequency is often routinely monitored by commercial aircraft, but has not been monitored by satellite since Feb. 1, 2009.

These distress signals could be detected by satellite over only 60% of the earth, required up to 6 hours for notification, located within 20 km (12 mi) (search area of 1200 km^2), were anonymous, and could not be located well because their frequency is only accurate to 50 parts per million and the signals were broadcast using only 75–100 milliwatts of power. Coverage was partial because the satellite had to be in view of both the beacon and a ground station at the same time; the satellites did not store and forward the beacon's position. Coverage in polar and Southern Hemisphere areas was poor.

False alarms were common, as the beacon transmitted on the aviation emergency frequency, with interference from other electronic and electrical systems. To reduce false alarms, a beacon was confirmed by a second satellite pass, which could easily slow confirmation of a 'case' of distress to as much as 4 hours (although in rare circumstances, the satellites could be positioned such that immediate detection becomes possible.)

#### Location by Doppler (without GPS)

The Cospas-Sarsat system was made possible by Doppler processing. Local-user terminals (LUTs) detecting nongeostationary satellites interpret the Doppler frequency shift heard by LEOSAR and MEOSAR satellites as they pass over a beacon transmitting at a fixed frequency. The interpretation determines both bearing and range. The range and bearing are measured from the rate of change of the heard frequency, which varies both according to the path of the satellite in space and the rotation of the earth. This triangulates the position of the beacon. A faster change in the Doppler indicates that the beacon is closer to the satellite's orbit. If the beacon is moving toward or away from the satellite track due to the Earth's rotation, it is on one side or other of the satellite's path. Doppler shift is zero at the closest point of approach between the beacon and the orbit.

If the beacon's frequency is more precise, it can be located more precisely, saving search time, so modern 406-MHz beacons are accurate to 2 parts per billion, giving a search area of only 2 km^2, compared to the older beacons accurate to 50 parts per million that had 200 km^2 of search area.

To increase the useful power, and handle multiple simultaneous beacons, modern 406-MHz beacons transmit in bursts, and remain silent for about 50 seconds.

Russia developed the original system, and its success drove the desire to develop the improved 406-MHz system. The original system was a brilliant adaptation to the low-quality beacons, originally designed to aid air searches. It used just a simple, lightweight transponder on the satellite, with no digital recorders or other complexities. Ground stations listened to each satellite as long as it was above the horizon. Doppler shift was used to locate the beacon(s). Multiple beacons were separated when a computer program analysed the signals with a fast Fourier transform. Also, two satellite passes per beacon were used. This eliminated false alarms by using two measurements to verify the beacon's location from two different bearings. This prevented false alarms from VHF channels that affected a single satellite. Regrettably, the second satellite pass almost doubled the average time before notification of the rescuing authority. However, the notification time was much less than a day.

### Satellites

Receivers are auxiliary systems mounted on several types of satellites. This substantially reduces the program's cost. The weather satellites that carry the SARSAT receivers are in "ball of yarn" orbits, inclined at 99 degrees. The longest period that all satellites can be out of line-of-sight of a beacon is about two hours. The first satellite constellation was launched in the early 1970s by the Soviet Union, Canada, France and the United States.

Some geosynchronous satellites have beacon receivers. Since the end of 2003, there are four such geostationary satellites (GEOSAR) that cover more than 80% of the surface of the earth. As with all geosynchronous satellites, they are located above the equator. The GEOSAR satellites do not cover the polar caps. Since they see the Earth as a whole, they see the beacon immediately, but have no motion, and thus no Doppler frequency shift to locate it. However, if the beacon transmits GPS data, the geosynchronous satellites give nearly instantaneous response.

## Search-and-rescue response

Emergency beacons operating on 406 MHz transmit a unique 15-, 22-, or 30-character serial number called a hex code. When the beacon is purchased, the hex code should be registered with the relevant national (or international) authority. After one of the mission control centers has detected the signal, this registration information is passed to the rescue coordination center, which then provides the appropriate search-and-rescue agency with crucial information, such as:

- phone numbers to call
- a description of the vessel, aircraft, vehicle, or person (in the case of a PLB)
- the home port of a vessel or aircraft
- any additional information that may be useful to SAR agencies

Registration information allows SAR agencies to start a rescue more quickly. For example, if a shipboard telephone number listed in the registration is unreachable, it could be assumed that a real distress event is occurring. Conversely, the information provides a quick and easy way for the SAR agencies to check and eliminate false alarms (potentially sparing the beacon's owner from significant false alert fines).

An unregistered 406-MHz beacon still carries some information, such as the manufacturer and serial number of the beacon, and in some cases, an MMSI or aircraft tail number/ICAO 24-bit address. Despite the clear benefits of registration, an unregistered 406-MHz beacon is very substantially better than a 121.5-MHz beacon, because the hex code received from a 406-MHz beacon confirms the authenticity of the signal as a real distress signal.

Beacons operating on 121.5 MHz and 243.0 MHz only simply transmit an anonymous siren tone, thus carry no position or identity information to SAR agencies. Such beacons now rely solely on the terrestrial or aeronautical monitoring of the frequency.

### Responsible agencies

RCCs are responsible for a geographic area, known as a "search-and-rescue region of responsibility" (SRR). SRRs are designated by the International Maritime Organization and the International Civil Aviation Organization. RCCs are operated unilaterally by personnel of a single military service (e.g. an air force, or a navy) or a single civilian service (e.g. a national police force, or a coast guard).

#### Americas

These international search-and-rescue points of contact receive SAR alerts from the USMCC.

| SPOC | SRR Name | Geographic Coverage | SAR Agency |
|---|---|---|---|
| Argentina - Servicio de Alerta de Socorro Satelital (SASS) | ARMCC |   |   |
| Bermuda Maritime Operations Centre | BERMUDASP |   |   |
| Central American Corporation for Navigation Area Services | COCESNA |   |   |
| Colombia | COLMSP |   |   |
| Dominican Republic | DOMREPSP |   |   |
| Ecuador | ECSP |   |   |
| Guyana | GUYSP |   |   |
| Mexico | MEXISP |   |   |
| Mexico Telecommunications | MEXTEL |   |   |
| Netherlands Antilles | NANTSP |   |   |
| Panama | PANSP |   |   |
| Trinidad and Tobago | TTSP |   |   |
| Venezuela | VZMCC |   |   |
| Bolivia | BOLSP |   |   |
| Chile RCC | ChileRCC |   |   |
| Paraguay | PARSP |   |   |
| Uruguay | URSP |   |   |

##### United States

The U.S. NOAA operates the U.S. Mission Control Center (USMCC) in Suitland, Maryland. It distributes beacon signal reports to one or more of these RCCs:

**United States SPOCs**

| RCC | SRR name | Geographic coverage | SAR agency | Phone number |
|---|---|---|---|---|
| Air Force Rescue Coordination Center | AFRCC | Land-based emergency signals in the lower 48 states | United States Air Force Auxiliary Civil Air Patrol |   |
| Alaska Air National Guard operates the Alaska Rescue Coordination Center | AKRCC | Alaskan inland areas | On-shore beacons are investigated by local search-and-rescue services in Alaska. |   |
| U.S. Coast Guard |   | The Coast Guard investigates offshore beacons and rescues victims. |   |   |
| Coast Guard Atlantic Area | LANTAREA |   |   | 757-398-6700 |
| District 1: Boston, MA (RCC Boston) | CGD01 |   |   | (617)223-8555 |
| District 5: Portsmouth, VA (RCC Norfolk) | CGD05 |   |   | (757)398-6231 |
| District 7: Miami, FL (RCC Miami) | CGD07 |   |   | (305)415-6800 |
| District 8: New Orleans, LA (RCC New Orleans) | CGD08 |   |   | (504)589-6225 |
| District 9: Cleveland, OH (RCC Cleveland) | CGD09 |   |   | (216)902-6117 |
| District 11: Alameda, CA (RCC Alameda and Pacific SAR Coordinator) | PACAREA |   |   | (510)437-3701 |
| District 13: Seattle, WA (RCC Seattle) | CGD13 |   |   | (206)220-7001 |
| District 14: Honolulu, HI (RCC Honolulu; operated as JRCC with DOD) | CGD14 |   |   | (808)535-3333 |
| District 17: Juneau, AK (RCC Juneau) | CGD17 |   |   | (907)463-2000 |
| U.S. Coast Guard Sector San Juan (RSC) (sub-sector of RCC Miami) | SANJN |   |   | (787)289-2042 |
| U.S. Coast Guard Sector Guam (RSC) (coordinates SAR under RCC Honolulu) | MARSEC |   |   | (671)355-4824 |

The US Coast Guard web page for EPIRBs states: "You may be fined for false activation of an unregistered EPIRB. The US Coast Guard routinely refers cases involving the nondistress activation of an EPIRB (e.g., as a hoax, through gross negligence, carelessness, or improper storage and handling) to the Federal Communications Commission. The FCC will prosecute cases based upon evidence provided by the Coast Guard, and will issue warning letters or notices of apparent liability for fines up to $10,000."

##### Canada

The Canadian Mission Control Centre receives and distributes distress alerts.

In Canada, the Canadian Coast Guard and Canadian Forces Search and Rescue (Royal Canadian Air Force and Royal Canadian Navy) are partners in Joint Rescue Co-ordination Centres; CCG operates Maritime Rescue Subcentres to offload work from JRCC.

| RCC | SRR Name | Geographic Coverage | SAR Agency |
|---|---|---|---|
| Joint Rescue Coordination Centre Halifax | HALIFAX | Halifax Search and Rescue Region |   |
| Maritime Rescue Sub-Centre Quebec | QuebecCity | the St. Lawrence River within the province of Quebec the northern and western waters of the Gulf of St. Lawrence within the province of Quebec the navigable estuary portion of the Saguenay River the Richelieu River within the province of Quebec the southern portion of the Ottawa River downstream from the Carillon Generating Station |   |
| Joint Rescue Coordination Centre Trenton | TRENTON | Trenton Search and Rescue Region. AIRCOM also operates the Canadian Mission Control Centre (CMCC) from JRCC Trenton |   |
| Joint Rescue Coordination Centre Victoria | VICTORIA | Victoria Search and Rescue Region |   |
| Maritime Rescue Sub-Centre St. John's |   | waters surrounding the province of Newfoundland and Labrador |   |

#### Europe

##### United Kingdom

The United Kingdom, the Department for Transport, Maritime and Coastguard Agency operates the Mission Control Centre (UKMCC), which receives and distributes distress alerts.

In the UK, the Distress and Diversion Cell of the Royal Air Force provides continuous monitoring of 121.5 MHz and 243.0 MHz, with autotriangulation from a network of terrestrial receivers on both frequencies.

#### Russia

In Russia, operations are supported by the Federal State Unitary Enterprise Morsvyazsputnik.

#### Asia

In Hong Kong, operations are supported by the Hong Kong Marine Department's Hong Kong Maritime Rescue Co-ordination Centre (MRCC)

In India, operations are supported by the Indian Space Research Organisation (ISRO) and by the Indian Coast Guard's Maritime Rescue Coordination Centre Mumbai (MRCC)

In China, operations are supported by the Maritime Safety Administration, Bureau of Harbour Superintendency.

In Japan, operations are supported by the Japan Coast Guard

In Vietnam, operations are supported by the Ministry of Transport, Vietnam Maritime Administration (VINAMARINE).

In Singapore, operations are supported by the Civil Aviation Authority of Singapore.

In the Republic of Korea, operations are supported by the Korea Coast Guard.

In Indonesia, operations are supported by the National SAR Agency of Indonesia (BASARNAS).

In Taiwan, operations are supported by the International Telecommunication Development Company (ITDC)

## Phase-out of 121.5 MHz satellite alerting service

Because of the extremely high numbers of false alerts on the 121.500 MHz frequency (over 98% of all COSPAS-SARSAT alerts), the IMO eventually requested a termination of COSPAS-SARSAT processing of 121.5 MHz signals. The ICAO Council also agreed to this phase-out request, and the COSPAS-SARSAT Council decided that future satellites would no longer carry the 121.5 MHz search and rescue repeater (SARR). Since 1 February 2009, only 406 MHz beacons are detected by the international Cospas-Sarsat SAR satellite system. This affects all maritime beacons (EPIRBs), all aviation beacons (ELTs) and all personal beacons (PLBs). In other words, Cospas-Sarsat has ceased satellite detection and processing of 121.5/243 MHz beacons. These older beacons are now only detectable by ground-based receivers and aircraft.

EPIRBs that do not transmit on 406 MHz are banned on boats in the United States and in many other jurisdictions. More information about the switch to 406 MHz is available on Cospas-Sarsat's 121.5/243 Phase-Out page.

Despite the switch to 406 MHz, pilots and ground stations are encouraged to continue to monitor for transmissions on the emergency frequencies, as most 406 MHz beacons are required to be equipped with 121.5 "homers." Furthermore, the 121.5 MHz frequency remains the official global VHF aircraft voice distress frequency.

### FAA transition status

In a Safety Recommendation released September 2007, the U.S. National Transportation Safety Board once again recommended that the U.S. FAA require all aircraft have 406 MHz ELTs. They first recommended this back in 2000 and after vigorous opposition by AOPA, the FAA declined to do so. Citing two recent accidents, one with a 121.5 MHz ELT and one with a 406 MHz ELT, the NTSB concludes that switching all ELTs to 406 MHz is a necessary goal to work towards.

NASA has conducted crash tests with small airplanes to investigate how ELTs perform.
