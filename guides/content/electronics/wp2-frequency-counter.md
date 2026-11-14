---
id: electronics/wp2-frequency-counter
title: Frequency counter
category: electronics
tags: [frequency counter, cymometer, electronics]
summary: A frequency counter is an electronic instrument, or component of one, that is used for measuring frequency.
source: https://en.wikipedia.org/w/index.php?title=Frequency_counter&oldid=1375057682
source_title: Wikipedia: Frequency counter
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Frequency counter", Wikipedia, The Free Encyclopedia (revision 1375057682), https://en.wikipedia.org/w/index.php?title=Frequency_counter&oldid=1375057682 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Frequency counter

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

A **frequency counter** is an electronic instrument, or component of one, that is used for measuring frequency. Frequency counters usually measure the number of cycles of oscillation or pulses per second in a periodic electronic signal. Such an instrument is sometimes called a **cymometer**, particularly one of Chinese manufacture.

## Operating principle

All frequency counters rely on an internal electronic oscillator, known as the *timebase*, which serves as a time reference for measurements.

Most frequency counters work by using a digital counter to count the number of rising or falling signal edges occurring in the measured signal within a specific period of time, known as the *gate time*. At the end of the gate time, the accumulated count is transferred to a holding register and the counter is reset to zero to prepare for the next measurement. The value stored in the holding register directly indicates the measured frequency in Hertz (Hz) when the gate time is one second; for other gate times the holding register value must be scaled to convert it to Hz. In frequency counter instruments, the measured frequency is typically output to a display.

If the frequency of the measured signal is sufficiently stable and significantly lower than that of the timebase, the measurement resolution may be improved by measuring the period of the measured signal and computing the reciprocal to obtain frequency; this is sometimes referred to as the *reciprocal measurement technique*. Period measurement is done by measuring the time required for an integer number of cycles, rather than counting cycles for a constant gate time, and dividing that time by the number of cycles.

If the frequency to be measured is already in electronic form, simple interfacing with the instrument is all that is required. More complex signals may need some conditioning to make them suitable for counting. Most general-purpose frequency counters will include some form of amplifier, filtering, and shaping circuitry at the input. DSP technology, sensitivity control and hysteresis are other techniques to improve performance. Other types of periodic events that are not inherently electronic in nature will need to be converted using some form of transducer. For example, a mechanical event could be arranged to interrupt a light beam, and the counter made to count the resulting pulses.

Frequency counters designed for radio frequencies (RF) are also common and operate on the same principles as lower frequency counters. Often, they have more range before they overflow. For very high (microwave) frequencies, many designs use a high-speed prescaler to bring the signal frequency down to a point where normal digital circuitry can operate. The displays on such instruments consider this so they still display the correct value. Microwave frequency counters can currently measure frequencies up to almost 56 GHz. Above these frequencies, the signal to be measured is combined in a mixer with the signal from a local oscillator, producing a signal at the difference frequency, which is low enough to be measured directly.

## Accuracy and resolution

The accuracy of a frequency counter is strongly dependent on the stability of its timebase. In particular, timebase stability may be degraded by vibration, electromagnetic interference, drift due to age, and temperature variations, thus making the measured frequency seem higher or lower than the actual value. Consequently, highly stable timebase circuits are usually employed in frequency counters. Simple crystal oscillators are commonly used when high accuracy is not required; for higher accuracy measurements, a GPS-disciplined oscillator or oven-controlled oscillator is typically used.

Accuracy is often limited by the available resolution of the measurement. The resolution of a single count is generally proportional to the timebase oscillator frequency and the gate time. Improved resolution can be obtained by several techniques such as oversampling/averaging.

Additionally, accuracy can be significantly degraded by jitter on the signal being measured. It is possible to reduce this error by oversampling/averaging techniques.

It is also possible to measure frequency using the same techniques in software in an embedded system. A central processing unit (CPU), for example, can be arranged to measure its own frequency of operation, provided it has some reference timebase to compare with.

## I/O Interfaces

I/O interfaces allow the user to send information to the frequency counter and receive information from the frequency counter. Commonly used interfaces include RS-232, USB, GPIB and Ethernet. Besides sending measurement results, a counter can notify users when user-defined measurement limits are exceeded. Common to many counters are the SCPI commands used to control them. A new development is built-in LAN-based control via Ethernet complete with GUIs. This allows one computer to control one or several instruments and eliminates the need to write SCPI commands.
