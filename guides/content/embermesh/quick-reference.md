---
id: embermesh/quick-reference
title: "EmberMesh 30: Glossary and Quick Reference Card"
category: embermesh
tags: [embermesh, reference, glossary, ports, addresses, link types, status, priority, phonetic alphabet, menu, emberd, cheat sheet]
summary: "One-page reference: where things are in the app, ports and default addresses, link types, link and message status, priorities, check-in and bulletin categories, emberd commands, the phonetic alphabet and a glossary."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 30
---

# Glossary and Quick Reference Card

Per-device guides: [Android](app-android.md), [iPhone/iPad](app-ios.md), [browser](app-web.md),
[computer](app-desktop.md). Every screen: [Screens tour](screens-tour.md). Every setting:
[Settings reference](settings-reference.md). Cables and adapters: [What you need](what-you-need.md).

## Where things are in the app

| I want to ... | Go to |
|---|---|
| Chat in Public, start a chat, create/join a channel | **Chats** (**New**: direct message, **Create channel**, **Join channel**) |
| Add, verify, ping people; see who is nearby | **People** → **Contacts** / **Nearby** / **Radar** / **Stats** |
| Send SOS, check in, see roll call | **SOS** |
| Read the survival library and these manuals | **Guides** (category "EmberMesh Manuals") |
| Add a radio or connection, run the hub, net plan, WebRTC | **More → Links & radios** |
| Notice board | **More → Bulletins** |
| Calculators, band plans, Morse send & read, station profiles | **More → Tools** |
| Send a typed message by flashlight or screen, read it with a camera | **Tools → Morse send & read** (**Send** / **Read by camera**) |
| This manual, Connect a radio, What you need, Read Morse code | **Instructions** (the **Help** tab on phones) |
| Notes, checklists, supply inventory | **More → Notes & lists** |
| Name, callsign, region, low power, relaying, position, clock, identity, passphrase, wipe, bundles | **More → Settings** |
| Airtime and relay statistics | **More → Mesh statistics** |
| Run a hub on this phone or computer | **More → Links & radios → Hub → Start hub** |
| Share the app, start on boot, battery, single-file copy, iOS permissions | **More → Settings → This device** |
| Rebuild EmberMesh from source | [Building from source](building-from-source.md) |

On wide screens the items under **More** are in the side bar.

## Ports and default addresses

| What | Value |
|---|---|
| Hub web app | `http://<hub address>:8437/` |
| Hub mesh WebSocket | `ws://<hub address>:8437/ws` |
| Hub downloads | `http://<hub address>:8437/download/` |
| Hub probe | `http://<hub address>:8437/hub.json` |
| Operator page (on the hub) | `http://localhost:8437/?remote=1&token=<token>` |
| Hub address with the Linux hotspot recipes | `10.42.0.1` (hostapd recipe also `ember.mesh`) |
| Hub address with macOS Internet Sharing | `192.168.2.1` |
| Hub address with Windows Mobile hotspot | usually `192.168.137.1` |
| iPhone hub on Personal Hotspot | usually `172.20.10.1` |
| Android hub downloads | `http://<phone address>:8437/download/EmberMesh.apk` |
| Wi-Fi / LAN discovery (UDP multicast) | `239.83.77.69` port `47474` |
| Phone TCP server (Android, iPhone) | port `47475` |
| Android Wi-Fi Direct | TCP port `47476` |
| Local network service name | `_embermesh._tcp` |
| Direwolf KISS over TCP | port `8001` |
| Meshtastic over TCP | port `4403` |
| rtl_tcp SDR server | port `1234` |
| Hamlib rigctld (CAT PTT) | port `4532` |
| EmberMesh Bluetooth service (phone to phone) | `8e3b0001-5d1f-4c6a-9b7e-0d2a6f1c4e10` |
| Nordic UART (Bluetooth TNCs, serial bridges) | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Apple peer-to-peer service name | `embermesh` |
| Contact cards, invites, pairing codes | text starting with `ember:` |

Serial speeds: Meshtastic, RNode and Reyax modules **115200**; most KISS TNCs **9600** or
**57600** (NinoTNC 57600).

## Link types

| Shown as | Type | For |
|---|---|---|
| Direct datagrams (Wi-Fi, hub, WebRTC) | `raw` | Wi-Fi multicast, hub WebSocket, WebRTC, Apple peer-to-peer, loopback |
| Serial / TCP / Bluetooth byte stream | `stream` | Cables, TCP, Bluetooth phone-to-phone and serial adapters, Ebyte/HC-12/SiK/XBee radios |
| KISS TNC (packet radio) | `kiss` | TNCs, Direwolf, radios with TNC, APRS |
| Sound-card modem (radio audio) | `audio` | Any radio by cable or acoustic coupling |
| Meshtastic device (LoRa) | `meshtastic` | Stock Meshtastic radios |
| RNode LoRa radio | `rnode` | RNode firmware boards |
| LoRa AT-command module (Reyax) | `lora_at` | REYAX RYLR998/498/896/406 |
| SDR receiver (rtl_tcp) | `rtl_tcp` | Listen only |

## Link card

| Shows | Meaning |
|---|---|
| **Connected** / **Connecting…** / **Down** / **Error** | State |
| **No encryption (amateur)** | Cleartext and callsign only on this link |
| **encryption ok** | Private traffic allowed |
| **station ID** | Callsign sent with every packet |
| **duty limit N %** and **Airtime used** bar | Legal airtime limit and how much is used |
| **half duplex**, **broadcast**, **reliable**, speed, **MTU** | Link properties |
| **TX ON AIR** | Transmitter keyed (sound-card links) |
| ▲ TX / ▼ RX | Data moving now |

## Message status and icons

| Status | Meaning |
|---|---|
| Queued | Waiting for a link |
| Sent | Handed to a link |
| Relayed | Another node passed it on |
| Delivered | Recipient's device confirmed (direct messages) |
| Read | Recipient opened it |
| Failed | Gave up; tap → **Retry** |

Lock icon = end-to-end encrypted. Globe icon / **cleartext** tag = readable by anyone. **⚠
unverified sender** = the sender's key could not be checked (strangers' SOS alerts are grouped below
verified ones and never notify). **direct** / **N hops** = how far it travelled.

## Priorities

**Low**, **Normal** (default), **High**, **Emergency** (asks for confirmation). Relays send in this
order: SOS and emergency → acknowledgements → direct messages → channel messages and check-ins →
announcements, positions, bulletins, net plans → low priority → background exchange.

## Check-in and bulletin categories

- Check-ins: **I'm OK**, **Need help**, **Injured**, **Moving**, **Sheltering**.
- Bulletins: **Water**, **Food**, **Medical**, **Shelter**, **Hazard**, **Trade**, **Missing**,
  **Transport**, **Info**. Keep for 1 hour to 30 days.

## How long things are kept

Announcements 7 days; positions and SOS 1 day; check-ins and net plans 2 days; pings 10 minutes;
other messages as set in **Carry undelivered messages for** (default 3 days, 1-30).
The SOS beacon repeats about every 60 seconds. Your net plan is re-announced about every 30
minutes.

## Settings defaults

| Setting (Settings → Mesh & relaying) | Default | Range |
|---|---|---|
| Relay messages for others | on | |
| Carry undelivered messages for | 3 days | 1-30 |
| Announce this node every | 1800 s | 60-86400 s, or 0 = never |
| Send read receipts | on | |
| Default hop limit (Advanced) | 7 | 1-15 |

Other defaults: region United States / Canada, position sharing Manual, units metric, theme dark,
low-power mode off.

## emberd (computer hub) essentials

| Command | Does |
|---|---|
| `emberd` | Node + hub on port 8437 + Wi-Fi/LAN link |
| `emberd --web-root dist/web` | Serve the full web app |
| `emberd --open` | Open the operator page |
| `emberd --print-token` | Show the operator token |
| `emberd --console` / `--attach` | Text console (`help`, `peers`, `say`, `dm`, `sos`, `checkin`, `links`, `hub`, `quit`) |
| `emberd --kiss-tcp 127.0.0.1:8001` | Direwolf |
| `emberd --kiss-serial /dev/ttyUSB0:9600` | Hardware TNC |
| `emberd --link '<json>'` | Any link at start |
| `emberd --list-serial` / `--list-audio` | List devices |
| `emberd --no-hub` / `--no-udp` | Relay only / no Wi-Fi multicast |
| `emberd --callsign QX0ABC` | Amateur callsign for radio links |
| `emberd --passphrase-file FILE` | Unlock a passphrase-protected store at start (console: `unlock <passphrase>`) |
| `--link` | Not saved; add links from the operator page to keep them |

## Phonetic alphabet (ITU / NATO)

A Alfa · B Bravo · C Charlie · D Delta · E Echo · F Foxtrot · G Golf · H Hotel · I India ·
J Juliett · K Kilo · L Lima · M Mike · N November · O Oscar · P Papa · Q Quebec · R Romeo ·
S Sierra · T Tango · U Uniform · V Victor · W Whiskey · X X-ray · Y Yankee · Z Zulu.

**Tools → Signalling → NATO phonetic spelling** spells any text; see also the guide
[NATO phonetic alphabet](../radio/nato-phonetic-alphabet.md).

## Glossary

| Term | Meaning |
|---|---|
| **Node** | Any device running EmberMesh |
| **Link** | One way a node reaches others (a Bluetooth connection, a radio, a hub ...) |
| **Endpoint / connection** | The hardware or network pipe a link uses (USB serial, Bluetooth, sound card, TCP ...) |
| **Hop** | One relay step |
| **Hop limit (TTL)** | How many times a message may be relayed |
| **Store-carry-forward** | Keeping messages and handing them over when another node is met |
| **Bundle** | A file or QR series of stored messages, for carrying by hand |
| **Hub** | A computer serving the app and the mesh over Wi-Fi (emberd) |
| **Gateway** | A node with both a local link and a radio link |
| **Relay** | A node placed to extend range, often unattended |
| **Net / net control** | A group on agreed frequencies / the person coordinating it |
| **Net plan** | The announced list of frequencies, modes, tones and schedules |
| **Node ID** | Your 16-character identity number |
| **Fingerprint words** | Eight words that let people verify your identity by voice |
| **Unverified sender** | A sender whose key your device cannot check; anyone can invent a name |
| **Random-key channel** | Encrypted channel joined only by invite; the recommended kind |
| **Contact card** | Your identity as an `ember:` text or QR code |
| **Public channel** | The open channel for everyone; cleartext, signed |
| **Open / private channel** | Named channel without / with a passphrase (encrypted) |
| **Interop** | Plain text exchanged with other systems (APRS, Meshtastic, Morse, RTTY ...) |
| **Callsign / SSID** | Amateur licence identifier; `-7` style suffix for several stations |
| **Legal mode / no encryption** | Amateur links: cleartext with callsign only |
| **KISS / TNC** | Packet-radio modem protocol / device |
| **AX.25** | The packet-radio frame format |
| **APRS** | Amateur position and messaging system on 144.390 / 144.800 MHz |
| **AFSK 1200** | Standard 1200 bit/s packet modem sound |
| **Robust (62 etc.)** | EmberMesh's slow, error-corrected modem for weak signals and acoustic coupling |
| **VOX / PTT** | Voice-operated transmit / push-to-talk |
| **TX delay** | Pause between keying the transmitter and sending data |
| **Squelch / CTCSS** | Mutes the speaker without signal / sub-audible tone filter |
| **LoRa, SF, BW, CR** | Long-range radio; spreading factor, bandwidth, coding rate |
| **Duty cycle** | Share of time a transmitter may be on air (e.g. 10 % per hour) |
| **RSSI / SNR** | Received signal strength (dBm) / signal-to-noise ratio (dB) |
| **dBFS** | Audio level relative to the maximum the sound card can handle |
| **SDR** | Software-defined radio receiver (e.g. RTL-SDR dongle) |
| **Duress passphrase** | A second passphrase that, when entered, erases your identity and content but keeps your name, settings and connections, and looks like a normal unlock |
| **Forward secrecy** | Protection of past messages if keys are later stolen (EmberMesh does not have it) |
