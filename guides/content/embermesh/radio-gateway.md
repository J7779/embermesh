---
id: embermesh/radio-gateway
title: "EmberMesh 15: Radio Gateway - Connecting a Hub or Phone to a Radio"
category: embermesh
tags: [embermesh, gateway, radio, tnc, kiss, lora, meshtastic, rnode, sound card, sdr, amateur, licence-free, bridge]
summary: "How a hub or phone with a radio links the local crowd to distant groups, how to choose between the sound-card modem, KISS TNCs, LoRa devices, serial radio modules and SDR receive, and which devices support which."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 15
---

# Radio Gateway: Connecting a Hub or Phone to a Radio

Wi-Fi and Bluetooth reach a building. Radios reach the next valley. A **gateway** is any
EmberMesh node that has a local link (Wi-Fi hotspot, Bluetooth) **and** a radio link. It relays
between them automatically; there is no routing to configure.

```
  Camp A                                                     Camp B
  phones --Wi-Fi-- [hub + radio] )))  ~~~ 5-30 km ~~~  ((( [hub + radio] --Wi-Fi-- phones
                                          |
                                   ((( [relay on a hill] )))      (optional, see Relays)
```

> **Hardware status:** none of the radio drivers has been tested on a real radio, TNC, LoRa board
> or SDR dongle yet. They were verified against emulators, hand-built protocol examples and a
> software channel simulator; the sound-card modem also ran between real programs through virtual
> audio paths, and KISS, serial and SDR links between real desktop daemons over virtual cables.
> Test every radio link yourself with the procedure at the end of this chapter before you depend
> on it.

## Choose your radio path

| Path | Link type (as shown under "What is on the other end?") | Hardware | Speed | Licence | Chapter |
|---|---|---|---|---|---|
| Sound-card modem | **Sound-card modem (radio audio)** (`audio`) | Any voice radio + audio cable, USB sound card, or phone held to the radio | 1200 bit/s (AFSK 1200) down to about 31 bit/s (Robust 31) | Amateur bands, or licence-free services where data is allowed | [Handheld radios](handheld-audio-modem.md) |
| KISS TNC | **KISS TNC (packet radio)** (`kiss`) | Hardware TNC, Direwolf, Mobilinkd, radios with built-in TNC, NinoTNC, LoRa KISS modems | 300 / 1200 / 9600 bit/s | Normally amateur | [KISS TNCs](kiss-tnc-packet.md) |
| Meshtastic device | **Meshtastic device (LoRa)** (`meshtastic`) | Heltec, T-Beam, RAK, T-Echo, Station G2 ... | About 1 kbit/s at LONG_FAST | Licence-free ISM | [LoRa](lora.md) |
| RNode | **RNode LoRa radio** (`rnode`) | LoRa boards with RNode firmware | About 1.7 kbit/s at the defaults | Licence-free ISM | [LoRa](lora.md) |
| AT-command LoRa module | **LoRa AT-command module (Reyax)** (`lora_at`) | REYAX RYLR998/498/896/406 | About 1.7 kbit/s at the defaults | Licence-free ISM | [LoRa](lora.md) |
| Transparent serial radio | **Serial / TCP / Bluetooth byte stream** (`stream`) with a radio preset | Ebyte E22/E32, HC-12, SiK/3DR telemetry, XBee | 2.4-32 kbit/s | Depends on band | [LoRa](lora.md) |
| Listen only | **SDR receiver (rtl_tcp)** (`rtl_tcp`) | RTL-SDR dongle | as the modem | Receive needs no licence | [SDR](sdr-listening.md) |

Rules of thumb:

- **You only have walkie-talkies or ham handhelds:** sound-card modem.
- **You have Meshtastic radios already:** use them; it is the easiest long-range option and needs
  no licence.
- **You are a licensed amateur with packet gear or Direwolf:** KISS TNC, and you can exchange
  text with ordinary APRS stations.
- **You want to hear a distant net without transmitting:** SDR.
- Everyone on one frequency must use the **same** method and settings (same modem, same LoRa
  settings). Write them in the net plan ([Running a net](running-a-net.md)).

## Which device can drive which radio?

What each platform offers today. The list of protocols for each connection now comes from the
mesh core itself, so all apps agree:

| | Computer (emberd hub) | Android app | iPhone app | Browser (Chrome/Edge on a computer) |
|---|---|---|---|---|
| Sound-card modem | Yes; PTT by VOX, serial RTS/DTR or CAT (rigctld) | Yes; PTT by VOX or the RTS/DTR line of a serial connection or USB-serial adapter | Yes; **VOX only** | Yes; PTT by VOX or serial RTS/DTR |
| KISS TNC over USB/serial | Yes | Yes (USB OTG) | No (no USB serial on iPhone) | Yes (Web Serial) |
| KISS TNC over TCP (Direwolf) | Yes | Yes | Yes | No |
| KISS TNC over Bluetooth | No | Yes (Bluetooth LE serial device, or Bluetooth serial SPP) | Yes (Bluetooth LE, including the Mobilinkd BLE KISS service) | Yes (Bluetooth LE) |
| Meshtastic | USB or TCP (port 4403) | USB, TCP or Bluetooth | Bluetooth or TCP | USB |
| RNode | USB / TCP | USB / TCP | Bluetooth (serial bridge) / TCP | USB |
| Reyax AT module | USB / TCP | USB / TCP | Bluetooth (serial bridge) / TCP | USB |
| SDR (rtl_tcp) | Yes | Yes (TCP client to the `rtl_tcp` computer) | Yes | No |

- Browsers list Meshtastic for their Bluetooth connection, but do not speak the Meshtastic
  Bluetooth protocol: connect Meshtastic radios to a browser by USB. On Android choose **Device type:
  Meshtastic radio (Meshtastic Bluetooth API)** in the Bluetooth LE serial device connection.
- Firefox and Safari have no serial or Bluetooth access; they should manage the sound card but
  have not been tested.
- A computer can also use a **virtual sound card** (`audio_pipe`: named pipes carrying raw audio)
  to connect EmberMesh to other programs such as Direwolf, sox or arecord/aplay (Linux and macOS).

## The legal rule that shapes a gateway

On amateur bands encryption is not allowed and every transmission must carry your callsign.
EmberMesh enforces this on every link marked **No encryption (amateur)**:

- Only signed **cleartext** goes on such links: the **Public** channel, open channels, SOS,
  check-ins, bulletins, net plans, announcements, pings and receipts.
- **Encrypted direct messages and private channels never go out on them.** They wait for another
  path (a licence-free LoRa link, Wi-Fi, a USB stick).
- The link refuses to transmit until you set a real callsign (the default placeholder is
  `N0CALL`). Set it once in **Settings → Profile → Callsign**: the node copies it into every
  amateur link. The Links screen and the add-connection wizard show **Your callsign is needed**
  with a **Save callsign** button when it is missing. On a computer hub, `emberd --callsign QX0ABC`
  does the same.

So a gateway on a ham link carries the hotspot crowd's **Public** chat, SOS and bulletins to the
far camp, but not their private messages. If private traffic must cross, add a licence-free link
(LoRa) or carry bundles by hand.

## Adding any radio link (general procedure)

1. Connect the hardware (cable, USB, Bluetooth pairing, Direwolf running ...).
2. **More → Links & radios → Add connection.** On a hub, do this on the **operator page**.
3. Choose the connection (how the device is attached): USB serial, Bluetooth, TCP, sound card ...
   If **Scan for devices** is offered, tap it and pick your device from the list.
4. Under **Protocol → What is on the other end?** pick the link type from the table above.
5. Fill the settings. Fields under **Advanced settings** can usually stay as they are.
6. Type a **Name** you will recognise ("Baofeng on 2 m", "Meshtastic T-Beam").
7. Tap **Connect**.

On a headless hub the same can be done with a command-line flag, e.g.
`emberd --kiss-tcp 127.0.0.1:8001` for Direwolf, or with `--link '<json>'` (examples in each
chapter and in `apps/desktop/README.md`).

### Reading the link card

| Item | Meaning |
|---|---|
| **Connected / Connecting… / Down / Error** | Link state. LoRa devices stay at Connecting until the device answers. |
| Red line | The last error in words (for example "set your callsign before transmitting on amateur bands"). |
| Tags | Speed, maximum frame size (MTU), half duplex, broadcast, **No encryption (amateur)** or **encryption ok**, **station ID**, **duty limit**. |
| **Airtime used** bar | Shown when the link has a legal duty-cycle limit. |
| **TX ON AIR** | Shown while a sound-card link keys the transmitter. |
| **RX level** meter with **CLIP**, **DCD** and **TX** lamps, SNR and frame counts | Sound-card and SDR links: see [Handheld radios](handheld-audio-modem.md) |
| **temporary** | The link is not saved and disappears after a restart |
| ▲ TX / ▼ RX | Data moving right now. |
| **Statistics** | Frames and bytes sent and received, errors, airtime, last heard, queue, and every driver detail (signal levels, device information) in full. |
| **Reconnect**, **Settings**, **Plain-text message**, **Remove**, **Help** | Actions. **Settings** changes both the protocol settings and the connection (device, port, PTT ...); changing the connection closes and reopens it. **Help** opens the matching manual chapter. **Plain-text message** sends ordinary text that non-EmberMesh stations can read (APRS, Meshtastic, Morse...). |

## Verify every radio link yourself

Because the radio paths are untested on real hardware, prove each one before relying on it:

1. **Bench test first.** Put two complete stations (radio + device) a few metres apart, set to the
   same frequency and settings, with low power.
2. On both, open the link's **Statistics**. Send a **Public** message from one side.
   - The sender's **Sent** count rises; the receiver's **Received** count rises.
   - If **Received** stays at zero but **Errors** rises, the signal arrives but cannot be decoded:
     check audio levels or radio settings.
   - If nothing changes on the receiver, nothing arrives: check frequency, PTT/keying, cables.
3. Add each other as contacts and use **Ping** (**People → Nearby** or the contact page). A pong
   with a round-trip time proves both directions.
4. Move apart step by step and ping again to learn the real range.
5. Write down the working settings and put them in the net plan.

**People → Stats** (also **More → Mesh statistics**) shows the airtime and duty cycle used per
link and how many messages were relayed for others.
