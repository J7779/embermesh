---
id: radio/wp2-direct-conversion-receiver
title: Direct-conversion receiver
category: radio
tags: [direct-conversion receiver, dcr, homodyne, synchrodyne, zero intermediate frequency receiver, radio]
summary: A direct-conversion receiver (DCR), also known as a homodyne, synchrodyne, zero intermediate frequency receiver (zero-IF receiver), is a radio receiver design that demodulates the incoming radio signal using synchronous detection driven by a local oscillator whose frequency is identical…
source: https://en.wikipedia.org/w/index.php?title=Direct-conversion_receiver&oldid=1364262336
source_title: Wikipedia: Direct-conversion receiver
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Direct-conversion receiver", Wikipedia, The Free Encyclopedia (revision 1364262336), https://en.wikipedia.org/w/index.php?title=Direct-conversion_receiver&oldid=1364262336 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Direct-conversion receiver

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **direct-conversion receiver** (**DCR**), also known as a **homodyne**, **synchrodyne**, **zero intermediate frequency receiver** (**zero-IF receiver**), is a radio receiver design that demodulates the incoming radio signal using synchronous detection driven by a local oscillator whose frequency is identical to, or very close to the carrier frequency of the intended signal. This contrasts with the standard superheterodyne receiver, which uses an initial conversion to an intermediate frequency (IF).

The simplification of performing only a single frequency conversion reduces the basic circuit complexity but other issues arise, for instance, regarding dynamic range. In its original form it was unsuited to receiving AM and FM signals without implementing an elaborate phase locked loop. Although these and other technical challenges made this technique rather impractical around the time of its invention (1930s), current technology, and software radio in particular, have revived its use in certain areas including some consumer products.

## Principle of operation

The conversion of the modulated signal to baseband is done in a single frequency conversion. This avoids the complexity of the superheterodyne's two (or more) frequency conversions, IF stage(s), and image rejection issues.
The received radio frequency signal is fed directly into a frequency mixer, just as in a superheterodyne receiver. However unlike the superheterodyne, the frequency of the local oscillator is not offset from, but identical to, the received signal's frequency. The result is a demodulated output just as would be obtained from a superheterodyne receiver using synchronous detection (a *product detector*) following an intermediate frequency (IF) stage.

## Technical issues

To match the performance of the superheterodyne receiver, a number of the functions normally addressed by the IF stage must be accomplished at baseband. Since there is no high gain IF amplifier utilizing automatic gain control (AGC), the baseband output level may vary over a very wide range dependent on the received signal strength. This is one major technical challenge which limited the practicability of the design. Another issue is the inability of this design to implement envelope detection of AM signals. Thus direct demodulation of AM or FM signals (as used in broadcasting) requires phase locking the local oscillator to the carrier frequency, a much more demanding task compared to the more robust envelope detector or ratio detector at the output of an IF stage in a superheterodyne design. However this can be avoided in the case of a direct-conversion design using quadrature detection followed by digital signal processing. Using software radio techniques, the two quadrature outputs can be processed in order to perform any sort of demodulation and filtering on down-converted signals from frequencies close to the local oscillator frequency. The proliferation of digital hardware, along with refinements in the analog components involved in the frequency conversion to baseband, has thus made this simpler topology practical in many applications.

## History and applications

The homodyne was developed in 1932 by a team of British scientists searching for a design to surpass the superheterodyne (*two stage conversion model*). The design was later renamed the "synchrodyne". Not only did it have superior performance due to the single conversion stage, but it also had reduced circuit complexity and power consumption. The design suffered from the thermal drift of the local oscillator which changed its frequency over time. To counteract this drift, the frequency of the local oscillator was compared with the broadcast input signal by a phase detector. This produced a correction voltage which would vary the local oscillator frequency keeping it in lock with the wanted signal. This type of feedback circuit evolved into what is now known as a *phase-locked loop*. While the method has existed for several decades, it was difficult to implement due largely to component tolerances, which must be small for this type of circuit to function successfully.

### Advantages

Unwanted by-product beat signals from the mixing stage do not need any further processing, as they are completely rejected by use of a low-pass filter at the audio output stage. The receiver design has the additional advantage of high selectivity, and is therefore a precision demodulator. The design principles can be extended to permit separation of adjacent channel broadcast signals whose sidebands may overlap the wanted transmission. The design also improves the detection of pulse-modulated transmission mode signals.

### Disadvantages

Signal leakage paths can occur in the receiver. The high audio frequency gain required can result in difficulty in rejecting mains hum. Local-oscillator energy can leak through the mixer stage to the antenna input then reflect back into the mixer stage. The overall effect is the local oscillator energy will self-mix and create a DC offset signal. The offset may be large enough to overload the baseband amplifiers and prevent receiving the wanted signal. There are design modifications that deal with this issue, but they add to the complexity of the receiver. The additional design complexity often outweighs the benefits of a direct-conversion receiver.

### Modern usage

Wes Hayward and Dick Bingham's 1968 article brought new interest in direct-conversion designs.

The development of the integrated circuit and incorporation of complete phase-locked loop devices in low-cost IC packages made this design widely accepted. Usage is no longer limited to the reception of AM radio signals, but also finds use in processing more complex modulation methods. Direct-conversion receivers are now incorporated into many receiver applications, including cellphones, pagers, televisions, avionics, medical imaging apparatus and software-defined radio systems.
