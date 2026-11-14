---
id: embermesh/app-web
title: "EmberMesh 5: Operating the Web App (Browser, Single File, Installed)"
category: embermesh
tags: [embermesh, web, browser, single file, html, pwa, chrome, safari, firefox, webrtc, web serial, web bluetooth, hub guest, install]
summary: "Complete guide to running EmberMesh in a web browser: the single-file HTML from a USB stick, the installed web app, joining a hub from any phone, what each browser can and cannot do, connections (hub, WebRTC pairing, USB serial, Bluetooth, sound card), storage, backup and browser-specific troubleshooting."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 5
---

# Operating the Web App

The web app is the whole EmberMesh node compiled to run **inside a browser**, on any computer or
phone. It comes in four forms:

| Form | How you get it | Good for |
|---|---|---|
| **Hosted website** | The project's build pipeline publishes the web app as a website (the source repository's README links to it). Open it while you still have internet | Getting the app before a crisis, on any device with a modern browser |
| **Installed web app** | From the website: "Add to Home Screen" / "Install app" (or **Settings → This device → Install as app**) | **Stores everything on the device** and then works with no internet at all (section 5) |
| **Single file** `embermesh.html` | Copy from a USB stick, SD card, or a hub's `/download/` page; double-click | Laptops with no install rights; spreading the app by hand. About 3.2 MiB without the guide library, about 80.4 MiB with the whole library inlined (works, but slow to copy and open on old phones) |
| **Hub page** | Join a hub's Wi-Fi and open `http://<hub address>:8437/` | Anyone with any phone, including iPhones without the app |

> **What is proven:** in headless Chrome on macOS: the full node, storage across reloads, saved
> links, loopback between tabs, WebRTC pairing on one machine, hub guests through a real computer
> hub, the guide library, offline reload of the installed app, and the sound-card modem chain inside
> the browser. **Not proven:** real microphone capture and speaker output, real USB-serial and
> Bluetooth devices, WebRTC between two separate devices, **Firefox, Safari and phone browsers**, and
> background-tab behaviour on phones.

## 1. Secure pages: why the form matters

Browsers give the microphone, camera, USB serial, Bluetooth, location, notifications and offline
installation only to **secure pages**: `file://` (the single file), `https://`, or `localhost`.

- The **single file** is a secure page: on Chrome or Edge on a computer it can use radios, TNCs and
  sound cards.
- A **hub page** at `http://192.168.x.x:8437/` is **not** secure. Messaging, the hub connection,
  WebRTC, files, tone and flash work; microphone, camera, serial, Bluetooth, location,
  notifications and installing do not.

## 2. What each browser can do

From the browsers' documented support (Chrome headless on macOS is the only one actually tested):

| | Chrome/Edge (computer) | Chrome (Android) | Firefox | Safari (Mac) | Any browser on iPhone/iPad |
|---|---|---|---|---|---|
| Messaging, storage, hub, WebRTC, files, tone, flash | yes | yes | yes | yes | yes |
| USB serial (radios, TNCs, LoRa boards) | yes | no | no | no | no |
| Bluetooth LE (TNCs, phones) | yes (Linux: behind a flag) | yes | no | no | no |
| Sound-card modem | yes | yes | yes | yes | yes |
| Choosing the audio output device | yes | no | no | no | no |
| QR scanning with the camera | macOS/ChromeOS only | yes | no | no | no |
| Location, keep screen awake | yes | yes | yes | yes | yes |
| Notifications | yes | yes | yes | yes | installed web app only |
| Phone torch for Morse | no | yes | no | no | no |
| Morse reader (camera / microphone) | yes | yes | yes | yes | yes |

All of these that use the camera or microphone need a secure page (section 1).

## 3. First run and storage

1. Open the file or the address. **Welcome to EmberMesh** → name, optional callsign, region →
   **Continue** → **Start using EmberMesh**.
2. Your identity and messages are stored **inside this browser** on this device (not in the HTML
   file, and per hub address for hub pages). Clearing the browser's site data deletes them.
3. A second tab of the app gets a **temporary** node with its own identity. Use one tab.
4. Back up the identity: **Settings → Identity → Back up identity** (**Save file** downloads it).

On a hub page the connection to the hub is added automatically; nothing to configure.

## 4. Connections in a browser

**More → Links & radios → Add connection**:

| Entry | Use it for | Protocol |
|---|---|---|
| **WebSocket (hub)** | A hub on the network (**Hub WebSocket URL**, e.g. `ws://10.42.0.1:8437/ws`). Added automatically on a hub page. An `https://` page cannot open a `ws://` hub | Direct datagrams |
| **WebRTC (pair by QR / copy-paste)** | Another browser on the same network, directly; see below | Direct datagrams |
| **USB serial (radio, TNC, LoRa)** | Chrome/Edge on a computer: TNCs, Meshtastic boards, RNodes, Reyax modules, transparent radio modules; **Baud rate**, **PTT keying line** (None / RTS / DTR), **Invert PTT line** | KISS, Meshtastic, RNode, Reyax or byte stream |
| **Bluetooth LE** | Chrome: **Device type** Auto, **EmberMesh phone (peer service)**, **Nordic UART (BLE TNC / serial bridge)** or **Custom UUIDs** | Byte stream or KISS |
| **Sound card / radio audio** | Any radio by cable or acoustically: **Input (microphone / line in)**, **Output (speaker / line out)** (choice works in Chrome/Edge), **Transmit level** (default 70 %), **Transmitter keying (PTT)**: VOX, Serial port RTS (USB cable / Digirig), Serial port DTR | Sound-card modem |
| **Loopback (other tabs)** | Testing between two tabs | Direct datagrams |

The browser asks you to pick the serial port or Bluetooth device in its own dialog (it needs a
click from you). Saved serial and Bluetooth links reopen at the next start without asking.

**Not possible in a browser:** hosting a hub (browsers cannot accept connections), Wi-Fi broadcast
(UDP) and plain TCP links (so no Direwolf over TCP, no SDR), Apple peer-to-peer, and Meshtastic over
Bluetooth (use USB). Use a computer with emberd or a phone app for those.

### WebRTC pairing step by step

1. Device A: **Links & radios → Pair directly (WebRTC) → Create invite**. It shows a QR code and the
   invite text (`ember:rtc:...`, about 100-180 characters).
2. Device B: **Answer an invite**, **Scan invite** or paste it, **Create answer**.
3. Device A: **Scan answer** or paste it, **Connect**. "Paired - connecting".

Both must be on the same network; no internet server is needed on a LAN or hotspot. A WebRTC link
cannot be restored after a restart: pair again.

## 5. Installing for offline use, and updates

Opened from the hosted website (`https`), the app makes itself independent of the internet:

1. On the first visit it downloads **everything** to the device: the interface, the mesh core and the
   whole guide library (about **62 MiB** in total), each file checked. An interrupted download
   resumes later.
2. Install it: **Chrome/Edge** (computer, Android) "Install app", or **Settings → This device →
   Install as app**; **Safari on iPhone/iPad**: **Share → Add to Home Screen**; **Firefox** works as
   a website (no install prompt on computers).
3. From then on it starts, searches the library and works as a full node **with no network at all**.
4. When the internet is available, it checks for a newer version (at start, every 6 hours and when
   the device comes back online), downloads it completely in the background and switches to it at the
   **next start**, never in the middle of use and never to a half-downloaded version. A new version
   that fails to start is rolled back.

**Settings → This device** shows the state in each entry and offers, when they apply:

- **Download everything for offline use** — store it all now (also done automatically).
- **Check for updates**, and **Restart to update** when a downloaded update is ready.
- **Install as app** — when the browser offers installation (secure pages only).
- **Download single-file copy to share** — when the app was opened from a web address: one HTML
  file to put on a USB stick or another phone.

**Per browser** (from the browsers' documented behaviour; only Chrome was tested):

- **Chrome/Edge:** a website's stored data is "best effort" and can be removed when the device runs
  short of storage, unless the browser grants persistent storage (installed apps and frequently used
  sites usually get it). The app asks for it and shows honestly whether it was granted.
- **Safari on iPhone/iPad:** Safari may delete a website's stored data when it has not been used for
  about a week and is **not** on the Home Screen; the status then shows a warning. **Add it to the
  Home Screen**: a Home Screen app has its own storage.
- **A plain-http hub page** cannot install or store itself (not a secure page). Download the single
  file from the hub instead.

Nothing is ever fetched from a third party. Offline, everything keeps working except what inherently
needs a network: a hub, WebSocket links and update checks.

## 6. Keeping it running

Browsers slow down background tabs and phones eventually freeze them. On a relay or hub-guest
device keep the tab **visible**, the screen on and the device on a charger. Starting an SOS asks the
browser to keep the screen awake.

## 7. The operator page of a computer hub

The same web app, opened as `http://localhost:8437/?remote=1&token=...` on a computer running
emberd, controls that computer's node instead of running its own. See
[Operating the computer hub](app-desktop.md).

## 8. Updating and moving

- **Single file:** replace the file with the newer one. The identity lives in the browser, so it
  carries over when the new file is opened in the same browser on the same computer; back up first
  anyway.
- **Hub page:** updates when the hub is updated.
- **To another device or browser:** **Back up identity** here, **Restore identity** there; carry
  messages with **Export bundle** / **Import bundle**.

## 9. Browser troubleshooting

| Problem | Fix |
|---|---|
| "EmberMesh could not start" on a hub page | The hub serves its interface without the mesh core; the hub operator must serve `dist/web` |
| Microphone, Bluetooth or serial shown as unavailable | Not a secure page (use the single file or `localhost`), or the browser lacks the feature (see the table) |
| A new identity every time | Site data cleared, private window, or a different file location/hub address. Restore your backup |
| "Temporary node" note | Another tab already has the app open; close it |
| Changed a connection's settings, nothing changed | Reload the page (known bug) |
| WebRTC "Invalid invite" | Copy the whole code, or scan the QR |
| Hub link fails on an `https://` page | Browsers block `ws://` from `https://`; open the hub page itself or the single file |
