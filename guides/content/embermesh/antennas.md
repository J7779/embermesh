---
id: embermesh/antennas
title: "EmberMesh 22: Antennas You Can Build"
category: embermesh
tags: [embermesh, antenna, ground plane, dipole, j-pole, slim jim, yagi, tape measure yagi, lora antenna, coax, feedline, swr, choke, balun, safety, lightning, rf exposure]
summary: "When a better antenna helps, the app's antenna designer tools, fully worked builds from scavenged materials (quarter-wave ground plane, dipole, J-pole, Slim Jim, tape-measure Yagi, LoRa quarter-wave), feedline choice and loss, checking an antenna without an SWR meter, and safety."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 22
---

# Antennas You Can Build

## When and why

- The rubber antenna on a handheld is a compromise: roughly **-2 dBi**. A quarter-wave whip is
  about **2 dBi**, a J-pole about **3 dBi**, a small Yagi **7-10 dBi** (figures used by the app's
  station profiles). In free space every 6 dB doubles the distance; on the ground the gain is
  smaller but still worth having.
- **Getting the antenna high and in the clear matters more than anything else**, including
  transmitter power. A simple antenna on a pole beats a perfect one at chest height.
- An outdoor antenna on a pole, fed by coax, lets the radio and the phone stay dry indoors.
- Licence-free radios such as **FRS** and **PMR446** must keep their fixed antennas; do not modify
  them. MURS, GMRS, CB, amateur and LoRa radios may use external antennas (within each service's
  limits).

## The antenna designer tools

All under **Tools → Antennas**. Enter the frequency, tap **Calculate**: you get dimensions in
metric and imperial, a build diagram and tips.

| Tool | Use it for |
|---|---|
| **Quarter-wave ground plane** | Simple vertical for VHF/UHF; the best first antenna |
| **Half-wave dipole** | Wire antenna for any band; horizontal or vertical |
| **Inverted-V dipole** | Dipole hung from one central support (HF) |
| **J-pole** | End-fed vertical; copper pipe or twin-lead |
| **Slim Jim** | Folded J-pole from ladder line; hang it from a tree |
| **5/8-wave vertical** | Vertical with a base loading coil |
| **Yagi-Uda beam** | 3, 5 or 6 element beam for long point-to-point links |
| **Tape-measure Yagi (2 m / 70 cm)** | Handheld 3-element beam; also for direction finding |
| **Moxon rectangle** | Two-element wire beam with good front-to-back |
| **Full-wave loop (quad / delta)** | HF loop with a 75 ohm match |
| **End-fed half wave (EFHW)** | HF wire fed at one end through a 49:1 unun |
| **Random-wire lengths to avoid** | HF wire lengths that tune easily on chosen bands |
| **NVIS dipole** | Low HF dipole for 0-400 km coverage |
| **Coaxial collinear (COCO)** | Stacked coax sections for omnidirectional gain (VHF/UHF/LoRa) |
| **Coax choke (ugly balun)** | Keeps RF off the outside of the coax |
| **Coax baluns (1:1 sleeve, 4:1 loop)** | Balanced feeds |
| **Cantenna (2.4 GHz waveguide)** | Tin-can antenna for 2.4 GHz Wi-Fi |
| **Biquad (2.4 GHz)** | About 10 dBi wire antenna for Wi-Fi |
| **LoRa quarter-wave / ground plane** | 915 / 868 / 433 MHz LoRa and Meshtastic |

Each tool's **Shortening factor** (Advanced, default 0.95) suits thin bare wire; insulated wire is
about 0.93-0.94.

### Rules that apply to every build

- **Cut every element a little long, then trim in small steps (about 1 % at a time).** Shorter =
  higher frequency.
- Materials that work: electrical house wire, speaker wire split in two, fence wire, copper pipe,
  aluminium tube or curtain rods, steel tape measure, coat hangers. Insulators: plastic bottles,
  PVC offcuts, nylon rope, dry wood.
- Connect the coax **centre** to the radiating element and the **braid** to the radials / other
  leg. Seal the coax end against water (tape, then more tape, or silicone).

## Build 1: quarter-wave ground plane for 2 m (146.52 MHz)

Tool: **Quarter-wave ground plane**, frequency `146.52`, defaults (4 radials, 45° droop).

```
            |
            |  A  (centre conductor)
            |
      ======O======   <- SO-239 / chassis socket or plate
     /     coax     \    B radials (braid), drooped
    /                \   C = drop, D = reach
```

| Part | Length |
|---|---|
| A: vertical radiator | 48.6 cm (19.1 in) |
| B: each of 4 radials | 51.2 cm (20.1 in) |
| C/D: drop and reach of each radial tip at 45° | 36.2 cm |
| Feed impedance | about 50 ohm |

1. Mount a chassis socket (SO-239) in a scrap of sheet metal or a tin lid, or solder directly to
   the coax.
2. Solder the radiator (stiff wire, coat hanger, rod) to the centre pin.
3. Solder or bolt four radials to the metal plate / braid, spaced evenly, bent down about 45°.
4. Mount it on a non-metal pole as high as you can, feed it with 50 ohm coax.

For other bands enter the frequency: for 462.6 MHz (GMRS) the radiator is 15.4 cm and the radials
16.2 cm.

## Build 2: half-wave dipole for 2 m (146.52 MHz)

Tool: **Half-wave dipole**, frequency `146.52`.

```
        B                     B
  o===========o  ||  o===========o
  insulator   feed point (coax)  insulator
  |<---------------- A ---------------->|
                  coax: centre to one leg, braid to the other
```

| Part | Length |
|---|---|
| A: total, tip to tip | 97.2 cm (38.3 in) |
| B: each leg from the feed point | 48.6 cm (19.1 in) |
| Feed impedance | about 50-75 ohm at typical heights |

- Hang it **vertically** for VHF/UHF FM (handhelds are vertically polarised), horizontally for
  HF and SSB.
- Put a choke (a few turns of the coax itself) at the feed point.
- Example for CB channel 19 (27.185 MHz): each leg 2.62 m, total 5.24 m.

## Build 3: J-pole and Slim Jim for 2 m (146.52 MHz)

Tool: **J-pole** (copper pipe or twin-lead) and **Slim Jim** (ladder line), frequency `146.52`.

```
   J-POLE                          SLIM JIM
   |  ^                            +------+  <- closed top
   |  |                            |      |
   |  R (half wave)                |      |  R (folded half wave)
   |  |                            |      |
   |  v                            |      =  <- gap G in the inner side
   |      |  ^                   A |      |
   |      |  B (stub)              |      |  B (matching section)
 A |      |  |                     +-o--o-+  <- feed taps at height C
   +-o--o-+  v  <- feed at C       |      |
   |      |                        +------+  <- bottom bar; D = spacing
   +------+  <- shorting bar; D = spacing
```

| Part | J-pole | Slim Jim |
|---|---|---|
| A: overall / long element | 145.8 cm | 145.8 cm |
| R: radiating half wave | 97.2 cm | 97.2 cm (folded) |
| B: quarter-wave stub / matching section | 48.6 cm | 48.6 cm |
| C: feed tap above the bottom bar (start) | 4.1 cm | 5.1 cm |
| G: gap in the inner side (start) | - | 2.5 cm |
| D: spacing between the two sides | 2.6 cm | 2.6 cm |
| Gain | about 2.1 dBi | about 2.1 dBi |

1. **J-pole from 300 ohm twin-lead** (a "roll-up J"): cut the twin-lead to length A, short the
   two wires at the bottom, cut one wire at height B to form the stub, attach the coax at height C
   (centre to the long side, braid to the short side).
2. **Slim Jim from ladder line:** a closed loop of length A with the gap G in the inner wire
   just above the matching section.
3. **Tune:** slide both feed taps up or down together for the best match first, then trim the
   length for frequency.
4. Put a choke right below the feed point; J-poles are prone to radiating from the coax. Run the
   coax away horizontally for a quarter wave if you can. Hang a Slim Jim from a tree with fishing
   line.

## Build 4: tape-measure Yagi for 2 m (146.565 MHz)

Tool: **Tape-measure Yagi (2 m / 70 cm)**, band **2 m**. A three-element beam of about **7 dBi**
from a 25 mm (1 inch) steel tape measure and PVC pipe fittings (after the WB2HOL design).

```
   R          DE (two halves)        D
   |            |    |               |
   |            |    |               |
   +====S1======+-HH-+=====S2========+==>  beam
   |            | ^  |               |
   |            | hairpin U across the feed, coax centre to one half,
   |            |      braid to the other (G = gap)
```

| Part | Length |
|---|---|
| R: reflector (one piece) | 105.1 cm (41.4 in) |
| DE: each driven-element half | 45.1 cm (17.75 in) |
| G: gap between the driven halves (start) | 2.5 cm |
| D: director (one piece) | 89.2 cm (35.1 in) |
| S1: boom, reflector to driven element | 17.8 cm (7.0 in) |
| S2: boom, driven element to director | 29.2 cm (11.5 in) |
| H: hairpin wire (bent into a U across the feed) | 11.4 cm (4.5 in) |

1. Cut the tape with tin snips; cover the sharp ends with tape.
2. PVC tees and crosses form the boom; hose clamps hold the elements.
3. Sand the paint off where the coax attaches.
4. Point the director (the shortest element) at the other station.
5. The sharp null at the back makes it good for **finding a transmitter** (direction finding).
   The flexible elements survive knocks.

For 70 cm choose band **70 cm (435 MHz)**.

## Build 5: LoRa quarter-wave ground plane

Tool: **LoRa quarter-wave / ground plane**. Same shape as Build 1.

| Band | Radiator | Each of 4 radials (45° droop) | Half-wave dipole alternative, each half |
|---|---|---|---|
| 915 MHz (US/AU) | 7.8 cm | 8.2 cm | 7.8 cm |
| 868 MHz (EU) | 8.2 cm | 8.6 cm | 8.2 cm |
| 433 MHz (EU) | 16.4 cm | 17.3 cm | 16.4 cm |

- Solder the radiator to the SMA/u.FL centre pin. A quarter wave of stiff wire straight from the
  board works, but a proper ground plane is much better.
- **Never transmit without an antenna attached**; it can destroy the LoRa module.
- Keep the cable between board and antenna as short as possible (see below).

## Feedline (coax) choice and loss

Matched loss in dB per 100 ft (30.5 m) of new cable, from **Tools → Coax / feedline loss**:

| Cable | Impedance | 146 MHz | 446 MHz | 915 MHz | Notes |
|---|---|---|---|---|---|
| RG-174 | 50 | 10.8 | 19.4 | 28.6 | Thin; short jumpers only |
| RG-58 | 50 | 6.1 | 11.9 | 18.9 | Common; lossy at UHF |
| RG-8X (mini-8) | 50 | 4.5 | 8.4 | 12.8 | |
| RG-213 | 50 | 2.7 | 5.0 | 7.6 | Robust, good on HF/VHF |
| LMR-240 | 50 | 3.0 | 5.3 | 7.6 | |
| LMR-400 | 50 | 1.5 | 2.7 | 3.9 | Usual choice for VHF/UHF/LoRa runs |
| RG-6 (TV cable) | 75 | 2.4 | 4.3 | 6.2 | Low loss, easy to scavenge; about 1.5:1 SWR on 50 ohm radios from the mismatch alone |
| 450 ohm ladder line | 450 | 0.4 | 0.8 | 1.3 | Balanced; much worse when wet; keep away from metal |

- **Example:** 10 m of RG-58 loses about 2 dB at 146 MHz but about 6 dB at 915 MHz (three
  quarters of the power). LMR-400 loses about 1.3 dB over 10 m at 915 MHz.
- Every 3 dB lost is half your power, both ways. At UHF and LoRa frequencies keep runs short or
  use better cable; often it is better to put the radio up the mast and run power (and USB or a
  network cable) instead of coax.
- Old, kinked or water-logged coax is worse than these figures.

## Checking an antenna without an SWR meter

An SWR meter is best. Without one (approximate methods):

1. **Trim for the best received signal.** Listen to a steady distant station (another node's
   beacon, a repeater, a Meshtastic node). Note the **Signal** (RSSI/SNR) on that node's contact
   page or the link's **Statistics** after each 1 % trim. The length with the strongest reception
   is close to the best transmitting length too.
2. **Range test** with **Ping** ([Relays](relays-and-repeaters.md)) before and after a change.
3. **Watch the radio.** Many radios reduce power or get hot with a bad antenna; some show an SWR
   or reflected-power warning. If you can read forward and reflected power, **Tools → SWR /
   return loss / mismatch** converts them to SWR. Above 3:1, many solid-state radios fold back
   power: fix the antenna.
4. Check the obvious: centre and braid not shorted, no broken solder joint, connector dry.

See also the library guides [SWR](../radio/swr.md) and [Feed line](../radio/feed-line.md).

## Safety

- **Power lines kill.** Never put up an antenna or mast where it could touch, fall onto or blow
  into a power line. Keep well clear: a common rule of thumb is a distance of at least twice the
  mast height. Treat every overhead line as live.
- **Lightning.** A mast on a hill is a lightning target. Do not operate or work on outdoor
  antennas when thunder is heard. When storms come, disconnect the coax from the radio and lay
  it outside, away from the building, or connect it to a ground rod.
- **RF exposure.** Do not touch an antenna while it transmits (RF burns). Keep transmitting
  antennas away from people's heads and bodies, especially beams and higher-power radios. Mount
  antennas above head height.
- **Falls.** Do not climb alone. Raise masts with ropes and helpers; guy them in three directions.
