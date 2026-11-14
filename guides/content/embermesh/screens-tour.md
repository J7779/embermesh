---
id: embermesh/screens-tour
title: "EmberMesh 7: A Tour of Every Screen"
category: embermesh
tags: [embermesh, screens, menu, navigation, chats, people, sos, guides, links, tools, notes, settings, interface, buttons]
summary: "Every screen and control of the EmberMesh app explained once, in the order you meet them: first run, lock screen, status strip, Chats, People, SOS, Guides, More, Bulletins, Links & radios, Tools, Notes & lists and Settings."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 7
---

# A Tour of Every Screen

The same interface runs in the Android and iPhone apps, in browsers, and on a computer hub's
operator page. Labels in **bold** are exactly what the screen shows.

## Getting around

- **Phone:** a bottom bar with **Chats**, **People**, **SOS**, **Guides**, **Help** and **More**.
  The **Help** tab opens the screen called **Instructions** (the side bar on wide screens calls it
  **Instructions** too); this manual writes **Instructions** for both. **More**
  lists **Bulletins**, **Links & radios**, **Tools**, **Notes & lists**, **Settings** and **Mesh
  statistics**.
- **Wide screen:** a side bar with **Chats**, **People**, **SOS**, **Bulletins**, **Links**,
  **Guides**, **Instructions**, **Tools**, **Notes & lists**, **Settings**.
- **Status strip** at the top: `links up / links total` (tap: Links; it reads "links — none up" in
  red when no link is up), how many nodes were **heard**
  in the last 15 minutes (tap: People → Nearby), and messages **queued**. Red banners appear here
  for your own active SOS ("SOS ACTIVE — beacon repeating ...") and for SOS alerts received; tap them
  to open the SOS screen.
- Small messages (toasts) pop up at the bottom for new messages and results.

## First run: Welcome to EmberMesh

Shown until a name is set. **Your name**, **Amateur radio callsign** (only if licensed), **Region**,
**Continue**; then **Your identity** with your node ID and fingerprint words, **Start using
EmberMesh**.

## Lock screen: EmberMesh is locked

Shown when a storage passphrase is set and the app is locked. Type the passphrase, **Unlock**.

## Chats

- **New**: start a **Direct message** with a contact, or **Create channel** / **Join channel**.
- **Search messages**: searches all conversations as you type.
- The list: **Public** first, then conversations, newest first. A lock icon marks encrypted ones, a
  **cleartext** tag the others; unread counts as badges.

**Inside a conversation:**

- The header shows the name and **Verified** / **Not verified**, the path (direct or hops) and when
  last seen (direct messages), or "Encrypted channel" / "Cleartext, signed". The **⋯** button
  (Conversation options) offers **Contact details**, **Ping**, **Invite people** (channels),
  **Mute**, **Share my position**, **Export as bundle (sneakernet)**, **Clear conversation**,
  **Leave channel**.
- Each message shows the sender, its priority if not normal, a lock (encrypted) or globe
  (cleartext), **⚠ unverified sender** when the signature could not be checked, the hop count, the
  link it came in on, the time, and on your own messages a status icon (Queued, Sent, Relayed,
  Delivered, Read, Failed). Tap a message for **Copy**, **Details**, **Delete**, and **Retry** if it
  failed.
- **Compose:** the message box, the paperclip (**Send a small file**), the send button and the
  priority selector (**Low**, **Normal**, **High**, **Emergency**). Under the box: the size, the
  estimated bytes on air, the estimated airtime on the slowest link, and warnings such as "not via
  ... (no encryption allowed)".

## People (People & mesh)

Four tabs:

- **Contacts:** **Add contact** (**Scan contact QR**, paste a **Contact card**, or **Add by node
  ID**) and **My card** (your QR code). Each contact shows its status, verified or not, last heard,
  path and distance.
- **Nearby:** every node heard, with path, signal (RSSI/SNR) per link and distance; **Announce
  myself**; per node a ping button and **Add to contacts** (+) or **Message**.
- **Radar:** nodes with a known position placed around you, north up (green = direct, blue = via
  relays, red = SOS), with a distance/bearing table. Needs your own position (**Use GPS** or
  **Enter position manually**).
- **Stats:** stored messages and data, relayed for others, frames sent/received, bad frames,
  duplicates dropped, expired, uptime, and per link airtime, duty cycle used and queue.

**A contact's page:** **Message**, **Ping** (shows "Pong: ... round trip, N hops"), **Verify
identity** (their fingerprint words; **Words match — verify** or **Verify by scanning their
card**), the **Mesh** details (node ID, last heard, path, via, signal, position, distance),
**Favourite**, **Rename**, **Block**, **Remove**.

## SOS (Emergency)

- The big **SOS** button (press and hold 3 seconds), **Message**, **Include my position**. While
  active: since when, how many times sent, **Heard by:**, **I am safe — stop SOS**.
- **Safety check-in:** **I'm OK**, **Need help**, **Injured**, **Moving**, **Sheltering**, **Note
  (optional)**,
  **Include my position**.
- **SOS alerts received:** verified alerts first, then those from unverified senders (grouped);
  **Acknowledge — I hear you** and **Message**.
- **Roll call (latest check-in per person)** with counts per status.

## Guides

- **Search all guides**, results as you type, with the **Smart search** switch; results found by
  meaning are marked **≈ by meaning**. **Ask the library** answers a question with passages quoted
  from the guides ([Finding guides](library-search.md)).
- **How to use EmberMesh**: this operator manual.
- **Bookmarks**, **Recently read**, **Categories** (the survival library by subject).
- **Reading a guide:** **Contents**, **Smaller text** / **Larger text**, **Bookmark**, **Related guides**, **Previous** /
  **Next chapter** for books, and at the end the source, licence and attribution. Links to other
  guides open in the app; links to the internet are only shown, with **Copy address**.

## Instructions

The operator manual as a book: **Search the instructions**, **Print**, quick buttons (**Connect a
radio**, **What you need**, **Add a connection**, **Read Morse code**), a **Your device** section for
this platform's guide, and chapters grouped as **Start here**, **Connect devices**, **Radios**,
**Emergencies and the field**, **Settings, accessibility and security**, **When something does not
work** and **About**.

- **What you need:** the hardware list, filtered by **I have** (Any device / I have a phone / I have
  a computer) and **I want to connect**; each entry shows **Required** and **Optional** parts and what
  is not possible on your device.
- **Connect a radio:** the eight-step radio wizard (also under Links & radios → Add connection); see
  [Baofeng UV-5R](baofeng-uv5r.md).

## Bulletins

The community notice board: category chips (**All**, **Water**, **Food**, **Medical**, **Shelter**,
**Hazard**, **Trade**, **Missing**, **Transport**, **Info**), **Post** (category, **Title**,
**Details**, **Keep for**, **Attach my position**), and on your own bulletins a delete button.
Bulletins from unverified senders carry an **unverified sender** tag.

## Links & radios

- **Add connection** opens the wizard. At the top, **Connect a radio (guided)** starts the radio
  wizard. Otherwise pick the connection type ("How do you want to reach other
  devices?"), **Scan for devices** where offered, fill the **Connection** card and the **Protocol**
  card (**What is on the other end?** and its settings, with **Advanced settings** folded away),
  **Name**, **Reconnect automatically when the app starts**, **Connect**. A "Step-by-step guide in
  the manual" link opens the right chapter. Amateur protocols show the no-encryption notice and,
  without a callsign, **Your callsign is needed** / **Save callsign**. Sound-card and SDR links end
  with **Set the audio level** (a live meter) and **Done**.
- **Link cards:** name and type, state (**Connected**, **Connecting…**, **Down**, **Error**) with the
  error in red, tags (speed, MTU or **text only**, half duplex, broadcast, reliable, **No encryption
  (amateur)** or **encryption ok**, **station ID**, duty limit, **temporary**), the airtime bar, the
  **RX level** meter (sound and SDR links), **Radio setup: ...** (the preset in use), a red notice
  "This link cannot transmit until you set a callsign" with a **Set callsign** button when an
  amateur link has no callsign, **Statistics**, **Settings in use** (the settings the link actually
  runs with; the ones you changed are tagged **changed**), and **Reconnect**, **Settings**,
  **Plain-text message**, **Remove**, **Help**.
- **Share this device:** the **Hub** card (**Start hub** / **Stop hub**, addresses with QR and copy
  buttons; or "not on this device") and **Pair directly (WebRTC)** (**Create invite**, **Answer an
  invite**) in browsers.
- **Net plan (where to tune):** **My announced plan** with **Edit** (**Add frequency**, **Save &
  announce**), and plans **Heard from** others.

## Tools

- **Band plans** (choose **Region**, filter), **Morse send & read** (the **Morse** screen: **Send** (**Send a message by light**: type, choose **Slow · 6** / **Normal · 12** /
  **Fast · 15**, **Flash with flashlight** or **Flash with screen**, **Play as sound**, **Stop**),
  **Read by camera**, **Read by sound**; see
  [Morse by light and sound](morse-reading.md)), **Station
  profiles** (your radios' power, feedline, antenna and height, used to fill calculators).
- Calculators by group: **Antennas**, **Radio & propagation**, **Power**, **Navigation**,
  **Signalling**, **Survival**, **Crypto & codes**. Each has its inputs, **Calculate**, a **Result**
  table, a diagram where useful, and notes. Inputs are remembered.

## Notes & lists

**Personal notes** (saved as you type), **Checklists** (with templates: Bug-out bag (72 h),
Communications plan, First-aid kit, Shelter in place, Vehicle kit), **Supply inventory** (item,
quantity, unit, category, location, expiry date; filter; + / − buttons). A notice says whether they
are protected by a storage passphrase.

## Settings

At the top, **Accessibility** (Simple mode, text size, contrast, motion; see [Simple mode and
accessibility](accessibility.md)) and an **Instructions** button. Then, in order: **Profile** (name, callsign, status), **Region & display** (region, units, theme, low-power
mode), **Mesh & relaying**, **Position**, **Clock**, **Identity** (node ID, fingerprint words, **Show
my card**, **Back up identity**, **Restore identity**), **Security** (passphrase, **Lock now**,
**Duress passphrase**, **Panic wipe**), **Sneakernet bundles** (**Export bundle**, **Import
bundle**), **This device** (platform actions, when the app offers any) and **About** (versions,
platform, connections and features, licences). Every setting is described in the
[Settings reference](settings-reference.md).
