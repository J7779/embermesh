---
id: embermesh/handheld-audio-modem
title: "EmberMesh 17: Handheld Radios with the Sound-Card Modem"
category: embermesh
tags: [embermesh, audio, modem, handheld, walkie-talkie, baofeng, kenwood, cable, trrs, usb sound card, vox, ptt, acoustic coupling, afsk1200, robust, txdelay, squelch, levels]
summary: "Turning any handheld radio into a data link with the audio link: acoustic coupling, cables for phones and computers, VOX versus serial PTT, setting levels with the app's meters, choosing a modem, TX delay, squelch, and fixing decode failures."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 17
---

# Handheld Radios with the Sound-Card Modem

The **Sound-card modem (radio audio)** link (`audio`) turns messages into sound. Any radio that
can carry a voice can carry EmberMesh: the cheapest walkie-talkie, a ham handheld, a base station.
The phone or computer is connected to the radio by a cable, or simply **held against it**.

> **Hardware status:** the modems were tested through a software channel simulator and between real
> programs over virtual audio paths (desktop daemons over audio pipes, the Android emulator, the iOS
> simulator and Chrome), but **never with a real radio, cable, sound card or microphone capture**.
> The levels and timings below are starting points. Verify your setup with the bench test at the
> end of this chapter.

Which cable, sound card or interface to get for your phone or computer: [What you
need](what-you-need.md).

## Before you start: licence

- **Amateur (ham) bands** (for example 2 m, 70 cm): you need a licence. Keep **Band / licence:
  Amateur radio (callsign, no encryption)** and set your callsign once in **Settings → Profile →
  Callsign** (the wizard and the Links screen also ask for it with **Your callsign is needed**).
  The node copies it into the link; you can add an SSID such as `-7` in the link's **Your
  callsign** field. Until a real callsign is set the link refuses to transmit: it shows that it is blocked for lack of
  a callsign, and its error line reads "Set your callsign to transmit on this link (amateur band)". No encryption is
  carried.
- **Licence-free services** (FRS, PMR446, MURS, CB, ...): check whether your service allows data
  transmissions; rules differ by country and some services allow voice only. Then choose
  **Band / licence: Licence-free or emergency use (encryption allowed)**. Leave the callsign at
  `N0CALL`: the link then identifies itself with a made-up station id such as `QXABCD-9` (it
  starts with QX and contains no digit, so it can never be a real callsign), shown in the link's
  Statistics.
- In a genuine emergency where life is in immediate danger, most jurisdictions permit any means
  of radio communication to summon help (the band-plan notes in **Tools → Band plans** say the
  same). Return to normal rules when the emergency is over.

**Everyone on the frequency must use the same Modem and the same Band / licence choice.** With
the Robust modems the two licence settings put different framing on the air, so a licence-free
station and an amateur station will not understand each other.

## Fastest path: the "Connect a radio" wizard

**Links & radios → Add connection → Connect a radio (guided)** (or **Instructions** (the **Help** tab on phones) **→ Connect a
radio**; in Simple mode **Connections** and **How to use**) asks which radio you have (22 known models plus generic entries), how it is connected,
shows what you will need and the settings to make on the radio, asks for band and licence, creates
the link with the right **Radio setup** preset, then shows the level meter and an on-air test. For
the Baofeng UV-5R family there is a full walkthrough: [Baofeng UV-5R](baofeng-uv5r.md).

## How stations find each other on a slow channel

Stations on a slow shared radio channel (sound-card modems, KISS TNCs, LoRa) now find each other by
themselves even when they are switched on at different times: the first time a station hears a new
neighbour it announces itself, and a first message carries its sender's announcement with it, so it
can be checked at once. In tests over simulated audio channels, stations switched on in random order
up to a minute apart all held each other's keys after (median / worst) 18 / 49 s for two stations,
53 / 67 s for three and 82 / 203 s for five at AFSK 1200; at Robust 250 it took 58 / 79 s, 140 / 171 s
and 292 / 559 s. The first Public message between strangers reached every station in a median of
about 4-5 s at AFSK 1200 (about 20 s at Robust 250). Stations also wait their turn after hearing
someone, so they rarely talk over each other.

## Radio setup presets

A sound-card link's **Radio setup** field picks a set of tested timing and level values. Any field
you set yourself wins; the link list shows the active preset and the settings actually in use (the
values you set yourself, plus the preset's values for everything else, and the callsign actually
transmitted).

| Radio setup | Use when | Modem | VOX wake-up tone | TX delay | TX tail | Transmit volume | Packets per transmission |
|---|---|---|---|---|---|---|---|
| **Handheld radio keyed by its own VOX (audio cable)** | A handheld keyed by its VOX from a K1/audio cable | AFSK 1200 | 400 ms at 1200 Hz | 600 ms | 150 ms | 0.35 | 8 |
| **Handheld radio, PTT keyed by a serial line or interface (AIOC, Digirig)** | PTT by a serial RTS/DTR line or an AIOC/Digirig-class interface | AFSK 1200 | – | 450 ms | 50 ms | 0.40 | 6 |
| **No cable: device speaker/microphone held to the radio, PTT by hand** | Acoustic coupling; you press PTT | Robust 125 | – | 1500 ms | 400 ms | 0.80 | 2 |
| **Mobile / base radio with a data port or USB sound card** | A data port or a radio with built-in USB audio | AFSK 1200 | – | 300 ms | 50 ms | 0.30 | 4 |
| **Custom (set everything yourself)** (default) | Everything by hand | AFSK 1200 | – | 300 ms | 50 ms | 0.50 | 4 |

Wait-for-a-clear-channel is on in all of them. Why: VOX needs 200-600 ms of audio before it keys and
the far receiver's squelch about 150 ms more, so the VOX preset leads in for about a second and
sends all queued packets in one key-up so VOX stays held. A human reacts 0.5-1 s after the app
starts sending, hence the long lead-in of the no-cable preset. Measured through software models of
these radios, the presets delivered every test message (for example 8/8 single messages through the
VOX model, where the custom defaults lost all 8). No real radio has been tested.

## No cable: hold the phone to the radio (acoustic coupling)

Works with any radio, slowly.

On the **radio**: tune to the agreed frequency ([Running a net](running-a-net.md)); squelch low but
closed; volume moderate (too loud distorts in the phone's microphone); key and roger tones off;
lowest power that reaches the other station.

In **EmberMesh** (or use the wizard):

1. **More → Links & radios → Add connection →** the audio entry: "Audio (soft modem)" (Android),
   "Sound (radio cable or speaker)" (iPhone, with **Transmitter keying: I press PTT myself
   (acoustic / hand-held)** and **Play through: Loudspeaker (acoustic)**), "Sound card / radio audio"
   (browser), "Sound card (radio audio)" (computer).
2. Under **Protocol**, **Radio setup: No cable: device speaker/microphone held to the radio, PTT by
   hand**. **Band / licence** and callsign as above.
3. Name it and tap **Connect**; allow the microphone; set the level in **Set the audio level**.

Hold the phone **1-3 cm** from the radio, its microphone and speaker (usually at the bottom edge)
against the radio's grille, in a **quiet place**. When the link starts to send (**TX ON AIR** on the
card, the **TX** lamp on the meter), **press and hold the radio's PTT** until it stops.

**You should see:** on the receiving side **DCD** lights, **frames ok** counts up and the message
appears. Short messages take many seconds at Robust 125: keep them short.

If your radio has VOX you can instead turn VOX on and let it key itself (no PTT pressing), with the
VOX preset values for the lead-in.

All settings, both the **Protocol** ones (preset, modem, callsign, TX delay ...) and the
**Connection** ones (input and output device, Transmit level, PTT), can be changed later with the
link card's **Settings** button. Changing a connection setting closes the connection and opens it
again.

## Cables

A cable is faster and far more reliable than acoustic coupling: it allows **AFSK 1200** (about
ten times faster than Robust 125) and works in noisy places.

### The radio side: the two-pin "Kenwood-style" handheld connector

Kenwood, Baofeng and many other cheap handhelds have a speaker-microphone socket with **two
round plugs side by side: one 3.5 mm and one 2.5 mm**. Usually the 3.5 mm plug carries the
speaker audio (radio to you) and the 2.5 mm plug carries the microphone (you to radio).

The exact contacts, and how the push-to-talk button is wired, **differ between makes**. Do not
guess:

- **Best:** use a ready-made radio interface cable for your radio (sold as "APRS cable",
  "packet cable" or a sound-card interface such as a Digirig), or the cable from a speaker-mic.
- **Or measure:** take a working speaker-microphone or headset for that radio. With a multimeter
  on its plugs, find which contacts connect when its PTT button is pressed, and which contacts
  lead to its speaker and to its microphone. Copy that wiring.
- Check your radio's manual; many printed manuals show the accessory socket.

### The phone side: TRRS headset jack (CTIA standard)

A phone with a 3.5 mm headset jack takes a four-contact (TRRS) plug:

```
   tip    ring 1   ring 2   sleeve
  [ L out | R out | ground | mic in ]      (CTIA standard, most phones)
```

Phones without a jack: use a USB-C to 3.5 mm headset adapter, or a USB sound card (CM108-style
dongles and class-compliant interfaces work over USB-C or, on iPhone, the Lightning camera
adapter). **Avoid Bluetooth headsets**: they run at 8-16 kHz with heavy voice processing.

### Wiring the two directions

Audio levels (the canonical figures from the link driver documentation). Phone and headset
microphone inputs expect roughly 10-50 mV; radio speaker and line outputs give 0.3-2 V.

| Path | Attenuation | Example |
|---|---|---|
| Radio speaker / line out → computer **line in** | none (turn the radio volume down) | direct |
| Radio speaker / line out → phone/headset **mic in** | 20:1 to 100:1 | 10 kΩ in series, 100-470 Ω to ground (plus about 2.2 kΩ across mic and ground if the phone needs it to detect a headset) |
| Phone/computer headphone or line out → radio **mic in** | about 100:1 + DC block | 10 kΩ in series, 100 Ω to ground, 1 µF in series |
| Phone/computer out → radio data / accessory (line-level) input | none to 10:1 | per the radio manual |

```
 RADIO SPEAKER --> PHONE MIC (receive)

   radio spk + ---[ R1 10 k ]---+---------------> phone MIC (sleeve)
                                |
                             [ R2 100-470 R ]  [ 2.2 k ] from MIC to ground,
                                |               if the phone needs it to detect a headset
   radio spk - / ground --------+---------------> phone GROUND (ring 2)

 PHONE EARPHONE --> RADIO MIC (transmit)

   phone L out (tip) ---[ C 1 uF ]---[ R3 10 k ]---+---> radio MIC input
                                                    |
                                                 [ R4 100 R ]
                                                    |
   phone GROUND (ring 2) ---------------------------+---> radio ground
```

- Resistor values are not critical. Keep wires short and away from the antenna.
- Then fine-tune in the app with the level meter (below): receive volume until the meter sits
  around -30 to -10 dBFS without clipping; transmit volume until the far end decodes.
- On a computer the same applies to a USB sound card's mic or line input and headphone output.

## Keying the transmitter (PTT)

### VOX (works on every platform; the only option on iPhone)

Easiest: **Radio setup: Handheld radio keyed by its own VOX (audio cable)** sets all of this. By hand:

1. Turn on the radio's VOX.
2. In the link settings set **VOX wake-up tone** to 200-500 ms (it plays a tone so VOX has time to
   key up; **VOX tone pitch** is under Advanced settings, default 1000 Hz).
3. Set **TX delay** to 300-600 ms.
4. If the first part of each packet is lost, increase both.

VOX may also key on noise or voices near an acoustic setup. Keep the room quiet.

### Serial-line PTT (computers, Android, browsers)

A USB-serial adapter's **RTS** or **DTR** line, through a small transistor or opto-isolator
circuit (or a commercial interface), closes the radio's PTT contact.

- **Computer (emberd):** in the sound-card connection set **Transmit keying (PTT)** to **Serial
  port RTS line** or **Serial port DTR line** and **PTT serial port** (e.g. `/dev/ttyUSB0`,
  `/dev/cu.usbserial-1410`, `COM3`). **Invert PTT line** is under Advanced settings.
- **Computer with CAT control:** **CAT via Hamlib rigctld**: run `rigctld -m <model> -r /dev/ttyUSB0`
  (port 4532). Advanced settings can also set the radio's frequency and mode at start.
- **Android:** in the audio connection choose **Push-to-talk (PTT)**:
  - **VOX / none (the radio keys itself)**;
  - **Serial connection "name" (RTS/DTR)**: a serial link you already have open to the radio (for
    example its data cable);
  - **USB serial adapter ..., PTT only**: a plugged-in USB-serial adapter used only for keying.
  Choose **PTT line** RTS or DTR (and **Invert PTT** if needed) under Advanced settings. PTT and
  transmit level can be changed while the link runs. Android holds PTT for the phone's audio
  output delay after the data ends, so the tail is not cut off.
- **Browser (Chrome/Edge):** the sound-card connection offers PTT on a serial port's RTS or DTR.
- **iPhone:** VOX only.

Example command line for a Linux hub with a USB sound card and RTS PTT:

```sh
emberd --link '{"endpoint":{"kind":"audio","config":{"input_device":"USB Audio","output_device":"USB Audio","sample_rate":48000,"ptt":"serial_rts","ptt_port":"/dev/ttyUSB0"}},"link":{"type":"audio","config":{"callsign":"QX0ABC-1"}}}'
```

`emberd --list-audio` and `emberd --list-serial` list the device names. A `--link` given on the
command line lasts only while the flag is used; add the link from the operator page to keep it.

## Setting levels with the app's level meter

Every sound-card link (and SDR link) shows a live **RX level** meter on its card in **Links &
radios**, and the add-connection wizard shows the same meter as its last step (**Set the audio
level**).

```
 RX level  [##########|.......:...]  -24 dBFS      <- bar, peak mark, level
 [CLIP] [DCD] [TX]   SNR 18 dB   frames 12 ok / 0 bad
 Level OK. (ok)                                     <- hint
```

| Part | Meaning |
|---|---|
| Bar and dBFS figure | Receive level, -60 dBFS (empty) to 0 dBFS (full scale); a thin mark shows the recent peak |
| **CLIP** lamp | Input is clipping (more than 0.1 % of samples at full scale): too loud |
| **DCD** lamp | The modem hears a signal |
| **TX** lamp | The link is transmitting |
| **SNR** | Signal-to-noise of the last decoded packet ("—" until one is decoded) |
| **frames ok / bad** | Packets decoded with a good check / packets that failed |
| Hint | **No audio arriving** (nothing for 2 s: cable, input device, microphone permission), **Too quiet** (below -50 dBFS), **Level OK.**, **Too loud** (peaks above -1 dBFS), **Clipping** |

The meter updates a few times a second while it is on screen. A driver error is shown in red under
the hint. **Statistics** lists the same values in full (`rx_level_dbfs`, `rx_peak_dbfs`,
`clip_percent`, `snr_db`, `frames_ok`, `frames_bad` ...).

**Receive level:**

1. Have the other station transmit (send a message, or hold their PTT and talk), or let the radio
   receive a busy channel.
2. Turn the radio volume until the bar sits in the middle (about -30 to -10 dBFS), the hint says
   **Level OK.** and **CLIP** stays off.
3. **Clipping** or **Too loud**: turn the radio down or add attenuation. **Too quiet**: turn it up.
4. When packets arrive, **DCD** lights and **frames ok** counts up. If **frames bad** rises instead,
   the level or the sender's audio is wrong.

**Transmit level:** the protocol setting **Transmit volume** (0.01-1, default 0.5); Android also has
**Transmit level** (percent) and computers **Transmit gain** (dB) on the connection. Listen to your
transmission on a second radio: it should sound like a clean warble, not a harsh buzz. If it sounds
distorted, or the far end's **frames bad** rises, lower it.

## Choosing the modem

All modems below are in the **Modem** list. Measured thresholds are from the software channel
simulator (signal-to-noise ratio in a 3 kHz band at which half of the test packets got through;
lower is better). Real radios will be worse.

| Modem (as listed) | Speed | Needs about (50 % of packets) | Use for |
|---|---|---|---|
| **AFSK 1200 - VHF/UHF packet, works with APRS gear** (default) | 1200 bit/s | +4.6 dB (flat audio), +5.6 dB (typical FM audio) | Cable to a VHF/UHF FM radio. Talks to TNC and APRS users. |
| **AFSK 300 - HF packet** | 300 bit/s | -1.2 dB | HF single-sideband radios |
| **PSK 2400 - fast, audio cable only** | 2400 bit/s | +7 dB (clean cable path) | Good cable to a radio with a flat data port; not for normal FM mic/speaker audio |
| **PSK 4800 - fastest, clean audio cable only** | 4800 bit/s | +13.5 dB | Same, very clean paths only |
| **Robust 250 - error-corrected, good audio** | 250 bit/s | -7 dB | Cable on a noisy channel |
| **Robust 125 - error-corrected** | 125 bit/s | -9.7 dB | Weak signals |
| **Robust 62 - phone held to a radio, weak signals** | 62.5 bit/s | -12.7 dB | **Acoustic coupling**, very weak signals |
| **Robust 31 - weakest signals, very slow** | 31 bit/s | -15.5 dB | Last resort |
| **Morse code (CW)**, **RTTY 45 baud**, **PSK31**, **DTMF touch tones** | - | - | Text only, for talking to people and other radio systems (see below) |

Everyone on a Robust channel should keep **Largest frame** (Advanced) at its default 0 (=128
bytes): the receiver uses it to reject noise that looks like a frame header.

Speeds are before overheads. In the simulator the robust modes decode signals roughly 12 dB
(Robust 250) to 20 dB (Robust 31) weaker than AFSK 1200 needs, but they are much slower: send
short messages. Their framing is AX.25 when **Band / licence** is amateur
and a bare frame when licence-free.

## Timing settings

| Setting | Default | When to change |
|---|---|---|
| **TX delay** | 300 ms | Raise to 400-600 ms for VOX and cheap handhelds, or if the far end misses the start of packets |
| **TX tail** (Advanced) | 50 ms | Raise slightly if the end of packets is cut off |
| **VOX wake-up tone** | 0 (off) | 200-500 ms when using VOX |
| **Wait for a clear channel** | on | Leave on: the link listens before sending |
| **Persistence**, **Slot time** (Advanced) | 63, 100 ms | Lower persistence on a busy channel |
| **Packets per transmission** (Advanced) | 4 | Fewer on very slow modes |
| **Send each packet** (Advanced) | 1 | 2 or 3 on very lossy paths; duplicates are discarded |
| **Duty-cycle limit** (Advanced) | 1000 ‰ (none) | Set if your band has a legal limit |

## Squelch

The squelch decides when the radio's speaker opens.

- Set it **low but closed**: the radio is silent when nobody transmits (UV-5R: 1-3). Too high hides
  weak packets; **fully open (0) lets noise keep the channel busy**, because the link waits for a
  clear channel before sending.
- If the link never transmits (**DCD** stays lit, the `csma_deferrals` count in Statistics keeps
  rising), raise the squelch a step.
- If the other station misses the start of packets, its squelch or VOX is slow: raise **TX delay**
  (the presets already allow for it).
- The **DCD** lamp goes out by itself when audio stops arriving, so a dead input cannot block
  transmitting.
- **CTCSS/DCS tones** (sub-audible tones) work as a squelch filter too. Everyone must use the same
  tone; put it in the net plan. For simplex data, off is simplest.

## Keyboard modes: Morse, RTTY, PSK31, DTMF

With the modem set to **Morse code (CW)**, **RTTY 45 baud**, **PSK31** or **DTMF touch tones**,
the link carries **plain text only**, not mesh messages:

- Send with **Plain-text message** on the link card.
- Text heard on the channel appears in Chats as a radio conversation (named after the mode).
- **CW** is useful to reach people with only a radio and a pencil: they can copy Morse by ear. In
  **Modem fine-tuning** (Advanced) type `wpm=12` for a slower speed (default 20 wpm).

## Bench test (do this before going into the field)

0. **Computers only, no hardware:** two emberd daemons can be wired together with the virtual sound
   card `audio_pipe` (named pipes carrying raw audio), which is how the modem was tested; see
   `apps/desktop/README.md`.

1. **Without radios:** two phones, both with an audio link set to **Robust 62** and **Band /
   licence: Licence-free** (no radio is involved). Hold them speaker to microphone in a quiet
   room. Send a Public message. This proves the modem and the apps.
2. **With radios on the bench:** two radios a few metres apart on low power, same frequency, set
   the levels as above, send messages both ways, then **Ping** each other.
3. **Switch to AFSK 1200** if you have cables, and repeat.
4. **Move apart** and test again at the real distance.

## If it doesn't work

| Symptom | Likely cause | Fix |
|---|---|---|
| Red line "set your callsign before transmitting on amateur bands" | Band / licence is amateur and callsign is `N0CALL` | Enter your callsign in the link settings, or choose licence-free if that is legal for you |
| Radio never transmits (**TX** lamp lights, radio stays silent) | VOX not on, VOX level too low, or serial PTT wrong | Turn VOX on and up; set VOX wake-up tone; check the PTT choice and line; try **Invert PTT line** / **Invert PTT** |
| "no audio output: the platform did not pull samples" | Output device not working or busy | Choose another output device in **Settings**; close other apps using the sound card |
| Computer hub: link goes to **Error** "the sound device did not open within 8 s..." | On macOS the microphone permission prompt is waiting or was refused | Allow microphone access for the program that runs emberd (System Settings → Privacy & Security → Microphone), or set **Input (from radio)** to `none` for transmit only; then **Reconnect** |
| Meter hint **No audio arriving** | Input not connected or not permitted | Check cable and input device, microphone permission (browsers: only on `file://`, `https://` or `localhost` pages) |
| **frames bad** rises, **frames ok** does not | Level wrong or distortion | Set the receive level to **Level OK.**; lower the sender's transmit volume |
| Nothing decodes at all | Different modem or Band / licence on the two sides | Make both identical |
| Beginning of packets lost | Radio or VOX slow to key | Increase TX delay (and VOX wake-up tone) |
| Works at home, fails outdoors | Wind and noise (acoustic) or weak signal | Shield the phone and radio from wind; use a slower Robust mode; get higher |
| Radio keys up forever | PTT line inverted or stuck | Toggle **Invert PTT line**; remove the link to release |
| Long delays before sending | Channel busy or hiss holding carrier detect | See Squelch above |
