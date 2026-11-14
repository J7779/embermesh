---
id: electronics/wp2-oscilloscope
title: Oscilloscope
category: electronics
tags: [oscilloscope, oscillograph, scope, o-scope, electronics]
summary: An oscilloscope (formerly known as an oscillograph, informally scope or O-scope) is a type of electronic test instrument that graphically displays varying voltages of one or more signals as a function of time.
source: https://en.wikipedia.org/w/index.php?title=Oscilloscope&oldid=1383578184
source_title: Wikipedia: Oscilloscope
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Oscilloscope", Wikipedia, The Free Encyclopedia (revision 1383578184), https://en.wikipedia.org/w/index.php?title=Oscilloscope&oldid=1383578184 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# Oscilloscope

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

An **oscilloscope** (formerly known as an **oscillograph**, informally **scope** or **O-scope**) is a type of electronic test instrument that graphically displays varying voltages of one or more signals as a function of time. Their main purpose is capturing information on electrical signals for debugging, analysis, or characterization. The displayed waveform can then be analyzed for properties such as amplitude, frequency, rise time, time interval, distortion, and others. Originally, calculation of these values required manually measuring the waveform against the scales built into the screen of the instrument. Modern digital instruments may calculate and display these properties directly.

Oscilloscopes are used in the sciences, engineering, biomedical, automotive and the telecommunications industry. General-purpose instruments are used for maintenance of electronic equipment and laboratory work. Special-purpose oscilloscopes may be used to analyze an automotive ignition system or to display the waveform of the heartbeat as an electrocardiogram, for instance.

## Features and uses

An analog oscilloscope is typically divided into four sections: the display, vertical controls, horizontal controls, and trigger controls. The display is usually a CRT with horizontal and vertical reference lines called the *graticule*. CRT displays also have controls for focus, intensity, and beam finder.

The vertical section controls the amplitude of the displayed signal. This section has a volts-per-division (Volts/Div) selector knob, an AC/DC/Ground selector switch, and the vertical (primary) input for the instrument. Additionally, this section is typically equipped with the vertical beam position knob.

The horizontal section controls the time base or sweep of the instrument. The primary control is the Seconds-per-Division (Sec/Div) selector switch. Also included is a horizontal input for plotting dual X-Y axis signals. The horizontal beam position knob is generally located in this section.

The trigger section controls the start event of the sweep. The trigger can be set to automatically restart after each sweep or can be configured to respond to an internal or external event. The principal controls of this section are the source and coupling selector switches, and an external trigger input (EXT Input) and level adjustment.

In addition to the basic instrument, most oscilloscopes are supplied with a probe. The probe connects to any input on the instrument and typically has a resistor of ten times the oscilloscope's input impedance. This results in a 0.1 (−10×) attenuation factor; this helps to isolate the capacitive load presented by the probe cable from the signal being measured. Some probes have a switch allowing the operator to bypass the resistor when appropriate.

### Size and portability

Most modern oscilloscopes are lightweight, portable instruments compact enough for a single person to carry. In addition to portable units, the market offers a number of miniature battery-powered instruments for field service applications. Laboratory grade oscilloscopes, especially older units that use vacuum tubes, are generally bench-top devices or are mounted on dedicated carts. Special-purpose oscilloscopes may be rack-mounted or permanently mounted into a custom instrument housing.

### Inputs

The signal to be measured is fed to one of the input connectors, which is usually a coaxial connector such as a BNC or UHF type. Binding posts or banana plugs may be used for lower frequencies. If the signal source has its own coaxial connector, then a simple coaxial cable is used; otherwise, a specialized cable called a "scope probe", supplied with the oscilloscope, is used. In general, for routine use, an open wire test lead for connecting to the point being observed is not satisfactory, and a probe is generally necessary. General-purpose oscilloscopes usually present an input impedance of 1 megohm in parallel with a small but known capacitance such as 20 picofarads. This allows the use of standard oscilloscope probes. Scopes for use with very high frequencies may have 50 Ω inputs. These must be either connected directly to a 50 Ω signal source or used with Z₀ or active probes.

Less-frequently-used inputs include one (or two) for triggering the sweep, horizontal deflection for X‑Y mode displays, and trace brightening/darkening, sometimes called *z*‑axis inputs.

### Probes

Open wire test leads ("flying leads") are likely to pick up interference, so they are not suitable for low-level signals. Furthermore, the leads have a high inductance, so they are not suitable for high frequencies. Using a shielded cable (i.e., coaxial cable) is better for low-level signals. Coaxial cable also has lower inductance, but it has higher capacitance: a typical 50 ohm cable has about 90 pF per meter. Consequently, a one-meter direct (1×) coaxial probe loads a circuit with a capacitance of about 110 pF and a resistance of 1 megohm.

To minimize loading, attenuator probes (e.g., 10× probes) are used. A typical probe uses a 9 megohm series resistor shunted by a low-value capacitor to make an RC compensated divider with the cable capacitance and scope input. The RC time constants are adjusted to match. For example, the 9 megohm series resistor is shunted by a 12.2 pF capacitor for a time constant of 110 microseconds. The cable capacitance of 90 pF in parallel with the scope input of 20 pF and 1 megohm (total capacitance 110 pF) also gives a time constant of 110 microseconds. In practice, there is an adjustment so the operator can precisely match the low frequency time constant (called compensating the probe). Matching the time constants makes the attenuation independent of frequency. At low frequencies (where the resistance of *R* is much less than the reactance of *C*), the circuit looks like a resistive divider; at high frequencies (resistance much greater than reactance), the circuit looks like a capacitive divider.

The result is a frequency compensated probe for modest frequencies. It presents a load of about 10 megohms shunted by 12 pF. Such a probe is an improvement, but does not work well when the time scale shrinks to several cable transit times or less (transit time is typically 5 ns). In that time frame, the cable looks like its characteristic impedance, and reflections from the transmission line mismatch at the scope input and the probe causes ringing. The modern scope probe uses lossy low capacitance transmission lines and sophisticated frequency shaping networks to make the 10× probe perform well at several hundred megahertz. Consequently, there are other adjustments for completing the compensation.

Probes with 10:1 attenuation are by far the most common; for large signals (and slightly-less capacitive loading), 100:1 probes may be used. There are also probes that contain switches to select 10:1 or direct (1:1) ratios, but the latter setting has significant capacitance (tens of pF) at the probe tip, because the whole cable's capacitance is then directly connected.

Most oscilloscopes provide for probe attenuation factors, displaying the effective sensitivity at the probe tip. Historically, some auto-sensing circuitry used indicator lamps behind translucent windows in the panel to illuminate different parts of the sensitivity scale. To do so, the probe connectors (modified BNCs) had an extra contact to define the probe's attenuation. (A certain value of resistor, connected to ground, "encodes" the attenuation.) Because probes wear out, and because the auto-sensing circuitry is not compatible between different oscilloscope makes, auto-sensing probe scaling is not foolproof. Likewise, manually setting the probe attenuation is prone to user error. Setting the probe scaling incorrectly is a common error, and throws the reading off by a factor of 10.

_(This section is shortened.)_

_(Later sections of the original article were left out of this offline copy for length: the rest of Features and uses, Other features, Types and models.)_
