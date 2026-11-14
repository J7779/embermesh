---
id: embermesh/emergency-use
title: "EmberMesh 25: Emergency Use - SOS, Check-ins, Bulletins, Position and Signalling"
category: embermesh
tags: [embermesh, emergency, sos, distress, check-in, roll call, bulletins, position, gps, grid, morse, flashlight, signalling, tools]
summary: "Using the SOS beacon and responding to one, safety check-ins and roll call, the bulletins board, sharing position without phone networks, Morse by flashlight or tone, emergency frequencies, and the offline tools worth knowing."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 25
---

# Emergency Use

Where each control is: [Screens tour](screens-tour.md). Getting ready on your own device:
[Android](app-android.md), [iPhone/iPad](app-ios.md), [browser](app-web.md),
[computer](app-desktop.md).

## Send an SOS (someone's life is in danger)

1. Tap the red **SOS** tab (**Send for help** in Simple mode).
2. Optional: in **Message** say what is happening ("broken leg, 2 people, north side of the
   bridge"). Short is better.
3. Leave **Include my position** ticked. If your position is unknown the app tries GPS.
4. **Press and hold the big SOS button for 3 seconds.**

**You should see:** "SOS beacon active" and, at the top of every screen, a red banner **SOS ACTIVE
— beacon repeating.** The beacon repeats about once a minute. Every device that hears it relays it
before anything else, over every kind of link, including amateur-radio links.

While it runs:

- **Heard by:** lists everyone who acknowledged. "nobody has acknowledged yet" means keep going.
- Keep the device **switched on, high up or outdoors** if you can. The app keeps the screen awake
  where the device allows it.
- When you are safe or help has arrived, tap **I am safe — stop SOS** and confirm **Stop SOS**.

If you have no working link at all, the beacon waits in the queue and goes out the moment any link
appears. Also signal by other means (Morse light, below; ground-to-air signals).

## When you receive an SOS

- A red banner shows "SOS from (name): (text)" and a notification appears. Tap it.
- The **SOS** tab lists **SOS alerts received** with the text, the time first and last heard, how
  many hops away it came from, and the position with **distance and bearing from you** (if your
  own position is known).
- Tap **Acknowledge — I hear you**. The sender sees your name under **Heard by**. Tap
  **Message** to talk to them directly.
- **Then act:** relay it by voice on the emergency frequencies, organise help, keep your device
  relaying.
- **Verified and unverified alerts.** An SOS is shown immediately even if the sender's identity
  cannot be checked. Such alerts carry an **unverified sender** tag and are listed **below** the
  verified ones (several are grouped as "N alerts from unverified senders"). They may be real people
  whose keys you do not have yet, or forgeries. They **never raise a notification** (the red banner
  only counts them), and their number is limited, so a flood of fake alerts cannot hide a real one.
  Use judgement and confirm by message or voice.
- An SOS from someone you have **blocked** is still shown.

## Before the emergency: add and verify your people

The SOS screen can only put an alert at the top, with a notification, if it can check who sent it.
That needs the sender's keys, and "verified" in the contact list needs you to have checked them.
So, while everyone is still together:

1. Exchange contact cards (**People → My card** / **Add contact → Scan contact QR**), or add each
   other from **People → Nearby**.
2. Compare the eight fingerprint words (or scan the cards) and tap **Words match — verify**.
3. Do it for family, neighbours, the group leader and the hub or gateway operators.

See [How EmberMesh works](how-it-works.md) and [Security](security.md).

## Safety check-ins and roll call

A **check-in** tells everyone how you are. On the **SOS** tab, under **Safety check-in**:

1. Optionally type a **Note** (where, who with, what you need).
2. Tick **Include my position** if you want.
3. Tap one: **I'm OK**, **Need help**, **Injured**, **Moving**, **Sheltering**.

Everyone sees it under **Roll call (latest check-in per person)**, with counts per status at the top.
**Need help** and **Injured** raise an alert on other devices. Check-ins from unverified senders
carry an **unverified sender** tag.

**Roll call routine for a group leader or net control:**

1. Announce on voice and in **Public**: "Roll call now, everyone check in."
2. Watch the counts in **Roll call**.
3. Anyone missing after the agreed time: message them, then send someone to look.
4. Repeat at fixed times (put them in the net plan's **Schedule**).

Check-ins are kept for 2 days; only the latest from each person is shown.

## The bulletins board

**Bulletins** (on phones: **More → Bulletins**) is a community notice board that floods the whole
mesh and stays until it expires.

- Categories: **Water, Food, Medical, Shelter, Hazard, Trade, Missing, Transport, Info**. Tap a
  category to filter.
- **Post:** choose **Category**, a short **Title** ("Clean water at Lincoln school"), **Details**
  (where, when, who to contact), **Keep for** (1 hour, 6 hours, 1 day, 3 days, 1 week, 30 days)
  and optionally **Attach my position**.
- New **Hazard**, **Missing** and **Medical** bulletins pop up on everyone's screen, unless the
  sender is unverified (then the bulletin carries an **unverified sender** tag and does not pop up).
  Only the verified author can withdraw a bulletin.
- Your own bulletins have a delete button. A deletion notice is sent, but copies may survive on
  devices that were out of range.
- **Bulletins are public and cleartext.** Do not post anything that puts people at risk (for
  example where supplies are stored, or who is alone).

## Position without phone networks

GPS works with **no internet and no phone network**; it only needs a view of the sky.

- **Settings → Position → Position sharing:** **Off — never share**, **Manual - only when I share
  it** (default), or **Automatic — include in announcements**.
- **Update from GPS** takes a fix now; **Keep updating** follows you.
- **Enter position manually** (latitude and longitude in decimal degrees, e.g. `39.8283` and
  `-98.5795`) when there is no GPS. Use a paper map and **Tools → Navigation** to convert.
- To send your position to one chat: open the chat, tap the options button (⋯) → **Share my
  position**.
- **People → Radar** shows everyone with a known position around you, north up, with a table of
  distance and bearing. Green = heard directly, blue = via relays, red = SOS.

Navigation tools (**Tools → Navigation**), all offline:

| Tool | Use |
|---|---|
| **Maidenhead grid locator** | Short grid squares (like `FN31pr`) to give a location by voice |
| **UTM / MGRS converter** | Grid references used on many maps and by rescue services |
| **Decimal <-> degrees/minutes/seconds** | Convert between coordinate formats |
| **Distance and bearing** | From you to a reported position |
| **Destination point** | Where you end up after a distance on a bearing |
| **Pace count**, **Map scale** | Walking distances and map measurements |
| **Sunrise / sunset / sun position**, **Moon phase** | Planning movement and light |

Read coordinates over voice slowly, digit by digit, using **Tools → Signalling → NATO phonetic
spelling** for letters.

## Signalling with light and sound

**Tools → Morse send & read** opens the **Morse** screen; on its **Send** tab:

1. **Text** (default `SOS`), **Speed (words per minute)** (default 10).
2. Tick **Repeat until stopped** for a beacon.
3. Tap **Flashlight** (the camera light, where available; otherwise **Screen flash**), **Screen
   only**, or **Tone** (an audible 700 Hz beep).
4. **Stop** ends it.

SOS is · · · — — — · · · sent as one sign. Signal from high ground at night: a phone screen is
visible for about 1 km in darkness, a flashlight much further. **Tools → Signalling → Ground-to-air
emergency signals** shows the symbols to lay out on the ground for aircraft; **Morse code** converts
any text. To **read** someone else's flashes or tones, use the **Read by camera** and **Read by
sound** tabs: [Morse by light and sound](morse-reading.md).

## Emergency frequencies (from the built-in band plans)

Check **Tools → Band plans** for your region. Examples:

| Where | Frequency | Use |
|---|---|---|
| Worldwide (aviation) | 121.5 MHz AM | Aeronautical emergency, monitored by aircraft and air traffic control |
| Worldwide (marine) | 156.800 MHz, Marine channel 16 | Distress, safety and calling |
| CB 27 MHz | Channel 9 (27.065 MHz) | Emergency |
| US FRS/GMRS | Channel 20 (462.675 MHz) | Common emergency / traveller channel |
| Australia/NZ UHF CB | Channels 5 / 35 | Emergency only |
| HF amateur | 3.760, 7.110, 14.300 MHz ... | Emergency centres of activity |

In a genuine emergency where life is in immediate danger, most jurisdictions permit any means of
radio communication to summon help. Return to normal rules as soon as the emergency is over.

## Offline tools worth knowing

| Where | Tool |
|---|---|
| **Tools → Survival** | **Water disinfection dosing**, **Fallout decay (7-10 rule)**, **Wind chill**, **Heat index**, **Food and water ration planner**, **Unit converter** |
| **Tools → Power** | **Battery runtime**, **Solar panel and battery sizing** |
| **Tools → Crypto & codes** | **One-time pad sheet**, **One-time pad encrypt**, **One-time pad decrypt** (hand-made secret messages for voice or paper, on services where encryption is legal; never on amateur bands) |
| **Guides** | The whole survival library; search it with the search box |
| **Notes & lists** | Personal notes; checklists from templates (**Bug-out bag (72 h)**, **Communications plan**, **First-aid kit**, **Shelter in place**, **Vehicle kit**); **Supply inventory** with quantities and expiry dates |

Notes and lists are stored on your device only. They are encrypted when a storage passphrase is
set ([Security](security.md)).
