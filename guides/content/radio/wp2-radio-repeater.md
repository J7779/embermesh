---
id: radio/wp2-radio-repeater
title: Radio repeater
category: radio
tags: [radio repeater, radio]
summary: A radio repeater is a combination of a radio receiver and a radio transmitter that receives a signal and retransmits it, so that two-way radio signals can cover longer distances.
source: https://en.wikipedia.org/w/index.php?title=Radio_repeater&oldid=1317590737
source_title: Wikipedia: Radio repeater
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Radio repeater", Wikipedia, The Free Encyclopedia (revision 1317590737), https://en.wikipedia.org/w/index.php?title=Radio_repeater&oldid=1317590737 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Radio repeater

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **radio repeater** is a combination of a radio receiver and a radio transmitter that receives a signal and retransmits it, so that two-way radio signals can cover longer distances. A repeater sited at a high elevation can allow two mobile stations, otherwise out of line-of-sight propagation range of each other, to communicate. Repeaters are found in professional, commercial, and government mobile radio systems and also in amateur radio.

Repeater systems use two different radio frequencies; the mobiles transmit on one frequency, and the repeater station receives those transmissions and transmits on a second frequency. Since the repeater must transmit at the same time as the signal is being received, and may even use the same antenna for both transmitting and receiving, frequency-selective filters are required to prevent the receiver from being overloaded by the transmitted signal. Some repeaters use two different frequency bands to provide isolation between input and output or as a convenience.

In a communications satellite, a transponder serves a similar function, but the transponder does not necessarily demodulate the relayed signals.

## Full duplex operation

A repeater is an automatic radio-relay station, usually located on a mountain top, tall building, or radio tower. It allows communication between two or more bases, mobile or portable stations that are unable to communicate directly with each other due to distance or obstructions between them.

The repeater receives on one radio frequency (the "input" frequency), demodulates the signal, and simultaneously re-transmits the information on its "output" frequency. All stations using the repeater transmit on the repeater's input frequency and receive on its output frequency. Since the repeater is usually located at an elevation higher than the other radios using it, its range is greatly extended.

Because the transmitter and receiver are on at the same time, isolation must exist to keep the repeater's own transmitter from degrading the repeater receiver. If the repeater transmitter and receiver are not isolated well, the repeater's own transmitter *desensitizes* the repeater receiver. The problem is similar to being at a rock concert and not being able to hear the weak signal of a conversation over the much stronger signal of the band.

In general, isolating the receiver from the transmitter is made easier by maximizing, as much as possible, the separation between input and output frequencies.

When operating through a repeater, mobile stations must transmit on a different frequency than the repeater output. Although the repeater site must be capable of simultaneous reception and transmission (on two different frequencies), mobile stations can operate in one mode at a time, alternating between receiving and transmitting; so, mobile stations do not need the bulky and costly filters required at a repeater site. Mobile stations may have an option to select a "talk around" mode to transmit and receive on the same frequency; this is sometimes used for local communication within range of the mobile units.

## Frequency separation: input to output

There is no set rule about spacing of input and output frequencies for all radio repeaters. Any spacing where the designer can get sufficient isolation between receiver and transmitter will work.

In some countries, under some radio services, there are agreed-on conventions or separations that are required by the system license. In the case of input and output frequencies in the United States, for example:

- Amateur repeaters in the 144–148 MHz band usually use a 600 kHz (0.6 MHz) separation, in the 1.25-meter band use a 1.6 MHz separation, in the 420–450 MHz band use a 5 MHz separation, and in the 902–928 MHz band use a 25 MHz separation.
- Systems in the 450–470 MHz band use a 5 MHz separation with the input on the higher frequency. Example: input is 456.900 MHz; output is 451.900 MHz.
- Systems in the 806–869 MHz band use a 45 MHz separation with the input on the lower frequency. Example: input is 810.1875 MHz; output is 855.1875 MHz.
- Military systems are suggested to use no less than a 10 MHz spacing.

These are just a few examples. There are many other separations or spacings between input and output frequencies in operational systems.

### Same band frequencies

Same band repeaters operate with input and output frequencies in the same frequency band. For example, in the US, two-way radio, 30–50 MHz is one band and 150–174 MHz is another. A repeater with an input of 33.980 MHz and an output of 46.140 MHz is a same band repeater.

In same band repeaters, a central design problem is keeping the repeater's own transmitter from interfering with the receiver. Reducing the coupling between transmitter and input frequency receiver is called *isolation*.

#### Duplexer system

In same-band repeaters, isolation between transmitter and receiver can be created by using a single antenna and a device called a *duplexer*. The device is a tuned filter connected to the antenna. In this example, consider a type of device called a *band-pass duplexer*. It allows, or passes, a band, (or a narrow range) of frequencies.

There are two legs to the duplexer filter, one is tuned to pass the input frequency, the other is tuned to pass the output frequency. Both legs of the filter are coupled to the antenna. The repeater receiver is connected to the receive leg while the transmitter is connected to the transmit leg. The duplexer prevents degradation of the receiver sensitivity by the transmitter in two ways. First, the receive leg greatly attenuates the transmitter's carrier at the receiver input (typically by 90-100 dB), preventing the carrier from overloading (blocking) the receiver front end. Second, the transmit leg attenuates the transmitter broadband noise on the receiver frequency, also typically by 90-100 dB. By virtue of the transmitter and receiver being on different frequencies, they can operate at the same time on a single antenna.

According to Bertrand, "Radio repeater stations nearly always use cavity filters." Their advantages include very high Q factor, and the ability to handle high power levels. These filters are cavity resonators. The cylinder provides inductance, and surrounds a capacitor with a tuning knob.

#### Combining system

There is often not enough tower space to accommodate a separate antenna for each repeater at crowded equipment sites. In same-band repeaters at engineered, shared equipment sites, repeaters can be connected to shared antenna systems. These are common in trunked systems, where up to 29 repeaters for a single trunked system may be located at the same site. (Some architectures such as iDEN sites may have more than 29 repeaters.)

In a shared system, a receive antenna is usually located at the top of the antenna tower. Putting the receive antenna at the top helps to capture weaker received signals than if the receive antenna were lower of the two. By splitting the received signal from the antenna, many receivers can work satisfactorily from a single antenna. Devices called *receiver multicouplers* split the signal from the antenna into many receiver connections. The multicoupler amplifies the signals reaching the antenna, then feeds them to several receivers, attempting to make up for losses in the power dividers (or splitters). These operate similarly to a cable TV splitter but must be built to higher quality standards so they work in environments where strong interfering signals are present.

On the transmitter side, a transmit antenna is installed somewhere below the receive antenna. There is an electrical relationship defined by the distance between transmit and receive antennas. A desirable null exists if the transmit antenna is located exactly below the receive antenna beyond a minimum distance. Almost the same isolation as a low-grade duplexer (about −60 decibels) can be accomplished by installing the transmit antenna below, and along the centerline of, the receive antenna. Several transmitters can be connected to the same antenna using filters called *combiners*. Transmitters usually have directional devices installed along with the filters that block any reflected power in the event the antenna malfunctions. The antenna must have a power rating that will handle the sum of energy of all connected transmitters at the same time.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Frequency separation: input to output, Siting as part of system design, Community Repeater, Particular forms of RF repeaters, External lists.)_
