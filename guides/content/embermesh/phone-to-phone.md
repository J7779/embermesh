---
id: embermesh/phone-to-phone
title: "EmberMesh 12: Phone to Phone with Nothing Else"
category: embermesh
tags: [embermesh, bluetooth, ble, wifi, hotspot, udp, multipeer, iphone, android, webrtc, qr, tcp]
summary: "Connecting phones directly: Bluetooth, Wi-Fi or a phone hotspot, Apple peer-to-peer, Android with iPhone, WebRTC pairing by QR in browsers, and what to try when none of it works."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 12
---

# Phone to Phone with Nothing Else

No hub, no radio, no internet: just phones (and laptops) near each other.

## Which method?

| Devices | Best method | Typical reach |
|---|---|---|
| Any two phones with the app | Bluetooth | Roughly 10-50 m |
| Android phones (or computers) on one Wi-Fi network or one Android hotspot | Wi-Fi (UDP) | The Wi-Fi network |
| iPhones and iPads | Apple peer-to-peer | Same room / building |
| Android with iPhone | Bluetooth (iPhone in the foreground), or TCP over shared Wi-Fi, or a hub | see below |
| Browsers only (no app installed) | WebRTC pairing with two codes | The local network |
| Nothing connects | Sound (speaker to microphone), or files and QR codes | Arm's length / carried by hand |

All of these start the same way: **More → Links & radios → Add connection**, pick the
connection type, check the settings, tap **Connect**. You can add several links; EmberMesh uses
all of them at once and relays between them.

## Bluetooth (phone to phone)

The connection type is called **Bluetooth LE (phone to phone)** on Android and **Bluetooth
(nearby phones)** on iPhone. Both phones run a Bluetooth service and scan for each other at the
same time; several phones can be connected at once.

1. Switch Bluetooth on. On Android 11 or older, also switch **Location** on.
2. **More → Links & radios → Add connection →** the Bluetooth phone-to-phone entry.
3. Under **Protocol**, "What is on the other end?" should be **Serial / TCP / Bluetooth byte
   stream**. If a **Device / cable** choice is shown, pick **Bluetooth LE (UART adapter or
   phone-to-phone)**.
4. Tap **Connect**. Allow the Bluetooth permission ("Nearby devices" on Android).
5. Do the same on the other phone. Keep both apps open for the first connection.

**You should see:** the link card says **Connected**; after a short while the other phone appears
in **People → Nearby**. Tap **Announce myself** to hurry it.

**If it doesn't work:**

- Both phones need EmberMesh open, or its background service running (Android shows a
  persistent notification while it runs).
- Android: Bluetooth on, and on Android 11 or older Location on too.
- Some old Android phones cannot advertise. They can still connect *to* other phones but not be
  found. Look at the link's **Statistics**; pair such a phone with a newer one.
- An iPhone with EmberMesh in the **background** is usually **not found by Android phones**
  (iOS hides its Bluetooth advertisement). Bring EmberMesh to the screen on the iPhone.
- Move closer; walls and bodies absorb Bluetooth.

> Status: the Bluetooth phone-to-phone code exists on Android and iOS and the byte format is the
> same on both, but it is **compiled and unit-tested only**: no Bluetooth link has yet run between
> real phones (emulators and simulators have no Bluetooth). Test it with **Ping** before relying
> on it, and have Wi-Fi or a hub as a fallback.

## Wi-Fi or hotspot (UDP)

Every EmberMesh device on the same Wi-Fi network finds the others automatically
(multicast `239.83.77.69`, port `47474`). No router internet is needed; a router with no
connection, or one phone's hotspot, is enough.

- Android: **Wi-Fi LAN / hotspot (UDP)**.
- Computer (emberd): this link, "LAN multicast (Wi-Fi / Ethernet)", is on by default.
- iPhone: listed as **Wi-Fi network broadcast (UDP)**, but normally **unavailable** (Apple only
  allows it with a special permission). Use Apple peer-to-peer, Bluetooth, TCP or a hub instead.

Steps:

1. Join all devices to the same Wi-Fi, or switch on the hotspot on one Android phone and join the
   others to it.
2. **Add connection → Wi-Fi LAN / hotspot (UDP)**, leave **Network** at "All networks
   (automatic)", tap **Connect**.
3. Repeat on every device, **including the phone that makes the hotspot**.

**If it doesn't work:**

- Some hotspots and guest networks isolate clients so they cannot see each other. The phone that
  provides the hotspot can still see everyone, because EmberMesh joins the hotspot interface, so
  run EmberMesh there too.
- If multicast is blocked, open **Advanced settings** and fill **Extra peers** with the other
  devices' addresses, separated by commas (`192.168.43.20, 192.168.43.31`; add `:port` if not
  47474).
- Android 17 and newer: allow **local network** access when asked; links blocked by it are retried
  with the prompt the next time you open the app.
- Turn off "Wi-Fi power saving" and check that the phones did not silently switch to mobile data
  or another Wi-Fi.

## iPhone and iPad: Apple peer-to-peer

**Apple peer-to-peer (iPhone, iPad, Mac)** uses Apple's built-in nearby networking (Wi-Fi and
Bluetooth together). It finds other Apple devices running EmberMesh automatically, up to 7 at a
time.

1. **Add connection → Apple peer-to-peer**, leave the defaults, **Connect**.
2. Allow **Local Network** when asked.
3. Repeat on the other iPhone or iPad.

Apple peer-to-peer stops shortly after the app leaves the screen. For long unattended use keep
the app open with the phone on a charger, or use Bluetooth, which keeps working in the
background on iPhone.

## Android and iPhone together

In order of preference:

1. **A hub.** A laptop, a Raspberry Pi, or one of the phones themselves can run a hub
   (**Links & radios → Hub → Start hub**; see [Setting up a hub](hub-setup.md)). Everyone joins
   the same Wi-Fi (or the hub phone's hotspot) and opens the hub address in a browser, or adds a
   **WebSocket (hub)** / **Hub (WebSocket)** link in their app.
2. **Bluetooth**, as above, with EmberMesh **in the foreground on the iPhone** (unverified between
   the two platforms; test with Ping).
3. **TCP over a shared Wi-Fi network** (one device listens, the other connects):
   1. On the iPhone: **Add connection → Local network listener**. Leave the port at `47475`.
      **Connect.**
   2. Find the iPhone's address: iPhone **Settings → Wi-Fi →** the (i) next to the network →
      **IP Address** (for example `192.168.43.17`).
   3. On the Android phone: **Add connection → TCP client**. Enter the iPhone's address and port
      `47475`. Under **Protocol** choose **Serial / TCP / Bluetooth byte stream** and, if offered,
      **Device / cable: TCP connection**. **Connect.**
   4. The reverse also works: Android **TCP server** (default port `47475`), iPhone **TCP
      connection** to the Android phone's address. The iPhone's **Scan for devices** may find
      the Android listener by name; if not, type the address.
4. **The iPhone opens the hub web page in Safari** (no install at all), if a hub exists.
5. **WebRTC in browsers** (next section).

## Browsers: WebRTC pairing by two codes

Any two browsers running the EmberMesh web app (the single HTML file, the hub page, or the
installed web app) on the same local network can link directly. There is no automatic discovery:
you exchange two codes.

On **device A**:

1. **More → Links & radios**, find **Pair directly (WebRTC)** under "Share this device".
2. Tap **Create invite**. A sheet "Pair: create invite" shows a QR code and the invite text
   (`ember:rtc:...`, about 100-180 characters).

On **device B**:

3. Tap **Answer an invite**. **Scan invite** with the camera, or paste the text into the box.
4. Tap **Create answer**. Device B now shows an answer code and **Show as QR**.

Back on **device A**:

5. **Scan answer** or paste the answer into "Enter the answer code it shows".
6. Tap **Connect**. You see "Paired - connecting". The link comes up on both devices.

Notes:

- Camera scanning works only where the browser supports it (for example Chrome on Android).
  Otherwise use **Copy invite** / **Copy answer** and paste, or type the code carefully.
- No internet server is needed on a LAN or hotspot.
- A WebRTC link cannot be restored after a restart. Pair again.
- The Android and iPhone *apps* do not offer WebRTC; it is a browser feature.

## Wi-Fi Direct (Android only, experimental)

**Wi-Fi Direct** connects Android phones to each other without a router. It is only lightly
tested. Prefer Bluetooth or a hotspot; try Wi-Fi Direct only if both fail.

## A cable between two devices

Two devices can be joined by a serial cable: for example two USB-serial adapters (on USB OTG
adapters for phones) with transmit crossed to receive and grounds joined. Add **USB serial** (or
**Serial port / USB TNC** on a computer) on both ends with link type **Serial / TCP / Bluetooth
byte stream**, **Device / cable: USB or serial cable between two devices**, and the same baud
rate on both. Both ends must use the same **Device / cable** preset and the same **Error
correction** setting, or frames are dropped. This path is untested on real adapters (it was tested
only over a virtual serial cable between two computers).

## When none of it works

Work down this list:

1. **Permissions.** Check in the phone's system settings that EmberMesh has Bluetooth / Nearby
   devices, Local Network (iPhone), local network access (Android 17 and newer: without it every
   Wi-Fi link times out) and, on old Android, Location. On iPhone, **Settings → This device → iOS
   permissions** opens the right page.
2. **Same kind of link on both ends.** A Bluetooth link on one phone cannot talk to a Wi-Fi link
   on the other.
3. **Reboot Bluetooth/Wi-Fi**, then tap **Reconnect** on the link card.
4. **Sound, phone to phone.** Two phones can talk through the air with the sound-card modem: on
   both, **Add connection →** the audio entry ("Audio (soft modem)" on Android, "Sound (radio
   cable or speaker)" on iPhone, "Sound card / radio audio" in a browser). Choose **Modem: Robust
   62 - phone held to a radio, weak signals** and **Band / licence: Licence-free or emergency use**.
   Hold the phones' speaker and microphone close together in a quiet room. It is very slow (tens
   of bits per second) but needs no radio at all. Details in
   [Handheld radios](handheld-audio-modem.md).
5. **Carry messages by hand**: export a bundle to a file or a series of QR codes and import it on
   the other device ([Sneakernet](sneakernet.md)).
