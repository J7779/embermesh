---
id: embermesh/relays-and-repeaters
title: "EmberMesh 22: Relays and Repeaters"
category: embermesh
tags: [embermesh, relay, repeater, digipeater, solar, raspberry pi, spare phone, siting, line of sight, radio horizon, fresnel, link budget, coverage, ping]
summary: "How every EmberMesh node relays, how to build an unattended relay from a battery, solar panel, radio and a small computer or spare phone, how to site it for height and line of sight using the app's planning tools, chaining relays, and testing coverage with ping."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 22
---

# Relays and Repeaters

## Every node is already a relay

You do not need special equipment to relay. Every EmberMesh node with **Settings → Mesh &
relaying → Relay messages for others** switched on (the default):

- re-sends new messages it hears, a limited number of hops;
- **bridges** between all its links (Bluetooth to radio, Wi-Fi to LoRa ...), except that
  encrypted traffic is never put on amateur links;
- **stores** messages it cannot deliver and hands them over later (store-carry-forward).

A **dedicated relay** is just a node placed somewhere high, left running unattended, usually with
a radio and a good antenna.

## What a good relay site gives you

At VHF, UHF and LoRa frequencies, signals travel roughly in straight lines. The Earth's curve and
hills block them. **Height is everything.**

Results of **Tools → Radio & propagation → Radio horizon / line of sight** (standard
refraction, flat ground):

| Antenna 1 | Antenna 2 | Maximum line-of-sight distance |
|---|---|---|
| 1.5 m (handheld) | 1.5 m (handheld) | about 10 km |
| 15 m (mast, roof) | 1.5 m | about 21 km |
| 30 m (tower, tall building) | 1.5 m | about 28 km |
| 100 m (hill above the plain) | 1.5 m | about 46 km |

These are upper limits on open, flat ground; trees, buildings and hills cut them down a lot.
Two handhelds in a town often reach only a kilometre or two.

## Planning with the app's tools

All are under **Tools → Radio & propagation**:

| Tool | What it answers |
|---|---|
| **Radio horizon / line of sight** | How far can two antennas at these heights see each other? |
| **Fresnel zone and earth bulge** | How much clear space is needed around the straight line? Keep at least 60 % of the first Fresnel zone free of trees, roofs and ridges. |
| **Link budget and range** | With this power, these antennas and cables, and this receiver, how much path loss can the link take, and what range does that suggest? Includes sensitivity presets for LoRa, FM voice, 1200 bd packet and slow robust modes. |
| **Repeater / relay site planner** | From relay height, user height, height of trees/buildings, powers and gains: talk-out range (relay to user), talk-in range (user to relay) and the expected coverage radius, and which of them limits it. |
| **Free-space path loss**, **Coax / feedline loss**, **ERP / EIRP**, **dBm / W / V converter** | Supporting calculations |

**Tools → Station profiles** saves your radios (power, feedline, antenna gain, height) so the
calculators fill the transmitter and receiver fields with one tap (**Use a station profile**).

### Worked example: is a 15 m mast enough?

**Repeater / relay site planner** with its defaults (146 MHz, relay antenna 15 m, users 1.5 m,
5 W = 37 dBm both ways, FM voice receivers, 10 dB fade margin, open rural terrain) gives a
coverage radius of about **21 km, limited by the radio horizon**. The power budget alone would
allow much more. Lesson: **raise the antenna before adding power.** With trees or buildings
10 m high around the users and a 30 m mast, the horizon from the treetops limits coverage to about
18 km, and users under the trees lose a further 8 dB.

### Worked example: Fresnel clearance

**Fresnel zone and earth bulge** for a 10 km path, at the midpoint:

- at 146 MHz the first Fresnel zone has a radius of about 72 m; 60 % clearance is about 43 m;
- at 915 MHz (LoRa) the radius is about 29 m; 60 % is about 17 m;
- the earth bulge at the midpoint of 10 km is about 1.5 m.

So a 10 km link between two 5 m masts across flat land is technically "line of sight" but has
far from enough clearance; expect extra loss. Put at least one end high.

Remember: these are models. The tools say so themselves ("real terrain varies a lot"). Measure
with the ping test below.

## Building an unattended relay

```
     antenna (as high as possible)
        |
        |  short, good coax
        |
  +-----+---------------------------------------------+
  | weatherproof box                                  |
  |   [radio or LoRa device] --USB/audio-- [Pi or     |
  |                                         phone]    |
  |        \__________ 5 V / 12 V ________/           |
  |                      |                            |
  |              [charge controller]---[battery]      |
  +----------------------|----------------------------+
                         |
                   [solar panel] facing the equator
```

### Option A: Raspberry Pi (or any small Linux computer)

1. Install emberd as a service ([Setting up a hub](hub-setup.md)).
2. For a pure relay without Wi-Fi guests, add `--no-hub` (in the service's `ExecStart`, or
   `"hub": false` in `/var/lib/embermesh/emberd.json`). Keep the hub on if you also want a local
   hotspot.
3. Add the radio link(s) from the operator page or the console (`link add`) before you take it up
   the hill: they are saved by the node and come back after every restart. (Links in
   `emberd.json` are re-added at every start too; links given with `--link` exist only while that
   flag is used.) Use `--callsign` for amateur radio links.
4. If its storage has a passphrase, give it `--passphrase-file` or it will wait locked after a
   power cut, relaying nothing on its radios until someone unlocks it.
5. Test it at home with the full ping procedure, then deploy.

A relay with **two radios** is better than one: for example LoRa towards the other relays and a
VHF audio link towards the handhelds.

### Option B: a spare Android phone

1. Install EmberMesh, complete the first-run screens, add the radio link (USB OTG to a Meshtastic
   radio or RNode, or an audio cable to a handheld with VOX).
2. In EmberMesh open **Settings → This device**:
   - tap **Keep running with the screen off** and allow the exemption from battery optimisation
     (the entry then reads **Battery optimisation: exempt**);
   - tap **Start when the phone starts**, so the relay comes back by itself after a reboot.
   On Xiaomi, Huawei, OPPO/Realme, Samsung and similar, also allow **autostart** and remove
   background restrictions in the vendor's own battery settings.
3. Keep the phone on a charger. EmberMesh runs as a background service with a persistent
   notification and restarts itself if killed.
4. Radios that draw a lot of power from USB may need a powered hub or a Y-cable.

Unlike a computer relay, a phone relay has only been tested on the Android emulator; long
unattended running on a real phone has not been tried.

### Option C: a spare iPhone

iPhones suspend apps in the background. Bluetooth links keep working; other links stop. For relay
duty keep the app **open on screen, on a charger**. A running sound-card link also keeps the app
alive.

### Option D: a Meshtastic router on its own

A Meshtastic radio set up as a router on a hill rebroadcasts EmberMesh packets like any other
packet, with no computer at all. It does **not** store-and-forward for EmberMesh: messages pass
only when both ends are on the air at the same time.

### Power for a relay

Use **Tools → Power → Solar panel and battery sizing** with the relay's real daily energy use
(the tool's own guidance: a LoRa node about 1-3 Wh per day; a 5 W handheld transmitting 10 % of the
time about 10 Wh). Size for several days without sun. See [Power and field
operation](power-and-field.md).

## Chaining relays

```
  Camp A ))) ---- relay 1 (ridge) ---- relay 2 (water tower) ---- ((( Camp B
```

- Each relay adds delay and airtime. On a **single shared frequency**, a relay that repeats on the
  same frequency uses airtime twice; on slow links throughput drops quickly with every hop.
- Better: **different links between relays and to users** (LoRa between relays, VHF or Wi-Fi to
  the users).
- The default hop limit allows several relays. On a long chain check **Settings → Mesh & relaying
  → Default hop limit** (Advanced).
- Store-and-forward means a chain still works if a relay is briefly down: messages wait.

## Testing coverage with ping

1. Add the relay (or a node beyond it) as a contact.
2. Walk or drive outwards. At each stop open the contact and tap **Ping**. Note the result:
   "Pong: 2.1 s round trip, 1 hop" (or no answer within a minute).
3. The contact page also shows **Signal** (RSSI in dBm and SNR in dB, where the radio reports them)
   and **Path** (direct or how many hops). **People → Nearby** shows the same for every node heard.
4. If positions are shared, **People → Radar** draws nodes around you by distance and bearing.
5. Write down where it works and where it fails. Move the relay antenna higher before trying
   anything else.
