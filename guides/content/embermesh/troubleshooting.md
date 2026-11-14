---
id: embermesh/troubleshooting
title: "EmberMesh 29: Troubleshooting and Known Limits"
category: embermesh
tags: [embermesh, troubleshooting, problems, errors, fix, limits, untested, android, iphone, ios, browser, desktop, bluetooth, wifi, audio, kiss, meshtastic, rnode, lora]
summary: "How to verify a link yourself, symptom-cause-fix tables for every link type, message problems, and an honest list of what is untested or unsupported on each platform."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 29
---

# Troubleshooting and Known Limits

## Read this first: what has and has not been tested

**Proven** (by automated tests on macOS, the Android emulator and the iOS simulator):

- End-to-end messaging between real programs: desktop daemons (emberd) on one computer over Wi-Fi-style
  multicast, TCP, the hub WebSocket and a virtual serial cable, including relaying through a chain
  and store-and-forward between two nodes never online at the same time; browser tabs (Chrome) as
  hub guests and over WebRTC on one machine; the Android app on the emulator and the iPhone app in
  the simulator, each exchanging Public messages and encrypted direct messages with delivery
  receipts with a desktop daemon over real sockets; phone hubs (emulator and simulator) with a guest.
- The sound-card modem through **virtual** audio paths: between daemons over audio pipes (including
  a radio gateway where encrypted messages correctly stayed off the amateur link), through the
  Android app's real audio loops in the emulator, through the iPhone app's audio render and capture
  functions, and through Chrome's audio processing.
- The KISS, Meshtastic, RNode, Reyax and SDR drivers against emulators and hand-built protocol
  examples (KISS and SDR also between daemons).
- Identity, storage, passphrase lock and unlock, link persistence across restarts, the guide
  library.

**Not proven** (never exercised anywhere):

- **No real radio, TNC, LoRa board, SDR dongle, Bluetooth link, USB-serial adapter or microphone
  capture** has been used. Bluetooth phone-to-phone, Apple peer-to-peer and Wi-Fi Direct have never
  run between real devices; nor has a hub on a real phone hotspot.
- The desktop program has been built and run on macOS only: **the Linux (including Raspberry Pi)
  and Windows builds have not been compiled** in the current round (Windows never).
- **Firefox and Safari** (and mobile browsers) are untested; the browser capability table is from
  the browsers' documentation.
- Long unattended running, battery life and range on real hardware.

This does not mean it will not work. It means **you must test each link yourself** before you depend
on it, using the procedure below, and keep a fallback (voice radio, a courier, a bundle on a stick).

Platform-specific problem lists are at the end of each app guide: [Android](app-android.md),
[iPhone/iPad](app-ios.md), [browser](app-web.md), [computer](app-desktop.md). Missing a cable or
adapter? [What you need](what-you-need.md).

## How to verify a link yourself

1. **Link card state.** **More → Links & radios**: the card must say **Connected**. If it says
   **Error**, read the red line.
2. **Counters.** Expand **Statistics** on both ends. Send a Public message. The sender's **Sent**
   count must rise and, on the receiver, **Received**. **Errors** rising with no **Received** means
   something arrives but cannot be decoded.
3. **Ping.** Add the other node as a contact (**People → Nearby → +**), open it, tap **Ping**. A
   "Pong: ... round trip, N hops" answer proves both directions.
4. **Signal meters.** Audio and SDR links: the **RX level** meter on the link card (level, **CLIP**,
   **DCD**, **TX**, SNR, frames ok/bad and a plain-language hint). LoRa links: last RSSI and SNR in
   Statistics; also **Signal** on the contact page and in **People → Nearby**.
5. **Bench test, then distance.** Two stations side by side first, then move apart and ping again.
6. **Loopback.** "Loopback (testing)" is a link that talks to itself. It only shows that the app
   and its link machinery start; it does not test any hardware.
7. **Mesh statistics.** **People → Stats** (or **More → Mesh statistics**): frames sent and
   received, bad frames, duplicates dropped, relayed for others, airtime and duty cycle per link.

## Universal checks

| Symptom | Likely cause | Fix |
|---|---|---|
| A connection type shows **unavailable** | Not supported on this device, or a permission is missing | Read the reason under it; see the platform limits below |
| "Not supported on this device." / "Not available in this version yet." | That feature is missing on this platform or build | Use another device or method |
| Link **Error**, then nothing | Hardware unplugged, permission refused, wrong settings | Fix, then **Reconnect** |
| Link **Connected** but nobody appears | Other side uses a different kind of link or different settings | Same link type and settings on both ends; **Announce myself** |
| Everything worked, then stopped after the screen went off | Background restrictions | [Power and field operation](power-and-field.md) |
| App opens on **EmberMesh is locked** | A storage passphrase is set | Enter it; relaying continues while locked |
| "Storage is locked." errors | Locked | Unlock |
| A link stays **Down** after a restart, nothing reconnects | Storage passphrase set: a restarted device stays locked with no connections | Unlock it (hub: `--passphrase-file` or console `unlock`) |
| Android 17+: Wi-Fi links and the hub time out | Local network permission not granted | Allow it; open EmberMesh again to get the prompt |

## Message problems

| Symptom | Likely cause | Fix |
|---|---|---|
| Stays **Queued** | No link up, or no link allowed for this message | Add or fix a link. Encrypted messages never use amateur links: the message box shows "not via ... (no encryption allowed)" |
| Direct message stays **Sent**, never **Delivered** | Recipient out of reach; it is stored and retried | Wait; it is re-sent a few times at growing intervals, then stays stored and is handed over by store-and-forward until it expires |
| **Failed** | Retries exhausted or expired | Tap the message → **Retry** |
| **⚠ unverified sender** on a message, bulletin, check-in or SOS | The sender's key is unknown here | Add and verify the contact (words or QR); until then treat the name with suspicion. The tag disappears if an authenticated copy arrives |
| An SOS gave no notification | It came from an unverified sender (grouped below verified alerts, never notifying) | Check the SOS tab regularly; verify your people in advance |
| Messages appear in a strange order or expire early | A device's clock is badly wrong. The mesh does not correct a device whose clock looks sane | Correct the phone's clock, then **Settings → Clock → Set from this device** |
| A new stranger's chat is refused | Conversation table full of conversations you wrote in or with contacts | Clear old conversations (they are never deleted for strangers) |
| "File too large for the mesh" | Files are for small things only | Send text; carry large files by stick |
| A channel member cannot read the channel | Different name or passphrase | Rejoin from the invite QR/text |

## Bluetooth phone-to-phone

| Symptom | Likely cause | Fix |
|---|---|---|
| Finds nobody (Android) | Bluetooth off; Location off on Android 11 or older; permission refused | Turn on; allow **Nearby devices** |
| Android cannot find an iPhone | iPhone app in the background (iOS hides the advertisement) | Bring EmberMesh to the foreground on the iPhone |
| An old Android phone is never found | It cannot advertise | Let it connect to a newer phone instead |
| Connects, then drops | Distance or obstacles | Move closer; keep both apps open |

## Wi-Fi (UDP), TCP, hub and WebRTC

| Symptom | Likely cause | Fix |
|---|---|---|
| UDP: nobody on a hotspot | Client isolation | Run EmberMesh on the hotspot phone too; or **Extra peers** (Advanced) with addresses |
| UDP unavailable on iPhone | Needs a special Apple permission | Use **Local network listener** / TCP, Apple peer-to-peer, Bluetooth or a hub |
| TCP: connection refused | Wrong address or port; listener not running | Check the other device's IP address; default port 47475 |
| Hub page will not load | Not on the hub's Wi-Fi; typed wrongly; firewall | `http://` and `:8437`; allow emberd through the firewall |
| Hub page: "EmberMesh could not start" | Hub serves a web folder without the mesh core | Start emberd with `--web-root dist/web` |
| Hub guests got new identities | Hub address changed | Give the hub a fixed address; restore from backup |
| WebRTC: "Invalid invite" | Code damaged or incomplete | Copy the full `ember:rtc:` code, or scan the QR |
| WebRTC: paired, never connects | Not on the same network | Join the same Wi-Fi/hotspot |
| WebRTC link gone after restart | WebRTC links cannot be restored | Pair again |

## Sound-card modem (audio)

| Symptom | Likely cause | Fix |
|---|---|---|
| "set your callsign before transmitting on amateur bands", "Set your callsign to transmit on this link (amateur band)", or the link shown as blocked for lack of a callsign | Callsign is `N0CALL` with Band / licence amateur | Enter callsign (**Save callsign**), or licence-free where legal |
| Two radio stations do not see each other at first | They announce themselves when they first hear each other; with several stations or Robust modes this can take minutes | Wait; check that **DCD** lights on both when the other transmits |
| Packets start late or are clipped with a handheld | Wrong timing for the radio | Pick the matching **Radio setup** preset ([Handheld radios](handheld-audio-modem.md)) |
| Meter: **No audio arriving** | No input / microphone permission (browsers need `file://`, `https://` or `localhost`) | Check cable, input device, permission |
| **Too quiet** / **Too loud** / **CLIP** lamp | Receive level | Adjust radio volume or attenuation until **Level OK.** |
| Nothing decodes | Modem or Band / licence differ | Identical on all stations |
| Start of packets lost | Keying too slow | Raise **TX delay** and **VOX wake-up tone** |
| Never transmits | VOX off, PTT wrong, channel always "busy" | VOX on; check PTT; squelch just above the noise |
| "no audio output: the platform did not pull samples" | Output device problem | Choose another output device under **Settings** on the link card |
| Computer hub: **Error** "the sound device did not open within 8 s ..." | macOS microphone permission prompt waiting or refused | Allow microphone access for the program running emberd, or set the input to `none`; **Reconnect** |
| Licence-free link shows a station id like `QXABCD-9` | Callsign left at `N0CALL` in licence-free mode | Normal: a made-up id that can never be a real callsign |

More detail: [Handheld radios](handheld-audio-modem.md).

## KISS TNC

| Symptom | Likely cause | Fix |
|---|---|---|
| Serial link Down/Error | Port, baud, permission | 9600 / 57600; Linux: `dialout` group; macOS: `/dev/cu.*` |
| Connected, nothing sent | Callsign `N0CALL` | Set **Your callsign** |
| TNC in text mode | Needs KISS ON | **Enter KISS mode: Send KISS ON + RESTART** |
| ESP32 TNC resets | DTR/RTS toggling | DTR/RTS lines Off (computer) / Assert DTR off (Android) |

More detail: [KISS TNCs and packet radio](kiss-tnc-packet.md).

## LoRa

| Symptom | Likely cause | Fix |
|---|---|---|
| Meshtastic stuck **Connecting…**, then **Error** | Wrong port/baud, Meshtastic app on the same port, region unset | 115200; close the other app; set the region with the Meshtastic app |
| Meshtastic TCP fails | Another client connected | Only one TCP client at a time |
| Meshtastic up, no EmberMesh traffic | Channel slot, key or preset differ | Identical everywhere |
| RNode "no RNode answered" | Wrong firmware / not provisioned | `rnodeconf --autoinstall` |
| RNode "rejected the TX power setting" | Power too high for the board | Lower **Transmit power** |
| RNode/Reyax frequency error | Frequency outside the selected region | Right region, or **Other / custom frequency** |
| Reyax error with command and code | Value not accepted | Check ranges for the module model; 3.3 V |
| Airtime bar full | Legal duty cycle reached | Wait; send less |

More detail: [LoRa](lora.md).

## USB serial on Android

| Symptom | Likely cause | Fix |
|---|---|---|
| Device not listed | No USB host support, wrong cable, unknown chip | Use an OTG adapter (not a charging cable); powered hub for hungry radios; supported chips: FTDI, CP210x, CH340/CH341, PL2303 (genuine), CDC-ACM |
| Counterfeit "PL2303HXA" refuses | Fake chip | Use another adapter |
| Board resets | DTR/RTS | Turn off **Assert DTR** (Advanced) |

## SDR (rtl_tcp)

| Symptom | Likely cause | Fix |
|---|---|---|
| "not an rtl_tcp server" | Something else on the port | Check `rtl_tcp` and port 1234 |
| No decodes | Frequency (in Hz!), ppm error, gain, channel width | See [SDR](sdr-listening.md) |

## Known limits by platform

### Everywhere

- No real radio hardware tested (see the top of this chapter).
- No forward secrecy; who-talks-to-whom is visible ([Security](security.md)).
- Amateur links carry only cleartext; private traffic needs another path.
- A device with a storage passphrase has no connections after a restart until unlocked.

### Computer (emberd)

- Built and tested on macOS; **Linux / Raspberry Pi not built or run in the current
  round; Windows never compiled**.
- No Bluetooth connections; no `wss://` or `https://`.
- No file dialog: imports are taken from the data folder's `inbox/`, saves go to `saved/`.
- `--link` links exist only while the flag is used (add links from the operator page to keep them).

### Android app

- Proven on the Android 17 emulator (see top). **Never exercised:** real Bluetooth (phone-to-phone or
  TNC), USB-serial chips, classic Bluetooth serial, Wi-Fi Direct, Bluetooth headsets, real radios,
  a real phone hotspot. Multicast from another device into the emulator could not be tested.
- Android 17 and newer need the **local network** permission for every Wi-Fi link and the hub.
- Not available: WebRTC, Apple peer-to-peer, the operator page on its hub.
- Meshtastic over Bluetooth uses the **Meshtastic radio** device type of the Bluetooth LE serial
  device connection (compiled only, never run on hardware).
- Some brands kill background apps aggressively; see [Power](power-and-field.md).

### iPhone and iPad app

- Proven in the iOS simulator (see top). **Compiled only:** Bluetooth phone-to-phone, Bluetooth TNC /
  Meshtastic, Apple peer-to-peer, real audio input and output, discovery between devices, the hub on
  a real Personal Hotspot, background modes.
- No USB serial, no classic Bluetooth serial, no Wi-Fi Direct, no WebRTC.
- Wi-Fi UDP needs a special Apple permission and is normally unavailable.
- Background: only Bluetooth keeps running; the hub and Wi-Fi links stop when the app leaves the
  screen. Android phones generally cannot find a backgrounded iPhone.
- PTT only by VOX. Acoustic coupling: expect only the slowest modes to work.
- A free Apple ID signature expires after 7 days; an expired app will not start offline.

### Browsers (single file, hub page)

- Proven in headless Chrome on macOS only. **Firefox, Safari and mobile browsers are untested.**
- Real microphone capture, real Web Serial / Web Bluetooth devices and WebRTC between two separate
  devices have not been exercised.
- USB serial only in Chrome/Edge on computers; Bluetooth only in Chrome (Linux: behind a flag) and
  Chrome on Android. Meshtastic over browser Bluetooth does not work (no Meshtastic Bluetooth
  profile).
- Microphone, camera, serial, Bluetooth, location and notifications need a secure page (`file://`,
  `https://` or `localhost`); a plain `http://` hub page has none of them and cannot be installed
  for offline use.
- QR scanning only where the browser supports it (Chrome on Android, Chrome on macOS/ChromeOS).
- A second tab runs a temporary node with its own identity.
- Background tabs are slowed or frozen; WebRTC links must be re-paired after a restart.
- After changing a connection's settings in a browser, reload the page for them to take effect
  (known bug).
