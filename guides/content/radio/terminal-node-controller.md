---
id: radio/terminal-node-controller
title: Terminal node controller
category: radio
tags: [radio, TNC, packet, modem]
summary: "Terminal node controllers (TNCs): the modem/protocol box between a radio and a computer."
source: "https://en.wikipedia.org/w/index.php?title=Terminal_node_controller&oldid=1369862971"
source_title: Terminal node controller (Wikipedia)
license: CC BY-SA 4.0
attribution: "Wikipedia contributors, 'Terminal node controller' (rev 1369862971), Wikipedia, https://en.wikipedia.org/wiki/Terminal_node_controller. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/). Adapted: converted to Markdown; references, images, navigation and some trailing sections removed."
retrieved: 2026-11-13
---

# Terminal node controller

A terminal node controller (TNC) is a device used by amateur radio operators to participate in AX.25 packet radio networks. It is similar in function to the packet assembler/disassemblers (PAD) used on X.25 networks, with the addition of a modem to convert baseband digital signals to audio tones.

The first TNC, the VADCG board, was originally developed by Doug Lockhart, VE7APU, of Vancouver, British Columbia.

Amateur Radio TNCs were first developed in 1978 in Canada by the Montreal Amateur Radio Club and the Vancouver Area Digital Communications group. These never gained much popularity because only a bare printed circuit board was made available and builders had to gather up a large number of components.

In 1983, the Tucson Amateur Packet Radio (TAPR) association produced complete kits for their TNC-1 design. This was later available as the Heathkit HD-4040. A few years later, the improved TNC-2 became available, and it was licensed to commercial manufacturers such as MFJ.

In 1986, the improved "TNC+" was designed to run programs and protocols developed for the original TNC board. TNC+ also included an assembler and a version of Forth (STOIC), which runs on the TNC+ itself, to support developing new programs and protocols.

In 2018 Nino Carillo (KK4HEJ) developed and produced a TNC (the *NinoTNC*) with (currently) 16 different data modes, from 300 baud AFSK to 19,200 C4FSK. It has a KISS interface to the compute device, and implements IL2P (Improved Layer 2 Protocol) for Forward Error Correction (FEC) in some modes.

## Description

A typical model consists of a microprocessor, a modem, and software (in EPROM) that implements the AX.25 protocol and provides a command line interface to the user. (Commonly, this software provides other functionality as well, such as a basic bulletin board system to receive messages while the operator is away.) Because the TNC contains all the intelligence needed to communicate over an AX.25 network, no external computer is required. All of the network's resources can be accessed using a dumb terminal.

The TNC connects to the terminal and a radio transceiver.Data from the terminal is formatted into AX.25 packets and modulated into audio signals (in traditional applications) for transmission by the radio. Received signals are demodulated, the data unformatted, and the output sent to the terminal for display. In addition to these functions, the TNC manages the radio channel according to guidelines in the AX.25 specification.

Early usage was mostly one-to-one communication, either between two people or a person to an automated bulletin board or e-mail system.

## Current status

Since the late 1990s, most AX.25 usage has shifted to a different one-to-many communication paradigm with the Automatic Packet Reporting System (APRS).

The TNCs of the 1980s and 1990s were complete solutions that only needed a radio and an optional dumb terminal. As home computers made their way into ham "shacks," there was a movement toward simpler, cheaper "KISS" (Keep It Simple, Stupid) devices. These have a modem and minimal processing of the AX.25 protocol. Most of the processing is moved to the personal computer.

The next logical step in the evolution is to eliminate the specialized hardware and move all of the processing to a computer. A "soundcard" is used for audio to and from the radio. Everything else is done in software.

Some of the earlier attempts performed poorly, giving this approach a bad reputation. Modern software TNCs, using digital signal processing (DSP) techniques, can successfully decode poor quality signals that the older equipment can't.

Some handheld and mobile VHF radios currently on the market incorporate TNC abilities within the radio itself in support of the APRS protocol.

Many TNCs are still in use, especially in unattended stations where reliability is important. The importance of location to the APRS system has fueled development of a new generation of small low-power TNCs often integrated with a GPS module for use in mobile tracking stations. Not all AX.25 activity is APRS: Some AX.25 data networks exist, including the EastNet Packet Network mostly in the Northeastern United States and TARPN Network mostly in North Carolina.

APRS digipeating protocols require specific naming conventions, and older TNCs may not have the required support to be used as a digipeater. However, almost any TNC can be pressed into service for home or tracker use. TNCs that were clones of the popular TAPR TNC-2 may be upgradeable via the UIDIGI firmware project.

## TNC/Radio data ports

Although typically described as data ports, the interface between a radio and a TNC is almost entirely analog audio (plus lines for PTT and squelch). The connectors used for this purpose vary widely across TNC manufacturers, but modern radios that offer data ports have widely adopted the Mini-DIN 6-pin connector.

Also used are full-size DIN connectors, DE-9 (D-sub) connectors, modular telephone (RJ) connectors, and combinations of mini- and micro-headphone connectors (typically on handheld radios).

- 6-pin Mini-DIN

## Terminal data port

TNCs were originally designed to accept messages from a human typing on a dumb terminal. TNCs had a DB-25 or DE-9 connector carrying asynchronous start-stop bytes with RS-232 signal levels. This could also be used with the serial port of a personal computer. Serial ports are now pretty much extinct, but USB-to-serial converter cables are available.

However, typically a human uses a more sophisticated software package running on a personal computer that sends messages to the TNC using the KISS (TNC) protocol or the 6PACK protocol.

Modern software TNCs provide TCP/IP interfaces that can be accessed across computer networks. For example, it is possible to run a software TNC on a Raspberry Pi near the radio and access it from a laptop computer in another location. Software TNCs also provide "pseudo terminals" so they can be used with old applications that only know how to use serial ports.
