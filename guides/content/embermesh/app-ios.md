---
id: embermesh/app-ios
title: "EmberMesh 4: Operating the iPhone and iPad App"
category: embermesh
tags: [embermesh, ios, iphone, ipad, app, install, xcode, signing, permissions, bluetooth, multipeer, hub, personal hotspot, background, backup]
summary: "Complete guide to the EmberMesh iPhone/iPad app: installing it with Xcode and its signing limits, the Safari alternative, first run, permissions, every connection iOS supports and the ones it cannot, hosting a hub on Personal Hotspot, radios over Bluetooth and audio, background limits, security, backup and iOS-specific troubleshooting."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 4
---

# Operating the iPhone and iPad App

The iOS app runs the full EmberMesh node on an iPhone or iPad with **iOS 15 or newer** (iPhone 6s /
SE first generation and later). Apple's rules make three things harder than on Android: installing
without the App Store, running in the background, and connecting cables. This chapter says how to
live with each.

> **What is proven:** in the iOS simulator against the real core: identity, storage and passphrase
> lock, saved connections, encrypted messages with delivery receipts between two nodes and with a
> computer hub (WebSocket, TCP and multicast), the phone hub with a guest, the modem through the
> app's audio functions, and the guide library. **Compiled only, never run on a real phone:**
> Bluetooth to phones and radios, Apple peer-to-peer, real speaker/microphone audio, discovery
> between devices, the hub on a real Personal Hotspot, background operation.

## 1. Getting the app

### With a Mac and Xcode (before the crisis)

1. Open `apps/ios/EmberMesh.xcodeproj` in Xcode → target **EmberMesh** → *Signing & Capabilities*.
2. Tick *Automatically manage signing* and choose a **Team**. A free *Personal Team* (any Apple ID,
   added in Xcode → Settings → Accounts) works. If the bundle id is taken, change it (e.g.
   `org.embermesh.app.yourname`).
3. Connect the iPhone, enable *Developer Mode* (Settings → Privacy & Security, iOS 16+), choose it as
   the run destination, press Run.
4. On the iPhone: Settings → General → VPN & Device Management → trust your developer certificate.

**Signing limits:**

- **Free Apple ID:** the app **stops launching after 7 days** until it is re-run from Xcode (your
  data is kept), and at most 3 such apps per device. An expired app will not start offline.
- **Paid Apple Developer Program:** one-year provisioning and **Ad Hoc** builds for up to 100
  registered devices per type (best for preparedness kits); **TestFlight** builds last 90 days and
  need internet to install.
- **AltStore / SideStore** can refresh the 7-day signature over Wi-Fi if set up in advance.
- **EU (iOS 17.4+):** alternative marketplaces / web distribution for registered developers.

Install and open the app once on every phone **while you still can**, and grant the permissions.

### Without Xcode, while you still have internet: the hosted web app

Open the project's website in Safari and choose **Share → Add to Home Screen**. The web app then
stores everything on the iPhone, including the whole library (about 62 MiB), and works with no
internet; a Home Screen web app keeps its own storage (a website that is only visited may lose its
data after about a week without use). It is a browser app: no Bluetooth and no Wi-Fi links of its
own, but messaging, hubs, the library, Morse reading and the sound-card modem. See [Operating the
web app](app-web.md).

### Without any install: Safari

Join any hub's Wi-Fi (a computer, an Android phone, or another iPhone running the hub) and open
`http://<hub address>:8437/` in Safari. The full app runs in the page with its own identity and
encryption. Limits: no microphone, camera, Bluetooth or location on a plain `http://` page, and it
works only while connected to the hub. See [Operating the web app](app-web.md).

## 2. First run

1. **Welcome to EmberMesh**: **Your name**, optional **Amateur radio callsign**, **Region**,
   **Continue**.
2. **Your identity**: eight fingerprint words. **Start using EmberMesh**.
3. Back up the identity now (section 8): iPhone backups and iCloud **do not include** EmberMesh's
   keys, on purpose.

## 3. Permissions and why

| Prompt | Needed for |
|---|---|
| **Bluetooth** | Phone-to-phone chat, Bluetooth TNCs, Meshtastic radios |
| **Local Network** | Apple peer-to-peer, finding other devices, the TCP listener, hubs, Wi-Fi links |
| **Microphone** | Sound-card modem while such a link runs, and reading Morse by sound (nothing is recorded or stored) |
| **Camera** | Scanning QR contact cards and invites, reading Morse by camera |
| **Location** (While Using) | Your position in SOS and check-ins (GPS works without network) |
| **Notifications** | Messages and SOS alerts while in the background |

**More → Settings → This device → iOS permissions** opens the iOS Settings page for EmberMesh, to
change any of them later.

## 4. Connections on iPhone and iPad

Add them in **More → Links & radios → Add connection**; every setting can be changed later with the
link card's **Settings**.

| Entry on iOS | Use it for | Protocol |
|---|---|---|
| **Bluetooth (nearby phones)** | Phones (iPhone and Android) within about 10-50 m | Serial / TCP / Bluetooth byte stream |
| **Apple peer-to-peer (iPhone, iPad, Mac)** | Other Apple devices running EmberMesh nearby; up to 7 at once | Direct datagrams |
| **Hub (WebSocket)** | Joining a hub (**Hub address**, e.g. `ws://10.42.0.1:8437/ws`) | Direct datagrams |
| **Local network listener** | Letting other devices on the same Wi-Fi connect to this phone (port 47475); the way to reach Android phones over Wi-Fi | Byte stream or KISS |
| **TCP connection** | Direwolf (port 8001), Meshtastic on Wi-Fi (4403), another node's listener (47475); **Scan** finds EmberMesh listeners | KISS, Meshtastic, RNode, Reyax, SDR or byte stream |
| **Bluetooth radio modem / TNC** | BLE devices; **Device type**: **Serial bridge / KISS TNC (Nordic UART)**, **KISS TNC with the BLE KISS service (Mobilinkd TNC3/TNC4)**, **Meshtastic radio**, **Custom UUIDs** | KISS, Meshtastic (keep its Connection setting at the default), RNode, Reyax or byte stream |
| **Sound (radio cable or speaker)** | Any radio by cable or held to the phone | Sound-card modem |
| **SDR receiver (rtl_tcp)** | Listening through an RTL-SDR on a computer running `rtl_tcp` | SDR receiver |
| **Hub (Wi-Fi guests)** | This phone as a hub (easier: the Hub card, section 5) | Direct datagrams |
| **Wi-Fi network broadcast (UDP)** | Normally **unavailable**: Apple allows it only with a special permission | |
| **Loopback (testing)** | Testing | |

**Not possible on iPhone/iPad, and what to do instead:**

| Missing | Instead |
|---|---|
| USB serial cables (no app access to USB serial) | A Bluetooth LE TNC or serial bridge, or the sound-card modem |
| Classic Bluetooth serial (SPP) | Bluetooth LE devices |
| Wi-Fi Direct | Apple peer-to-peer (Apple devices) or Bluetooth (Android) |
| WebRTC pairing | Use a browser on another device, or a hub |
| PTT keying by cable | Radio **VOX**, or press PTT yourself |
| Wi-Fi multicast (UDP) | **Local network listener** + **TCP connection**, Apple peer-to-peer, or a hub |

### The sound connection on iOS

| Field | Choices |
|---|---|
| **Listen with** | Automatic (cable / USB if plugged in), Headset jack adapter (TRRS), USB-C / Lightning audio interface, Built-in microphone (acoustic), Bluetooth headset (not suitable for data) |
| **Play through** | Automatic (cable if plugged in), Loudspeaker (acoustic), Earpiece |
| **Transmitter keying (PTT)** | VOX on the radio (the radio keys itself on our audio), or I press PTT myself (acoustic / hand-held) |
| **Transmit volume** | 1-100 %, default 80 % |

Lightning/USB-C to 3.5 mm adapters with a **TRRS** plug, and class-compliant USB audio interfaces
(CM108-type dongles, a Digirig in audio mode) over USB-C or the Lightning camera adapter, work in
principle (untested). For acoustic coupling choose **Loudspeaker**, keep the phone a few cm from the
radio, work in a quiet room, and expect only the slowest modes to work. Cables and levels:
[Handheld radios](handheld-audio-modem.md), [What you need](what-you-need.md).

**Radios:** **Add connection → Connect a radio (guided)** walks through it; methods needing a PTT
line are marked **not here** on iPhone. See [Baofeng UV-5R](baofeng-uv5r.md).

### Reaching Android phones over Wi-Fi

iPhones cannot use EmberMesh's Wi-Fi broadcast. On the same Wi-Fi: iPhone adds **Local network
listener** (port 47475); the Android phone adds **TCP client** to the iPhone's IP address
(iPhone Settings → Wi-Fi → (i) → IP Address) and port 47475. Or use a hub.

## 5. Hosting a hub on Personal Hotspot

1. Turn on **Personal Hotspot** and let guests join it.
2. **More → Links & radios → Hub → Start hub**.
3. Show guests the QR code; the address is usually `http://172.20.10.1:8437/`. The phone also offers
   the single-file app at `/download/`.
4. **Keep EmberMesh on screen**, the phone awake and on a charger: iOS stops the hub when the app
   leaves the screen. (Tested in the simulator only; never on a real Personal Hotspot.)

## 6. Background running

- **Bluetooth links keep working** with the screen off and wake the app.
- Apple peer-to-peer, Wi-Fi, TCP and the hub **stop shortly after the app leaves the screen**.
- A running sound link keeps the app alive.
- iOS wakes the app now and then (at most about every 15 minutes) to pass on stored messages.
- **Android phones generally cannot find an iPhone whose EmberMesh is in the background.** Keep it on
  screen when meeting Android users.

For relay duty: app open, on a charger.

## 7. Security

**More → Settings → Security:** passphrase (after a restart the app is locked with no connections
until unlocked), **Lock now**, **Duress passphrase**, **Panic wipe**. EmberMesh's files on the phone
are protected by iOS until the phone is first unlocked after starting. See [Security](security.md).

## 8. Backup, moving and updating

- **Back up:** Settings → Identity → **Back up identity** → passphrase → **Save file** (the share
  sheet offers Save to Files and AirDrop), **Copy** or **QR**.
- **Restore** on the new phone: Settings → Identity → **Restore identity**.
- **Messages:** Settings → Sneakernet bundles → **Export bundle**, then import on the new device.
- **Updating** a Xcode-installed app: run the newer project from Xcode on the same phone with the same
  bundle id and team; data is kept. Re-running also renews a 7-day free signature.

## 9. iOS troubleshooting

| Problem | Fix |
|---|---|
| The app will not open after a week | Free signature expired: re-run from Xcode, or use the Safari hub page |
| Android cannot find the iPhone over Bluetooth | Bring EmberMesh to the screen on the iPhone |
| Wi-Fi broadcast shows unavailable | Expected; use Local network listener / TCP, Apple peer-to-peer or a hub |
| Nothing works over Wi-Fi | **Settings → This device → iOS permissions**: allow Local Network |
| Hub stops | The app left the screen; keep it open and awake |
| Radio never transmits | iOS cannot key PTT by cable: turn on VOX or press PTT |
| Bluetooth radio asks for a PIN | Meshtastic radios may request their pairing PIN |
| Mobilinkd not found as a serial bridge | Choose **KISS TNC with the BLE KISS service (Mobilinkd TNC3/TNC4)** |
