---
id: embermesh/power-and-field
title: "EmberMesh 28: Power and Field Operation"
category: embermesh
tags: [embermesh, battery, power, low power, solar, charging, background, android battery optimisation, iphone background, weatherproofing, field kit]
summary: "Making batteries last: the app's low-power and announce settings, keeping EmberMesh running in the background on Android and iPhone, sizing batteries and solar panels with the app's tools, keeping phones alive, and weatherproofing field equipment."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 28
---

# Power and Field Operation

## The app's battery settings

| Setting | Where | Effect |
|---|---|---|
| **Low-power mode (no animation, fewer refreshes)** | Settings → Region & display | The screen updates less often and without animation. The node also announces itself at most once an hour and sends fewer stored messages per exchange. |
| **Theme: Dark** | Settings → Region & display | Saves battery on OLED screens. **Night (red)** protects night vision. |
| **Announce this node every** | Settings → Mesh & relaying | How often your node announces itself: default 1800 s (30 min), 60 s to 86400 s (1 day), or 0 = never automatically. Longer saves airtime and battery; shorter helps others find you. |
| **Relay messages for others** | Settings → Mesh & relaying | Leave **on**. Turn off only to save battery in an emergency: the mesh depends on relays. |
| **Carry undelivered messages for** | Settings → Mesh & relaying | Days to keep messages for others: default 3, 1 to 30. Storage, not battery. |
| **Send read receipts** | Settings → Mesh & relaying | On by default; turning it off saves a little airtime. |
| **Keep updating** (GPS) | Settings → Position | Follows your position continuously; leave it off and tap **Update from GPS** when needed to save power. |

Radio and screen use far more power than the app itself. The biggest savings:

1. **Transmit less**: short messages, fewer and slower announcements, lowest radio power that works.
2. **Screen off** whenever possible; low brightness.
3. **Scheduled windows**: radios on only at agreed times; the gateway stays on and stores
   everything ([Running a net](running-a-net.md)).
4. **Airplane mode** with Bluetooth and Wi-Fi switched back on: a phone searching for a dead
   mobile network wastes power.

## Keeping EmberMesh running in the background

### Android

EmberMesh runs as a background service with a persistent notification ("relaying") and restarts
itself if Android kills it. Phone makers' battery savers are the usual problem:

1. **Settings → This device → Keep running with the screen off** opens Android's dialog to exempt
   EmberMesh from battery optimisation. Afterwards the entry reads **Battery optimisation: exempt**.
2. **Settings → This device → Start when the phone starts** makes the mesh start relaying by
   itself after a reboot (off by default).
3. On Xiaomi, Huawei, OPPO/Realme, Samsung and similar brands, also allow **autostart** and remove
   background restrictions in the vendor's battery settings.
4. The notification's **Quit** action stops EmberMesh completely (no relaying).

Measured on the Android emulator with a hub running and no guests: no measurable processor use
while idle. Real-phone battery life has not been measured.

The app holds the processor awake only while a link needs it (USB serial, audio, Wi-Fi/UDP,
TCP/WebSocket, the hub, classic Bluetooth). A **Bluetooth-LE-only** phone lets the Bluetooth system wake it,
which uses the least battery.

### iPhone

iOS suspends apps in the background:

- **Bluetooth links keep working** with the screen off and wake the app when data arrives.
- Apple peer-to-peer, Wi-Fi, TCP and hub links **stop shortly after the app leaves the screen**.
- A running **sound-card link** keeps the app alive (and uses more power).
- iOS also wakes the app now and then (at most every 15 minutes or so) to pass on stored messages.

For relay duty keep the app open on a charger.

### Browser (single file or hub page)

Browsers slow down and eventually freeze background tabs, especially on phones. Keep the tab
visible on a relay and the device on a charger.

## How long will the battery last?

**Tools → Power → Battery runtime** takes battery capacity, receive/idle current, transmit current,
the share of time transmitting, other loads and the usable share of capacity.

Example with the tool's defaults: 3000 mAh battery, 15 mA receiving, 120 mA transmitting, 5 % of
the time transmitting, 80 % usable gives an average of about 20 mA and **about 118 hours (just under
5 days)**. Measure or look up your own radio's currents.

Cold can **halve** the usable capacity of most batteries. Keep spare batteries inside your
clothing. (General advice: do not charge lithium batteries below freezing.)

## Sizing solar power

**Tools → Power → Solar panel and battery sizing** takes your daily energy use, peak sun hours per
day, system efficiency, days without sun to ride through, battery voltage and allowed depth of
discharge.

The tool's own guidance for **daily energy use**: a LoRa node about 1-3 Wh, a 5 W handheld that
transmits 10 % of the time about 10 Wh, a laptop 50-100 Wh. **Peak sun hours**: winter at high
latitude 1-2, temperate summer 4-6, desert 6-7.

Worked example: a relay using **10 Wh per day**, 3 sun hours, 75 % efficiency, 2 days without
sun, 12 V battery, 80 % depth of discharge:

| Result | Value |
|---|---|
| Minimum panel rating | about 4.4 W (round up generously, e.g. 10 W) |
| Battery capacity | 25 Wh, about 2.1 Ah at 12 V |
| Charge controller current | about 0.5 A |

The tool's advice: round the panel up generously (clouds, short winter days, ageing), tilt panels
towards the equator at roughly your latitude, and avoid even partial shade on one cell, which can
collapse the output.

Other power tools: **Wire voltage drop / ampacity** (choose cable for long runs), **Series /
parallel battery pack**, **Ohm's law / power**.

## Keeping phones alive

- Charge from power banks, a car's 12 V socket, or a small solar panel during the day.
- One charged phone per group as the always-on node; others switch on in the windows.
- Turn off what you do not need: GPS **Keep updating**, automatic position sharing, extra links.
- Keep phones out of direct sun; heat damages batteries and makes phones throttle.

## Weatherproofing field equipment

- Put the computer/phone, radio and battery in a **plastic box with a lid**; antennas **outside**
  the box (radio waves do not pass through metal, and pass poorly through wet material).
- Bring cables in **from below** with a **drip loop** (a downward loop so water drips off before
  the entry).
- **Seal coax connectors**: wrap with self-amalgamating tape, then electrical tape over it.
- Fight condensation: a small drain hole at the lowest point, a bag of silica gel inside.
- **Shade** the box from midday sun; a light colour helps.
- Mark the box with a note: what it is, who to contact, "do not disconnect".

## Field kit checklist

- [ ] Phone with EmberMesh, identity backed up, passphrase set
- [ ] Radio, spare battery, charger, the net plan on paper
- [ ] Interface cable or USB sound card and OTG adapter
- [ ] Antenna (or materials and the dimensions from [Antennas](antennas.md))
- [ ] Coax with connectors, self-amalgamating tape, cable ties, cord
- [ ] Power bank, solar panel, 12 V adapter
- [ ] USB stick with the app and a sneakernet bundle
- [ ] Notebook and pencil
