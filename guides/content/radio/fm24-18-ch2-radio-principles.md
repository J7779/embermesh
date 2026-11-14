---
id: radio/fm24-18-ch2-radio-principles
title: "FM 24-18 Ch. 2: Radio Principles"
category: radio
tags: [radio, propagation, modulation, frequency, wavelength, ground wave, sky wave]
summary: "Radio set components, frequency and wavelength, ground and sky wave propagation, and modulation types."
source: "https://archive.org/details/milmanual-fm-24-18-tactical-single-channel-radio-communications-techni"
source_title: "FM 24-18 Tactical Single-Channel Radio Communications Techniques (Department of the Army, 1987)"
license: "Public Domain (US Government work, 17 U.S.C. §105)"
attribution: "Headquarters, Department of the Army, FM 24-18, 30 September 1987. US Government work; distribution statement: 'Approved for public release; distribution is unlimited.' Figures and image-only tables not reproduced."
retrieved: 2026-11-13
part_of: radio/fm24-18
order: 2
---

# FM 24-18 Ch. 2: Radio Principles

## Section I. Theory and Propagation

### 2-1. The Radio Set

#### Basic Components

A radio set (fig 2-1) consists basically of a transmitter and a receiver. Other items necessary for operation include a source of electrical power and an antenna for radiation and reception of radio waves. The transmitter contains an oscillator which generates radio frequency (RF) energy in the form of alternating current (AC). A transmission line or cable feeds the RF to the antenna. The antenna converts the AC into electromagnetic energy which is radiated into space. A keying device is used to control the transmission.

Normally, in single-channel radio operations, the receiver uses the same antenna as the transmitter to receive electromagnetic energy. The antenna converts the received electromagnetic energy into RF alternating current. The RF is fed to the receiver by a transmission line or cable. In the receiver the RF is converted to audio frequencies (AF). The audio frequencies are then changed into sound waves by a headset or loud speaker.

Separate antennas are used for transmitting and receiving in some single-channel radio installations.

**Note:** When two radio sets operate on the same frequency, with the same type of modulation, and are within operating range, communications between both sets is possible.

*[Figure 2-1: Basic radio set (figure not reproduced).]*

#### Radio Transmitter

The simplest radio transmitter consists of a power supply and an oscillator (fig 2-2). The power supply can be batteries, a generator, an alternating current power source with a rectifier and a filter, or, a direct current (DC) rotating power source. See appendix A for additional information on power sources. The oscillator, which generates RF energy, must contain a circuit to tune the transmitter to the desired operating frequency. The transmitter must also have some device for controlling the emission of the RF signal. The simplest device is a telegraph key, which is a type of switch for controlling the flow of electric current. As the key is operated the oscillator is turned on and off for varying lengths of time. The varying pulses of RF energy produced correspond to dots and dashes. This method is used when transmitting international Morse code (IMC). This method is called continuous wave (CW) operation.

*[Figure 2-2: Block diagram of a simple radio transmitter (continuous wave/morse code operation) (figure not reproduced).]*

Continuous wave (CW) transmitter. A radio transmitter is used to generate RF energy which is radiated into space. The transmitter may contain only a simple oscillator stage. Usually, the output of the oscillator is applied to a buffer stage to increase oscillator stability and to a power amplifier (figure 2-3) which produces greater output. A telegraph key may be used to control the energy waves produced by the transmitter. When the key is closed, the transmitter produces its maximum output. When the key is opened, no output is produced.

*[Figure 2-3: Block diagram of oscillator-RF amplifier CW transmitter (figure not reproduced).]*

Radiotelephone transmitter. To transmit messages by voice, you need a way to vary the output of the transmitter. This is accomplished by adding a modulator and a microphone (fig 2-4). When the modulating signal causes the amplitude of the radio wave to change, the radio is an amplitude modulated (AM) set. When the modulating signal varies the frequency of the radio wave, the radio is a frequency modulated (FM) set.

*[Figure 2-4: Block diagram of a radiotelephone transmitter (figure not reproduced).]*

#### Antennas

After an RF signal has been generated and amplified in the transmitter, it is radiated into space by an antenna. At the distant station, a receiving antenna is used to receive the signal from space. An antenna consists of wires or rods designed for use with either a radio transmitter or a radio receiver. Chapter 3 discusses antennas in detail.

#### Radio Receiver

There are two general kinds of RF signals that can be received by a radio receiver. They are (1) modulated RF signals that carry speech, music, or other audio energy and (2) continuous wave signals that are bursts of RF energy conveying messages by means of coded (dot/dash) signals. Detector. The process of recovering intelligence from an RF signal is called detection, and the circuit in which it occurs is called a detector (fig 2-5). The detector recovers the intelligence from the carrier and makes it available for direct use or for further amplification. In an FM receiver, the detector is usually called a discriminator.

*[Figure 2-5: Block diagram of a simple radio receiver (figure not reproduced).]*

Radio frequency amplifier. An RF signal diminishes in strength at a very rapid rate after it leaves the transmitting antenna. Many RF signals of various frequencies are crowded into the radio frequency spectrum. Therefore, some means must be used to both select and amplify the desired signal. This is accomplished by an RF amplifier (fig 2-6). It is included in the receiver to sharpen the selectivity (the ability to choose one frequency out of many) and to increase the sensitivity (the ability to respond to very weak signals). The RF amplifier normally uses tunable circuits to select the desired signal. It contains transistors, electron tubes, or integrated circuits (IC) to amplify the signal to a usable level.

*[Figure 2-6: Block diagram of a detector and an RF amplifier (figure not reproduced).]*

Audio frequency amplifier (fig 2-7). The signal level of the output of a detector, with or without an RF amplifier, is generally very low. To build up the signal level to a useful value that will operate headphones, a loudspeaker, a teletypewriter, or data devices (one or more AF amplifiers) are used in the receiver.

*[Figure 2-7: Block diagram of a complete radio receiver (figure not reproduced).]*

### 2-2. Radio Waves

#### Propagation Velocity

Radio waves travel near the surface of the Earth and also radiate skyward at various angles to the Earth's surface (fig 2-8). These electromagnetic waves travel through space at the speed of light, approximately 300,000 kilometers (186,000 mi) per second.

*[Figure 2-8: Radiation of radio waves from a vertical/antenna (figure not reproduced).]*

#### Wavelength

Wavelength is defined as the distance between the crest of one wave and the crest of the next wave. It can also be defined as the length of one complete cycle of the waveform. It is the distance traveled during one complete cycle. The length of the wave is always measured in meters.

*[Figure 2-9: Wavelength of a radio wave (figure not reproduced).]*

#### Radio Frequency

The frequency of a radio wave is the same as the number of complete cycles that occur in one second. The longer the time of one cycle, the longer is the wavelength and the lower the frequency. The shorter the time of one cycle, the shorter is the wavelength and the higher the frequency (fig 2-10). Frequency is measured and stated in units called hertz (Hz). One cycle per second is stated as 1 hertz. Because the frequency of a radio wave is very high, it is generally measured and stated in thousands of hertz (kilohertz, kHz) or in millions of hertz (megahertz, MHz). One kHz is equal to 1000 cycles per second, and 1 MHz is equal to 1,000,000 cycles per second. Sometimes frequencies are expressed in billions of hertz (gigahertz, GHz). One GHz is equal to 1,000,000,000 cycles per second.

Frequency calculation. For practical purposes, the velocity of a radio wave is considered to be constant, regardless of the frequency or the amplitude of the transmitted wave. Therefore, to find the frequency when the wavelength is known, divide the velocity by the wavelength. Formula: Frequency (hertz) = 300,000,000 (meters per second) / wavelength (meters) To find the wavelength when the frequency is known, divide the velocity by the frequency. Formula: Wavelength (meters) = 300,000,000 (meters per second) / frequency (hertz)

*[Figure 2-10: Comparison of two waves of different frequency (figure not reproduced).]*

Frequency bands. Within the radio frequency spectrum, radio frequencies are divided into groups or bands of frequencies as shown in table 2-1. Most tactical radio sets operate within a 2 MHz to 400 MHz range within the frequency spectrum.

*[Table 2-1: Frequency band coverage (table not reproduced).]*

Characteristics of the frequency bands. Each frequency band has certain characteristics which are reflected in table 2-2. The ranges and power requirements shown are for normal operating conditions (proper siting and antenna orientation, and correct operating procedures). The ranges will change according to the condition of the propagation medium and the transmitter output power.

*[Table 2-2: Frequency band characteristics (table not reproduced).]*

### 2-3. Radio Wave Propagation

There are two principal paths by which radio waves travel from a transmitter to the receiver (fig 2-11). One is by ground wave which travels directly from the transmitter to the receiver. The other is by sky wave which travels up to the ionosphere and is refracted (bent downward) back to the Earth. Short distance and all UHF and upper VHF transmissions are by ground waves. Long distance transmission is principally by sky waves. Single-channel radio sets can use either ground wave or sky wave propagation for communications.

*[Figure 2-11: Principal paths of radio waves (figure not reproduced).]*

#### Ground Wave Propagation

Radio communications which use ground wave propagation do not use or depend on waves that are refracted from the ionosphere (sky waves). Ground wave propagation is affected by the electrical characteristics of the Earth and by the amount of diffraction (bending) of the waves along the curvature of the Earth. The strength of the ground wave at the receiver depends on the power output and frequency of the transmitter, the shape and conductivity of Earth along the transmission path, and the local weather conditions. The following paragraphs describe the components of a ground wave (fig 2-12).

Direct wave. The direct wave is that part of the radio wave which travels direct from the transmitting antenna to the receiving antenna. This part of the wave is limited to the line-of-sight (LOS) distance between the transmitting and receiving antennas, plus the small distance added by atmospheric refraction and diffraction of the wave around the curvature of the Earth. This distance can be extended by increasing the height of either the transmitting or the receiving antenna, or both.

Ground reflected wave. The ground reflected wave is that portion of the radio wave which reaches the receiving antenna after being reflected from the surface of the earth. Cancellation of the radio signal can occur when the ground reflected component and the direct wave component arrive at the receiving antenna at the same time and are 180° out of phase with each other. Surface wave. The surface wave, which follows the curvature of the Earth, is that part of the ground wave which is affected by the conductivity and dielectric constant of the earth.

*[Figure 2-12: Possible routes for ground waves (figure not reproduced).]*

#### Sky-Wave Propagation

Radio communications that use sky-wave propagation depend on the ionosphere to provide the signal path between the transmitting and receiving antennas.

Ionospheric structure. The ionosphere has four distinct layers (see fig 2-13). In the order of increasing heights and decreasing molecular densities, these layers are labeled D, E, F1, and F2. During the day, when the rays of the Sun are directed toward that portion of the atmosphere, all four layers may be present. During the night, the F1 and F2 layers seem to merge into a single F layer, and the D and E layers fade out. The actual number of layers, their height above the Earth, and their relative intensity of ionization vary constantly.

D region. The D region exists only during daylight hours and has little effect in bending the paths of high frequency radio waves. The main effect of the D region is to attenuate high frequency waves when the transmission path is in sunlit regions.

- E region. The E region is used during the day for high frequency radio transmission over intermediate distances (less than 2,400 km (1,500 mi)). At night the intensity of the E region decreases and it becomes useless for radio transmission.
- F region. The F region exists at heights up to 380 kilometers (240 mi) above the Earth and is ionized all the time. It has two well-defined layers (F1 and F2) during the day, and one layer (F) during the night. At night the F region remains at a height of about 260 kilometers (170 mi) and is useful for long-range radio communications (over 2,400 km (1 ,500 mi)). The F2 layer is the most useful of all layers for long-range radio communications, even though its degree of ionization varies appreciably from day to day.
- *[Figure 2-13: Average layer distribution of the ionosphere (figure not reproduced).]*

Regular variations of the ionosphere. The movements of the Earth around the Sun and changes in the Sun's activity contribute to ionospheric variations. There are two main classes of these variations: regular, which is predictable; and irregular, which occurs from abnormal behavior of the sun. The regular variations are classed as-- Daily: caused by the rotation of the Earth.

- Seasonal: caused by the north and south progression of the Sun.
- 27-day: caused by the rotation of the Sun on its axis.
- 11-year: caused by the sunspot activity cycle going from maximum through minimum back to maximum levels of intensity.
- Irregular variations of the ionosphere. In planning a communications system, the current status of the four regular variations must be anticipated. There are also unpredictable irregular variations that must be considered. They have a degrading effect (at times blanking out communications) which we cannot control or compensate for at the present time. Some irregular variations are listed below.

Sporadic E. When it is excessively ionized, the E layer often blanks out the reflections back from the higher layers. It can also cause unexpected propagation of signals hundreds of miles beyond the normal range. This effect can occur at any time.

- Sudden ionospheric disturbance (SID). A sudden ionospheric disturbance coincides with a bright solar eruption and causes abnormal ionization of the D layer. This effect causes total absorption of all frequencies above approximately 1 MHz. It can occur without warning during daylight hours and last from a few minutes to several hours. When it occurs, receivers seem to go dead.
- Ionospheric storms. During these storms, sky wave reception above approximately 1.5 MHz shows low intensity and is subject to a type of rapid blasting and fading called "flutter fading." These storms may last from several hours to days, and usually extend over the entire Earth.
- Frequency characteristics in the ionosphere. The range of long distance radio transmission is determined primarily by the ionization density of each layer. The higher the frequency, the greater the ionization density required to reflect radio waves back to Earth. The upper (E and F) regions reflect the higher frequencies because they are the most highly ionized. The D region, which is the least ionized, does not reflect frequencies above approximately 500 kHz. Thus, at any given time and for each ionized region, there is an upper frequency limit at which radio waves sent vertically upward are reflected back to Earth. This limit is called the critical frequency. Radio waves directed vertically at frequencies higher than the critical frequency pass through the ionized layer out into space. All radio waves directed vertically into the ionosphere at frequencies lower than the critical frequency are reflected back to Earth. Radio waves used in communications generally are directed towards the ionosphere at some oblique angle, called the angle of incidence. Radio waves at frequencies above the critical frequency will be reflected back to Earth if transmitted at angles of incidence smaller than a certain angle, called the critical angle. At the critical angle, and at all angles larger than the critical angle, the radio waves will pass through the ionosphere if the frequency is higher than the critical frequency. As the angle of transmission becomes smaller, an angle is reached at which the radio waves will be reflected back to Earth.

*[Figure 2-14: Sky wave transmission paths (figure not reproduced).]*

Transmission Paths. Sky-wave propagation refers to those types of radio transmissions that depend on the ionosphere to provide signal paths between transmitters and receivers (fig 2-14). The distance from the transmitting antenna to the place where the sky waves first return to Earth is called the skip distance. The skip distance is dependent on the angle of incidence, the operating frequency, and the height and density of the ionosphere. The antenna height, in relation to the operating frequency, affects the angle that transmitted radio waves strike and penetrate the ionosphere and then return to Earth. This angle of incidence can be controlled to obtain the desired area of coverage. Lowering the antenna height will increase the angle of transmission and provides broad and even signal patterns in an area the size of a typical corps. The use of near-vertical transmission paths is known as near-vertical incidence sky-wave (NVIS) (see appendix M). Raising the antenna height will lower the angle of incidence. Lowering the angle of incidence can produce a skip zone (fig 2-15) in which no usable signal can be received. This area is bounded by the outer edge of usable ground wave propagation and the point nearest the antenna at which the sky wave returns to Earth. In corps area communications situations, the skip zone is not a desirable condition. However, low angles of incidence make long distance communications possible.

*[Figure 2-15: Low angle sky-wave transmission path (figure not reproduced).]*

When a transmitted wave is reflected back to the surface of the Earth, part of its energy is absorbed by the Earth. The remainder of its energy is reflected back into the ionosphere to be reflected back again. This means of transmission--by alternately reflecting the radio wave between the ionosphere and the Earth--is called hops (fig 2-16) and enables radio waves to be received at great distances from the point of origin.

*[Figure 2-16: Sky-wave transmission hop paths (figure not reproduced).]*

Maximum usable and lowest usable frequencies. Using a given ionized layer and a transmitting antenna with a fixed angle of radiation, there is a maximum frequency at which a radio wave will return to Earth at a given distance. This frequency is called the maximum usable frequency (MUF). It is the monthly median of the daily highest frequency that is predicted for sky-wave transmission over a particular path at a particular hour of the day. The MUF is always higher than the critical frequency because the angle of incidence is less than 90ø. If the distance between the transmitter and the receiver is increased, the maximum usable frequency will also increase. Radio waves lose some of their energy through absorption by the D region and the portion of the E region of the ionosphere at certain transmission frequencies. The total absorption is less and communications more satisfactory as higher frequencies are used--up to the level of the MUF. The absorption rate is greatest for frequencies ranging from approximately 500 kHz to 2 MHz during the day. At night the absorption rate decreases for all frequencies. As the frequency of transmission over any sky-wave path is increased from low to high frequencies, a frequency will be reached at which the received signal just overrides the level of atmospheric and other radio noise interference. This is called the lowest useful frequency (LUF), because frequencies lower than the LUF are too weak for useful communications. It should be noted that the LUF depends also on the power output of the transmitter as well as the transmission distance. When the LUF is greater than the MUF, no sky-wave transmission is possible.

Ionospheric Sounder AN/TRQ-35(V) is an electronic device that enables operators to determine which frequencies are best to use at any particular time of day or night. See appendix B for a more detailed discussion of this equipment.

## Section II. Types of Modulation and Methods of Transmission

### 2-4. Single-Channel Communications

Single-channel communications radio equipment is used primarily to transmit intelligence in the form of speech, data, RATT, or telegraphic code. Although sound can be converted to audio frequency electrical energy, it is not practical to transmit it in this energy form through the Earth's atmosphere by electromagnetic radiation. For example, efficient transmission of a 20-hertz audio signal would require an antenna almost 8,000 kilometers (5,000 mi) long. None of the above limitations apply when radio frequency electrical energy is used to carry the intelligence. Great distances can be covered, efficient antennas for radio frequencies are of practical lengths, and antenna power losses are at reasonable levels.

The frequency of the radio wave affects its propagation characteristics. At low frequencies (.03 to .3 MHz), the ground wave is very useful for communications over great distances. The ground wave signals are quite stable and show little seasonal variation. In the medium frequency band (.3 to 3.0 MHz), the range of the ground wave varies from about 24 kilometers (15 mi) at 3 MHz to about 640 kilometers (400 mi) at the lowest frequencies of this band. Sky wave reception is possible during the day or night at any of the lower frequencies in this band. At night, the sky wave is receivable at distances up to 12,870 kilometers (8,000 mi). In the high frequency band (3 to 30 MHz), the range of the ground wave decreases as frequency increases and the sky waves are greatly influenced by ionospheric considerations. In the very high frequency band (30 to 300 MHz), there is no usable ground wave and only slight refraction of sky waves by the ionosphere at the lower frequencies. The direct wave provides communications if the transmitting and receiving antennas are elevated high enough above the surface of the Earth. In the ultrahigh frequency band (300 to 3,000 MHz), the direct wave must be used for all transmissions. Communications is limited to a short distance beyond the horizon. Lack of static and fading in these bands makes line-of-sight reception very satisfactory. Antennas that are highly directional can be used to concentrate the beam of RF energy, thus, increasing the signal intensity.

### 2-5. Amplitude and Frequency Modulation

Both AM and FM transmitters produce RF carriers. The carrier is a wave of constant amplitude, frequency, and phase which can be modulated (fig 2-17) by changing its amplitude, frequency, or phase. Thus, the RF carrier "carries" intelligence by being modulated. Modulation is the process of superimposing intelligence (voice or coded signals) on the carrier.

*[Figure 2-17: Wave shapes (figure not reproduced).]*

#### Amplitude Modulation

Amplitude modulation is defined as the variation of the RF power output of a transmitter at an audio rate. In other words, the RF energy increases and decreases in power according to the audio frequencies superimposed on the carrier signal.

When audio frequency signals are superimposed on the radio frequency carrier signal, additional RF signals are generated. These additional frequencies are equal to the sum and the difference of the audio frequencies and the radio frequency used. For example, assume a 500 kHz carrier is modulated by a 1 kHz audio tone. Two new frequencies are developed, one at 501 kHz (the sum of 500 kHz and 1 kHz) and the other at 499 kHz (the difference between 500 kHz and 1 kHz). If a complex audio signal is used instead of a single tone, two new frequencies will be set up for each of the audio frequencies involved. The new frequencies resulting from superimposing an AF signal on an RF signal are called sidebands.

As described above, when the RF carrier is modulated by complex tones such as speech, each separate frequency component of the modulating signal produces its own upper and lower sideband frequencies (fig 2-18). These additional frequencies occupy a band of frequencies called sidebands. The sideband that contains the sum of the RF and AF signals is called the upper sideband. The sideband that contains the difference between the RF and AF signals is called the lower sideband.

The space occupied by a carrier and its associated sidebands in the radio frequency spectrum is called a channel. In amplitude modulation, the width of the channel (bandwidth) is equal to twice the highest modulating frequency. For example, if a 5000 kHz (5 MHz) carrier is modulated by a band of frequencies ranging from 200 to 5000 cycles (.2 to 5 kHz), the upper sideband extends from 5000.2 to 5005 kHz. The lower sideband extends from 4999.8 kHz to 4995 kHz. Thus, the bandwidth is the difference between 5005 kHz and 4995 kHz, a total of 10 kHz.

*[Figure 2-18: Amplitude-modulated system (figure not reproduced).]*

Amplitude modulation generally is used by radiotelephone and radio teletypewriter transmitters operating in the medium and high frequency bands. The intelligence of an amplitude modulated signal exists solely in the sidebands.

#### Frequency Modulation

Frequency modulation is the process of varying the frequency (rather than the amplitude) of the carrier signal in accordance with the variations of the modulating signals. The amplitude or power of the FM carrier does not vary during modulation (see fig 2-17).

The frequency of the carrier signal when it is not modulated is called the center or rest frequency. When a modulating signal is applied to the carrier, the carrier signal will move up and down in frequency away from the center or rest frequency.

The amplitude of the modulating signal determines how far away from the center frequency the carrier will move. This movement of the carrier is called deviation; how far the carrier moves is called the amount of deviation. During reception of the FM signal, the amount of deviation determines the loudness or volume of the signal.

The FM signal leaving the transmitting antenna is constant in amplitude, but varying in frequency according to the audio signal. As the signal travels to the receiving antenna, it picks up natural and manmade electrical noises that cause amplitude variations in the signal. All of these undesirable amplitude variations are amplified as the signal passes through successive stages of the receiver until the signal reaches a part of the receiver called the limiter. The limiter is unique to FM receivers as is the discriminator.

The limiter eliminates the amplitude variations in the signal, then passes it on to the discriminator which is sensitive to variations in the frequency of the RF wave. The resultant constant amplitude, frequency modulated signal is then processed by the discriminator circuit, which changes the frequency variations into corresponding voltage amplitude variations. These voltage variations reproduce the original modulating signal in a headset, loudspeaker, or teletypewriter. Frequency modulation is generally used by radiotelephone transmitters operating in the VHF and higher frequency bands.

### 2-6. Methods of Transmission

#### Single Sideband

It was stated in paragraph 2-5 that the intelligence of an AM signal was contained solely in the sidebands. In fact, each sideband alone contains all the intelligence we need for communications. Since this is true, it may be correctly inferred that one sideband and the carrier signal can be eliminated. This is the principle on which single sideband (SSB) communications is based. Although both sidebands are generated within the modulation circuitry of the SSB radio set, the carrier and one sideband are removed before any signal is transmitted (fig 2-19). The sideband that is higher in frequency than the carrier is called the upper sideband (USB). The sideband that is lower in frequency than the carrier is called the lower sideband (LSB). Either sideband can be used for communications as long as both the transmitter and the receiver are adjusted to the same sideband. Most Army SSB equipment operates in the USB mode. The transmission of only one sideband leaves open that portion of the RF spectrum normally occupied by the other sideband of an AM signal. This allows more emitters to be used within a given frequency range.

Single sideband transmission is used in applications where it is desired to-- Obtain greater reliability.

- Limit size and weight of equipment.
- Increase effective output without increasing antenna voltage.
- Operate a large number of radio sets without heterodyne interference (whistles and squeals) from radio frequency carriers.
- Operate over long ranges without loss of intelligibility due to selective fading.
- *[Figure 2-19: Single-sideband system (figure not reproduced).]*

#### Radiotelegraphy (Continuous Wave Transmission)

Radiotelegraph information can be transmitted by starting and stopping the carrier by means of a switch or key. Each letter and number of a message is indicated by combining short and long pulses (dots and dashes) in groups according to a determined sequence or code. For example, if an operator wants to send the letter A, using international Morse code (fig 2-20), he or she would close the key for a fraction of a second (a dit), open the key for the same length of time, and then close it again for a period three times longer than the first (a dah). This process of transmitting information, called radiotelegraphy, is also called continuous wave transmission--or, more simply, CW.

Radiotelegraph information can also be transmitted by using a tone modulated radio wave. In tone transmission, the carrier is modulated at a fixed audio rate usually between .5 and 1 kilohertz. The carrier signal is again stopped and started to form dots and dashes. This is called modulated continuous wave or MCW. Because tone emission occupies a broader band, it may be used successfully against some types of jamming. However, the broad signal used in tone transmission is an easy target for radio direction finding equipment. Also, the distance range of a tone modulated transmitter is less than that of a nonmodulated CW transmitter of the same power output. Manual radiotelegraph transmission has a limited traffic handling capacity. Consequently, its use is confined to lower echelons of the Army and special situations where the traffic load is light. It may also be used in isolated or remote locations if other means of communications are not available.

*[Figure 2-20: Continuous wave signal for the letter A (figure not reproduced).]*

#### Radiotelephony

The microphone of a radiotelephone set converts voice or audio waves to weak electrical impulses. These impulses are strengthened by a series of audio amplifiers and are passed to a modulator. In AM transmission, the modulator provides the audio power necessary to modulate the RF amplifier. At the receiver, the modulated RF wave is demodulated, allowing only the audio component of the incoming signal to be reproduced by a loudspeaker or headset.

#### Radio Teletypewriter

Radio teletypewriter transmission is possible over distances of up to several thousand miles. This type of transmission is often used in rapidly changing tactical situations where time does not permit installation of wire lines.

The teletypewriter itself consists of a transmitting keyboard and a receiving and printing mechanism. The depression of a key activates the transmitting mechanism that sends a series of electrical signals over the radio to a receiving device with the receiving radio. This device translates the signals into a mechanical action so the printer can select and print the proper character. Each key sends a different arrangement of pulses (fig 2-21) and the message may be printed in page form or on a tape. The teletypewriter keyboard contains the letters of the alphabet, basic numbers, and punctuation marks (fig 2-21). The machine also performs the function of carriage return, line feed, letter/number shift, and spacing.

In the special signaling code used for teletypewriter transmission, each character or signal is of uniform length and consists of five intervals of time. The units are equal in length and are known as either marking or spacing impulses. A marking impulse (mark) exists when current flows in the circuit and the selector magnets in the receiving printer are operated. A spacing impulse (space) is the open condition in the circuit when selector magnets in the receiving printers are not operated. Various combinations of marking and spacing impulses are used for different letters, numerals, and functions.

In the most commonly used method of operation, the teletypewriter signals a radio transmitter which, in turn, radiates on one frequency for the marking condition and on a slightly different frequency for a spacing condition. This type of operation, which is a form of frequency modulation, is called frequency-shift keying (FSK). The receiving mechanism converts the two frequencies, which are 850 hertz apart or 85 hertz apart depending on the transmitter used, to teletypewriter pulses. The pulses then cause the receiving teletypewriter to operate. The striking of a key on the transmitting teletypewriter will thus activate the receiving teletypewriter.

### 2-7. Radio Equipment

Most Army single-channel radio equipment is designed to operate in only one mode and one frequency band--for example, FM only, VHF (30 to 76 MHz), for the AN/VRC-12 series. Configurations using the AN/GRC-106 and -106A can operate in either AM or SSB mode from 2 to 29.999 MHz. The AN/PRC-70 is a new radio set that can operate multimode (FM, AM, or SSB) and in both HF and VHF bands (2 to 76 MHz). The AN/PRC-70 is described more fully in appendix C.

*[Figure 2-21: Teletypewriter code character set and standard start-stop, five-unit code chart (figure not reproduced).]*

**Note:** The letters on the perforated tape will appear 6 characters in front of the actual perforations on the tape. This is necessary for alignment of the tape in the tape distributor.

*[Figure 2-2: (figure not reproduced).]*

*[Figure 2-3: (figure not reproduced).]*

*[Figure 2-4: (figure not reproduced).]*

*[Figure 2-5: (figure not reproduced).]*

*[Figure 2-6: (figure not reproduced).]*

*[Figure 2-7: (figure not reproduced).]*

*[Figure 2-8: (figure not reproduced).]*

*[Figure 2-9: (figure not reproduced).]*

*[Figure 2-10: (figure not reproduced).]*

*[Table 2-1: (table not reproduced).]*

*[Table 2-2: (table not reproduced).]*

*[Figure 2-11: (figure not reproduced).]*

*[Figure 2-12: (figure not reproduced).]*

*[Figure 2-13: (figure not reproduced).]*

*[Figure 2-14: (figure not reproduced).]*

*[Figure 2-15: (figure not reproduced).]*

*[Figure 2-16: (figure not reproduced).]*

*[Figure 2-17: (figure not reproduced).]*

*[Figure 2-18: (figure not reproduced).]*

*[Figure 2-19: (figure not reproduced).]*

*[Figure 2-20: (figure not reproduced).]*
