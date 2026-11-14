---
id: electronics/wp2-555-timer-ic
title: 555 timer IC
category: electronics
tags: [555 timer ic, electronics]
summary: The 555 timer IC is an integrated circuit used in a variety of timer, delay, pulse generation, and oscillator applications.
source: https://en.wikipedia.org/w/index.php?title=555_timer_IC&oldid=1384359247
source_title: Wikipedia: 555 timer IC
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "555 timer IC", Wikipedia, The Free Encyclopedia (revision 1384359247), https://en.wikipedia.org/w/index.php?title=555_timer_IC&oldid=1384359247 . Converted to Markdown for the EmberMesh offline library; references, images, infoboxes, navigation templates and non-practical sections (such as history, etymology and culture) omitted. Licensed CC BY-SA 4.0 (https://creativecommons.org/licenses/by-sa/4.0/).
retrieved: 2026-11-13
---

# 555 timer IC

_Encyclopedia article (Wikipedia, CC BY-SA 4.0), converted to plain Markdown and excerpted: references, images, infoboxes, navigation boxes and non-practical sections were removed._

The **555 timer IC** is an integrated circuit used in a variety of timer, delay, pulse generation, and oscillator applications. It is one of the most popular timing ICs due to its flexibility and price. Derivatives provide two (556) or four (558) timing circuits in one package. The design was first marketed in 1972 by Signetics and used bipolar junction transistors. Since then, numerous companies have made the original timers and later similar low-power CMOS timers. In 2017, it was said that over a billion 555 timers are produced annually by some estimates, and that the design was "probably the most popular integrated circuit ever made".

## Design

Depending on the manufacturer, the standard 555 package incorporated the equivalent of 25 transistors, 2 diodes, and 15 resistors on a silicon chip packaged into an 8-pin dual in-line package (DIP-8). Variants available included the 556 (a DIP-14 combining two complete 555s on one chip), and 558 / 559 (both variants were a DIP-16 combining four reduced-functionality timers on one chip).

The **NE555** parts were commercial temperature range, 0 °C to +70 °C, and the **SE555** part number designated the military temperature range, −55 °C to +125 °C. These chips were available in both high-reliability metal can (T package) and inexpensive epoxy plastic (V package) form factors. Thus, the full part numbers were NE555V, NE555T, SE555V, and SE555T.

Low-power CMOS versions of the 555 are now available, such as the Intersil ICM7555 and Texas Instruments LMC555, TLC555, TLC551.

### Internal schematic

The internal block diagram and schematics of the 555 timer below are highlighted with the same colors across all three drawings to clarify how the chip is implemented:
-: Between the positive supply voltage V_CC and the ground GND is a voltage divider consisting of three identical resistors (5 kΩ for bipolar timers, 100 kΩ or higher for CMOS) to create reference voltages for the analog comparators. CONTROL is connected between the upper two resistors, allowing an external voltage to control the reference voltages:
 - When CONTROL is not driven, this divider creates an upper reference voltage of 2/3 V_CC and a lower reference voltage of 1/3 V_CC.
 - When CONTROL is driven, the upper reference voltage will instead be V_CONTROL and the lower reference voltage will be 1/2 V_CONTROL.
-: The comparator's negative input is connected to voltage divider's upper reference voltage, and the comparator's positive input is connected to THRESHOLD.
-: The comparator's positive input is connected to voltage divider's lower reference, and the comparator's negative input is connected to TRIGGER.
-: A set-reset latch stores the state of the timer and is controlled by the two comparators. RESET overrides the other two inputs, thus the latch (and therefore the entire timer) can be reset at any time.
-: The output of the latch is followed by an output stage with push–pull output drivers that can supply up to 200 mA for bipolar timers, lower for CMOS timers.
-: Also, the output of the latch controls a transistor acting as an electronic switch that connects DISCHARGE to ground.

### Pinout

The pinout of the 8-pin 555 timer and 14-pin 556 dual timer are shown in the following table. Since the 556 is conceptually two 555 timers that share power pins, the pin numbers for each half are split across two columns.

| 555 pin# | 556 (unit 1) | 556 (unit 2) | Pin name | Pin direction | Pin description |
|---|---|---|---|---|---|
| Yes | 7 | | GND | Power | Ground supply: this pin is the ground reference voltage (zero volts). |
| Yes | 6 | 8 | | Input | Trigger: when *V*_TRIGGER falls below 1/2 *V*_CONTROL (1/3 *V*_CC, except when CONTROL is driven by an external signal), OUTPUT goes to the high state and a timing interval starts. As long as TRIGGER continues to be kept at a low voltage, OUTPUT will remain in the high state. |
| Yes | 5 | 9 | OUTPUT | Output | Output: this pin is a push-pull (P.P.) output that is driven to either a low state (GND) or a high state (*V*_CC minus approximately 1.7 volts for bipolar timers, or *V*_CC for CMOS timers). |
| Yes | 4 | 10 | | Input | Reset: a timing interval may be reset by driving this pin to GND, but the timing does not begin again until this pin rises above approximately 0.7 volts. This pin overrides, which in turn overrides THRESHOLD. If this pin is not used, it should be connected to *V*_CC to prevent electrical noise accidentally causing a reset. |
| Yes | 3 | 11 | CONTROL | Input | Control: this pin provides access to the internal voltage divider (2/3 *V*_CC by default). By applying a voltage to this pin, the timing characteristics can be changed. In astable mode, this pin can be used to frequency-modulate the OUTPUT state. If this pin is not used, it should be connected to a 10 nF decoupling capacitor (between this pin and GND) to ensure electrical noise doesn't affect the internal voltage divider. |
| Yes | 2 | 12 | THRESHOLD | Input | Threshold: when the voltage at this pin is greater than *V*_CONTROL (2/3 *V*_CC by default except when CONTROL is driven by an external signal), then the OUTPUT high state timing interval ends, causing OUTPUT to go to the low state. |
| Yes | 1 | 13 | DISCHARGE | Output | Discharge: This pin is an open-collector (O.C.) output for bipolar timers, or an open-drain (O.D.) output for CMOS timers. This pin can be used to discharge a capacitor when OUTPUT is low. In bistable latch and bistable inverter modes, this pin is unused, which allows it to be used as an alternate output. |
| Yes | 14 | | *V*_CC | Power | Positive supply: For bipolar timers, the supply voltage range is typically 4.5 to 16 volts (some are spec'ed for up to 18 volts, though most will operate as low as 3 volts). For CMOS timers, the supply voltage range is typically 2 to 15 volts (some are spec'ed for up to 18 volts, and some are spec'ed as low as 1 volt). See the supply min and max columns in the derivatives table in this article. Decoupling capacitor(s) are generally applied (between this pin and GND) as a good practice. |

## Packages

In 1972, Signetics originally released the 555 timer in DIP-8 and TO5-8 metal can packages, and the 556 timer was released in a DIP-14 package.

In 2006, the dual 556 timer was available in through-hole packages as DIP-14 (2.54 mm pitch), and surface-mount packages as SO-14 (1.27 mm pitch) and SSOP-14 (0.65 mm pitch).

In 2012, the 555 was available in through-hole packages as DIP-8 (2.54 mm pitch), and surface-mount packages as SO-8 (1.27 mm pitch), SSOP-8 / TSSOP-8 / VSSOP-8 (0.65 mm pitch), BGA (0.5 mm pitch).

The MIC1555 is a CMOS 555-type timer with three fewer pins available in SOT23-5 (0.95 mm pitch) surface-mount package.

_(Later sections of the original article were left out of this offline copy for length: Modes, Derivatives.)_
