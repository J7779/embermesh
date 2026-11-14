---
id: radio/wp2-radioteletype
title: Radioteletype
category: radio
tags: [radioteletype, rtty, ratt, radio]
summary: Radioteletype (RTTY) is a telecommunications system consisting originally of two or more electromechanical teleprinters in different locations connected by radio rather than a wired link.
source: https://en.wikipedia.org/w/index.php?title=Radioteletype&oldid=1383993009
source_title: Wikipedia: Radioteletype
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Radioteletype", Wikipedia, The Free Encyclopedia (revision 1383993009), https://en.wikipedia.org/w/index.php?title=Radioteletype&oldid=1383993009 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Radioteletype

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

**Radioteletype** (**RTTY**) is a telecommunications system consisting originally of two or more electromechanical teleprinters in different locations connected by radio rather than a wired link. Radioteletype evolved from earlier landline teleprinter operations that began in the mid-1800s. The US Navy Department successfully tested printing telegraphy between an airplane and ground radio station in 1922. Later that year, the Radio Corporation of America (RCA) successfully tested printing telegraphy via their Chatham, Massachusetts, radio station to the RMS *Majestic*. Commercial RTTY systems were in active service between San Francisco and Honolulu as early as April 1932 and between San Francisco and New York City by 1934. The US military used radioteletype in the 1930s and expanded this usage during World War II. From the 1980s, teleprinters were replaced by personal computers (PCs) running software to emulate teleprinters.

The term radioteletype is used to describe both the original radioteletype system, sometimes described as "Baudot", as well as the entire family of systems connecting two or more teleprinters or PCs using software to emulate teleprinters, over radio, regardless of alphabet, link system or modulation.

In some applications, notably military and government, radioteletype is known by the acronym **RATT** (Radio Automatic Teletype).

## Technical description

A radioteletype station consists of three distinct parts: the Teletype or teleprinter, the modem and the radio.

The Teletype or teleprinter is an electromechanical or electronic device. The word *Teletype* was a trademark of the Teletype Corporation, so the terms "TTY", "RTTY", "RATT" and "teleprinter" are usually used to describe a generic device without reference to a particular manufacturer.

Electromechanical teleprinters are heavy, complex and noisy, and have largely been replaced with electronic units. The teleprinter includes a keyboard, which is the main means of entering text, and a printer or visual display unit (VDU). An alternative input device is a perforated tape reader and, more recently, computer storage media (such as floppy disks). Alternative output devices are tape perforators and computer storage media.

The line output of a teleprinter can be at either digital logic levels (+5 V signifies a logical "1" or *mark* and 0 V signifies a logical "0" or *space*) or line levels (−80 V signifies a "1" and +80 V a "0"). When no traffic is passed, the line idles at the "mark" state.

When a key of the teleprinter keyboard is pressed, a 5-bit character is generated. The teleprinter converts it to serial format and transmits a sequence of a *start bit* (a logical 0 or space), then one after the other the 5 data bits, finishing with a *stop bit* (a logical 1 or mark, lasting 1, 1.5 or 2 bits). When a sequence of start bit, 5 data bits and stop bit arrives at the input of the teleprinter, it is converted to a 5-bit word and passed to the printer or VDU. With electromechanical teleprinters, these functions required complicated electromechanical devices, but they are easily implemented with standard digital electronics using shift registers. Special integrated circuits have been developed for this function, for example the Intersil 6402 and 6403. These are stand-alone UART devices, similar to computer serial port peripherals.

The 5 data bits allow for only 32 different codes, which cannot accommodate the 26 letters, 10 figures, space, a few punctuation marks and the required control codes, such as carriage return, new line, bell, etc. To overcome this limitation, the teleprinter has two *states*, the *unshifted* or *letters* state and the *shifted* or *numbers* or *figures* state. The change from one state to the other takes place when the special control codes *LETTERS* and *FIGURES* are sent from the keyboard or received from the line. In the *letters* state the teleprinter prints the letters and space while in the shifted state it prints the numerals and punctuation marks. Teleprinters for languages using other alphabets also use an additional *third shift* state, in which they print letters in the alternative alphabet.

The modem is sometimes called the terminal unit and is an electronic device which is connected between the teleprinter and the radio transceiver. The transmitting part of the modem converts the digital signal transmitted by the teleprinter or tape reader to one or the other of a pair of audio frequency tones, traditionally 2295/2125 Hz (US) or 2125/1955 Hz (Europe). One of the tones corresponds to the *mark* condition and the other to the *space* condition. These audio tones, then, modulate an SSB transmitter to produce the final audio-frequency shift keying (AFSK) radio frequency signal. Some transmitters are capable of direct frequency-shift keying (FSK) as they can directly accept the digital signal and change their transmitting frequency according to the *mark* or *space* input state. In this case the transmitting part of the modem is bypassed.

On reception, the FSK signal is converted to the original tones by mixing the FSK signal with a local oscillator called the BFO or *beat frequency oscillator*. These tones are fed to the demodulator part of the modem, which processes them through a series of filters and detectors to recreate the original digital signal. The FSK signals are audible on a communications radio receiver equipped with a BFO, and have a distinctive "beedle-eeeedle-eedle-eee" sound, usually starting and ending on one of the two tones ("idle on mark").

The transmission speed is a characteristic of the teleprinter while the shift (the difference between the tones representing mark and space) is a characteristic of the modem. These two parameters are therefore independent, provided they have satisfied the minimum shift size for a given transmission speed. Electronic teleprinters can readily operate in a variety of speeds, but mechanical teleprinters require the change of gears in order to operate at different speeds.

Today, both functions can be performed with modern computers equipped with digital signal processors or sound cards. The sound card performs the functions of the modem and the CPU performs the processing of the digital bits. This approach is very common in amateur radio, using specialized computer programs like fldigi, MMTTY or MixW.

Before the computer mass storage era, most RTTY stations stored text on paper tape using paper tape punchers and readers. The operator would type the message on the TTY keyboard and punch the code onto the tape. The tape could then be transmitted at a steady, high rate, without typing errors. A tape could be reused, and in some cases - especially for use with ASCII on NC Machines - might be made of plastic or even very thin metal material in order to be reused many times.

The most common test signal is a series of "RYRYRY" characters, as these form an alternating tone pattern exercising all bits and are easily recognized. Pangrams are also transmitted on RTTY circuits as test messages, the most common one being "The quick brown fox jumps over the lazy dog", and in French circuits, "Voyez le brick géant que j'examine près du wharf"

## Comparison with other modes

RTTY has a typical baud rate for Amateur operation of 45.45 baud (approximately 60 words per minute). It remains popular as a "keyboard to keyboard" mode in Amateur Radio. RTTY has declined in commercial popularity as faster, more reliable alternative data modes have become available, using satellite or other connections.

For its transmission speed, RTTY has low spectral efficiency. The typical RTTY signal with 170 Hz shift at 45.45 baud requires around 250 Hz receiver bandwidth, more than double that required by PSK31. In theory, at this baud rate, the shift size can be decreased to 22.725 Hz, reducing the overall band footprint substantially. Because RTTY, using either AFSK or FSK modulation, produces a waveform with constant power, a transmitter does not need to use a linear amplifier, which is required for many digital transmission modes. A more efficient Class C amplifier may be used.

RTTY, using either AFSK or FSK modulation, is moderately resistant to vagaries of HF propagation and interference, however modern digital modes, such as MFSK, use Forward Error Correction to provide much better data reliability.

_(Later sections of the original article were left out of this offline copy for length: Technical specification, Primary users.)_
