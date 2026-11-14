---
id: embermesh/settings-reference
title: "EmberMesh 10: Settings Reference"
category: embermesh
tags: [embermesh, settings, reference, defaults, ranges, configuration, link settings, kiss, audio, meshtastic, rnode, lora, stream, rtl_tcp]
summary: "Every EmberMesh setting with its default, allowed range, what it changes and when to change it: the Settings screen, the add-connection settings of every link type, and the main connection (hardware) settings per platform."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 10
---

# Settings Reference

Defaults and ranges below are the ones the mesh core enforces (the app's forms use the same
values). Settings are saved immediately; a value outside its range is refused with a message.

## Settings screen

### Accessibility

**Settings → Accessibility**: **Simple mode** (off), **Text size** (1 = normal; 0.85-1.6, presets
Normal / Larger / Largest), **High contrast** (Follow system), **Animation** (Follow system; Full /
Reduced / Off), **Start-up animation** (Full; Short / Off), **Extra reading space** (off), **Vibrate on
touch** (on, where the device can vibrate), **Keyboard shortcuts** (on). Details: [Simple mode and accessibility](accessibility.md).

### Guides

**Smart search** (on, when the library includes the search model): also finds guides by meaning. See
[Finding guides](library-search.md).

### Profile

| Setting | Default | Limits | What it changes / when to change it |
|---|---|---|---|
| **Name** | set at first run | up to 40 characters | Shown to everyone you message and in announcements. Use something people recognise |
| **Callsign (licensed amateurs only)** | empty | up to 12 characters, letters and digits, optional `-SSID` | Copied into every amateur KISS and sound-card link; required before they transmit. Leave empty without a licence |
| **Status** | empty | up to 80 characters | A short public line ("At the school shelter, have water"), sent in announcements |

Tap **Save profile** after changing these.

### Connections

| Setting | Default | What it changes |
|---|---|---|
| **Nearby devices** | on after first-run setup (asked there) | Finds EmberMesh devices on the same Wi-Fi or local network and connects automatically. Others on the network can see your name and EmberMesh identity. Not in the browser app (a web page cannot search the network) |
| **Internet relay** | off | Reaches contacts (and random-key channels you allow) through relay servers on the internet when they cannot be reached directly. The relay can see your IP address and when you are online. Not yet in the computer program (emberd) |
| **Relays** | one public relay on, two listed but off | Which relay servers are used (at most 8). **Add your own relay or hub**: **Relay address** (`wss://...`), user name and password only if needed, **Add relay**. Remove one of yours with **Remove this relay?** |

Bluetooth and radio links are never automatic: add them under **Links & radios**. Full explanation
and what each choice reveals: [Connections](connections.md).

### Region & display

| Setting | Default | Choices | What it changes |
|---|---|---|---|
| **Region** | United States / Canada | United States / Canada (ITU 2), Europe (ITU 1), United Kingdom, Australia / NZ, Japan, India, Brazil / South America, Southern Africa, Other / unknown | Which band plans the tools show first, and the starting region of new RNode and Reyax LoRa links (see [LoRa](lora.md)) |
| **Units** | Metric | Metric, Imperial | Distances in the app and tools |
| **Theme** | Dark | Dark, Light, Night (red) | Dark saves battery on OLED screens; Night uses only dim red for night vision |
| **Low-power mode (no animation, fewer refreshes)** | off | on/off | Fewer screen updates; the node announces itself at most once an hour and sends fewer stored messages per exchange |

### Mesh & relaying

| Setting | Default | Range | What it changes / when to change it |
|---|---|---|---|
| **Pass on messages for others** | on | on/off | Your device forwards and carries mesh messages for other people. Off only to save battery in an emergency: relaying is what makes the mesh work. (This is not the internet relay; that is under Connections) |
| **Carry undelivered messages for** | 3 days | 1–30 days | How long your device keeps messages for people out of reach. Longer for sneakernet and sparse meshes |
| **Announce this node every** | 1800 s | 60–86400 s, or 0 = never automatically | Longer saves airtime and battery; shorter helps others find you |
| **Send read receipts** | on | on/off | Tells the sender of a direct message you read it. Off saves a little airtime and privacy |
| **Default hop limit** (Advanced) | 7 | 1–15 | How many times a message may be relayed. Raise on large meshes, lower on dense busy ones |

### Position

| Setting | Default | Choices | What it changes |
|---|---|---|---|
| **Position sharing** | Manual — only when I share it | Off — never share, Manual — only when I share it, Automatic — include in announcements | Automatic puts your position into every announcement, readable by anyone |
| **Update from GPS** / **Keep updating** | — | | Take one fix / follow continuously (more battery) |
| **Enter position manually** | — | latitude −90…90, longitude −180…180 (decimal degrees) | When there is no GPS |

### Clock

**Set from this device** sets the node time from the device's own clock. A device with a sane clock
is never moved by the mesh; set this only after correcting a wrong clock.

### Identity and security

| Item | Rule |
|---|---|
| **Back up identity** passphrase | at least 8 characters |
| Storage passphrase (**Set passphrase**) | at least 6 characters; empty removes it |
| **Duress passphrase** | at least 4 characters, different from the real one; only after a storage passphrase |
| **Panic wipe** | type `ERASE`, hold 3 s |

See [Security](security.md).

### Sneakernet bundles

**Export bundle**: **What to include** (Everything I carry / Only messages for one person) and
**Size limit** (No limit, 2 KB (one QR code), 16 KB, 256 KB, 4 MB). See [Sneakernet](sneakernet.md).

### This device

Platform actions, only on platforms that offer them:

| Platform | Actions |
|---|---|
| Android | **Share this app**, **Save the app as a file**, **Start when the phone starts** / **Don't start when the phone starts**, **Keep running with the screen off** / **Battery optimisation: exempt** |
| iPhone / iPad | **iOS permissions** (opens the iOS Settings page for EmberMesh) |
| Browser | **Install as app** (when offered), **Download single-file copy to share** (when opened from a web address) |
| Computer (operator page) | none |

## Connection (hardware) settings by platform

These are on the **Connection** card when adding a link, and under **Settings** on its card.
Changing one closes and reopens the connection.

| Connection | Main settings |
|---|---|
| Wi-Fi LAN / hotspot (UDP), Android | **Network** (All networks), Advanced: **Multicast group** 239.83.77.69, **Port** 47474, **Multicast TTL** 1, **Extra peers**, **Max datagram** 1472 |
| LAN multicast, computer | **Multicast group**, **Port**, **Interface address**, **Multicast TTL**, **Extra unicast peers** |
| Bluetooth phone to phone | Android: **Let nearby phones connect to me**, **Connect to nearby phones**, **Max simultaneous peers**; iPhone: **Name shown to nearby phones**, **Let other phones find me**, **Look for other phones**, **Maximum phones at once** (7) |
| TCP client / connection | **Host** / **Address**, **Port** (KISS/Direwolf 8001, Meshtastic 4403, rtl_tcp 1234, EmberMesh 47475) |
| TCP server / Local network listener | **Port** 47475, announce on the network |
| WebSocket / Hub | **Hub URL** / **Hub address** / **Hub WebSocket URL**, e.g. `ws://10.42.0.1:8437/ws` |
| USB serial / Serial port | **Device** or **Serial port**, **Baud rate** (default 9600), Advanced: data bits, parity, stop bits, flow control, DTR and RTS lines, **PTT keying line** (None / RTS / DTR), invert |
| Bluetooth LE device | **Device** (scan), **Device type** (Android: Serial (Nordic UART / custom UUIDs), Meshtastic radio; iPhone: Nordic UART, BLE KISS service (Mobilinkd), Meshtastic radio, Custom UUIDs; browser: Auto, EmberMesh phone, Nordic UART, Custom), custom UUIDs (Advanced) |
| Sound card | Input and output device, sample rate, transmit level (Android %, iPhone 80 %, browser 70 %, computer **Transmit gain** dB), PTT (see [Handheld radios](handheld-audio-modem.md)) |
| SDR (computer) | **rtl_tcp host**, **Port** 1234; tuning fields only with **Tune from these settings** |

## Link (protocol) settings

These are on the **Protocol** card ("What is on the other end?"). Items marked (Advanced) are folded
under **Advanced settings**. RNode and Reyax start from the region in Settings. For sound-card
links, **Radio setup** chooses a preset that fills the modem, lead-in, tail, level and
channel-access settings (table in [Handheld radios](handheld-audio-modem.md)); any field you set
yourself wins. The link list shows the preset and the effective settings in use.

### Direct datagrams (Wi-Fi, hub, WebRTC) (`raw`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Connection kind** | Wi-Fi / LAN (UDP multicast) | Wi-Fi / LAN (UDP multicast), Hub WebSocket (browser to hub), WebRTC data channel (paired by QR), Apple MultipeerConnectivity, Same-device loopback (testing), Custom (set the values below) | Pick what the connection runs over; the other settings default to sensible values for it. |
| **Largest frame** (Advanced) | 0 bytes | 0–8192 bytes | Biggest frame sent in one datagram. 0 = preset default. |
| **Speed estimate** (Advanced) | 0 bit/s | 0–1000000000 bit/s | Used for scheduling only. 0 = preset default. |
| **Reaches everyone** (Advanced) | Preset default | Preset default, Yes, No | Whether one datagram reaches all peers (multicast, hub bus) or only one (WebRTC). |
| **Reliable delivery** (Advanced) | Preset default | Preset default, Yes, No | Whether the medium itself guarantees delivery and order (WebSocket, reliable WebRTC). |
| **Routing cost** (Advanced) | 0 | 0–255 | Lower is preferred when several links reach a node. 0 = preset default. |
| **Queue size** (Advanced) | 64 frames | 1–1024 frames | Frames held while the connection is down before refusing more. |

### Serial / TCP / Bluetooth byte stream (`stream`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Device / cable** | USB or serial cable between two devices | USB or serial cable between two devices, TCP connection (also Wi-Fi Direct), Bluetooth LE (UART adapter or phone-to-phone), Classic Bluetooth serial (SPP), Ebyte E22 / E32 LoRa module (transparent mode), HC-12 433 MHz serial radio, SiK / 3DR telemetry radio, XBee in transparent mode, Custom (set the values below) | What the byte stream runs over. Both ends should use the same choice. |
| **Largest frame** (Advanced) | 0 bytes | 0–4096 bytes | Biggest mesh frame per packet. 0 = preset default. |
| **Speed estimate** (Advanced) | 0 bit/s | 0–100000000 bit/s | Real throughput, used for scheduling. 0 = preset default. |
| **Write pacing** (Advanced) | 0 bit/s | 0–10000000 bit/s | Slow writes to this rate so cheap radio modems are not overrun (set it to the module's air data rate). 0 = preset default; the cable/TCP/Bluetooth presets do not pace. |
| **Gap between packets** (Advanced) | -1 ms | -1–10000 ms | Pause after each packet and after receiving, so a half-duplex radio can switch over. -1 = preset default. |
| **Error correction** (Advanced) | Preset default | Preset default, Yes, No | Add Reed-Solomon error correction (recommended for radio modules, useless on cables). |
| **Correction strength** (Advanced) | 16 | 2–64 | Parity bytes per block: corrects half as many byte errors. Higher = more robust, more overhead. |
| **Half duplex** (Advanced) | Preset default | Preset default, Yes, No | The medium cannot send and receive at the same time. |
| **Reaches everyone** (Advanced) | Preset default | Preset default, Yes, No | One transmission reaches every listener (radio modules). |
| **Write size limit** (Advanced) | 0 bytes | 0–4096 bytes | Split writes into pieces of at most this many bytes (for endpoints with small buffers). 0 = no limit. |
| **Amateur radio band** | off |  | Turn on if the radio transmits on an amateur band (e.g. 433 MHz in the Americas): traffic is then sent unencrypted and must carry your callsign. |
| **Duty-cycle limit** (Advanced) | 1000 ‰ | 1–1000 ‰ | Legal transmit time limit in thousandths (1000 = none, 100 = 10 %, 10 = 1 %). |
| **Routing cost** (Advanced) | 0 | 0–255 | Lower is preferred. 0 = preset default. |
| **Queue size** (Advanced) | 32 frames | 1–512 frames | Frames waiting to be sent before new ones are refused. |

### KISS TNC (packet radio) (`kiss`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Your callsign** | N0CALL |  | Your amateur radio callsign with optional -SSID (e.g. N0CALL-7). Sent with every packet as the law requires. |
| **Band / licence** | Amateur radio (callsign, no encryption) | Amateur radio (callsign, no encryption), Licence-free or emergency use (encryption allowed) | Amateur bands forbid encryption and need your callsign on every packet. Choose licence-free only if the radio works on a band where that is legal. |
| **Packet format** | AX.25 (standard packet radio, works with APRS gear) | AX.25 (standard packet radio, works with APRS gear), Raw (LoRa KISS modems; no AX.25 header) | Keep AX.25 for normal packet radio TNCs. Raw saves 16 bytes per packet on licence-free LoRa KISS modems. |
| **TNC port** | 0 | 0–15 | Radio port on multi-port TNCs (0 for almost all devices). |
| **Radio data rate** | 1200 bit/s | 50–1000000 bit/s | Speed of the TNC's modem: 1200 for VHF packet/APRS, 9600 for fast UHF packet, 300 for HF. |
| **Digipeater path** (Advanced) | (empty) |  | Comma-separated digipeaters for mesh packets (normally empty: the mesh relays by itself). |
| **APRS messaging** | on |  | Show APRS messages, positions and status heard on the channel, and allow sending APRS messages. |
| **APRS path** (Advanced) | WIDE1-1,WIDE2-1 |  | Digipeater path for APRS messages you send. |
| **Acknowledge APRS messages** (Advanced) | on |  | Automatically confirm APRS messages addressed to your callsign. |
| **Configure the TNC** | on |  | Send the timing settings below to the TNC when it connects. Turn off for TNCs that ignore or dislike them. |
| **TX delay** (Advanced) | 300 ms | 0–2550 ms | Time the transmitter is keyed before data starts. Increase if the start of packets is lost. |
| **Persistence** (Advanced) | 63 | 0–255 | Chance (out of 255) of transmitting at each free slot. 63 suits busy channels. |
| **Slot time** (Advanced) | 100 ms | 0–2550 ms | Wait between channel checks. |
| **TX tail** (Advanced) | 30 ms | 0–2550 ms | Time the transmitter stays keyed after data. |
| **Full duplex** (Advanced) | off |  | Only for full-duplex radios/satellite setups. |
| **Enter KISS mode** (Advanced) | Already in KISS mode (Direwolf, Mobilinkd, most) | Already in KISS mode (Direwolf, Mobilinkd, most), Send KISS ON + RESTART (TNC-2, Kantronics, Kenwood), Send custom text (below) | Some TNCs and radios start in a text command mode and must be switched to KISS first. |
| **Custom init text** (Advanced) | (empty) |  | Text sent before KISS starts; use \r for Enter (e.g. TC 0\r for some Kenwood radios). |
| **Leave KISS on close** (Advanced) | off |  | Send the 'exit KISS' command when this link is removed, returning the TNC to its command mode. |
| **Largest frame** (Advanced) | 256 bytes | 32–2048 bytes | Largest mesh frame per packet. 256 is the AX.25 standard; some TNCs accept more. |
| **Duty-cycle limit** (Advanced) | 1000 ‰ | 1–1000 ‰ | Legal transmit time limit in thousandths (1000 = none). |
| **Routing cost** (Advanced) | 0 | 0–255 | Lower is preferred. 0 = automatic from the data rate. |
| **Queue size** (Advanced) | 32 frames | 1–256 frames | Frames waiting to be sent before new ones are refused. |

### Sound-card modem (radio audio) (`audio`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Radio setup** | Custom (set everything yourself) | Custom (set everything yourself), Handheld radio keyed by its own VOX (audio cable), Handheld radio, PTT keyed by a serial line or interface (AIOC, Digirig), No cable: device speaker/microphone held to the radio, PTT by hand, Mobile / base radio with a data port or USB sound card | How the device is connected to the radio. A preset picks the modem, lead-in, tail, level and channel-access settings; any field you set yourself wins. |
| **Modem** | AFSK 1200 - VHF/UHF packet, works with APRS gear | AFSK 1200 - VHF/UHF packet, works with APRS gear, AFSK 300 - HF packet, PSK 2400 - fast, audio cable only, PSK 4800 - fastest, clean audio cable only, Robust 31 - weakest signals, very slow, Robust 62 - phone held to a radio, weak signals, Robust 125 - error-corrected, Robust 250 - error-corrected, good audio, DTMF touch tones - digits only, Morse code (CW) - text only, PSK31 - text only, RTTY 45 baud - text only | How data is turned into sound. Everyone on the frequency must use the same modem. AFSK 1200 is standard packet radio; Robust works through a phone held against a radio. |
| **Your callsign** | N0CALL |  | Your amateur radio callsign with optional -SSID (e.g. N0CALL-7). Required on amateur bands. |
| **Band / licence** | Amateur radio (callsign, no encryption) | Amateur radio (callsign, no encryption), Licence-free or emergency use (encryption allowed) | Amateur bands forbid encryption and need your callsign on every packet. |
| **APRS messaging** | on |  | With AFSK modems: show APRS messages, positions and status heard on the channel and allow sending APRS messages. |
| **Modem fine-tuning** (Advanced) | (empty) |  | Optional key=value pairs separated by commas, e.g. wpm=15 for CW or mark_hz=1600 for AFSK. |
| **Digipeater path** (Advanced) | (empty) |  | Comma-separated digipeaters for mesh packets (normally empty: the mesh relays by itself). |
| **APRS path** (Advanced) | WIDE1-1,WIDE2-1 |  | Digipeater path for APRS messages you send. |
| **Acknowledge APRS messages** (Advanced) | on |  | Automatically confirm APRS messages addressed to your callsign. |
| **TX delay** | 300 ms | 0–5000 ms | Time between keying the radio and the data. Increase if the far end misses the start of packets (VOX and cheap handhelds need 300-600 ms). |
| **TX tail** (Advanced) | 50 ms | 0–2000 ms | Extra time the radio stays keyed after the data. |
| **Transmit volume** | 0.5 | 0.01–1 | Loudness of the generated audio (1 = full scale). Lower it if the radio's audio sounds distorted. |
| **VOX wake-up tone** | 0 ms | 0–2000 ms | Play a tone this long before each transmission so a VOX radio keys up (0 = off). |
| **VOX tone pitch** (Advanced) | 1000 Hz | 300–3000 Hz | Pitch of the wake-up tone. |
| **Wait for a clear channel** | on |  | Listen before transmitting and wait while someone else is sending (strongly recommended). |
| **Persistence** (Advanced) | 63 | 1–255 | Chance (out of 255) of sending at each free slot; lower values reduce collisions on busy channels. |
| **Slot time** (Advanced) | 100 ms | 10–2000 ms | Time between channel checks. |
| **Packets per transmission** (Advanced) | 4 | 1–16 | How many queued packets are sent in one key-up. |
| **Send each packet** (Advanced) | 1 | 1–3 | Transmit every packet this many times (1 = once). Repetition helps on very lossy paths; duplicates are discarded by the receiver. |
| **Largest frame** (Advanced) | 0 bytes | 0–2048 bytes | Largest mesh frame per packet. 0 = automatic for the modem. |
| **Duty-cycle limit** (Advanced) | 1000 ‰ | 1–1000 ‰ | Legal transmit time limit in thousandths (1000 = none). |
| **Routing cost** (Advanced) | 0 | 0–255 | Lower is preferred. 0 = automatic from the modem speed. |
| **Queue size** (Advanced) | 32 frames | 1–256 frames | Frames waiting to be sent before new ones are refused. |

### Meshtastic device (LoRa) (`meshtastic`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Connection** | USB serial or TCP (Wi-Fi, port 4403) | USB serial or TCP (Wi-Fi, port 4403), Bluetooth LE (one message per packet) | How the phone or computer is connected to the Meshtastic device. |
| **Meshtastic channel** | 0 | 0–7 | Channel slot on the device to use (0 = primary). Every Ember node must use the same channel. |
| **Hop limit** | 3 | 1–7 | How many times Meshtastic routers may repeat each packet. |
| **Radio preset** | As configured on the device | As configured on the device, Long Range - Fast (default), Long Range - Moderate, Long Range - Slow, Very Long Range - Slow, Medium Range - Slow, Medium Range - Fast, Short Range - Slow, Short Range - Fast, Short Range - Turbo (500 kHz), Long Range - Turbo (500 kHz) | Only used to estimate speed and airtime; the device keeps its own setting. 'As configured' reads it from the device. |
| **Region** | As configured on the device | auto, US, EU_868, EU_433, ANZ, UA_868, UA_433, IN, JP, KR, TW, CN, RU, NZ_865, TH, BR_902, LORA_24 | Legal region of the device; sets the duty-cycle limit (Europe 868/433 MHz: 10 %). |
| **Licensed (ham) mode** | off |  | Turn on if the device runs Meshtastic's licensed amateur mode: traffic is then sent unencrypted with your callsign. |
| **Bridge Meshtastic text chat** | on |  | Show Meshtastic text messages on this channel in Ember, and allow replying to them. |
| **Keep-alive interval** (Advanced) | 60 s | 10–900 s | How often to ping the device so it keeps the connection open. |
| **Connect timeout** (Advanced) | 10 s | 2–120 s | Give up a connection attempt when the device sends nothing for this long. |
| **Queue size** (Advanced) | 16 frames | 1–128 frames | Packets waiting to be handed to the device. |
| **Routing cost** (Advanced) | 0 | 0–255 | Lower is preferred. 0 = default (20). |

### RNode LoRa radio (`rnode`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Region** | from Settings → Region | Europe 868 MHz (869.525 MHz, 10 % duty cycle), Europe 868.1 MHz (1 % duty cycle), Europe 433 MHz (433.775 MHz, 10 % duty cycle), Americas 915 MHz (914.875 MHz), Australia / NZ 915 MHz (917 MHz), India 865 MHz (866 MHz), Asia 923 MHz (923.2 MHz, JP / KR / SG / TH ...), Other / custom frequency | Where you are. Sets a legal default frequency, power and duty-cycle limit. |
| **Frequency** | 0 Hz | 0–3000000000 Hz | Exact centre frequency. 0 = the region's default channel. All stations must match. |
| **Bandwidth** | 125.0 kHz | 7.8 kHz, 10.4 kHz, 15.6 kHz, 20.8 kHz, 31.2 kHz, 41.7 kHz, 62.5 kHz, 125.0 kHz, 250.0 kHz, 500.0 kHz | LoRa bandwidth. Narrower reaches further but is slower. All stations must match. |
| **Spreading factor** | 9 | 5–12 | 7 = fast/short range ... 12 = slow/long range. All stations must match. |
| **Coding rate** (Advanced) | 5 | 5–8 | Error-correction overhead 4/5 ... 4/8. All stations must match. |
| **Transmit power** | -1 dBm | -1–30 dBm | -1 = the region's conservative default. Check your local legal limit and your board's maximum. |
| **Short-term airtime limit** (Advanced) | 0 % | 0–100 % | Firmware-enforced airtime cap over 15 s, in percent. 0 = not set. |
| **Long-term airtime limit** (Advanced) | 0 % | 0–100 % | Firmware-enforced airtime cap over 1 hour, in percent. 0 = use the region's duty-cycle limit. |
| **Duty-cycle limit** (Advanced) | 0 ‰ | 0–1000 ‰ | Transmit time limit in thousandths (100 = 10 %). 0 = the region's legal limit. |
| **Largest frame** (Advanced) | 254 bytes | 32–508 bytes | 254 fits one LoRa packet; up to 508 lets the firmware split a frame over two packets. |
| **Amateur radio operation** | off |  | Turn on when operating under an amateur licence: traffic is then unencrypted and must carry your callsign. |
| **Routing cost** (Advanced) | 20 | 1–255 | Lower is preferred when several links reach a node. |
| **Queue size** (Advanced) | 32 frames | 1–256 frames | Frames waiting to be sent before new ones are refused. |
| **Detect timeout** (Advanced) | 2000 ms | 200–20000 ms | How long to wait for the board to answer before retrying (Bluetooth may need more). |

### LoRa AT-command module (Reyax) (`lora_at`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Module** | RYLR998 / RYLR498 (and clones) | RYLR998 / RYLR498 (and clones), RYLR896 / RYLR406 (older) | The module printed on the board. |
| **Region** | from Settings → Region | Europe 868 MHz (869.525 MHz, 10 % duty cycle), Europe 868.1 MHz (1 % duty cycle), Europe 433 MHz (433.775 MHz, 10 % duty cycle), Americas 915 MHz (914.875 MHz), Australia / NZ 915 MHz (917 MHz), India 865 MHz (866 MHz), Asia 923 MHz (923.2 MHz, JP / KR / SG / TH ...), Other / custom frequency | Where you are. Sets a legal default frequency, power and duty-cycle limit. |
| **Frequency** | 0 Hz | 0–1020000000 Hz | Exact frequency. 0 = the region's default channel. All modules must match. |
| **Spreading factor** | 9 | 5–12 | 7 = fast/short range ... 12 = slow/long range. All modules must match. |
| **Bandwidth** | 125.0 kHz | 7.8 kHz (RYLR896 only), 10.4 kHz (RYLR896 only), 15.6 kHz (RYLR896 only), 20.8 kHz (RYLR896 only), 31.2 kHz (RYLR896 only), 41.7 kHz (RYLR896 only), 62.5 kHz (RYLR896 only), 125.0 kHz, 250.0 kHz, 500.0 kHz | LoRa bandwidth. All modules must match. |
| **Coding rate** (Advanced) | 1 | 1–4 | 1 = 4/5 (fastest) ... 4 = 4/8 (most robust). All modules must match. |
| **Preamble** (Advanced) | -1 | -1–24 | -1 = module default (12 on RYLR998, 4 on RYLR896). |
| **Network ID** | -1 | -1–18 | Modules only hear others with the same network ID. -1 = module default (18 on RYLR998, 0 on RYLR896). |
| **Module address** | 0 | 0–65535 | This module's address (stored in the module). |
| **Send to address** (Advanced) | 0 | 0–65535 | 0 = broadcast to every module (normal for the mesh). |
| **Transmit power** | -1 dBm | -1–22 dBm | -1 = the region's conservative default. RYLR896 max 15 dBm, RYLR998 max 22 dBm. Respect local limits. |
| **Network password** (Advanced) | (empty) |  | Optional 8 hex digit AES password (AT+CPIN). Empty = none. All modules must match. |
| **Show plain-text messages** | on |  | Show text sent by ordinary (non-EmberMesh) Reyax modules as interop messages. |
| **Amateur radio operation** | off |  | Turn on when operating under an amateur licence: traffic is then unencrypted and must carry your callsign. |
| **Duty-cycle limit** (Advanced) | 0 ‰ | 0–1000 ‰ | Transmit time limit in thousandths. 0 = the region's legal limit. |
| **Routing cost** (Advanced) | 20 | 1–255 | Lower is preferred when several links reach a node. |
| **Queue size** (Advanced) | 16 frames | 1–256 frames | Frames waiting to be sent before new ones are refused. |
| **Command timeout** (Advanced) | 1000 ms | 200–10000 ms | How long to wait for the module to answer a command. |

### SDR receiver (rtl_tcp) (`rtl_tcp`)

| Setting | Default | Range / choices | What it does |
|---|---|---|---|
| **Frequency** | 144390000 Hz | 24000000–1766000000 Hz | Frequency to listen on, in Hz (144390000 = APRS in North America, 144800000 = APRS in Europe). |
| **SDR sample rate** (Advanced) | 240 kHz (lowest CPU, recommended) | 240 kHz (lowest CPU, recommended), 1.024 MHz, 2.048 MHz | How fast the dongle samples. 240 kHz is plenty for one channel and is easiest on phones. |
| **Tuning offset** (Advanced) | 50000 Hz | 0–400000 Hz | Tune the dongle this far below the channel to avoid its centre spike. 0 = tune directly. |
| **RF gain** | 0 dB | 0–50 dB | Receiver gain in dB; 0 = automatic. Raise it for weak signals, lower it near strong transmitters. |
| **Frequency correction** (Advanced) | 0 ppm | -200–200 ppm | Crystal error of your dongle in parts per million (find it with a known station). |
| **Channel width** (Advanced) | 12.5 kHz narrow FM (most radios) | 12.5 kHz narrow FM (most radios), 25 kHz wide FM | Match the transmitters you want to hear. |
| **Modem** | AFSK 1200 - VHF/UHF packet, works with APRS gear | AFSK 1200 - VHF/UHF packet, works with APRS gear, AFSK 300 - HF packet, PSK 2400 - fast, audio cable only, PSK 4800 - fastest, clean audio cable only, Robust 31 - weakest signals, very slow, Robust 62 - phone held to a radio, weak signals, Robust 125 - error-corrected, Robust 250 - error-corrected, good audio, DTMF touch tones - digits only, Morse code (CW) - text only, PSK31 - text only, RTTY 45 baud - text only | How data is turned into sound. Everyone on the frequency must use the same modem. AFSK 1200 is standard packet radio; Robust works through a phone held against a radio. |
| **Your callsign** | N0CALL |  | Your amateur radio callsign with optional -SSID (e.g. N0CALL-7). Required on amateur bands. |
| **Band / licence** | Amateur radio (callsign, no encryption) | Amateur radio (callsign, no encryption), Licence-free or emergency use (encryption allowed) | Amateur bands forbid encryption and need your callsign on every packet. |
| **APRS messaging** | on |  | With AFSK modems: show APRS messages, positions and status heard on the channel and allow sending APRS messages. |
| **Modem fine-tuning** (Advanced) | (empty) |  | Optional key=value pairs separated by commas, e.g. wpm=15 for CW or mark_hz=1600 for AFSK. |
