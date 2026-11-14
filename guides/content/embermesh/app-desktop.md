---
id: embermesh/app-desktop
title: "EmberMesh 6: Operating the Computer Hub (emberd)"
category: embermesh
tags: [embermesh, desktop, emberd, computer, raspberry pi, linux, macos, windows, operator page, console, hub, gateway, service, serial, sound card, rtl_tcp]
summary: "Complete guide to emberd, the EmberMesh program for computers and Raspberry Pi class boxes: getting and starting it, the operator page and the text console, every command-line option that matters, connections a computer supports and does not, radios and sound cards, running as a service, a passphrase-protected store, backup, updating and troubleshooting."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 6
---

# Operating the Computer Hub (emberd)

`emberd` is one program for macOS, Linux (including Raspberry Pi) and Windows. It is at the same
time a **mesh node**, a **hub** that serves the web app to every phone on its Wi-Fi (port 8437), and
a **radio gateway**. It has no window of its own: you use it through a web page (the **operator
page**) or a text **console**.

> **What is proven:** built and tested on macOS with the real core, including tests
> with several emberd processes over every network, serial and virtual-audio connection: relaying,
> store-and-forward, a radio gateway over a virtual KISS link and a virtual audio cable, 100 hub
> guests, restart persistence and a locked store. **Not proven:** Linux and Raspberry Pi builds (same
> code, not compiled in the latest round), Windows (never compiled), and any real radio, TNC, sound
> card or SDR dongle.

## 1. Getting emberd

- **Copy it:** `dist/desktop/emberd` (with `guides.pack` and the `deploy/` scripts next to it) plus
  the web app folder `dist/web`, from a USB stick or another machine. A program built for one kind of
  computer (macOS, Linux on a PC, Linux on a Raspberry Pi, Windows) does not run on another.
- **Build it** from the repository: `./build.sh desktop web` (see
  [Building from source](building-from-source.md)). On a Raspberry Pi: `sudo apt install cmake g++`
  first.

emberd needs `dist/web` (the interface **plus** the WebAssembly core) to serve guests. It finds it
by itself when it sits in the repository or next to the program, warns at start if it can only find
the plain interface without the core, and `--web-root DIR` names it explicitly.

## 2. Starting it

```sh
./emberd                         # node + hub on port 8437 + Wi-Fi/LAN multicast link
./emberd --callsign QX0ABC        # also set your amateur callsign (radio links need it)
./emberd --open                  # and open the operator page in the browser
```

It prints the hub address for each network interface (for example `http://192.168.4.1:8437/`), a QR
code of the first one (`--no-qr` hides it), and the operator address
`http://localhost:8437/?remote=1&token=...`.

**First run:** a new node is named after the computer; set your name on the operator page (**Settings
→ Profile**) or with `--name`.

### The options that matter

| Option | Does |
|---|---|
| `--name NAME` | This node's display name |
| `--callsign CALL` | Amateur callsign for the profile (copied into amateur radio links) |
| `--web-root DIR` | Web app folder served at `/` |
| `--port N`, `--bind ADDR` | Hub port (default 8437); bind address (default all; `127.0.0.1` = this computer only) |
| `--no-hub` | No web hub (pure radio gateway / relay) |
| `--no-udp` | No Wi-Fi/LAN multicast link (`239.83.77.69:47474`) |
| `--passphrase-file FILE` | Unlock a passphrase-protected store at start |
| `--kiss-tcp HOST:PORT`, `--kiss-serial PORT[:BAUD]` | Quick KISS TNC links (Direwolf: `127.0.0.1:8001`) |
| `--radio "preset=...,in=...,out=...,ptt=..."` | Quick sound-card radio link with a **Radio setup** preset, e.g. `--radio "preset=handheld_vox,in=USB Audio,out=USB Audio"` (see [Baofeng UV-5R](baofeng-uv5r.md)) |
| `--link '<json>'` | Any link **for this run only** (not saved) |
| `--console`, `--attach [URL]` | Text console in this process / against a running emberd |
| `--open`, `--print-token`, `--new-token` | Open the operator page / print / renew the admin token |
| `--list-serial`, `--list-audio` | List serial ports and sound devices, then exit |
| `--data-dir DIR`, `--config FILE` | Where state lives / a JSON config file |
| `--guides FILE`, `--download-dir DIR` | Guide library to load / files offered at `/download/` |
| `--log-level`, `--log-file`, `--in-memory` | Logging; testing without saving anything |

Data folder by default: macOS `~/Library/Application Support/EmberMesh`; Linux
`~/.local/share/embermesh`; Windows `%APPDATA%\EmberMesh`; the Linux service `/var/lib/embermesh`.

## 3. The operator page

Open `http://localhost:8437/?remote=1&token=...` (or `emberd --open`). It is the normal EmberMesh
interface, but it controls **the computer's own node**: its links and radios, its messages and its
settings. Guests' pages are separate nodes.

- The **token** is admin access. It travels unencrypted: use the operator page on the computer
  itself, or over a network you trust. `--new-token` replaces it.
- On the operator page, **Links & radios** shows the **Hub** card ("running · N connected") with the
  hub addresses and QR codes, and every radio link.

## 4. The text console

`emberd --console` (or `emberd --attach` against a running service, also over SSH):

| Command | Does |
|---|---|
| `help`, `info` | Commands; node id, name, fingerprint |
| `peers`, `contacts`, `channels`, `convs` | Lists |
| `read <conv> [n]`, `send <conv> <text>` | Read / write a conversation (`dm:<node_id>` or `ch:<channel_id>`) |
| `say <text>`, `dm <node_id> <text>` | Public channel / direct message |
| `sos [text]`, `sos stop`, `sos status`, `sos list` | Emergency beacon |
| `checkin <ok/need_help/injured/moving/sheltering> [note]` | Check-in |
| `links`, `link add <json>`, `link rm <id>` | Links (saved) |
| `hub` | Hub status and addresses |
| `unlock <passphrase>`, `lock` | Unlock / lock a protected store (the passphrase is visible as you type) |
| `morse listen [device]`, `morse status`, `morse stop` | Read Morse from the sound input (or `morse listen file:<path>` for raw audio); `stop` prints the transcript |
| `name NEW` | Rename this node |
| `call <method> [json]`, `quit` | Raw call; exit |

## 5. Connections on a computer

On the operator page: **Links & radios → Add connection**:

| Entry | Use it for | Protocol |
|---|---|---|
| **LAN multicast (Wi-Fi / Ethernet)** | Every EmberMesh device on the same network; on by default | Direct datagrams |
| **Hub (Wi-Fi guests)** | The hub itself (normally started automatically) | Direct datagrams |
| **WebSocket (hub-to-hub)** | Joining another hub: `ws://<other hub>:8437/ws` (no `wss://`) | Direct datagrams |
| **TCP client** | Direwolf (8001), Meshtastic on Wi-Fi (4403), another node's TCP server | KISS, Meshtastic, RNode, Reyax or byte stream |
| **TCP server** | Letting other programs or nodes connect | Byte stream or KISS |
| **Serial port / USB TNC** | TNCs, radios with a TNC, Meshtastic, RNode, Reyax, transparent radio modules, cables between computers (**Serial port**, **Baud rate**, **PTT keying line**) | KISS, Meshtastic, RNode, Reyax or byte stream |
| **Sound card (radio audio)** | Radios through a USB sound card or interface (fields below) | Sound-card modem |
| **Audio pipe (virtual sound card)** | Linux/macOS: raw audio through named pipes, to connect Direwolf, sox, arecord/aplay or another emberd | Sound-card modem |
| **RTL-SDR (rtl_tcp server)** | Listening through an RTL-SDR (`rtl_tcp` on port 1234) | SDR receiver |
| **Loopback (testing)** | Testing | |

**Not available on a computer:** Bluetooth of any kind (use a USB TNC or a phone), WebRTC, Apple
peer-to-peer, Wi-Fi Direct, `wss://` or `https://`.

### Sound card fields

**Input (from radio)**, **Output (to radio)** (names from `emberd --list-audio`; `none` = transmit
or receive only), **Sample rate**, **Transmit keying (PTT)**: **VOX (radio keys itself on audio)**,
**Serial port RTS line**, **Serial port DTR line** (with **PTT serial port**, e.g. `/dev/ttyUSB0`,
`/dev/cu.usbserial-1410`, `COM3`), **USB sound-card GPIO (CM108/CM119 HID, AIOC)** (with **PTT HID
device** and **PTT GPIO pin**, usually 3), **CAT via Hamlib rigctld** (port 4532, can set the radio's
frequency and mode at start), **None (receive only)**. Advanced: **Invert PTT line**, **Receive
gain**, **Transmit gain**, **PTT tail**, **Device open timeout** (8 s).

On macOS the first opening of a microphone input asks for permission. If nobody answers, the link
goes to **Error** after 8 s with an explanation: allow microphone access for the program running
emberd (System Settings → Privacy & Security → Microphone), or run emberd once from Terminal and
allow it.

Recipes for Direwolf, TNCs, Meshtastic, sound cards and SDR: [Radio gateway](radio-gateway.md),
[KISS TNCs](kiss-tnc-packet.md), [Handheld radios](handheld-audio-modem.md), [LoRa](lora.md),
[SDR](sdr-listening.md). Hardware lists: [What you need](what-you-need.md).

## 6. Permissions and system settings

- **Linux:** the user running emberd must be in `dialout` (serial ports) and `audio` (sound cards).
  The service installer does it. Use `/dev/ttyUSB*` or `/dev/ttyACM*`.
- **macOS:** allow incoming connections when the firewall asks (System Settings → Network →
  Firewall → Options), or guests cannot reach port 8437. Use `/dev/cu.*` serial ports, not
  `/dev/tty.*`.
- **Windows (untested):** allow emberd on **Private** networks; ports are `COM3` etc.

## 7. Making the Wi-Fi network

Linux `hotspot-nmcli.sh` or `setup-hostapd.sh` (guests use `http://10.42.0.1:8437/`), macOS Internet
Sharing (`http://192.168.2.1:8437/`), Windows Mobile hotspot (usually `http://192.168.137.1:8437/`).
Step by step: [Setting up a hub](hub-setup.md).

## 8. Running unattended

- **Linux / Raspberry Pi:** `sudo apps/desktop/deploy/linux/install.sh dist/desktop/emberd dist/web`
  installs a `systemd` service (user `ember`, data in `/var/lib/embermesh`).
  `systemctl status emberd`, `journalctl -u emberd -f`;
  operator token: `sudo -u ember /usr/local/bin/emberd --data-dir /var/lib/embermesh --print-token`.
- **macOS:** a launchd agent (`apps/desktop/deploy/macos/README.md`); runs while you are logged in.
- **Windows (untested):** `apps/desktop/deploy/windows/install-task.ps1` (Task Scheduler).

Links added on the operator page or with `link add` are saved and reopened at every start. Links
from `--link` are not saved.

## 9. Security on a hub

- **Settings → Security** on the operator page sets a storage passphrase. Then emberd starts
  **locked with no radio links**: it reads `--passphrase-file`, or asks in the terminal (3 tries),
  or waits for `unlock` in the console or the operator page. Guests can still join while it is locked.
- The hub uses plain HTTP: anyone on its Wi-Fi can see the traffic. Direct messages and private
  channels are end-to-end encrypted anyway.
- Guests are rate-limited; a misbehaving guest is disconnected.

## 10. Backup, moving and updating

- **Back up** on the operator page: **Settings → Identity → Back up identity** (**Save file** writes
  to the data folder's `saved/`). Or stop emberd and copy the whole data folder.
- **Import** files (identity backups, bundles): put them in the data folder's `inbox/`; **Choose file**
  takes the newest matching one.
- **Update:** stop emberd, replace the program and `dist/web`, start it again. The data folder is kept.

## 11. Computer troubleshooting

| Problem | Fix |
|---|---|
| Guests get "EmberMesh could not start" | Serve `dist/web` (`--web-root dist/web`) |
| Guests cannot reach the page | Firewall; wrong network; check `curl http://localhost:8437/hub.json` |
| Radio links missing after a restart | The store is locked: `--passphrase-file`, console `unlock` |
| Audio link **Error** "did not open within 8 s" | macOS microphone permission (section 5) |
| Serial port permission denied (Linux) | Add the user to `dialout` |
| Port not found (macOS) | Use `/dev/cu.*`; `emberd --list-serial` |
| A link given with `--link` disappeared | `--link` is for one run; add it on the operator page |
