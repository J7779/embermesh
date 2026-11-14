---
id: embermesh/what-you-need
title: "EmberMesh 11: What You Need to Connect Things (Devices, Cables, Adapters)"
category: embermesh
tags: [embermesh, hardware, cables, adapters, otg, usb serial, cp210x, ch340, ftdi, pl2303, trrs, k1, sound card, cm108, aioc, digirig, tnc, meshtastic, rnode, reyax, sdr, shopping list, scavenging]
summary: "For every thing you may want to connect (handheld radios with and without a cable, radios with PTT interfaces or data ports, TNCs, LoRa boards, radio modules, SDR dongles, phones, hubs, USB sticks) and every platform: exactly which devices, cables and adapters are required or optional, what to look for when buying or scavenging, and what is impossible."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 11
---

# What You Need to Connect Things

> **Hardware status:** EmberMesh has not yet been tested with any real radio, cable, sound card, TNC,
> LoRa board, SDR dongle, Bluetooth device or USB-serial adapter. Everything below is what the
> software is written to support; it is the same list the app shows under **Instructions → What
> you need** and in the **Connect a radio** wizard's "You will need" step. Test each setup on the bench before you depend on it
> ([Radio gateway](radio-gateway.md), "Verify every radio link yourself").

## The matrix

One row per connection method, exactly as the app lists them (Instructions → What you need).
"yes" = works on that platform; "—" = not possible there.

| Connection | Android | iPhone/iPad | Browser | Computer (emberd) | Status |
|---|---|---|---|---|---|
| Handheld radio by audio cable, keyed by its VOX | yes | yes | yes | yes | simulated only |
| Radio with a USB sound-card + PTT interface | yes | — | yes | yes | simulated only |
| Any radio with no cable (acoustic coupling) | yes | yes | yes | yes | simulated only |
| Mobile / base radio with a data port or USB audio | yes | — | yes | yes | simulated only |
| Virtual audio pipe to other radio software | — | — | — | yes | simulated only |
| Bluetooth KISS TNC | yes | yes | yes | — | simulated only |
| KISS TNC by USB serial or TCP (hardware TNC, Direwolf) | yes | yes | yes | yes | simulated only |
| Meshtastic LoRa board | yes | yes | yes | yes | simulated only |
| RNode LoRa board | yes | yes | yes | yes | simulated only |
| AT-command LoRa module (REYAX RYLR class) | yes | yes | yes | yes | simulated only |
| Transparent serial radio module (Ebyte E22/E32, HC-12, SiK, XBee) | yes | — | yes | yes | simulated only |
| Receive-only SDR dongle (RTL-SDR class) via rtl_tcp | yes | yes | — | yes | simulated only |
| Phone to phone, nothing else (Bluetooth LE / Wi-Fi Direct) | yes | yes | — | — | untested on hardware |
| Same Wi-Fi or Apple peer-to-peer, WebRTC pairing | yes | yes | yes | yes | simulated only |
| Serial cable between two devices | yes | — | yes | yes | simulated only |
| Hub on a hotspot-capable phone or computer | yes | yes | — | yes | simulated only |
| Join a hub from any browser | yes | yes | yes | yes | simulated only |
| Carry messages on a USB stick or card (sneakernet) | yes | yes | yes | yes | simulated only |
| QR contact cards and invites | yes | yes | yes | yes | untested on hardware |
| Morse by light (flashlight, lamp or screen) | yes | yes | yes | yes | simulated only |
| Morse by sound (tone, radio speaker, whistle) | yes | yes | yes | yes | simulated only |

"Simulated only" means the method was exercised with emulated devices, virtual cables or generated
signals, never real hardware. A **browser on a phone** has the browser column's abilities only on a
secure page (the single file, `https` or `localhost`); a plain-http hub page cannot use the
microphone, camera, USB or Bluetooth. A **Raspberry Pi class box** runs the computer program (its
Linux build was not compiled in the latest test round).

## Parts glossary: what to look for

| Part | What it is / what to check |
|---|---|
| **USB OTG adapter** (Android) | A small adapter or cable that lets the phone act as USB host: USB-C (or micro-USB) male to USB-A female. A normal charging cable does **not** work. The phone must support USB host (most do) |
| **Powered USB hub** or **Y-cable** | For radios and boards that draw more current than the phone supplies |
| **USB-serial adapter** | Must have one of the supported chip families: **FTDI** (FT232R/BM/FT2232/FT4232/FT232H/FT-X), **Silicon Labs CP210x** (CP2102/CP2104/CP2105/CP2108), **WCH CH340/CH341**, **Prolific PL2303** (HX/TA/TB/HXN; counterfeit "PL2303HXA" may refuse), or any **CDC-ACM** device. The chip name is printed on it or shown when plugged into a computer. For Reyax modules use a **3.3 V** adapter (or a level shifter) |
| **K1 two-pin radio audio cable (3.5 mm + 2.5 mm)** | The accessory socket of Kenwood, Baofeng and many cheap handhelds. Usually the 3.5 mm plug carries speaker audio and the 2.5 mm the microphone, but contacts and PTT wiring differ between makes: buy a cable for your radio or measure a working speaker-mic |
| **TRRS plug vs TRS plug** | A phone's headset jack needs a **4-contact TRRS** plug (CTIA: tip L, ring 1 R, ring 2 ground, sleeve mic). A 3-contact **TRS** (stereo headphone) plug has no microphone contact: the phone hears nothing |
| **USB-C to 3.5 mm headset adapter** | For phones with no jack. Must pass the microphone (headset adapters do; some music-only adapters do not) |
| **USB sound card** | Class-compliant USB audio dongle (the common CM108/CM119 type). Works on computers, Android (via OTG) and iPhone (USB-C or Lightning camera adapter) |
| **Sound-card + PTT radio interface** (AIOC / Digirig class) | A USB sound card plus a PTT output in one box, with the radio cable. On a computer, PTT through its serial RTS/DTR line (a Digirig's CP210x port) or a CM108-type GPIO; on Android through the serial line of the same adapter |
| **Attenuator / isolation** | Radio speaker outputs are hundreds of times louder than a phone microphone input expects, and a phone's output can overdrive a radio's microphone. A resistor divider (about 10 kΩ / 100-470 Ω) and a DC-blocking capacitor protect both and give clean audio; ready-made interfaces include this. An isolating audio transformer is a common extra against hum and RF pickup (optional, not required by the software) |
| **Bluetooth LE TNC / serial bridge** | Must offer the Nordic UART service, the Mobilinkd BLE KISS service, or known custom UUIDs |
| **Power bank** | For phones, LoRa boards and small hubs in the field; a USB output that does not switch off at low current is best for LoRa boards (general advice) |
| **RTL-SDR dongle** | A USB DVB-T dongle with the RTL2832 chip (R820T and similar tuners); needs a computer to run `rtl_tcp` |


## Connection by connection

### Handheld radio by audio cable, keyed by its VOX

Connects: phone or computer ↔ handheld radio audio. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   radio K1 socket                         phone / computer
   [3.5 mm speaker] ---[ attenuator ]---> mic in  (TRRS sleeve, or USB sound-card mic)
   [2.5 mm mic    ] <--[ attenuator + DC block ]-- headphone out
                       radio VOX keys the transmitter
```

**Required:**

- **FM handheld radio with VOX (e.g. the Baofeng UV-5R family)** — Transmits and receives the modem tones. Alternatives: any radio with VOX and an audio jack.
- **K1 two-pin radio audio cable (3.5 mm + 2.5 mm) to a 4-pole (TRRS) headset plug** — Carries the modem audio to the radio's microphone input and the radio's speaker audio back. Alternatives: cable for your radio's own accessory jack; USB sound-card radio interface (see the PTT interface method).

**Optional:**

- **USB-C (or Lightning) to 3.5 mm headset adapter with microphone support** — Phones without a headset jack need it to plug in the 4-pole radio cable. Alternatives: phone or computer with a 3.5 mm headset jack; TRRS splitter (separate mic and headphone plugs) on computers.
- **Audio attenuator / level-adjustable cable** — The radio microphone input is very sensitive; too much level distorts the transmitted signal. Alternatives: turn the device volume down (start around 30 %).

**Platform notes:**

- Android: Choose the headset/USB audio device as input and output in the link settings.
- iPhone/iPad: Use a Lightning/USB-C headset adapter with microphone support.
- Browser: Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): A computer with a combined headset jack needs a TRRS cable; separate mic/line jacks need a splitter.

**Steps:** Set VOX, squelch and power on the radio, plug the cable in, add an audio link with preset handheld_vox, send a test message to a second station. Chapter: [Handheld radios](handheld-audio-modem.md).

### Radio with a USB sound-card + PTT interface

Connects: phone or computer ↔ radio audio + PTT line. Works on: Android, Browser, Computer (emberd).

```
   [radio] ==K1 / interface lead==> [ USB sound card + PTT line ] --USB--(OTG)--> [phone / computer]
                                     PTT from serial RTS/DTR or a CM108-type GPIO
```

**Required:**

- **USB sound-card + serial-PTT radio interface (AIOC / Digirig class)** — Gives a clean sound card and keys the transmitter by RTS/DTR, so no VOX is needed. Alternatives: separate USB sound card plus a USB serial adapter driving a PTT transistor.
- **Interface lead for your radio's connector (e.g. K1 two-pin)** — Connects the interface to the radio.
- **FM radio** — The radio itself.

**Optional:**

- **USB-C (or micro-USB) OTG adapter** — Phones connect the interface's USB plug through it. Alternatives: USB-C interface cable.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- iPhone/iPad: iOS gives apps no USB serial and no Classic Bluetooth serial: use Bluetooth LE devices or the audio path instead.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): On Linux add your user to the dialout group (serial ports) and audio group (sound cards).
- **Not possible on:** iPhone/iPad.

**Steps:** Connect the interface, pick its sound card and its serial port as PTT, add an audio link with preset handheld_ptt. Chapter: [Handheld radios](handheld-audio-modem.md).

### Any radio with no cable (acoustic coupling)

Connects: device speaker/microphone ↔ radio microphone/speaker. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   device speaker  ))) 1-3 cm (((  radio microphone
   device mic      ((( 1-3 cm )))  radio speaker       you hold the radio's PTT
```

**Required:**

- **Any voice radio (handheld, FRS/GMRS/PMR446, CB)** — You press its PTT when the app asks.

**Optional:**

- **A quiet spot** — Room noise and echo slow the robust modem down.

**Platform notes:**

- Browser: Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.

**Steps:** Add an audio link with preset acoustic, hold the device to the radio, press PTT when asked. Chapter: [Handheld radios](handheld-audio-modem.md).

### Mobile / base radio with a data port or USB audio

Connects: computer or phone ↔ radio data port (audio + PTT). Works on: Android, Browser, Computer (emberd).

```
   [radio data port, e.g. 6-pin mini-DIN] <==line audio + PTT==> [sound-card interface] --USB--> [computer / phone]
   (or: radio with built-in USB audio + CAT -- one USB cable --> computer)
```

**Required:**

- **Sound-card radio interface with a data-port lead (e.g. 6-pin mini-DIN)** — Connects line-level audio and PTT to the data port. Alternatives: radio with a built-in USB sound card and CAT/CI-V serial port: just a USB cable.

**Optional:**

- **USB serial adapter with a CP210x, CH340/CH341, FTDI or PL2303 chip (or a CDC-ACM device)** — Keys PTT (or the radio's own USB serial port does).
- **USB-C (or micro-USB) OTG adapter** — Phones connect USB devices through it.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- iPhone/iPad: iOS gives apps no USB serial and no Classic Bluetooth serial: use Bluetooth LE devices or the audio path instead.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): On Linux add your user to the dialout group (serial ports) and audio group (sound cards).
- **Not possible on:** iPhone/iPad.

**Steps:** Wire the data port, select the radio's sound card and PTT port, add an audio link with preset sound_card. Chapter: [Radio gateway](radio-gateway.md).

### Virtual audio pipe to other radio software

Connects: emberd ↔ another program's audio (pipes / files). Works on: Computer (emberd).

```
   [emberd] --raw PCM pipes--> [Direwolf / sox / aplay / another emberd]
```

**Optional:**

- **Nothing extra** — The audio_pipe endpoint uses named pipes or files of raw 16-bit PCM.

**Platform notes:**

- Computer (emberd): emberd only (endpoint kind audio_pipe).
- **Not possible on:** Android, iPhone/iPad, Browser.

**Steps:** Start emberd with an audio_pipe endpoint and an audio link with your own settings. Chapter: [Radio gateway](radio-gateway.md).

### Bluetooth KISS TNC

Connects: phone or computer ↔ TNC ↔ radio. Works on: Android, iPhone/iPad, Browser.

```
   [radio] ==cable== [Bluetooth TNC] ))) Bluetooth ((( [phone / browser]
```

**Required:**

- **Bluetooth KISS TNC (Bluetooth LE or Classic SPP)** — Does the packet modem in hardware. Alternatives: radio with a built-in Bluetooth KISS TNC.
- **TNC-to-radio cable** — Connects the TNC to the radio's audio/PTT.
- **FM radio** — The radio itself.

**Platform notes:**

- Android: Both Bluetooth LE and Classic (SPP) TNCs work.
- iPhone/iPad: Bluetooth LE TNCs only (Nordic UART or BLE KISS service); Classic SPP is not available to apps.
- Browser: Web Bluetooth works in Chrome / Edge on a computer (Linux: behind a flag) and Chrome on Android. Bluetooth LE only. Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): Pair the TNC in the operating system; it appears as a serial port (use the serial TNC method).
- **Not possible on:** Computer (emberd).

**Steps:** Pair/scan the TNC, add a kiss link with your callsign. Chapter: [KISS TNCs](kiss-tnc-packet.md).

### KISS TNC by USB serial or TCP (hardware TNC, Direwolf)

Connects: computer or phone ↔ TNC ↔ radio. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [radio] ==cable== [TNC] --USB / serial-- [USB-serial adapter if needed] --(OTG)-- [phone / computer]
   or: [radio]==[sound card]==[computer running Direwolf] --TCP 8001-- [any app]
```

**Required:**

- **KISS TNC with USB or serial port** — Does the packet modem in hardware. Alternatives: Direwolf software TNC reached over TCP (no extra hardware beyond its sound card).
- **TNC-to-radio cable** — Connects the TNC to the radio.

**Optional:**

- **USB serial adapter with a CP210x, CH340/CH341, FTDI or PL2303 chip (or a CDC-ACM device)** — Needed when the TNC has an RS-232 or TTL serial port.
- **USB-C (or micro-USB) OTG adapter** — Phones connect USB devices through it.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- iPhone/iPad: USB serial is not available on iOS; Direwolf over TCP on another computer works.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): On Linux add your user to the dialout group (serial ports) and audio group (sound cards).

**Steps:** Connect the TNC (or start Direwolf), add a kiss link over serial or TCP port 8001. Chapter: [KISS TNCs](kiss-tnc-packet.md).

### Meshtastic LoRa board

Connects: phone or computer ↔ Meshtastic device. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [Meshtastic board + antenna] --USB--(OTG)-- [phone / computer]
                                ))) Bluetooth LE ((( [phone]
                                --- Wi-Fi TCP 4403 --- [app on the same network]
```

**Required:**

- **LoRa board running stock Meshtastic firmware** — Sends Ember traffic as Meshtastic private-app packets.
- **Antenna for the board's band** — Never transmit without an antenna.

**Optional:**

- **USB cable** — Serial connection and power (not needed over Bluetooth or Wi-Fi). Alternatives: Bluetooth LE; Wi-Fi (TCP port 4403).
- **USB-C (or micro-USB) OTG adapter** — Phones connect USB devices through it.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- iPhone/iPad: Bluetooth LE or Wi-Fi (TCP) only.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Web Bluetooth works in Chrome / Edge on a computer (Linux: behind a flag) and Chrome on Android. Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): USB serial or TCP. On Linux add your user to the dialout group (serial ports) and audio group (sound cards).

**Steps:** Flash Meshtastic, set the region on the device, add a meshtastic link. Chapter: [LoRa](lora.md).

### RNode LoRa board

Connects: phone or computer ↔ RNode. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [RNode board + antenna] --USB--(OTG)-- [phone / computer]   (or Bluetooth / TCP)
```

**Required:**

- **LoRa board running RNode firmware** — A LoRa modem controlled by the app.
- **Antenna for the board's band** — Never transmit without an antenna.

**Optional:**

- **USB cable** — Serial connection and power. Alternatives: Bluetooth.
- **USB-C (or micro-USB) OTG adapter** — Phones connect USB devices through it.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- iPhone/iPad: Bluetooth LE or TCP only.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Web Bluetooth works in Chrome / Edge on a computer (Linux: behind a flag) and Chrome on Android. Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): On Linux add your user to the dialout group (serial ports) and audio group (sound cards).

**Steps:** Flash RNode firmware, add an rnode link with your region's frequency. Chapter: [LoRa](lora.md).

### AT-command LoRa module (REYAX RYLR class)

Connects: phone or computer ↔ LoRa module. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   RYLR module  VDD ---- 3.3 V    USB-serial adapter (3.3 V logic)
                GND ---- GND
                TXD ---- RXD
                RXD ---- TXD      --USB--(OTG)-- [phone / computer]
```

**Required:**

- **AT-command LoRa module (REYAX RYLR998/498/896/406 class)** — Sends and receives LoRa packets.
- **3.3 V USB serial adapter (CP210x, CH340/CH341, FTDI or PL2303)** — The module has a 3.3 V UART. Alternatives: Bluetooth LE / SPP serial bridge.
- **Antenna for the module's band** — Never transmit without an antenna.

**Optional:**

- **USB-C (or micro-USB) OTG adapter** — Phones connect USB devices through it.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- iPhone/iPad: Only through a Bluetooth LE serial bridge or TCP.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): On Linux add your user to the dialout group (serial ports) and audio group (sound cards).

**Steps:** Wire the module to the adapter (3.3 V), add a lora_at link. Chapter: [LoRa](lora.md).

### Transparent serial radio module (Ebyte E22/E32, HC-12, SiK, XBee)

Connects: phone or computer ↔ serial radio module. Works on: Android, Browser, Computer (emberd).

```
   [module + antenna] --UART-- [USB-serial adapter] --USB--(OTG)-- [phone / computer]   x each station
```

**Required:**

- **Transparent serial radio module pair** — One per station; they act as a wireless serial cable.
- **USB serial adapter with a CP210x, CH340/CH341, FTDI or PL2303 chip (or a CDC-ACM device)** — Connects the module's UART (check its logic voltage).

**Optional:**

- **USB-C (or micro-USB) OTG adapter** — Phones connect USB devices through it.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): On Linux add your user to the dialout group (serial ports) and audio group (sound cards).
- **Not possible on:** iPhone/iPad.

**Steps:** Configure both modules alike, add a stream link with the matching preset (ebyte, hc12, sik, xbee). Chapter: [Radio gateway](radio-gateway.md).

### Receive-only SDR dongle (RTL-SDR class) via rtl_tcp

Connects: rtl_tcp server ↔ app (listen only). Works on: Android, iPhone/iPad, Computer (emberd).

```
   [antenna]--[RTL-SDR]--USB--[computer running rtl_tcp] --- network --- [emberd / iPhone / Android]
```

**Required:**

- **RTL-SDR class USB dongle** — Receives the channel; it cannot transmit.
- **Antenna for the band** — Reception.
- **A device running an rtl_tcp server** — The app connects to rtl_tcp over the network. Alternatives: the same computer (127.0.0.1:1234).

**Platform notes:**

- Android: Connect to an rtl_tcp server over the network (tcp endpoint).
- iPhone/iPad: Connect to an rtl_tcp server over the network.
- Computer (emberd): Run rtl_tcp locally and connect to 127.0.0.1:1234.
- **Not possible on:** Browser.

**Steps:** Start rtl_tcp, add an rtl_tcp link with the frequency. Chapter: [SDR](sdr-listening.md).

### Phone to phone, nothing else (Bluetooth LE / Wi-Fi Direct)

Connects: phone ↔ phone. Works on: Android, iPhone/iPad.

```
   [phone] ))) Bluetooth LE ((( [phone]        [Android] ))) Wi-Fi Direct ((( [Android]
```

**Optional:**

- **Nothing extra** — Both phones run the app; Bluetooth must be on.

**Platform notes:**

- Android: Bluetooth LE peer service; Wi-Fi Direct between Android phones.
- iPhone/iPad: Bluetooth LE peer service (iPhone ↔ Android or iPhone).
- **Not possible on:** Browser, Computer (emberd).

**Steps:** Turn on Bluetooth on both phones and add the phone-to-phone link. Chapter: [Phone to phone](phone-to-phone.md).

### Same Wi-Fi or Apple peer-to-peer, WebRTC pairing

Connects: devices on one network ↔ each other. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [device] --- same Wi-Fi / hotspot --- [device]      [iPhone] ))) Apple peer-to-peer ((( [iPhone]
```

**Optional:**

- **A shared Wi-Fi network or a phone hotspot** — Carries the multicast / peer traffic. Alternatives: Apple peer-to-peer (iPhones, no network needed); WebRTC pairing by QR on a hotspot.

**Platform notes:**

- iPhone/iPad: UDP multicast needs Apple's multicast entitlement; MultipeerConnectivity works without it.
- Browser: Browsers use WebRTC pairing (copy or scan the pairing blob), no UDP.

**Steps:** Join the same Wi-Fi; the LAN link finds others automatically. Chapter: [Phone to phone](phone-to-phone.md).

### Serial cable between two devices

Connects: device ↔ device by cable. Works on: Android, Browser, Computer (emberd).

```
   [device]--[USB-serial adapter]--TX>RX / RX<TX / GND--[USB-serial adapter]--[device]
```

**Required:**

- **USB serial adapter with a CP210x, CH340/CH341, FTDI or PL2303 chip (or a CDC-ACM device)** — One per device (USB devices are hosts, so two adapters are needed).
- **Null-modem cable (TX↔RX crossed, common ground)** — Connects the two adapters.

**Optional:**

- **USB-C (or micro-USB) OTG adapter** — Phones connect USB devices through it.

**Platform notes:**

- Android: Plug USB devices in with an OTG adapter; the app asks for USB permission.
- Browser: Web Serial works only in Chrome / Edge on a computer (not on phones, Firefox or Safari). Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.
- Computer (emberd): On Linux add your user to the dialout group (serial ports) and audio group (sound cards).
- **Not possible on:** iPhone/iPad.

**Steps:** Connect both adapters with the null-modem cable, add a stream link with preset cable. Chapter: [Phone to phone](phone-to-phone.md).

### Hub on a hotspot-capable phone or computer

Connects: hub ↔ every browser on its Wi-Fi. Works on: Android, iPhone/iPad, Computer (emberd).

```
   phones (browser or app) --- the hub's Wi-Fi hotspot --- [hub: computer or phone, :8437]
```

**Required:**

- **Hotspot-capable phone or computer** — Creates the Wi-Fi that guests join and serves the web app. Alternatives: an existing Wi-Fi router everyone can join.

**Optional:**

- **Power bank or charger** — A hub runs for hours with Wi-Fi on.

**Platform notes:**

- Android: Hotspot addresses are listed first in the hub URLs.
- iPhone/iPad: The hub runs only while the app is in the foreground; Personal Hotspot URL usually http://172.20.10.1:8437/.
- Computer (emberd): emberd serves the hub on port 8437; Raspberry Pi works as a hotspot hub.
- **Not possible on:** Browser.

**Steps:** Turn on the hotspot, start the hub, share the URL or QR with guests. Chapter: [Setting up a hub](hub-setup.md).

### Join a hub from any browser

Connects: phone or laptop ↔ hub. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [phone / laptop browser] --- hub Wi-Fi --- http://<hub address>:8437/
```

**Optional:**

- **Nothing extra** — Join the hub's Wi-Fi and open its address.

**Platform notes:**

- Browser: The hub page is plain http, so radios, microphone and camera are unavailable there; use the single file or an app for those.

**Steps:** Join the Wi-Fi, open `http://<hub address>:8437/`. Chapter: [Setting up a hub](hub-setup.md).

### Carry messages on a USB stick or card (sneakernet)

Connects: device ↔ storage ↔ device. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [device A] --export--> [USB stick / card / QR codes] --carry--> [device B] --import
```

**Optional:**

- **USB stick or memory card** — Carries exported bundles between devices. Alternatives: file sharing (Bluetooth, AirDrop, Quick Share); QR codes for small bundles.
- **USB-C (or micro-USB) OTG adapter** — Phones read USB sticks through it.

**Platform notes:**

- iPhone/iPad: Use the Files app for sticks on USB-C iPhones/iPads.

**Steps:** Export a bundle, carry it, import it on the other device. Chapter: [Sneakernet](sneakernet.md).

### QR contact cards and invites

Connects: screen ↔ camera. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [screen showing QR] ---> [camera of the other device]
```

**Optional:**

- **Nothing extra** — Shown on one screen, scanned by the other device's camera. Alternatives: copy and paste the ember: link.

**Platform notes:**

- Browser: QR scanning in the browser needs BarcodeDetector (Chrome on Android, macOS, ChromeOS). Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.

**Steps:** Show your card, scan it on the other device. Chapter: [Sneakernet](sneakernet.md).

### Morse by light (flashlight, lamp or screen)

Connects: light ↔ camera. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [flashlight / lamp / screen] ~~~ blinks ~~~> [camera, watched box on the light]
```

**Optional:**

- **Nothing extra** — The screen or torch flashes; another device's camera reads it. Alternatives: any flashlight or lamp keyed by hand.

**Platform notes:**

- Browser: Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.

**Steps:** Point the camera at the light and start Morse receive. Chapter: [Morse by light and sound](morse-reading.md).

### Morse by sound (tone, radio speaker, whistle)

Connects: sound ↔ microphone. Works on: Android, iPhone/iPad, Browser, Computer (emberd).

```
   [radio speaker / whistle / tone] ~~~ sound ~~~> [microphone]
```

**Optional:**

- **Nothing extra** — The device microphone hears the tone. Alternatives: radio speaker audio by cable.

**Platform notes:**

- Browser: Microphone, camera, Web Serial and Web Bluetooth need a secure page: the single file (embermesh.html), https or localhost. A hub page at `http://<ip>:8437/` is not secure.

**Steps:** Hold the device near the sound and start Morse receive. Chapter: [Morse by light and sound](morse-reading.md).

## A minimal kit per platform

| Platform | Bring |
|---|---|
| Android phone | OTG adapter, USB-C (or micro-USB) data cable, TRRS headset cable or USB-C headset adapter, a radio audio cable or interface for your radio, power bank |
| iPhone / iPad | Lightning/USB-C to 3.5 mm headset adapter, TRRS radio cable (VOX), or a Bluetooth LE TNC; power bank |
| Laptop (browser single file or emberd) | USB sound card or AIOC/Digirig-class interface, radio cable, USB-serial adapter, USB stick with the app |
| Raspberry Pi hub | Pi with Wi-Fi, power supply or battery and solar, SD card, radio interface, optional LoRa board |
