---
id: signaling/wp2-avalanche-transceiver
title: Avalanche transceiver
category: signaling
tags: [avalanche transceiver, avalanche beacon, signals]
summary: An avalanche transceiver or avalanche beacon is a type of emergency locator beacon, a radio transceiver (a transmitter and receiver in one unit) operating at 457 kHz for the purpose of finding people buried under snow.
source: https://en.wikipedia.org/w/index.php?title=Avalanche_transceiver&oldid=1383095864
source_title: Wikipedia: Avalanche transceiver
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Avalanche transceiver", Wikipedia, The Free Encyclopedia (revision 1383095864), https://en.wikipedia.org/w/index.php?title=Avalanche_transceiver&oldid=1383095864 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Avalanche transceiver

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **avalanche transceiver** or **avalanche beacon** is a type of emergency locator beacon, a radio transceiver (a transmitter and receiver in one unit) operating at 457 kHz for the purpose of finding people buried under snow. They are widely carried by skiers, particularly back country skiers for use in case a skier is buried by an avalanche. Before setting out on an expedition, all the members of a group activate their transceivers in the transmit mode, causing the device to emit low-power pulsed radio signals during the trip. Following an avalanche, if some members of the ski party are buried, the others may switch their transceivers from transmit into receive mode, allowing use as a radio direction finding device to search for signals coming from the lost skiers. The avalanche beacon is an active device powered by batteries; a ski suit may also contain a passive RECCO transponder sewn into the clothing.

Early avalanche transceivers transmitted at 2.275 kHz. In 1986, the international frequency standard of 457 kHz was adopted, and this remains the standard today. Many companies manufacture transceivers that comply with this standard.

An avalanche transceiver is not considered a preventive measure against possible avalanche burial, but rather it is a way to reduce the amount of time victims remain buried under the snow, which makes it more likely to save their lives.

## Types of beacons

There are two types of avalanche beacons: digital and analog. They both adhere to the international standard as described above, and only differ in the method(s) used to indicate to the user where the buried beacon is located. Most beacons currently being sold are digital, because of their enhanced ease of use and higher recovery rates.

### Analog

The original avalanche beacon was an analog beacon which transmitted the pulsed signal as an audible tone to the user. The tone gets louder when the user is closer to the transmitting beacon. These beacons have also been augmented with LEDs that provide a visual indication of signal strength, and earpieces to increase the ability of the listener to hear the tone.

### Digital

Digital transceivers take the strength of the signal and the emitted dipole flux pattern and compute distance and direction to the buried transceiver. To calculate the emitted dipole flux pattern, a digital transceiver must have at least two antennas, although most modern transceivers have three. The digital beacons will then indicate the direction to the victim's beacon as an arrow on the display, and provide audio cues such as varying pitch or frequency. Most low- to mid-range beacons have a segmented arrow capable of pointing in five to eight forward directions only, displaying a 'U-Turn' indicator if the user is traveling away from the victim. Higher end beacons are equipped with a digital compass and free-flowing arrow, facilitating more exact direction finding. In addition, many higher end beacons can point to victims 360°, including behind the user if the user is moving in the wrong direction. Many digital beacons are also capable of being used in analog mode for more advanced rescuers, or to enhance reception range.

## W-Link

Several high-end digital beacons are also equipped with a secondary "supplementary" frequency referred to as W-Link. This frequency broadcasts additional details to other transceivers capable of receiving the W-Link signal. Advertised brand-independent features of W-Link include:

- The ability to resolve multiple, complex burial situations by better differentiating individual transceivers
- More reliable estimation of the number of burials
- More reliable and quicker marking/unmarking of victims (i.e. forcing the transceiver to ignore an already found victim)
- More reliable selection of victim search, as the closest victim may not be the easiest to recover
- Ability to transmit and receive additional data including wearer's vital signs or identification

### Vitals detection

Beacons transmitting on the W-Link frequency send a specific device code to assist in isolating and pinpointing multiple signals, and facilitate all of the above features. Certain beacons also detect micro-movements in the user, including the minuscule movement generated by a heart beat. These beacons will transmit this information across the W-Link frequency, so that any user with another W-Link capable transceiver can determine whether or not a buried victim is alive, and formulate rescue triage based on that situation. The idea behind this is that if everyone in a group is wearing a vitals-capable W-Link transceiver and some group members are buried in an avalanche, the remaining group members will be able to determine which of the buried victims are still alive, and focus rescue efforts on those members.

To compensate for group members without vitals-capable beacons (including lower-end beacons without W-Link and W-Link capable beacons without vitals detection), the rescuer's W-Link beacon will often display two indicators on the display for each victim. One indicator shows that a victim's beacon is transmitting on the W-Link frequency while another shows that the victim is moving. This helps mitigate the potential risk of mis-categorizing an alive victim as dead because their beacon is not transmitting vitals data, and thus the rescuer does not see the "alive" indicator on their transceiver.

### Controversies of W-Link

As a universal rule, W-Link capable transceivers do not display personally identifiable characteristics of the buried victims, although they are capable of doing this. This is to eliminate conflicts of interest in rescue situations where a rescuer may choose to save one person before (or instead of) another, even if another person is closer or easier to rescue. By not identifying any buried victims, the rescuer is not left with a decision of which person to save, and are spared the moral implications and consequences of their choices. Critics of the W-Link system, especially of the vitals-detecting transceivers, argue that even without offering personally identifiable information, the W-Link transceivers still present moral implications, and complicate rescue efforts because these transceivers will distinguish between W-Link capable and incapable victims with an indicator on the display, further segregating victims with a vitals-data capable beacons. Critics argue that this leads to an unfair distribution of rescue resources and personnel to persons with higher-end or newer transceivers, and deprives everyone of an equal chance for rescue. For this reason transceiver manufacturer Arva Equipment has elected to omit received vitals data from being displayed on their Link transceiver, although the beacon does transmit them. A scenario that W-Link critics will use to exemplify their point is as follows:

> A four-person group goes on a backcountry tour into avalanche terrain. A husband and his wife are both equipped with the same W-Link, vitals-sensing transceiver. They just met the other two group members the day before. One of them has a basic digital beacon, and the other has a modern, digital W-Link beacon that does not transmit vitals data. Along their tour, three of the group members are caught in an avalanche, leaving only the husband to rescue them. He quickly activates his transceiver and it gets a lock on all three victims. The display shows two beacons 10 and 12 meters directly in front of him, one with W-Link signal and one with regular signal only. It also shows one beacon 33 meters behind him transmitting W-Link and vitals data saying the victim is alive.

In this scenario, it is clear to distinguish between all three victims even though the transceiver does not display their names; his wife is 33 meters behind him, while the other two people he just met are much closer, and close together, as well. The moral implications are that the man will either choose to save his wife, likely at the expense of the other two group members' lives, or he will choose to rescue one or both of the other group members, allowing his wife to die. In a rescue situation without the additional information, a competent rescuer would make the rational choice of initially rescuing the two closer victims. If the husband makes this choice, he will have chosen to not put his wife's life first, possibly resulting in her death, and will have to live with that for the rest of his life.

### Frequencies and technical information

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of W-Link, Search techniques.)_
