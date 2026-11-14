---
id: embermesh/kiss-tnc-packet
title: "EmberMesh 18: KISS TNCs and Packet Radio"
category: embermesh
tags: [embermesh, kiss, tnc, packet radio, ax25, direwolf, mobilinkd, ninotnc, kenwood, aprs, bluetooth, serial, tcp]
summary: "Using hardware TNCs, Direwolf over TCP, Bluetooth TNCs and radios with a built-in TNC through the kiss link, the settings that matter, and exchanging APRS text messages with ordinary APRS stations."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 18
---

# KISS TNCs and Packet Radio

A **TNC** (terminal node controller) is a packet-radio modem between a computer and a radio.
**KISS** is the simple protocol most TNCs speak. The **KISS TNC (packet radio)** link (`kiss`)
lets EmberMesh use any of them:

- hardware TNCs on USB or serial (TNC-Pi, NinoTNC, Kantronics, TNC-2 types, Mobilinkd over USB);
- **Direwolf**, the software TNC, over TCP;
- **Bluetooth TNCs** (Mobilinkd and many ESP32/nRF52 KISS adapters);
- **radios with a built-in TNC** (for example Kenwood TH-D72/D74, TM-D710);
- LoRa modems that speak KISS.

On the air every mesh message is an ordinary **AX.25 UI frame** addressed to `EMBER`, from your
callsign. Normal APRS gear ignores these frames, and EmberMesh also **reads and writes APRS text
messages**, so you can exchange messages with ordinary APRS users.

> **Hardware status:** the KISS driver is tested against hand-built protocol examples and between
> real desktop daemons over TCP and a virtual serial cable, but **never with a real TNC**. Verify with the bench test in [Radio gateway](radio-gateway.md).

## Fastest path: Direwolf on a hub

1. Install and configure Direwolf with a sound card and radio (see the Direwolf documentation).
   In `direwolf.conf` make sure the KISS TCP port is on: `KISSPORT 8001`.
2. Start emberd with the KISS link:

   ```sh
   emberd --kiss-tcp 127.0.0.1:8001
   ```

3. Set your callsign: start emberd with `--callsign QX0ABC`, or on the operator page answer **Your
   callsign is needed** on the Links screen (or **Settings → Profile → Callsign**). The node copies
   it into the KISS link; add an SSID (e.g. `QX0ABC-1`) in the link's **Settings** if you like.

**You should see:** the link is **Connected**; tags show 1.2 kbit/s, **No encryption (amateur)**,
**station ID**.

A phone can use a Direwolf running on another computer on the same Wi-Fi: **Add connection →
TCP client** (Android) / **TCP connection** (iPhone), address of that computer, port `8001`, link
type **KISS TNC (packet radio)**.

## Hardware TNC on USB or serial

1. Plug in the TNC. On Android use a USB OTG adapter; the phone asks once for permission (tick
   "use by default").
2. **Add connection →** "USB serial" (Android), "Serial port / USB TNC" (computer), "USB serial
   (radio, TNC, LoRa)" (Chrome/Edge browser). Tap **Scan for devices** and pick the TNC.
3. Set the **baud rate** of the TNC: most KISS TNCs use **9600 or 57600**; NinoTNC uses 57600.
4. **Protocol → KISS TNC (packet radio)**. If your profile has no callsign yet, the wizard asks for
   it (**Your callsign is needed** → **Save callsign**).
5. If the TNC starts in a text command mode (TNC-2 types, Kantronics, Kenwood radios), open
   **Advanced settings** and set **Enter KISS mode: Send KISS ON + RESTART (TNC-2, Kantronics,
   Kenwood)**, or enable KISS in the radio's menu.
6. **Connect.**

Command line on a hub:

```sh
emberd --list-serial                           # find the port
emberd --kiss-serial /dev/ttyUSB0:9600         # Linux
emberd --kiss-serial /dev/cu.usbserial-A1:9600 # macOS (use /dev/cu.*, not /dev/tty.*)
emberd --kiss-serial COM3:9600                 # Windows (untested)
```

On Linux the user running emberd must be in the `dialout` group (the service installer does this).

## Bluetooth TNC

- **Android:** a BLE TNC: **Add connection → Bluetooth LE serial device**, **Scan for devices**.
  A classic-Bluetooth TNC: pair it first in the phone's Bluetooth settings, then **Bluetooth
  serial (SPP)**.
- **iPhone:** **Bluetooth radio modem / TNC** (BLE only; iPhones cannot use classic Bluetooth
  serial). Many BLE TNCs and serial bridges use the "Nordic UART" service, the default.
  **Mobilinkd TNC3/TNC4 do not**: choose the profile **KISS TNC with the BLE KISS service**.
- **Mobilinkd on Android or in a browser:** there is no Mobilinkd profile; enter its BLE KISS
  service with the custom UUIDs: service `00000001-ba2a-46c9-ae49-01b0961f68bb`, write
  `00000002-ba2a-46c9-ae49-01b0961f68bb`, notify `00000003-ba2a-46c9-ae49-01b0961f68bb`. A TNC3 can
  also be paired over classic Bluetooth and used with **Bluetooth serial (SPP)** on Android.
- **Browser (Chrome/Edge):** **Bluetooth LE**, Nordic UART or custom UUIDs.
- Then **Protocol → KISS TNC (packet radio)** and your callsign.

Keep the TNC close and switched on while scanning; it must not be connected to another app.

## Radios with a built-in TNC

- **Kenwood TH-D72 / TH-D74 / TM-D710:** switch the TNC to KISS mode in the radio's menu, or set
  **Enter KISS mode: Send KISS ON + RESTART**. Some Kenwood radios need a custom text such as
  `TC 0\r` first: use **Send custom text (below)** and **Custom init text**.
- **Yaesu FTM-400** has no KISS mode and cannot be used this way.
- Radios with a USB port often appear as a standard serial device.

## Settings that matter

| Setting | Default | Notes |
|---|---|---|
| **Your callsign** | your profile callsign (otherwise `N0CALL`) | Required for amateur use; nothing is sent while it is `N0CALL`. In licence-free mode an `N0CALL` link transmits as a made-up id starting with `QX` |
| **Band / licence** | Amateur radio (callsign, no encryption) | Choose licence-free only where that is legal (LoRa KISS modems) |
| **Packet format** | AX.25 (standard packet radio, works with APRS gear) | **Raw** only for licence-free LoRa KISS modems |
| **TNC port** | 0 | Only multi-port TNCs need another value |
| **Radio data rate** | 1200 bit/s | 1200 for VHF packet/APRS, 9600 for fast UHF packet (radio must have a 9600 data port), 300 for HF |
| **APRS messaging** | on | Show APRS messages, positions and status; allow sending |
| **Configure the TNC** | on | Sends the timing values below when it connects; turn off for TNCs that dislike it |
| **TX delay**, **Persistence**, **Slot time**, **TX tail** (Advanced) | 300 ms, 63, 100 ms, 30 ms | Raise TX delay if the far end misses the start of packets |
| **Enter KISS mode** (Advanced) | Already in KISS mode | See above |
| **Leave KISS on close** (Advanced) | off | Returns the TNC to command mode when the link is removed |
| **Largest frame** (Advanced) | 256 bytes | AX.25 standard |
| **Digipeater path** (Advanced) | empty | Leave empty: the mesh relays by itself |

EmberMesh paces frames to the radio speed so the TNC's buffer does not overflow; the TNC still
does its own listen-before-transmit.

## APRS text messaging

With **APRS messaging** on, the link understands APRS:

- **Received messages** (to you, to other stations, and bulletins) appear in Chats in an APRS
  conversation, marked with the network name **APRS**, the sender and the addressee.
- Messages **to your callsign are acknowledged automatically** (switch off with **Acknowledge
  APRS messages**, Advanced).
- **Positions** appear as text like "Position 39.8283, -98.5795 ..." and **status reports** as
  "Status: ...".

To send an APRS message:

1. On the link card tap **Plain-text message**.
2. **To:** the station's callsign (e.g. `QX0XYZ-9`). Leave it empty to send a bulletin (`BLN0`)
   to everyone.
3. **Text:** keep it short. More than 67 characters is split into up to 4 APRS messages.
4. **Send.** The message is retried after 30, 60 and 120 seconds until the other station
   acknowledges it.

APRS messages use the **APRS path** (Advanced, default `WIDE1-1,WIDE2-1`) so digipeaters relay
them. To reach APRS users, the link must be on the APRS frequency (144.390 MHz in North America,
144.800 MHz in Europe). Do not put busy mesh traffic there; run a second station on a separate
packet frequency for mesh data if you need both.

## If it doesn't work

| Symptom | Likely cause | Fix |
|---|---|---|
| Link stays Down or Error on serial | Wrong port or baud rate, permission | Check the port list; try 9600, 57600, 1200, 19200; on Linux join `dialout` |
| Connected, but nothing is transmitted | Callsign still `N0CALL` | Set your callsign (**Save callsign** on the Links screen, or Settings → Profile) |
| TNC prints text / ignores data | TNC is in command mode | **Enter KISS mode: Send KISS ON + RESTART** |
| Radio transmits but nobody decodes | Audio level or deviation set wrong in the TNC | Adjust the TNC's transmit level per its manual; check frequency |
| Start of packets lost | TX delay too short | Raise **TX delay** to 400-500 ms |
| Received **Errors** rise | Frames for another TNC port, garbage, or corrupted data | Check **TNC port**; check cable and baud rate |
| ESP32-based TNC resets on connect | DTR/RTS toggling | On the serial connection, Advanced: set **DTR line** / **RTS line** to Off (computer), or turn off **Assert DTR** (Android) |
| APRS messages never acknowledged | Station out of range, or digipeaters not reached | Check path; try again nearer |
