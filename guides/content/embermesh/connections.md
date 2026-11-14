---
id: embermesh/connections
title: "EmberMesh 13: Connections - Nearby Devices and the Internet Relay"
category: embermesh
tags: [embermesh, nearby devices, wi-fi, local network, internet relay, relay, mqtt, privacy, contacts, reachability, hub relay]
summary: "How EmberMesh connects by itself: Nearby devices on the same Wi-Fi (on by default) and the optional Internet relay that reaches contacts at any distance over the internet, what each needs, exactly what other people and relay operators can see, how to switch them off, your own hub as a private relay, and why neither replaces radio in a disaster."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 13
---

# Connections: Nearby Devices and the Internet Relay

Two people add each other as contacts and want their messages to flow without fiddling with
links. EmberMesh now does two things by itself, each with its own switch in **Settings →
Connections**:

| | **Nearby devices** | **Internet relay** |
|---|---|---|
| Reaches | EmberMesh devices on the **same Wi-Fi or local network** (a router, a phone hotspot) | Your **contacts at any distance** over the internet (mobile data, Wi-Fi, satellite internet) |
| Needs the internet | No | Yes |
| Default | **On** after first-run setup | **Off** until you turn it on |
| Third parties involved | None | A relay server run by others (or your own hub) |

Both are conveniences for when Wi-Fi or the internet exist. **When they are gone, the radio,
Bluetooth, hub and sneakernet chapters still apply.** Never plan a disaster on the internet relay.

> **What is proven:** the listed public relays were checked to accept a connection and carry a test
> message, and to refuse or allow "watch everything" subscriptions as stated below. Neither feature
> has been proven yet between many real phones in the field; treat both as new.

## Where the switches are

- **First run:** the **Your identity** step has a **Connect to people** card: **Nearby devices**
  (marked **recommended**, on) and **Reach contacts over the internet** (off). "Start using
  EmberMesh" applies your choice.
- **Installs from before this feature:** a one-time sheet **New: connect to people** with the same two
  choices and **Continue**.
- **Later:** **Settings → Connections**, with **Nearby devices** and **Internet relay** (and the list
  of **Relays**).
- When adding a contact (under **Far away?**) and on a contact's page (in the **Reaching <name>**
  card): **Reach this contact over the internet**, shown while the internet relay is off. One tap turns the relay on; then: "Internet relay
  is on. You can turn it off in Settings → Connections."

Every switch shows the app's own explanation (the notice) before anything starts. The sentences
below say what those notices mean.

## Nearby devices (same Wi-Fi)

**What it does:** phones, tablets and computers running EmberMesh on the same Wi-Fi or local
network find each other and exchange messages, with **no internet** and no visit to Links &
radios. Each one shows on the Links screen as an automatic link ("Nearby device", later "Nearby:
<name>"), marked **Automatic · nearby device**; to change it use **Change in Connections**.

**What it needs:**

- All devices on **the same network**: one Wi-Fi, or one phone's hotspot.
- The **local-network permission** on phones: the iPhone asks to let EmberMesh find devices on your
  local network when you turn it on; Android 17 and newer may ask the same. If you refuse, nothing
  is found.
- A network that lets devices talk to each other. **Some public and guest Wi-Fi isolates devices**
  from each other; then use one phone's hotspot or a hub instead.
- The app open (iPhone), or open or with its background service running (Android). A browser page
  cannot search the network: browsers join a hub or pair with a code (Links & radios).

**What others on that Wi-Fi can see** (the notice: "Devices on the same network can see your name and
your EmberMesh identity"):

- Anyone on the network can see that a device at your address runs EmberMesh, and its port. The
  advertisement carries no name, model or node id, only a random token that changes at every start
  and network change. iPhones and Android phones also announce the phone's own network name through
  the system; on some phones that is derived from the device name, which the app cannot change.
- Anyone who connects (and every nearby EmberMesh device does) sees what every mesh neighbour sees:
  your **name**, **callsign** (if set), **EmberMesh identity** (node id and public keys) and the
  **Public** channel. These are stable: people in different places can recognise the same device.
- Direct messages and private channels stay end-to-end encrypted, as on any link.
- On public Wi-Fi strangers' devices become mesh neighbours, like strangers on a shared radio
  channel, under the usual limits (rate limits, at most 16 nearby links, unknown senders marked
  **⚠ unverified sender**).

**Bluetooth is not automatic.** Nearby devices uses Wi-Fi/local network only. Bluetooth and radio
links are added by hand under **Links & radios** ([Phone to phone](phone-to-phone.md)).

**Turn it off:** Settings → Connections → **Nearby devices**. Automatic nearby links close at once.

## Internet relay (any distance)

**What it does:** when a contact cannot be reached directly (nearby, by radio, through a hub),
messages to that **contact**, and to **random-key channels** you switch on for it, travel through a
relay server on the internet, at any distance, over mobile data, Wi-Fi or satellite internet. The
relay is a fallback: while the contact is reachable directly, nothing goes through the relay.

**What it needs:**

- **Both** of you have the internet relay on, and you have **at least one relay in common**.
- **Both apps open.** Messages are delivered while both apps are running. A phone whose app is
  closed receives nothing until it is opened; there are no push notifications. Until then messages
  wait on the sender's device.
- The internet, and a device clock that is roughly right.
- A little mobile data: while the app is open and idle, about 4.5 KB an hour per contact and relay
  (20 contacts on one relay: about 90 KB an hour, about 2 MB a day of open app), plus the messages
  themselves. Nothing while the app is closed.
- Every app can use it: phones, browsers and the computer program (emberd). Each checks the
  relay's certificate and refuses a connection that is not encrypted.

**What a relay operator can and cannot see** (the app's notice says the same):

| A relay operator **cannot** | A relay operator **can** see |
|---|---|
| read your direct messages or random-key channel messages | your **IP address**, and so roughly **where you are** |
| see names, node ids, keys or which contact a message is for | **when you are online** |
| forge or alter messages (they are sealed end to end) | roughly **how many contacts** you have |
| | **which other users you exchange messages with** (devices that share a topic) |
| | and follow all this **across days and networks** |

Also: anyone watching a public relay may see the size and timing of messages (not your address,
not the content); your **contacts can see when you come online**; a relay can delay, drop or block
messages at any time.

**Passphrase channels are not sent over relays.** Anyone on the internet could try to guess a weak
passphrase. Only channels with a **random key** (or a passphrase rated strong when created or joined)
can use the relay, and only after you switch **Also over the internet relay** on in that channel's
menu.

**The relays:**

- The default relay is a **free public service run by another company** (HiveMQ), with no
  guarantee: it may be slow, change its rules or disappear, and its terms may not cover this use. With one relay, reaching your contacts
  over the internet depends on that one service.
- Two other public services are listed but **off**; their entry says why (they let anyone watch
  topic names, sizes and timing).
- **Add your own relay or hub:** Settings → Connections → **Relays** → **Add your own relay or hub**:
  **Relay address** (starts with `wss://`, for example `wss://relay.example.org/mqtt`), a user name
  and password only if the relay needs them (the password is never shown again), then **Add relay**.
  An unencrypted `ws://` address is allowed only with **Allow an unencrypted connection (ws://)**,
  and only for a hub on your own network. At most 8 relays.

**Turn it off:** Settings → Connections → **Internet relay**. Every relay connection closes at once.

### Your own hub as a private relay

The robust and private option is a relay you run yourself, so no third party sees your traffic:
a computer hub started with `emberd --relay-broker` runs a small relay on its own port (8438) at
the path `/mqtt`. Phones and browsers connect to relays only with `wss://`, so put it behind a
TLS reverse proxy (for example Caddy or nginx) that forwards only `/mqtt`, make only the proxy
reachable from the internet, never the hub port 8437, and add `wss://<your name>/mqtt` as a relay on
every device. The exact recipe is in `apps/desktop/README.md`, "A private internet relay". Your relay
still sees what any relay sees (addresses, times, sizes, who shares a topic), but only you run it.

## "Reaching <name>": can a contact be reached right now?

A contact's page has a **Reaching** card (for example **Reaching Sam**):

| Line | Meaning |
|---|---|
| Nearby now (same Wi-Fi). | Heard directly on an automatic nearby link in the last 15 minutes |
| Connected now over ... | Heard directly on another network link (hub, TCP, WebRTC ...) |
| In radio range now (...) | Heard directly on a radio link |
| Reachable through the internet relay. | Only the relay reaches them now |
| Not reachable right now. Last heard ... ago. Messages wait on this device and are sent when a connection to them exists. | No direct path now. **Nothing is lost**; it will go when a path exists |

These lines describe **reachability, never delivery**. A message is delivered only when its status
says **Delivered**.

## Where this fits

- Same building, Wi-Fi up: Nearby devices does it by itself.
- Far away, internet up: the internet relay, if you accept what relays can see.
- No internet, no Wi-Fi: the mesh works exactly as before, over Bluetooth, radios, hubs and USB
  sticks. Internet traffic received through a relay is never passed on to amateur-radio links.

See also [Security](security.md) and [Troubleshooting](troubleshooting.md).
