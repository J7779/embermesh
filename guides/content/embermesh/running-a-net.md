---
id: embermesh/running-a-net
title: "EmberMesh 23: Running a Net on a Shared Frequency"
category: embermesh
tags: [embermesh, net, net plan, frequency, net control, callsign, amateur, licence-free, frs, gmrs, pmr446, murs, cb, airtime, duty cycle, schedule, check-in, voice]
summary: "How a gateway announces the frequency plan, how everyone tunes their handhelds to it, licence-free versus amateur bands, the no-encryption and callsign rules, channel discipline, airtime, scheduled check-in windows, and how voice-only radio users take part."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 23
---

# Running a Net on a Shared Frequency

A **net** is a group of stations that agree to use the same frequencies, settings and times. On
EmberMesh one person (the **net control**, usually whoever runs the gateway) writes the plan and
the app **announces it** to everyone on the mesh, so nobody has to guess where to tune.

## 1. Choose frequencies

Open **Tools → Band plans**, choose your **Region**, and type in the filter box (for example
`GMRS`, `446`, `LoRa`, `2 m`). Each service shows its licence requirement, power limit and
channels.

### Licence-free services (examples from the built-in band plans)

| Service | Region | Notes from the band plan |
|---|---|---|
| FRS | US | 22 channels, 12.5 kHz FM, up to 2 W (channels 8-14: 0.5 W). Channel 20 (462.675 MHz) is widely used as an emergency/traveller channel; channel 3 by community preparedness nets. |
| MURS | US | 5 VHF channels (151.820-154.600 MHz), 2 W, external antennas allowed. VHF carries better through woods. |
| PMR446 | EU, UK | 16 channels at 446 MHz, 0.5 W, fixed antenna. Channels 1-8 exist on every radio. |
| LPD433 | EU | 69 channels at 433 MHz, 10 mW. |
| CB 27 MHz | US, CA, AU, EU, UK | 40 channels, 4 W. Channel 9 = emergency, 19 = road. |
| UHF CB 477 MHz | AU, NZ | 80 channels, 5 W. **Channels 5 and 35 are for emergencies only (no chat).** Channel 11 = calling. Channels 22/23 are data/telemetry only. |
| GMRS | US | Same channels as FRS, up to 50 W and repeaters; **needs an FCC licence** (no exam). |

Whether a licence-free service allows **data** (EmberMesh modem sounds) differs between
countries and services. Check before you transmit data on it. Voice use is always fine.

### Amateur bands (licence and callsign required)

| Band | Useful frequencies from the band plan |
|---|---|
| 2 m, Americas | 146.520 MHz national FM simplex calling; 146.550 and 146.580 simplex; 144.390 APRS; 145.010 packet |
| 2 m, Europe (IARU Region 1) | 145.500 FM calling; 144.800 APRS; FM simplex channels around 145.2-145.6 |
| 70 cm, Americas | 446.000 national simplex calling |
| 70 cm, Europe | 433.500 FM calling; 433.775 LoRa APRS |
| HF emergency centres of activity | 3.760, 7.110, 14.300, 18.160, 21.360 MHz (listen first) |

Good practice:

- Put **mesh data on its own simplex frequency**, not on a calling frequency and not on the
  national APRS frequency (144.390 / 144.800 MHz), which is shared by many automatic stations.
- Keep a **separate voice frequency** in the plan for people without data gear.
- Listen for a while before choosing: use a frequency nobody else is using.

## 2. Write and announce the net plan

On the gateway (or any node):

1. **More → Links & radios**, scroll to **Net plan (where to tune)**.
2. Under **My announced plan** tap **Edit**.
3. For each frequency fill in:
   - **Label** (e.g. `Data`, `Voice`, `Calling`)
   - frequency in **MHz** (e.g. `146.550`)
   - **Mode**: **Voice (FM/SSB)**, every modem the sound-card link offers (**AFSK 1200**, **AFSK
     300**, **PSK 2400**, **PSK 4800**, **Robust 31**, **Robust 62**, **Robust 125**, **Robust
     250**, **DTMF touch tones**, **Morse code (CW)**, **PSK31**, **RTTY 45 baud**), then
     **LoRa**, **Meshtastic** and **Other**
   - **CTCSS** tone if used (e.g. `100.0`)
   - **Schedule** (e.g. `top of every hour, 10 min`)
   - **Notes** (e.g. `licence-free; callsign required`)
4. **Add frequency** for more rows (up to 16).
5. Tap **Save & announce**. You see "Net plan saved and will be announced".

The plan is sent across the mesh about every 30 minutes and kept for 2 days. Everyone else sees
it under **Net plan → Heard from** (name) with the time it was heard. It travels as signed
cleartext, so it is allowed on amateur links and nobody can forge it in your name.

Plans heard from older versions that wrote modes loosely (for example `bpsk31` or `Morse`) are
shown with the standard names.

### Example plan

| Label | Frequency | Mode | Tone | Schedule | Notes |
|---|---|---|---|---|---|
| Data | 146.550 MHz | AFSK 1200 | | continuous | gateway QX0ABC-1 |
| Voice | 146.580 MHz | Voice (FM/SSB) | 100.0 | top of every hour | net control reads bulletins |
| Long range | 906.875 MHz | Meshtastic | | continuous | LONG_FAST, channel slot 1 |
| Acoustic | 462.6125 MHz | Robust 62 | | top of every hour, 10 min | FRS ch 3, licence-free, if data is allowed |

## 3. Everyone tunes in

Give everyone the plan on paper too. For each person with a handheld:

1. Put the radio in **frequency mode** (often called VFO) and enter the plan's frequency; or
   choose the matching channel number on a licence-free radio.
2. Set the **CTCSS tone** if the plan has one, on both transmit and receive.
3. Use **narrow** (12.5 kHz) FM if the radio has a choice and the plan does not say otherwise.
4. Set the **lowest power** that works.
5. Attach the phone (cable or acoustic) and set the audio link to the plan's modem
   ([Handheld radios](handheld-audio-modem.md)).
6. Check in: **SOS → Safety check-in → I'm OK**. Net control sees you in the **Roll call**.

**Tools → CTCSS / DCS tone tables** lists the standard tones; **Tools → NATO phonetic
spelling** spells callsigns and names for voice.

## 4. Amateur rules: no encryption, callsign on every packet

- Set your callsign in **Settings → Profile → Callsign (licensed amateurs only)**. The node copies
  it into every amateur link; add an SSID such as `-7` in a link's **Your callsign** field if you
  run several stations.
- With **Band / licence: Amateur radio**, every frame carries your callsign automatically and
  nothing encrypted is ever transmitted. The Links screen shows **No encryption (amateur)** on
  such links and explains what this means.
- Voice operators still identify by voice as their licence rules require; see
  [FCC Part 97 operation](../radio/fcc-part97-subpart-b-operation.md) for the US rules, or your
  national rules.
- Encrypted direct messages and private channels simply wait for another path. Tell your users:
  on a ham-only net, **write in Public or an open channel, and assume everyone can read it.**

## 5. Channel discipline

- **Listen before you talk.** The audio link waits for a clear channel by itself; voice users
  must do the same.
- **Short messages.** At AFSK 1200 a short message takes about a second on air; at Robust 62
  about 15 seconds. A long message blocks the frequency for everyone.
- **One data frequency, one voice frequency.** Voice on the data frequency delays everyone's
  data; modem sounds on a voice frequency annoy people.
- **Emergency priority** only for real emergencies; SOS beacons repeat by themselves.
- **Low hop limits** on dense nets (Settings → Mesh & relaying → Default hop limit, Advanced).
- **Do not run many gateways on one frequency side by side** unless they are needed for
  coverage; each one relays.

## 6. Airtime and duty cycle

- Every link counts its airtime, including the key-up time and framing of each transmission, so a
  duty-cycle limit is never exceeded on the air.
- Every link counts its airtime. Where a band has a legal **duty-cycle limit** (for example LoRa
  in Europe: 10 % or 1 % per hour), the link enforces it and shows an **Airtime used** bar.
- **People → Stats** (or **More → Mesh statistics**) shows airtime and the duty cycle used per
  link.
- **Tools → Radio & propagation → LoRa time on air / duty cycle** shows how many packets fit in
  an hour.
- On amateur and licence-free voice bands there is normally no duty limit in the app (the
  **Duty-cycle limit** setting defaults to 1000 ‰ = none), but airtime is still shared: keep it low.

## 7. Scheduled check-in windows (saving batteries)

EmberMesh has **no automatic sleep schedule**. Windows are an agreement between people, written
in the net plan's **Schedule** field.

1. The **gateway stays on** all the time (on mains, battery or solar). It stores everything.
2. Everyone else switches radio and phone link on during the window, for example **10 minutes at
   the top of every hour**, and off in between.
3. When a station comes on, it and the gateway exchange everything stored for each other
   automatically (store-and-forward). Then people check in and read.
4. On slow radio links the stations first have to hear each other and exchange keys (see below);
   then the stored messages follow. With several stations, or the slow Robust modes, this takes
   minutes: allow **at least 10 minutes** per window rather than 2-3.

Stations on a slow shared radio channel (sound-card modems, KISS TNCs, LoRa) now find each other by
themselves even when they are switched on at different times: the first time a station hears a new
neighbour it announces itself, and a first message carries its sender's announcement with it, so it
can be checked at once. In tests over simulated audio channels, stations switched on in random order
up to a minute apart all held each other's keys after (median / worst) 18 / 49 s for two stations,
53 / 67 s for three and 82 / 203 s for five at AFSK 1200; at Robust 250 it took 58 / 79 s, 140 / 171 s
and 292 / 559 s. The first Public message between strangers reached every station in a median of
about 4-5 s at AFSK 1200 (about 20 s at Robust 250). Stations also wait their turn after hearing
someone, so they rarely talk over each other.

The **Communications plan** checklist template in **Notes & lists → Checklists** helps you write
the whole plan down.

## 8. People with only a voice radio

Not everyone will have a phone with EmberMesh. They can still take part:

- **Net control reads aloud** new bulletins, SOS alerts and messages on the voice frequency at
  the scheduled time, and types in replies for them.
- **Morse code:** set an audio link to **Morse code (CW)** and use **Plain-text message**: the
  text goes out as Morse that anyone can copy by ear (slow it down with `wpm=12` in **Modem
  fine-tuning**). Morse heard on the channel is decoded into a chat conversation.
- **DTMF touch tones:** a radio with a keypad can send digits (for example an agreed code number);
  a **DTMF touch tones** link decodes them.
- Useful tools for voice operators: **Tools → Signalling → Q-codes and prosigns**, **NATO phonetic
  spelling**, **RST signal report**. Procedure guides in the library:
  [radiotelephony procedure](../radio/radiotelephony-procedure.md),
  [procedure words](../radio/procedure-word.md).
