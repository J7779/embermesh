---
id: embermesh/app-android
title: "EmberMesh 3: Operating the Android App"
category: embermesh
tags: [embermesh, android, app, install, apk, sideload, permissions, usb otg, bluetooth, hub, battery, backup, setup]
summary: "Complete guide to the EmberMesh Android app: installing it without an app store, first run, every permission, every connection type Android supports (and the ones it does not), hosting a hub, radios over USB, Bluetooth and audio, background running, security, backup, updates and Android-specific troubleshooting."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 3
---

# Operating the Android App

The Android app runs the full EmberMesh node on the phone. It works on **Android 5.0 and newer**,
needs no Google services, and keeps relaying in the background.

> **What is proven:** the app was tested on the Android 17 emulator against the real mesh core:
> identity, storage and passphrase lock, saved connections, two nodes exchanging encrypted messages,
> messaging with a computer hub over Wi-Fi-style sockets, the phone hub with a guest, and the sound-card
> modem through the app's real audio code with a virtual cable. **Never exercised:** real Bluetooth,
> USB-serial chips, classic Bluetooth serial, Wi-Fi Direct, Bluetooth headsets, real radios and a
> real phone hotspot. Test those yourself before relying on them.

## 1. Getting the app with no internet

From another Android phone that has EmberMesh:

1. On that phone: **More → Settings → This device → Share this app** (sends it by Bluetooth,
   Quick Share / Nearby Share or a file manager) or **Save the app as a file** (to a USB stick or SD
   card). The same **Share app** action is in EmberMesh's notification and under a long press on its
   launcher icon.
2. On your phone, open the received `.apk` file (from the notification or a file manager).
3. Android asks to allow **Install unknown apps** for the app you opened it with. Allow it, go back,
   tap **Install**. On Android 7 and older the switch is **Settings → Security → Unknown sources**.

While you still have internet, the hosted web app can also be installed from the project's website
(Chrome: "Install app"); see [Operating the web app](app-web.md). The Android app is more capable
(Bluetooth, USB, background relaying).

From a hub: join its Wi-Fi and open `http://<hub address>:8437/download/` in the browser (an Android
phone hub offers `/download/EmberMesh.apk`). From a computer: `adb install EmberMesh.apk`.

**Updates** install over the old copy and keep your data, as long as the new APK is signed with the
same key (all copies built from this project are). A copy signed differently will not update; do not
uninstall to force it without backing up your identity first.

## 2. First run

1. Open EmberMesh. **Welcome to EmberMesh** asks for **Your name**, an optional **Amateur radio
   callsign** (only if licensed) and your **Region** (sets legal frequencies, power limits and the
   default LoRa region). Tap **Continue**.
2. **Your identity** shows your node ID and eight fingerprint words. Tap **Start using EmberMesh**.
3. Back up your identity now: **More → Settings → Identity → Back up identity** (section 8).
4. Add a connection (section 4).

## 3. Permissions and why they are asked

Android asks for each one the first time a feature needs it. Refusing only disables that feature.

| Permission (as Android shows it) | Needed for |
|---|---|
| **Nearby devices** (Android 12+) | Bluetooth phone-to-phone, Bluetooth TNCs and LoRa boards, classic Bluetooth serial |
| **Local network** access (Android 17+; "find, connect to, and determine the relative position of nearby devices") | **Every Wi-Fi link, Nearby devices and the hub.** Asked when Nearby devices is turned on. Without it they silently time out. Links blocked by it are retried with the prompt when you open the app |
| **Location** | Your GPS position for SOS and check-ins (GPS works with no network). On Android 11 and older also needed for Bluetooth and Wi-Fi scans, and Location must be switched on |
| Nearby Wi-Fi devices (Android 13+) | Wi-Fi Direct |
| **Microphone** | Sound-card modem (radio audio, acoustic coupling), reading Morse by sound |
| **Camera**, flashlight | Scanning QR codes, reading Morse by camera, flashlight Morse and SOS |
| **Notifications** | New messages, SOS alerts, the "relaying" notification |
| Run at startup | Only if you switch on **Start when the phone starts** |
| Ignore battery optimisations | Lets the app open Android's exemption dialog |

## 4. Connections on Android

**Automatic connections** (switches in **Settings → Connections**, see [Connections](connections.md)):
**Nearby devices** (on after setup) links up by itself with every EmberMesh device on the same Wi-Fi
or hotspot; the **Internet relay** (off until you turn it on) reaches contacts far away. Both run only
while the app is open or its background service runs (no push notifications: a closed app gets nothing over the relay). They appear on Links & radios as **Automatic · nearby device** / **Automatic
· internet relay** with **Change in Connections**. Everything else, including Bluetooth, is added
by hand:

All connections are added in **More → Links & radios → Add connection**. Each entry shows a
description; tap it, fill the **Connection** and **Protocol** cards, give it a **Name**, keep
**Reconnect automatically when the app starts** ticked, and tap **Connect**. Every setting can be
changed later with the link card's **Settings** button.

| Entry on Android | Use it for | Protocol to choose |
|---|---|---|
| **Wi-Fi LAN / hotspot (UDP)** | Every EmberMesh device on the same Wi-Fi or phone hotspot; zero setup | Direct datagrams |
| **Bluetooth LE (phone to phone)** | Phones within about 10-50 m, no other hardware | Serial / TCP / Bluetooth byte stream |
| **WebSocket (hub)** | Joining a hub's Wi-Fi from the app (**Hub URL** such as `ws://10.42.0.1:8437/ws`) | Direct datagrams |
| **Hub (serve the app on Wi-Fi)** | Making this phone a hub (easier: the Hub card, section 5) | Direct datagrams |
| **TCP client** | Direwolf on a computer (port 8001), Meshtastic radios on Wi-Fi (port 4403), an `rtl_tcp` SDR server (port 1234), another node's TCP server (port 47475) | KISS, Meshtastic, RNode, Reyax, SDR receiver or byte stream, as fits |
| **TCP server** | Letting another device connect to this phone (default port 47475, announced on the network) | Byte stream or KISS |
| **USB serial** | Through a USB OTG adapter: TNCs, radios with a TNC, Meshtastic boards, RNodes, Reyax modules, transparent radio modules, a USB-serial adapter for PTT | KISS, Meshtastic, RNode, Reyax or byte stream |
| **Bluetooth LE serial device** | BLE TNCs, serial bridges (Nordic UART or custom UUIDs), Meshtastic radios (**Device type: Meshtastic radio (Meshtastic Bluetooth API)**) | KISS, Meshtastic, RNode or byte stream |
| **Bluetooth serial (SPP)** | Classic Bluetooth TNCs and radios; pair them in Android's Bluetooth settings first | KISS or byte stream |
| **Audio (soft modem)** | Any radio by cable or held to the phone | Sound-card modem |
| **Wi-Fi Direct** | Android phones directly over Wi-Fi without a router; only lightly tested | Byte stream |
| **Loopback (test)** | A link that talks to itself, for testing | |

**Not available on Android:** WebRTC pairing (use a browser), Apple peer-to-peer (iPhones only),
the operator page on the phone's own hub, and a computer-style sound card or virtual audio pipe.

### Step by step: the common ones

**Wi-Fi or hotspot:** join the Wi-Fi (or switch on this phone's hotspot) → **Wi-Fi LAN / hotspot
(UDP)** → leave **Network** at "All networks (automatic)" → **Connect** → allow local network
access. If a hotspot isolates its clients, fill **Extra peers** (Advanced) with the other devices'
addresses.

**Bluetooth to another phone:** switch Bluetooth on (and Location on Android 11 and older) →
**Bluetooth LE (phone to phone)** → leave **Let nearby phones connect to me** and **Connect to
nearby phones** on → **Connect** → allow Nearby devices. Keep both apps open the first time.

**USB device (TNC, LoRa board, radio):**

1. Connect it with a **USB OTG adapter** or a USB-C/micro-USB OTG cable (a plain charging cable
   does not work). The phone must support USB host. Hungry devices may need a powered hub or a
   Y-cable.
2. Android opens EmberMesh and asks once for permission to use the device: tick "use by default"
   and allow.
3. **Add connection → USB serial → Scan for devices**, pick it, set **Baud rate** (KISS TNCs usually
   9600 or 57600; Meshtastic, RNode and Reyax 115200).
4. Choose the protocol, fill its settings, **Connect**.

Supported USB-serial chips: FTDI (FT232R/BM/FT2232/FT4232/FT232H/FT-X), Silicon Labs CP2102/CP2104/
CP2105 (port 0)/CP2108, WCH CH340/CH341, Prolific PL2303 HX/TA/TB/HXN (counterfeit "PL2303HXA" may
refuse) and any CDC-ACM device (Arduino, RP2040, nRF52 boards such as RAK4631 and T-Echo, ESP32-S2/
S3 native USB, STM32, Mobilinkd TNC3/4, many radios' USB ports). If an ESP32 board resets on
connect, turn off **Assert DTR** (Advanced).

**Radio by audio:** see [Handheld radios](handheld-audio-modem.md). On Android the connection card
has **Input (from radio)**, **Output (to radio)**, **Transmit level** (%) and **Push-to-talk (PTT)**:
**VOX / none (the radio keys itself)**, **Serial connection "name" (RTS/DTR)** (a serial link
already open to the radio) or **USB serial adapter ..., PTT only**. **PTT line** RTS/DTR and
**Invert PTT** are under Advanced. Transmit level and PTT can be changed while the link runs.

Easiest for radios: **Add connection → Connect a radio (guided)**. When an **AIOC** or a
**Digirig**-style interface is plugged in, the app recognises it and suggests the right input,
output and PTT line (AIOC keys on DTR, Digirig on RTS). For which cable or adapter you need, see
[What you need](what-you-need.md); for the Baofeng UV-5R, [Baofeng UV-5R](baofeng-uv5r.md).

## 5. Hosting or joining a hub

- **Host:** switch on the phone's hotspot (or join a router) → **More → Links & radios → Hub → Start
  hub** → allow local network access → show guests the QR code next to the address. Guests open it
  in any browser. The phone also offers the APK at `/download/EmberMesh.apk`. **Stop hub** ends it.
  (Tested on the emulator; never on a real phone hotspot.)
- **Join:** be on the hub's Wi-Fi; the app finds a computer hub automatically through **Wi-Fi LAN /
  hotspot (UDP)**, or add **WebSocket (hub)** with the hub's address.

Details: [Setting up a hub](hub-setup.md).

## 6. Running in the background and battery

EmberMesh runs as a background service with a persistent notification and restarts if Android stops
it. To keep it relaying:

1. **More → Settings → This device → Keep running with the screen off** and allow the exemption
   (the entry then reads **Battery optimisation: exempt**).
2. **Start when the phone starts** (same place) for unattended phones.
3. On Xiaomi, Huawei, OPPO/Realme, Samsung and similar brands also allow autostart and "no
   restrictions" in the vendor's battery settings.
4. The notification's **Quit** stops EmberMesh completely.

The app keeps the processor awake only while a link needs it (USB serial, audio, Wi-Fi/TCP/
WebSocket, the hub, classic Bluetooth). A Bluetooth-LE-only phone uses the least battery. More in
[Power and field operation](power-and-field.md).

## 7. Security on the phone

**More → Settings → Security:** **Set passphrase** (encrypts everything on the phone; after a
restart the app stays locked, with no connections, until unlocked), **Lock now**, **Duress
passphrase**, **Panic wipe** (type `ERASE`, hold 3 s). See [Security](security.md).

## 8. Backup and moving to another phone

1. Old phone: **Settings → Identity → Back up identity**, passphrase of at least 8 characters, then
   **Save file**, **Copy** or **QR**.
2. New phone: install EmberMesh, complete first run, **Settings → Identity → Restore identity**,
   paste or **From file** or **Scan QR**, enter the passphrase, **Restore**, confirm.
3. Messages are not in the backup. To carry them: on the old phone **Settings → Sneakernet bundles
   → Export bundle → Everything I carry**, import it on the new one.

Uninstalling the app deletes its data. Back up first.

## 9. Android troubleshooting

| Problem | Fix |
|---|---|
| Wi-Fi links and the hub time out (Android 17+) | Allow local network access; reopen the app to get the prompt again |
| Bluetooth finds nobody | Bluetooth on; Location on (Android 11 and older); allow Nearby devices; both apps open. Some old phones cannot advertise (see the link's Statistics) |
| Nobody seen on a hotspot | Run EmberMesh on the hotspot phone too, or fill **Extra peers** |
| USB device not listed | Use a real OTG adapter; powered hub for hungry devices; check the chip is in the list above |
| Board resets when connected | Turn off **Assert DTR** (Advanced) |
| Mesh stops with the screen off | Battery exemption (**This device**), vendor autostart settings |
| "native library failed to load" | The APK was built without this phone's processor type; get another build |
| A diagnostics page appears instead of the app | The APK was built without the interface; get another build |

For developers: `adb logcat -s EmberRuntime EmberLinks EmberNode EmberUI EmberWeb MeshService`.
