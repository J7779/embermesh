---
id: radio/wp2-radio-receiver
title: Radio receiver
category: radio
tags: [radio receiver, receiver, wireless receiver, radio]
summary: In radio communications, a radio receiver, also known as a receiver, a wireless receiver, or simply a radio, is an electronic device that receives radio waves and converts the information carried by them into a usable form.
source: https://en.wikipedia.org/w/index.php?title=Radio_receiver&oldid=1381742115
source_title: Wikipedia: Radio receiver
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Radio receiver", Wikipedia, The Free Encyclopedia (revision 1381742115), https://en.wikipedia.org/w/index.php?title=Radio_receiver&oldid=1381742115 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Radio receiver

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

In radio communications, a **radio receiver**, also known as a **receiver**, a **wireless receiver**, or simply a **radio**, is an electronic device that receives radio waves and converts the information carried by them into a usable form. It is used with an antenna. The antenna responds to radio waves (electromagnetic waves at radio frequencies) and converts them into tiny alternating currents that receiver applies, which then extracts the desired information. The receiver uses electronic filters to separate the desired radio-frequency signal from other signals picked up by the antenna, an electronic amplifier to increase the signal power for further processing, and finally recovers the desired information through demodulation.

Radio receivers are essential components of all radio based-systems. The information produced by the receiver may be in the form of sound, video (television), or digital data. A radio receiver may be a separate piece of electronic equipment, or an electronic circuit within another device. The most familiar type of radio receiver for most people is a *broadcast radio receiver*, which reproduces sound transmitted by radio broadcasting stations, historically the first mass-market radio application. A broadcast receiver is commonly called a "radio". However, radio receivers are widely used in other areas of modern technology, including televisions, cell phones, wireless modems, radio clocks, and other communications, remote controls, and wireless networking systems.

## Applications

Radio has many practical applications, which include broadcasting, voice communication, data communication, radar, radiolocation, medical treatments, and remote control.

## Principles

A radio receiver is connected to an antenna which converts some of the energy from the incoming radio wave into a tiny radio frequency AC voltage which is applied to the receiver's input. An antenna typically consists of an arrangement of metal conductors. The oscillating electric and magnetic fields of the radio wave push the electrons in the antenna back and forth, creating an oscillating voltage.

The antenna may be enclosed inside the receiver's case, as with the ferrite loop antennas of AM radios and the flat inverted F antenna of cell phones; attached to the outside of the receiver, as with whip antennas used on FM radios, or mounted separately and connected to the receiver by a cable, as with rooftop television antennas and satellite dishes.

Practical radio receivers perform three basic functions on the signal from the antenna:

- bandpass filtering,
- amplification, and
- demodulation

### Bandpass filtering

Radio waves from many transmitters pass through the air simultaneously without interfering with each other and are received by the antenna. These can be separated in the receiver because they have different frequencies; that is, the radio wave from each transmitter oscillates at a different rate. To separate out the desired radio signal, the bandpass filter allows the frequency of the desired radio transmission to pass through, and blocks signals at all other frequencies.

The bandpass filter consists of one or more resonant circuits (tuned circuits). The resonant circuit is connected between the antenna input and ground. When the incoming radio signal is at the resonant frequency, the resonant circuit has high impedance and the radio signal from the desired station is passed on to the following stages of the receiver. At all other frequencies the resonant circuit has low impedance, so signals at these frequencies are conducted to ground.

- ***Bandwidth and selectivity***: See graphs. The information (modulation) in a radio transmission is contained in two narrow bands of frequencies called sidebands *(SB)* on either side of the carrier frequency *(C)*, so the filter has to pass a band of frequencies, not just a single frequency. The band of frequencies received by the receiver is called its *passband* *(PB)*, and the width of the passband in kilohertz is called the bandwidth *(BW)*. The bandwidth of the filter must be wide enough to allow the sidebands through without distortion, but narrow enough to block any interfering transmissions on adjacent frequencies (such as *S2* in the diagram). The ability of the receiver to reject unwanted radio stations near in frequency to the desired station is an important parameter called *selectivity* determined by the filter. In modern receivers quartz crystal, ceramic resonator, or surface acoustic wave (SAW) filters are often used which have sharper selectivity compared to networks of capacitor-inductor tuned circuits.
- ***Tuning***: To select a particular station the radio is "*tuned*" to the frequency of the desired transmitter. The radio has a dial or digital display showing the frequency it is tuned to. *Tuning* is adjusting the frequency of the receiver's passband to the frequency of the desired radio transmitter. Turning the tuning knob changes the resonant frequency of the tuned circuit. When the resonant frequency is equal to the radio transmitter's frequency the tuned circuit oscillates in sympathy, passing the signal on to the rest of the receiver.

### Amplification

The power of the radio waves picked up by a receiving antenna decreases with the square of its distance from the transmitting antenna. Even with the powerful transmitters used in radio broadcasting stations, if the receiver is more than a few miles from the transmitter the power intercepted by the receiver's antenna is very small, perhaps as low as picowatts or femtowatts. To increase the power of the recovered signal, an amplifier circuit uses electric power from batteries or the wall plug to increase the amplitude (voltage or current) of the signal. In most modern receivers, the electronic components which do the actual amplifying are transistors.

Receivers usually have several stages of amplification: the radio signal from the bandpass filter is amplified to make it powerful enough to drive the demodulator, then the audio signal from the demodulator is amplified to make it powerful enough to operate the speaker. The degree of amplification of a radio receiver is measured by a parameter called its *sensitivity*, which is the minimum signal strength of a station at the antenna, measured in microvolts, necessary to receive the signal clearly, with a certain signal-to-noise ratio. Since it is easy to amplify a signal to any desired degree, the limit to the sensitivity of many modern receivers is not the degree of amplification but random electronic noise present in the circuit, which can drown out a weak radio signal.

### Demodulation

After the radio signal is filtered and amplified, the receiver must extract the information-bearing modulation signal from the modulated radio frequency carrier wave. This is done by a circuit called a demodulator (detector). Each type of modulation requires a different type of demodulator

- an AM receiver that receives an (amplitude modulated) radio signal uses an AM demodulator
- an FM receiver that receives a frequency modulated signal uses an FM demodulator
- an FSK receiver which receives frequency-shift keying (used to transmit digital data in wireless devices) uses an FSK demodulator
Many other types of modulation are also used for specialized purposes.

The modulation signal output by the demodulator is usually amplified to increase its strength, then the information is converted back to a human-usable form by some type of transducer. An audio signal, representing sound, as in a broadcast radio, is converted to sound waves by an earphone or loudspeaker. A video signal, representing moving images, as in a television receiver, is converted to light by a display. Digital data, as in a wireless modem, is applied as input to a computer or microprocessor, which interacts with human users.

#### AM demodulation

The easiest type of demodulation to understand is AM demodulation, used in AM radios to recover the audio modulation signal, which represents sound and is converted to sound waves by the radio's speaker. It is accomplished by a circuit called an envelope detector *(see circuit)*, consisting of a diode *(D)* with a bypass capacitor *(C)* across its output.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Principles, Designs.)_
