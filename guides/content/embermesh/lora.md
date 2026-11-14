---
id: embermesh/lora
title: "EmberMesh 19: LoRa - Meshtastic, RNode, Reyax Modules and Serial Radios"
category: embermesh
tags: [embermesh, lora, meshtastic, rnode, reyax, rylr998, ebyte, hc-12, sik, xbee, ism, 915 mhz, 868 mhz, 433 mhz, duty cycle, antenna]
summary: "Long-range licence-free links with stock Meshtastic devices, RNode boards, Reyax AT-command modules and transparent serial radios: setup, settings that must match, regional bands and duty-cycle limits, antennas and placement."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 19
---

# LoRa: Meshtastic, RNode, Reyax Modules and Serial Radios

**LoRa** radios send small packets kilometres away on very little power, on licence-free bands.
They are the easiest way to link camps without an amateur licence, and private (encrypted)
messages may travel over them.

| Device | EmberMesh link type | Connects by | Speed at default settings |
|---|---|---|---|
| Stock **Meshtastic** radio (Heltec, T-Beam, RAK WisBlock, T-Echo, Station G2 ...) | **Meshtastic device (LoRa)** | USB, TCP (Wi-Fi models), Bluetooth (Android, iPhone) | about 1074 bit/s (LONG_FAST) |
| Board with **RNode** firmware (T-Beam, LoRa32, T3S3, Heltec, RAK4631, homebrew) | **RNode LoRa radio** | USB, Bluetooth, TCP | about 1757 bit/s (SF9, 125 kHz) |
| **REYAX RYLR998 / 498 / 896 / 406** AT-command module | **LoRa AT-command module (Reyax)** | USB-serial adapter, serial bridge | about 1757 bit/s (SF9, 125 kHz) |
| **Ebyte E22/E32, HC-12, SiK, XBee** transparent serial radio | **Serial / TCP / Bluetooth byte stream** with a radio preset | USB-serial adapter | 2.4 - 32 kbit/s |

> **Hardware status:** the Meshtastic, RNode and Reyax drivers are tested only against scripted
> emulators of these devices, not real hardware. Verify with the bench test in
> [Radio gateway](radio-gateway.md).

## Regional bands and duty-cycle limits

From the band plans built into the app (**Tools → Band plans**, filter `LoRa`). Rules change;
check your local regulator.

| Region | Band | Limits (summary) | Meshtastic default |
|---|---|---|---|
| US, Canada | 902-928 MHz | Up to 1 W conducted with up to 6 dBi antenna (4 W EIRP); no duty-cycle limit | 906.875 MHz |
| Europe, UK | 863-870 MHz | 868.0-868.6 MHz: 25 mW ERP, 1 % duty cycle. 869.4-869.65 MHz: 500 mW ERP, **10 %** duty cycle. 1 % = 36 s per hour | 869.525 MHz |
| Europe, UK | 433.05-434.79 MHz | 10 mW ERP, at most 10 % duty cycle | |
| Australia | 915-928 MHz | Up to 1 W EIRP | |
| India | 865-867 MHz | Up to 1 W transmitter power (4 W ERP) | |
| Much of Asia | 920-925 MHz (AS923) | Rules per country; many limit to about 16 dBm EIRP | |
| Japan | about 920-928 MHz | Typically 20 mW, listen-before-talk, certified radios only | |
| Korea | about 920.9-923.3 MHz | Listen-before-talk, low power | |
| **Americas** | 433 MHz | **Amateur band**: licence, callsign, no encryption | |

- EmberMesh **enforces the duty cycle** for the region you select, but **not** antenna gain or
  ERP: those are your responsibility.
- **Tools → Radio & propagation → LoRa time on air / duty cycle** shows the airtime per packet.
  Example with its defaults (SF11, 250 kHz, 50 bytes, coding 4/5): about 641 ms per packet, so
  about 561 packets per hour fit into a 10 % limit.

## Meshtastic devices

EmberMesh uses an **unmodified** Meshtastic radio as a modem. It does not change the radio's
settings; you set those with the normal Meshtastic app, and EmberMesh then uses one channel of it.

### Set up the Meshtastic radios first (with the Meshtastic app)

1. Set the **region**. The radio will not transmit until it is set.
2. On every radio that should carry EmberMesh traffic use the **same channel** (same name and
   same key) and the **same modem preset**.
3. Recommended: create a **dedicated secondary channel** for EmberMesh so its packets stay out of
   people's normal Meshtastic chat. Note its slot number.

### Add the link

- **USB (computer, Android, browser):** plug in, **Add connection →** USB serial / Serial port,
  **Scan for devices**, baud **115200**. **Protocol → Meshtastic device (LoRa)**. Do not run the
  Meshtastic app on the same USB port at the same time.
- **TCP (Wi-Fi/Ethernet radios; computer, Android or iPhone):** **TCP client** / **TCP connection**
  to `<device-ip>`, port **4403**.
  Only one TCP client can use the radio at a time. Hub example:

  ```sh
  emberd --link '{"endpoint":{"kind":"tcp","config":{"host":"192.168.1.50","port":4403}},"link":{"type":"meshtastic","config":{"channel":1}}}'
  ```

- **Bluetooth (iPhone):** **Bluetooth radio modem / TNC**, choose the Meshtastic profile; iOS
  may ask for the radio's pairing PIN. Leave the link's **Connection** setting at its default
  (**USB serial or TCP (Wi-Fi, port 4403)**): the iPhone translates the Meshtastic Bluetooth
  protocol into that form. Do not choose **Bluetooth LE (one message per packet)** with it.
- **Bluetooth (Android):** **Bluetooth LE serial device**, **Device type: Meshtastic radio
  (Meshtastic Bluetooth API)**; keep the link's Connection setting at its default, as on iPhone.
- Browsers list Meshtastic for Bluetooth too, but do not speak the Meshtastic Bluetooth protocol; use
  USB there.

### Settings

| Setting | Default | Notes |
|---|---|---|
| **Meshtastic channel** | 0 (primary) | Slot number of the channel EmberMesh uses; the same on every radio |
| **Hop limit** | 3 | Meshtastic's own relaying (1-7). Meshtastic routers rebroadcast EmberMesh packets as well as EmberMesh relaying them, so keep this at 1-3 on busy meshes |
| **Radio preset** | As configured on the device | Used only to estimate airtime |
| **Region** | As configured on the device | Sets the duty-cycle limit (EU_868 / EU_433: 10 %) |
| **Licensed (ham) mode** | off | Turn on only if the radio runs Meshtastic's licensed mode; then no encryption and a callsign are required |
| **Bridge Meshtastic text chat** | on | Normal Meshtastic text messages on this channel appear in EmberMesh, and you can reply |

**You should see:** the link goes from **Connecting…** to **Connected** once the radio has sent its
configuration. **Statistics** shows the radio's name, firmware, region, preset, number of known
nodes and the last signal strength (RSSI) and SNR.

**Talking to Meshtastic users:** their text messages appear in a Meshtastic conversation in
Chats. Use **Plain-text message** on the link card to answer: leave **To** empty to send to the
whole channel, or put a node id like `!a1b2c3d4` for a direct message. Text is cut to 200 bytes.

## RNode boards

1. **Before a crisis**, flash the RNode firmware once with `rnodeconf --autoinstall` (from the
   Reticulum tools on a computer). A board that answers "EEPROM locked" is not provisioned yet.
2. **Add connection →** USB serial (115200 baud), Bluetooth or TCP; **Protocol → RNode LoRa
   radio**.
3. Check the **Region** preset. It starts at the preset matching **Settings → Region** (see
   below); change it if you are elsewhere.
4. Leave **Bandwidth** 125 kHz, **Spreading factor** 9, **Coding rate** 5 unless your net agrees
   otherwise. **All stations must use the same frequency, bandwidth, spreading factor and coding
   rate.**
5. **Transmit power** -1 uses the region's conservative default; up to 30 dBm for boards with an
   amplifier. Check your legal limit.
6. **Connect.** The app sets frequency, bandwidth, spreading factor, coding rate and power on the
   board each time; nothing is stored on the board.

Region presets: Europe 868 MHz (869.525 MHz, 10 % duty cycle, 14 dBm), Europe 868.1 MHz (1 %),
Europe 433 MHz (433.775 MHz, 10 %, 10 dBm), Americas 915 MHz (914.875 MHz), Australia / NZ
915 MHz (917 MHz), India 865 MHz (866 MHz), Asia 923 MHz (923.2 MHz, JP / KR / SG / TH ...),
Other / custom frequency.

**Which preset a new link starts with** (RNode and Reyax links) follows **Settings → Region**:

| Settings → Region | Starting preset |
|---|---|
| United States / Canada | Americas 915 MHz |
| Europe, United Kingdom, Southern Africa | Europe 868 MHz |
| Australia / NZ, Brazil / South America | Australia / NZ 915 MHz (917 MHz lies in both the 915-928 and 902-928 MHz bands) |
| India | India 865 MHz |
| Japan | Asia 923 MHz |
| Other / unknown | Europe 868 MHz: **check and change it** |

An existing link keeps its preset if you change the region setting later, and a preset you chose
yourself is never changed. Meshtastic links take the region from the Meshtastic radio itself.

Spreading factor 7 is fastest; 12 gives the longest range and the slowest speed.

If the link goes to **Error** with "no RNode answered", check the cable and the firmware; with "RNode
rejected the TX power setting", lower **Transmit power**. **Statistics** shows signal strength,
noise floor, channel load, battery and temperature.

EmberMesh can share a frequency with Reticulum users on identical settings, but the two do not
understand each other's packets.

## REYAX AT-command modules (RYLR998, RYLR498, RYLR896, RYLR406)

Wiring to a USB-serial adapter:

```
   RYLR module            USB-serial adapter (3.3 V logic!)
   VDD  ----------------  3.3 V   (NOT 5 V: the RYLR998 is not 5 V tolerant)
   GND  ----------------  GND
   TXD  ----------------  RXD
   RXD  ----------------  TXD     (use a level shifter with 5 V adapters)
```

1. **Add connection → USB serial**, baud **115200** (factory default).
2. **Protocol → LoRa AT-command module (Reyax)**.
3. **Module:** RYLR998/498 or RYLR896/406. **Region** as for RNode (starts from Settings →
   Region).
4. **Spreading factor** 9, **Bandwidth** code 7 (= 125 kHz; RYLR998 supports only 7, 8, 9 =
   125/250/500 kHz), **Coding rate** 1 (= 4/5), **Network ID**, **Network password** (optional
   8 hex digits): **the same on every module**. **Send to address** 0 = everyone (normal).
5. **Connect.** The link checks every setting with the module and reports any refusal in its red
   error line, for example the command and the module's error code.

Plain text from ordinary (non-EmberMesh) Reyax modules is shown as messages (**Show plain-text
messages**), and **Plain-text message** sends plain text to them. Maximum mesh frame: 177 bytes.

## Transparent serial radios (Ebyte, HC-12, SiK, XBee)

These modules simply pass serial bytes over the air. Use **Serial / TCP / Bluetooth byte stream**
and pick the module under **Device / cable**. **Every station must use the same preset and the
same Error correction setting**; frames with a different setting or larger than the link's
maximum frame size are dropped.

| Preset | Module | Notes |
|---|---|---|
| **Ebyte E22 / E32 LoRa module (transparent mode)** | Ebyte E22/E32 | M0 = M1 = LOW (normal mode). Same air rate, channel and address on all modules (vendor tool). Default pacing assumes 2.4 kbit/s air rate; set **Write pacing** to the air rate if you change it. Error correction on. |
| **HC-12 433 MHz serial radio** | HC-12 | Same channel and mode on all (`AT+FU3`, `AT+C001`), SET pin high for data. |
| **SiK / 3DR telemetry radio** | SiK | Point-to-point. |
| **XBee in transparent mode** | XBee | 9600 baud serial. |

**In the Americas 433 MHz is an amateur band**: tick **Amateur radio band** and identify with your
callsign.

## Antennas and placement

- **Never transmit without an antenna attached.** It can destroy the radio's output stage.
- The small stock antennas are weak. **Tools → Antennas → LoRa quarter-wave / ground plane**
  calculates a simple, much better antenna. Its results:

  | Band | Radiator (to centre pin) | 4 radials (to ground), drooped 45° |
  |---|---|---|
  | 915 MHz | 7.8 cm | 8.2 cm each |
  | 868 MHz | 8.2 cm | 8.6 cm each |
  | 433.92 MHz | 16.4 cm | 17.3 cm each |

  Build instructions are in [Antennas you can build](antennas.md).
- **Height beats power.** A LoRa radio on a roof, mast or hilltop reaches many times further than
  one in a pocket. Use **Tools → Radio & propagation → Radio horizon / line of sight** and
  **Link budget and range** (it has LoRa sensitivity presets) to estimate.
- Keep coax short at these frequencies: **Tools → Coax / feedline loss**. For example RG-58 loses
  about 18.9 dB per 100 ft at 915 MHz, LMR-400 about 3.9 dB.
- Put an outdoor radio in a weatherproof box with the antenna outside the box.

## If it doesn't work

| Symptom | Likely cause | Fix |
|---|---|---|
| Meshtastic link stays **Connecting…**, then **Error** | Wrong port or baud; Meshtastic app holding the port; region not set | Close other apps; use 115200; set the region in the Meshtastic app |
| Meshtastic link up, but nothing from other EmberMesh nodes | Different channel slot, name/key or preset | Make channel, key and preset identical everywhere |
| Meshtastic TCP refused | Another client is connected (only one allowed) | Disconnect the phone app from the radio |
| RNode **Error** "no RNode answered" | Not RNode firmware, or not provisioned | Flash with `rnodeconf --autoinstall` |
| RNode error about frequency outside the region | Frequency does not match the region preset | Choose the right region, or **Other / custom frequency** |
| Reyax error with a command and a code | Value not accepted by the module | Check the ranges for your module model; 3.3 V supply |
| Transparent module: garbage, many errors | Air rate/channel mismatch, or noise | Match settings on all modules; keep error correction on |
| Airtime bar full, messages wait | Duty-cycle limit reached | Normal in Europe: wait, send less, or use a faster setting |
