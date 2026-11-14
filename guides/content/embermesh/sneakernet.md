---
id: embermesh/sneakernet
title: "EmberMesh 25: Sneakernet - Carrying Messages by USB Stick, File and QR Code"
category: embermesh
tags: [embermesh, sneakernet, bundle, usb stick, qr code, courier, file, offline, store and forward]
summary: "Moving messages between groups that have no link at all: exporting bundles to a file or a series of QR codes, carrying them by hand, importing them, and a simple courier routine."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 25
---

# Sneakernet: Carrying Messages by Hand

When two groups have no radio or network between them, **people can carry the mesh**. EmberMesh
packs stored messages into a **bundle**: a file, or a series of QR codes. Importing a bundle on
another device is exactly like receiving those messages over a link: they are checked, duplicates
are dropped, messages for that person are delivered, and the rest is passed on to everyone that
device meets.

A bundle contains only **signed, already-encrypted mesh packets**. Direct messages and private
channels stay unreadable to the courier and to anyone who finds the stick.

## Fastest path: one file on a USB stick

On the device at camp A:

1. **Settings → Sneakernet bundles → Export bundle.**
2. **What to include: Everything I carry.** **Size limit: No limit.**
3. Tap **Create bundle**. It shows how many packets and how big it is.
4. Tap **Save file**. Save the `.ember` file to the USB stick or SD card (Android/iPhone: the
   share or save dialog; computer: a download).

Carry the stick to camp B. On a device there:

5. **Settings → Sneakernet bundles → Import bundle → Choose file**, pick the `.ember` file.
6. You see "Imported N new packets (M already known, K for you)", plus "R rejected as invalid"
   if part of the file was damaged or forged.

Messages for people at camp B are delivered as soon as their devices connect to the importing
device. Now repeat the export at camp B and carry it back.

## QR codes: no stick, no cable

Two phones face to face can move a bundle through their screens and cameras.

On the sending device:

1. **Export bundle**, choose a **Size limit**: **2 KB (one QR code)** for a single code, or a larger
   size for several codes.
2. **Create bundle → Show as QR codes.** The screen shows "Code 1 of N - scan each one on the
   other device". Use **Next** and **Previous**.

On the receiving device:

3. **Import bundle → Scan QR codes.** Scan code 1. It says "Got 1 of N codes - scan the next one".
4. Tap **Scan QR codes** again for each code, in any order. Repeats do no harm. When the last one
   arrives the bundle is imported.

Each code carries about 700 characters. Camera scanning needs a device with a working QR scanner
(the Android and iPhone apps; Chrome on Android). Without a scanner you can paste the text of a
code (starting `EMB1 1/N ...`) into the box and tap **Import pasted data**, one code at a time.

## What to include

| Choice | Use when |
|---|---|
| **Everything I carry** | A courier is carrying mail for a whole camp |
| **Only messages for one person** (then choose the contact) | You know who the messages are for and space is short |
| One conversation: open it, tap the options button (⋯) → **Export as bundle (sneakernet)** | Passing on a single chat or channel |

**Size limit** options: No limit, 2 KB (one QR code), 16 KB, 256 KB, 4 MB. Check the packet
count shown after **Create bundle**; if the limit left things out, export again with a larger
limit or split by person.

## A courier routine

```
   CAMP A hub                  courier                     CAMP B hub
   [export all] --> stick --> walks 1 day --> [import] --> delivered + flooded on at B
   [import]     <-- stick <-- walks back  <-- [export all]
```

1. Agree a fixed time to export (for example just before the courier leaves).
2. The courier carries the stick (and a spare) in a waterproof bag.
3. At each stop on the way, any EmberMesh device can import and re-export: the bundle grows with
   that group's mail.
4. On arrival, import at the hub (or the device most people connect to).
5. Before leaving, export everything again for the return trip.

The courier's own phone is also a carrier: if it runs EmberMesh with relaying on, it picks up
messages from every node it meets and delivers them at the other end automatically. The bundle is
a backup and works with no phone at all.

## On a computer hub (emberd)

There is no file dialog on emberd:

- **Saved** bundles go to the data folder's `saved/` directory.
- To **import**, copy the `.ember` file into the data folder's `inbox/` directory, then choose
  **Import bundle → Choose file** on the operator page; the newest matching file in `inbox/` is
  used.

## Limits and cautions

- Messages expire. A bundle carried longer than the senders' **Carry undelivered messages for**
  setting (and never more than 30 days) arrives with expired messages, which are dropped.
- The bundle reveals **who wrote to whom and when** (node IDs and times), though not the content
  of private messages. If the courier may be searched, consider that.
- Bundle contents are checked like radio traffic: forged or damaged packets are rejected, and
  messages from senders the importing device cannot verify are tagged **⚠ unverified sender**
  there too.
- Public channel messages, bulletins, check-ins and SOS beacons in the bundle are readable by
  anyone (they always are).
- Put the app itself on the same stick ([Getting the app onto other devices](spreading-the-app.md))
  so a group without EmberMesh can start using it.
