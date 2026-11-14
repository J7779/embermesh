---
id: embermesh/how-it-works
title: "EmberMesh 2: How EmberMesh Works, in Plain Language"
category: embermesh
tags: [embermesh, mesh, store and forward, encryption, identity, fingerprint, public channel, message status, priority, sos]
summary: "What a mesh is, how messages are carried and relayed, identities and fingerprint verification, what is and is not encrypted, the Public channel, message status, priorities and SOS."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 2
---

# How EmberMesh Works, in Plain Language

## The short version

- Every device running EmberMesh is a **node**. There is no central server.
- Nodes pass messages to whatever nodes they can reach: by Bluetooth, Wi-Fi, a radio, a cable,
  a USB stick or a QR code. Each of these is called a **link**.
- A node that has several links **relays between them automatically**. A laptop with a radio and
  a Wi-Fi hotspot is a gateway without any configuration.
- Nodes **keep undelivered messages for days** and hand them over to every new node they meet.
  A person walking from one camp to another is a working link.
- **Direct messages are end-to-end encrypted.** Relays carry them but cannot read them.
- The **Public** channel is open: anyone can read it, but nobody can forge your name on it.

## The mesh

```
  [Ana's phone] ~~Bluetooth~~ [Ben's phone] ~~Wi-Fi~~ [Hub laptop] ==radio== [Ridge relay] ==radio== [Far camp]
```

When Ana sends a message to someone at Far camp, Ben's phone relays it to the hub laptop, the hub
sends it out on its radio, the ridge relay repeats it, and Far camp receives it. Nobody set up a
route. Each node simply re-sends new messages on its other links, a limited number of times.

- **Hops.** Every relay is one hop. Each message carries a hop limit (default set by
  **Settings → Mesh & relaying → Default hop limit**, an advanced setting) so it cannot circle
  forever.
- **Duplicates** are recognised and dropped, so the same message arriving by two paths is shown
  once.
- **Relaying is on by default** (**Settings → Mesh & relaying → Relay messages for others**).
  Leave it on: it is what makes the mesh work. Turn it off only to save battery in a crisis.
- **Blocking** a contact hides their messages from you, but your device still relays their
  traffic for others.

## Store-carry-forward

Radio links come and go. EmberMesh assumes they will.

1. When a message cannot be delivered yet, your device stores it.
2. Whenever your device meets another node, the two quickly compare what they hold (a short
   summary, not the messages themselves) and exchange only what the other is missing.
3. When the recipient finally gets the message, it sends back an acknowledgement. Every node
   that sees the acknowledgement deletes its stored copy.

How long messages are carried is set by **Settings → Mesh & relaying → Carry undelivered
messages for** (days). Some kinds expire sooner by design: node announcements after 7 days,
positions and SOS after 1 day, check-ins and net plans after 2 days.

This is why EmberMesh works over very poor links, and why a messenger walking between groups,
or a USB stick carried by hand (see [Sneakernet](sneakernet.md)), can join two networks.

## Your identity

On first start your device creates a cryptographic identity:

- a **node ID**: 16 characters such as `0123456789abcdef`;
- eight **fingerprint words** derived from your public keys;
- a **contact card**: an `ember:` text that can be shown as a QR code.

Your name is just a label; anyone can call themselves "Sam". The keys are what matter. Every
message you send is signed with your key, so nobody can impersonate you **once the other person
has verified your key**.

### Verifying someone (do this with everyone you rely on)

Choose one method:

- **By voice (radio, phone, in person):** open their contact (**People → Contacts →** their
  name). Under **Verify identity** the app shows the eight words it has for them. Ask them to read
  their own words from **Settings → Identity → Fingerprint words**. If all eight match, tap
  **Words match — verify**.
- **By QR code (face to face):** they tap **People → My card** (or **Settings → Identity → Show
  my card**). You tap **Verify by scanning their card** on their contact page. The app says
  "Card matches - verified" or "That card belongs to a different node".

A verified contact shows **Verified** with a shield; others show **Not verified**. If the words
do **not** match, someone may be pretending to be that person: do not trust that contact.

Contacts added by node ID alone, or from the **Nearby** list, learn their keys from the mesh.
That is convenient but unverified until you check the words. If a node's keys have not been heard
yet, its contact page says there are no words to compare; they appear once its announcement
arrives, or add it from its QR card. The QR card is the strongest check.

### Strangers and unverified senders

Anyone can create a new identity in a second and call it anything. EmberMesh therefore treats
traffic from keys it cannot check differently:

- Messages, bulletins, check-ins and SOS alerts whose sender could not be verified are tagged
  **⚠ unverified sender** (or **unverified sender**). If an authenticated copy arrives later, the
  tag disappears.
- **SOS alerts from unverified senders are still shown**, but grouped below the verified ones
  ("N alerts from unverified senders"), limited in number, and they **never raise a
  notification**. A flood of forged alerts cannot bury a real one from someone you know.
- Traffic from completely unknown senders is rate-limited and relayed only one or two hops, so a
  stranger cannot flood the whole mesh. An unverified sender can never cancel a verified SOS or
  withdraw someone else's bulletin.
- Your conversations and contacts are never deleted to make room for strangers.

"Verified" in the app means two different things, both good: the **signature checked out** (the
message really comes from that key), and, on the contact page, **you confirmed the key belongs
to that person** (words or QR). Only the second protects you from an impostor. That is why you
should **add and verify the people you depend on before an emergency**.

### If you lose your phone

Without a backup your identity is gone and your contacts must re-verify a new one. Make a backup
now: **Settings → Identity → Back up identity** (details in [Security](security.md)).

## What is encrypted and what is not

| Kind of traffic | Who can read it | Signed (cannot be forged) |
|---|---|---|
| Direct message to one person (text, file, shared position) | Only you and that person | Yes |
| Private channel (created with a passphrase) | Everyone who knows the passphrase or has the invite | Yes |
| Open channel (created without a passphrase) | Anyone | Yes |
| **Public** channel | Anyone | Yes |
| SOS beacons, check-ins, bulletins, net plans, node announcements (name, callsign, status) | Anyone | Yes |
| Read receipts and pings | Anyone can see that they happened; they are authenticated | Yes |

Things to know:

- **Relays never need your keys.** They store and forward encrypted messages as sealed boxes.
- **Who talks to whom is visible.** Even for encrypted messages, the sender's and recipient's node
  IDs, the time, the size and the kind of packet travel in the open. Content is hidden; the
  fact of communication is not.
- **No forward secrecy.** Encryption uses your long-term keys. Someone who records traffic today
  and later steals your device and passphrase could decrypt what they recorded. See
  [Security](security.md).
- **Amateur-radio links carry no encryption.** On any link marked **No encryption (amateur)**,
  EmberMesh sends only signed cleartext with your callsign: the Public channel, open channels,
  SOS, check-ins and bulletins. Encrypted direct messages and private channels are **never** put
  on such links; they wait for another path. The Links screen shows a notice when such a link
  exists.

## Channels

- **Public**: everyone nearby. Cleartext, signed. It is the only chat allowed on amateur links.
- **Create channel** (in **Chats → New**): give it a name, then choose how it is protected:
  - **Random key (recommended)**: encrypted with a key nobody can guess. Share it only with
    **Invite people** (in the channel's options menu ⋯), which shows a QR code and an `ember:`
    text.
  - **Passphrase**: encrypted with a passphrase people type in. **Only as strong as the
    passphrase.** Anyone in radio range can record the channel and try guesses offline, so short
    words, names or dates are cracked quickly. Use 4 or more random words, or tap **Generate
    strong passphrase**. The app rates it (Weak / Fair / Strong) and warns before creating a
    channel with a weak one.
  - **Open (no encryption)**: anyone can read it; it may also travel over amateur-radio links.
- **Join channel**: scan or paste the invite, or open **Or join by name and passphrase** and type
  both exactly.
- **Leave channel**: you need a new invite to rejoin an encrypted channel.

## Message status

Your own messages show a small icon. Tap the message and choose **Details** to see it in words.

| Status | Meaning |
|---|---|
| Queued | Waiting for a link. Normal when no path exists; it will go when one appears. |
| Sent | Handed to at least one link. |
| Relayed | Another node was heard passing it on. |
| Delivered | The recipient's device confirmed receipt (direct messages only). |
| Read | The recipient opened it. |
| Failed | Gave up. Tap the message and choose **Retry**. |

Direct messages are re-sent automatically a few times at growing intervals (20 s, 40 s, 80 s
and so on) and stay stored, to be handed over whenever a path appears, until acknowledged or
expired. Channel messages have
no delivery confirmation; they stay at Sent or Relayed.

Received messages show how many hops they travelled (**direct** or **2 hops**), which link they
came in on, and a lock (encrypted) or globe (cleartext) icon. **⚠ unverified sender** means the
signature could not be checked: treat the sender's name with suspicion.

## Priorities

The selector under the message box offers **Low**, **Normal**, **High** and **Emergency**.

- Every relay sends higher-priority traffic first. The internal order is: SOS and emergency
  first, then acknowledgements, then direct messages, then channel messages and check-ins, then
  announcements, positions, bulletins and net plans, then low-priority traffic, and last the
  background exchange of stored messages.
- **Emergency** asks for confirmation ("Send as emergency?") because it jumps every queue on every
  relay. Use it only when a life is at risk.
- When a link's queue is full, new high-priority traffic pushes out queued low-priority traffic.

## SOS

The **SOS** tab sends a **repeating distress beacon** with your position, if known, to every
device in range. Every node relays it first. Others see an SOS banner and can tap **Acknowledge -
I hear you**, which tells you someone heard. See [Emergency use](emergency-use.md).

## Clocks

Message order and expiry depend on a roughly correct clock. **Settings → Clock** shows the node
time and where it came from (system, GPS, mesh or manual).

- **A device with a sane clock keeps its own time.** The mesh never moves it, so nobody can push
  a wrong time onto your device.
- Only a device that **has lost its clock** (for example one that shows a date before 2024 after
  its battery died) takes the time from the mesh, and only when several independent nodes, or
  one of your contacts, agree.
- If a device's clock is wrong but plausible, the mesh will **not** fix it: set it by hand with
  **Settings → Clock → Set from this device** after correcting the phone's own clock, or from GPS.
- Clocks that disagree by up to a day are tolerated: fresh messages are still accepted, and a wrong
  clock can never delete stored messages.

## The survival library

Every app carries an offline library of several thousand guides in about twenty categories (water,
medical, food, shelter, radio, power, repair ...), plus this manual. It is searched on the device
with no internet. The **Guides** screen (**Survival library**) shows the exact numbers of guides and
categories in your copy; the collection grows between versions, and a single-file web app may have
been built without it. This manual is under **Instructions** and in the **EmberMesh Manuals**
category.

## Big messages on slow links

The message box shows the size of your text and, when a slow link is up, an estimate such as
"~4 s on Baofeng on 2 m". Long messages are split into fragments on small links. On radio links
keep messages short; 1000 bytes or more is flagged in red.
