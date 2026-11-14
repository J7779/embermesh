---
id: embermesh/spreading-the-app
title: "EmberMesh 14: Getting the App onto Other People's Devices Without Internet"
category: embermesh
tags: [embermesh, install, sideload, apk, android, iphone, ios, safari, single file, html, usb stick, hub, download]
summary: "Passing the Android APK phone to phone, running the single-file HTML version from a USB stick or SD card, offering downloads from a hub, and the limits and workarounds for iPhones."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 14
---

# Getting the App onto Other People's Devices Without Internet

There is no app store after the grid goes down. EmberMesh is built to spread by hand.

Once installed, each platform has its own complete guide: [Android](app-android.md),
[iPhone/iPad](app-ios.md), [browser](app-web.md), [computer](app-desktop.md).

## Which way for which device?

| The other person has | Give them | How |
|---|---|---|
| Android phone (Android 5.0 or newer) | The Android app (APK file) | **Settings → This device → Share this app** over Bluetooth / Quick Share, or **Save the app as a file** to a USB stick or SD card |
| Laptop or desktop with Chrome, Edge, Firefox or Safari | The single file `embermesh.html` | USB stick, SD card, any file transfer; double-click it |
| iPhone or iPad | A hub web page in Safari | Join the hub's Wi-Fi and open `http://<hub address>:8437/` |
| Any phone or laptop near a hub | Nothing | Join the hub's Wi-Fi and open its address in a browser |
| Anyone who still has internet | The hosted web app, installed | Open the project's website and install it ("Add to Home Screen" / "Install app"): it then stores everything, about 62 MiB including the library, and works with no internet ([Operating the web app](app-web.md)) |

**Before a crisis**, the best preparation for any phone or computer is: install the real app
(Android APK, iPhone via Xcode, emberd) **or** install the hosted web app while the internet still
works. After the internet is gone, the app spreads by APK, by single file and through hubs.

## Android: pass the APK on

On a phone that already has EmberMesh:

1. Open **Settings → This device** and tap **Share this app** (send it by Bluetooth, Quick Share /
   Nearby Share or a file manager) or **Save the app as a file** (to a USB stick or SD card). The
   same **Share app** action is in EmberMesh's persistent notification and under a long press on the
   launcher icon.
2. An Android phone running a hub also offers the APK to its guests at
   `http://<phone address>:8437/download/EmberMesh.apk`.

On the receiving phone:

1. Open the received `.apk` file from the notification or a file manager.
2. Android asks to allow **Install unknown apps** for the app you used to open it (Files,
   Bluetooth, browser). Allow it, go back, and tap **Install**.
3. On Android 7 and older the switch is **Settings → Security → Unknown sources**.
4. Open EmberMesh and go through the first-run screens ([Quick start](quick-start.md)).

From a computer with the Android tools: `adb install EmberMesh.apk`.

**Updates:** every copy is signed with the same key, so a newer APK installs over an older one and
keeps the data. A copy signed with a different key will refuse to update; uninstalling it first
deletes that phone's identity and messages (back up first: **Settings → Identity → Back up
identity**).

**If it doesn't work:**

- "App not installed": not enough storage, or an existing EmberMesh signed with a different key.
- Nothing happens on tap: open the file from a file manager instead of the notification.
- Very old phones: Android 5.0 is the minimum.

## Laptops and desktops: the single HTML file

`embermesh.html` is the whole app (interface, mesh core and crypto) in **one file**. It runs from
a USB stick or SD card with no server and no installation. It is about **3.2 MiB** without the guide
library and about **80.4 MiB** with the whole library inlined (slow to copy and open on old phones).

To get a copy: from any hub's `/download/` page, or, in a browser that opened the app from a
web address, **Settings → This device → Download single-file copy to share**.

1. Copy `embermesh.html` to the computer (or run it straight from the stick).
2. Double-click it. It opens in the default browser.
3. Complete the first-run screens.

Good to know:

- Because the page is opened from a file, the browser treats it as a secure page. The microphone
  (sound-card modem), camera, USB serial and Bluetooth need such a page: `file://`, `https://`
  or `localhost`. In **Chrome or Edge on a desktop** it can use USB serial radios and TNCs,
  Bluetooth TNCs and the sound card. Firefox and Safari should manage the sound card but not serial
  or Bluetooth; they have **not been tested** at all.
- If the browser offers it, **Settings → This device → Install as app** adds EmberMesh like an app
  that opens offline.
- Messages and identity are stored inside that browser on that computer (not in the HTML file),
  so carrying the file to another computer does not carry your identity. Opening the app in a
  second tab gives a **temporary** node with its own identity; use one tab.
- Clearing the browser's site data deletes the identity. Back it up.
- The guide library is included only if the file was built with it (it can make the file tens of
  megabytes). If **Guides** is empty in your copy, the library was left out.
- Keep the browser tab visible on a relay; browsers slow down background tabs and phones
  eventually freeze them.

## Offering the app from a hub

A hub (see [Setting up a hub](hub-setup.md)) serves downloads to anyone on its Wi-Fi at
`http://<hub address>:8437/download/`.

1. On the hub computer, copy the files to offer into the hub's download folder,
   `<data-dir>/download/` (on a Raspberry Pi installed as a service: `/var/lib/embermesh/download/`).
   Useful files: the single-file `embermesh.html`, the Android APK, and a source archive of the
   repository.
2. The single file from the hub's web folder (`embermesh.html`) is offered automatically. A
   **phone hub** offers its own downloads: Android the APK, iPhone the single-file HTML.
3. Guests open `/download/` in a browser and save what they need, then pass it on.

## iPhone and iPad

iPhones cannot install apps from a file. The options:

1. **Open the hub page in Safari (works for everyone, no install).** Join the hub's Wi-Fi (an
   iPhone with the app can itself be the hub, over its Personal Hotspot), open
   `http://<hub address>:8437/`. The full app runs in the browser with its own identity and
   end-to-end encryption, connected to the mesh through the hub.
   - Limits: on a plain `http://` hub page Safari will not give the page the microphone, camera,
     Bluetooth or location, and it cannot be saved for offline use. It works while you are
     connected to the hub's Wi-Fi.
   - Your identity is kept in Safari for that hub address. If the hub's address changes, Safari
     treats it as a different site and you start with a new identity. Back up your identity from
     **Settings → Identity**.
2. **Install the real app before the crisis.** It needs a Mac with Xcode (see the iOS README in
   the source). With a free Apple ID the app **stops launching after 7 days** until it is re-signed,
   and it cannot be re-signed without that Mac; a paid developer account gives one-year Ad Hoc
   builds. Install and open the app once on every phone while you still can, and grant the
   permissions.
3. **AltStore / SideStore** re-sign apps with a free Apple ID over Wi-Fi, if someone has set them
   up in advance.
4. The single HTML file on an iPhone has not been tested; rely on the hub page.

## Keep the means to rebuild

- Keep the **source archive** of the repository on the same stick as the files. Everything can be
  rebuilt offline from it if the build toolchains are also saved (the web build's toolchain is
  about 1.5 GB; see `apps/web/README.md`).
- Keep the `emberd` program for each kind of computer you have (Linux on Raspberry Pi, Linux on
  a PC, macOS, Windows). A program built for one kind does not run on another.

## Checklist for a "seed stick"

- [ ] `embermesh.html` (with guides, if space allows)
- [ ] the Android APK
- [ ] `emberd` builds for your computers, plus the `ui` or `dist/web` folder
- [ ] the repository source archive
- [ ] a printed copy of [Quick start](quick-start.md) and [Quick reference](quick-reference.md)
