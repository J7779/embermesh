---
id: embermesh/hub-setup
title: "EmberMesh 14: Setting Up a Hub (Wi-Fi Hotspot Anyone Can Join)"
category: embermesh
tags: [embermesh, hub, emberd, raspberry pi, hotspot, wifi, nmcli, hostapd, macos, windows, operator, server, captive portal]
summary: "Turn a laptop, Raspberry Pi, Android phone or iPhone into a hub that serves EmberMesh over its own Wi-Fi so any phone can join by opening a web page: running emberd, hotspot recipes per OS, what guests do, the operator page, running as a service, power and placement."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 14
---

# Setting Up a Hub

A **hub** serves EmberMesh to everyone on a Wi-Fi network. It can be a computer running the
EmberMesh program `emberd`, or an **Android phone or iPhone** running the app (see "A phone as the
hub" below). A computer hub does three things at once:

1. It is a full mesh node of its own (identity, storage, store-and-forward, relaying).
2. It **serves the EmberMesh web app** on port `8437` to every phone and laptop on its network.
   Guests open a web page; their browser runs its own node with its own identity and end-to-end
   encryption, and joins the mesh through the hub. **Nothing has to be installed on the phones.**
3. It can be a **radio gateway**: add a radio and everyone on the Wi-Fi reaches distant groups
   ([Radio gateway](radio-gateway.md)).

```
   phone (browser) --\
   phone (app) ------+--- Wi-Fi hotspot --- [ HUB: emberd :8437 ] --- radio --- far groups
   laptop (browser) -/                         |
                                           (optional) Ethernet / other hubs
```

Computer hubs: macOS is built and tested. Linux and Raspberry Pi use the same code but were **not
built or run** in the latest round; Windows code has **never been compiled**. Phone hubs: see
below.

## A phone as the hub

Both phone apps can host a hub (only computers and phones; a browser cannot).

1. Make the network: turn on the phone's **hotspot** (Android) or **Personal Hotspot** (iPhone),
   or join a Wi-Fi router everyone else uses.
2. In EmberMesh open **More → Links & radios**. Under **Share this device**, the **Hub** card says
   **off**. Tap **Start hub**.
3. The card shows **running · 0 connected** and the hub addresses. Tap the QR button next to an
   address and let guests scan it, or read it out (an iPhone's Personal Hotspot address is usually
   `http://172.20.10.1:8437/`).
4. Guests join the Wi-Fi and open the address, exactly as with a computer hub.
5. **Stop hub** ends it. A running hub comes back after the app restarts.

Notes:

- **Android 17 and newer:** allow **local network** access when asked; without it the hub and every
  Wi-Fi link silently time out.
- **iPhone:** the hub runs only while EmberMesh is **on screen**. Keep the phone awake and on a
  charger.
- Phone hubs serve the web app, `/hub.json` and downloads (Android: the APK; iPhone: the single-file
  HTML), but no operator page: you manage the phone's own node in its app.
- Proven so far: the Android hub on the Android emulator and the iPhone hub in the iOS simulator,
  each with a guest exchanging messages. A hub over a **real** phone hotspot (Android or iPhone
  Personal Hotspot) has **not** been tested.

## A computer as the hub

Fastest path (computer already on a Wi-Fi router, no internet needed):

1. Put `emberd` and the web app folder `dist/web` on the computer (`./build.sh desktop web`
   builds both; see [Building from source](building-from-source.md)). emberd finds `dist/web` by
   itself when started from the repository folder or when the folder sits next to it.
2. Open a terminal there and run:

   ```sh
   ./emberd --callsign QX0ABC     # --callsign only if you hold an amateur licence
   ```

3. emberd prints one address per network interface, for example
   `http://192.168.1.23:8437/`, a QR code of the address (if available), and the **operator
   address** with a secret token: `http://localhost:8437/?remote=1&token=...`.
4. On a phone on the same Wi-Fi, open the printed address in the browser (or scan the QR code with
   the camera). EmberMesh loads. Go through the first-run screens.

**You should see:** on the phone, **Links & radios** lists a hub link that is **Connected**; both
the phone and the hub appear in **People → Nearby**.

The web app folder matters: `dist/web` contains the mesh core (WebAssembly). emberd looks for a
folder with both `index.html` and `wasm/ember.wasm`; if it can only find the plain `ui` folder it
warns at start, and guests would see "EmberMesh could not start". With no web folder at all, emberd
serves a small built-in fallback page. `--web-root DIR` chooses the folder explicitly.

A new hub's node takes the computer's name; `--name` sets another one. `--callsign` sets the
amateur callsign that radio links need.

## Make the computer its own Wi-Fi hotspot

No router? Let the hub create the network.

| Computer | Recipe | Hub address for guests |
|---|---|---|
| Linux with NetworkManager (Raspberry Pi OS bookworm, Ubuntu, Fedora) | `sudo ./hotspot-nmcli.sh --ssid EmberMesh` | `http://10.42.0.1:8437/` |
| Linux without NetworkManager (older Pi OS, minimal installs) | `sudo ./setup-hostapd.sh --ssid EmberMesh` | `http://10.42.0.1:8437/` (also `http://ember.mesh:8437/`) |
| macOS | Internet Sharing to Wi-Fi | `http://192.168.2.1:8437/` |
| Windows 10/11 (untested) | Mobile hotspot | usually `http://192.168.137.1:8437/` |

The scripts are in `apps/desktop/deploy/linux/` and `apps/desktop/deploy/linux/hostapd/`.

### Linux / Raspberry Pi with NetworkManager

```sh
sudo ./hotspot-nmcli.sh --ssid EmberMesh                    # open network (easiest for strangers)
sudo ./hotspot-nmcli.sh --ssid EmberMesh --password SECRET  # WPA2, 8 to 63 characters
sudo ./hotspot-nmcli.sh --down                              # remove the hotspot
```

Other options: `--iface wlan0` (default), `--band bg` (2.4 GHz, default) or `a` (5 GHz),
`--channel 6` (default). The hotspot starts again automatically at boot. The script prints the
guest address when it is done.

### Linux without NetworkManager (hostapd + dnsmasq)

```sh
sudo ./setup-hostapd.sh --iface wlan0 --ssid EmberMesh [--password SECRET]
```

This gives `wlan0` the fixed address `10.42.0.1`, runs an access point and hands out addresses
`10.42.0.10`-`10.42.0.200`. The name `ember.mesh` also points at the hub. The supplied
`hostapd.conf` sets `country_code=US`; change it to your country's two-letter code.

### macOS: Internet Sharing

macOS cannot create a stand-alone network any more; Internet Sharing does the job without any
internet:

1. **System Settings → General → Sharing → Internet Sharing → (i)**.
2. "Share your connection from": pick something other than Wi-Fi (Ethernet, a USB Ethernet
   adapter, Thunderbolt Bridge). It does not need to be connected to anything.
3. "To devices using": **Wi-Fi**. In Wi-Fi Options set the name (`EmberMesh`), channel and a
   password (macOS does not offer open networks).
4. Turn Internet Sharing on. Guests use `http://192.168.2.1:8437/`.

If Internet Sharing is greyed out, disconnect the Mac from other Wi-Fi networks first. On the first
start allow emberd through the firewall (**System Settings → Network → Firewall → Options**).

### Windows: Mobile hotspot (untested)

1. **Settings → Network & internet → Mobile hotspot**.
2. Pick a connection to "share" (it need not have internet; some builds want an adapter that is at
   least connected to something).
3. Set name, password and band; switch it on; turn off the power-saving option that switches the
   hotspot off when nobody is connected.
4. Allow emberd through the firewall (it asks the first time; allow on **Private** networks), or
   from an administrator PowerShell:

   ```powershell
   New-NetFirewallRule -DisplayName EmberMesh -Direction Inbound -Protocol TCP -LocalPort 8437 -Action Allow
   New-NetFirewallRule -DisplayName "EmberMesh LAN" -Direction Inbound -Protocol UDP -LocalPort 47474 -Action Allow
   ```

If the Wi-Fi card cannot host and connect at the same time, use a cheap USB Wi-Fi adapter or any
ordinary Wi-Fi router.

## What guests do

Print this and stick it next to the hub:

1. Join the Wi-Fi network **EmberMesh** (password if any: ______).
2. Open the browser and type `http://10.42.0.1:8437/` (or scan the QR code).
   - Type `http://` exactly. Do not use a search box.
   - If a "sign in to network" window pops up, close it and use the real browser.
3. Type your name, choose your region, tap **Continue**, then **Start using EmberMesh**.
4. Chat in **Public**, or add people as contacts and message them privately.
5. Stay on this Wi-Fi with the page open. If you leave, messages wait for you on the mesh.

Guests with the EmberMesh **app** do not need the web page: the Android app joins through its
**Wi-Fi LAN / hotspot (UDP)** link, which emberd also runs by default; any app can also add a
**WebSocket (hub)** link to `ws://<hub address>:8437/ws`.

Guests using the browser page get the full messenger. They do **not** get microphone, camera,
Bluetooth or location on a plain `http://` page; for radios and sound-card modems a guest needs
the app or the single HTML file.

## The operator page (controlling the hub's own node)

The guests' pages are separate nodes. To manage the **hub's** node (its links, radios, messages and
settings) use the operator page:

- On the hub machine: `emberd --open` opens `http://localhost:8437/?remote=1&token=...` in the
  browser. `emberd --print-token` prints the token.
- The operator page is the normal app interface. **Links & radios** shows the hub card ("running
  · N connected"), the hub addresses with a QR button, and every radio link.
- The token gives full control. It travels unencrypted, so use the operator page on the hub
  machine itself or over a network you trust. `--new-token` makes a new one.
- On a headless box over SSH: `emberd --console` (or `emberd --attach` to a running service)
  gives a text console: `help`, `info`, `peers`, `say <text>`, `dm <node_id> <text>`,
  `sos`, `checkin ok`, `links`, `link add <json>`, `hub`, `unlock <passphrase>`, `lock`, `quit`.

### A hub with a storage passphrase

If the hub's storage is protected by a passphrase (Settings → Security on the operator page), it
starts **locked**. Then emberd:

1. unlocks with the passphrase in a file given by `--passphrase-file FILE` (first line), or
2. asks for it in the terminal (3 tries; press Enter to do it later), or
3. prints "storage is LOCKED" and waits.

While locked, guests can still load the page and join, and the operator page or the console
(`unlock <passphrase>`; the passphrase is visible as you type) can unlock it. The hub's own radio
links come back as soon as it is unlocked. Keep the passphrase file readable only by the account
that runs emberd.

## Running as a service (starts at boot)

### Linux / Raspberry Pi (systemd)

Building on a Pi (Pi OS bookworm), from the repository folder:

```sh
sudo apt install cmake g++
./build.sh desktop          # or by hand: cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Release && cmake --build build/desktop -j
```

(Linux and Raspberry Pi builds use the same code as the tested macOS build but were not built or
run in the latest test round. Report problems.)

Build the web app too (`./build.sh web`, or copy `dist/web` from another machine), then install;
the installer uses `dist/web` by default and refuses a web folder without the mesh core:

```sh
sudo apps/desktop/deploy/linux/install.sh dist/desktop/emberd dist/web
```

This installs `/usr/local/bin/emberd`, the web app in `/usr/local/share/embermesh/web`, a system user `ember` (in groups `dialout`
and `audio` for serial ports and sound cards), the data folder `/var/lib/embermesh`, and the
`emberd` service, and starts it.

```sh
systemctl status emberd                       # is it running?
journalctl -u emberd -f                       # live log
sudo -u ember /usr/local/bin/emberd --data-dir /var/lib/embermesh --print-token   # operator token
```

Then run the hotspot script once; both start at every boot.

### macOS (launchd)

See `apps/desktop/deploy/macos/README.md`: install the binary, copy the plist to
`~/Library/LaunchAgents/`, `launchctl bootstrap`. It runs while you are logged in; data is in
`~/Library/Application Support/EmberMesh`; logs in `~/Library/Logs/emberd.log`. Run emberd once
from Terminal first if you will use a sound card, to grant microphone permission.

### Windows (untested)

`apps/desktop/deploy/windows/install-task.ps1` registers a Task Scheduler task that starts emberd
at logon (or `-AtStartup`). `emberd-start.bat` can also simply be double-clicked.

## Configuration file

Options can live in `emberd.json` in the data folder (Linux service: `/var/lib/embermesh/emberd.json`).
Keys mirror the command-line flags with `_` instead of `-`:

```json
{
  "name": "camp-north-hub",
  "port": 8437,
  "hub": true,
  "udp": true,
  "captive_portal": false,
  "links": []
}
```

Links added from the operator page or the console are saved by the node and restored
automatically (also after an unlock), so most people never edit this file. Links given with
`--link` on the command line are **not** saved: they exist only while that flag is used.

## Optional: captive portal

With `"captive_portal": true` the hub answers the phones' "is there internet?" checks with a
redirect to itself, so some phones pop up the EmberMesh page on joining. Those checks go to port
80, so you also need DNS pointing every name at the hub and a port redirect from 80 to 8437
(see `apps/desktop/README.md`). The pop-up mini-browsers often cannot store data: tell people to
open the page in their real browser.

## Linking hubs together

Two hubs on any shared IP network (a long Ethernet cable, a Wi-Fi bridge) can be joined: on one
hub's operator page add **WebSocket (hub-to-hub)** to `ws://<other hub>:8437/ws` with link type
**Direct datagrams (Wi-Fi, hub, WebRTC)**. Only `ws://` is supported, not `wss://`.

## Placement and power

- **Wi-Fi range is the limit.** Put the hub in the middle of the group, raised (a shelf, a pole),
  in the open, away from metal walls and vehicles. Use the 2.4 GHz band for range.
- **One hub serves a building or a camp**, not a town. Use radio links between groups
  ([Radio gateway](radio-gateway.md), [Relays](relays-and-repeaters.md)).
- **Power:** a Raspberry Pi or a laptop on a battery and solar panel. Size it with **Tools →
  Power → Solar panel and battery sizing** and **Battery runtime** ([Power and field
  operation](power-and-field.md)). An unexpected power cut is safe: storage is crash-safe, links
  come back after a restart.
- **Security:** the hub uses plain HTTP. Anyone on the Wi-Fi can see the traffic going through
  it. Direct messages and private channels are end-to-end encrypted anyway; the Public channel is
  readable by design. Guests never receive the operator token.

## If it doesn't work

| Problem | Check |
|---|---|
| Guests cannot open the page at all | Are they on the hub's Wi-Fi (not mobile data)? Did they type `http://` and `:8437`? Is emberd running? On macOS/Windows, did the firewall block it? |
| Android hub: nobody can connect | Android 17+ local network permission not granted | Allow it; open EmberMesh again to get the prompt |
| iPhone hub stops | The app left the screen | Keep EmberMesh open, screen awake, on a charger |
| Page opens but says "EmberMesh could not start" | The web folder lacks the mesh core. Use `--web-root dist/web`. |
| Page works but guests see nobody | Look at **Links & radios** on the guest: the hub link should be **Connected**. Reload the page. |
| The hub address changed (new DHCP lease) | Guests' browsers treat a new address as a new site and create new identities. Give the hub a fixed address (the nmcli and hostapd recipes do). |
| Test from the hub itself | `curl http://localhost:8437/hub.json` should print `{"embermesh_hub":true,...}`. |
| Phone pop-up window closes or forgets everything | Close the pop-up; open the address in the real browser. |
