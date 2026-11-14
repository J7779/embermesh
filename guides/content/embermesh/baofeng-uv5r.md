---
id: embermesh/baofeng-uv5r
title: "EmberMesh 18: Messages Through a Baofeng UV-5R (and Other Radios)"
category: embermesh
tags: [embermesh, baofeng, uv-5r, bf-f8hp, uv-82, handheld, vox, ptt, k1, aioc, digirig, acoustic, radio wizard, quansheng, yaesu, kenwood, icom, anytone, frs, cb]
summary: "A careful, complete walkthrough for sending EmberMesh messages through the common Baofeng UV-5R handheld: what you need, the radio settings from the app's verified profile, the VOX, PTT-interface and no-cable methods, setting levels, a first on-air test, running it from Android, iPhone, a browser or a computer, legal notes, common failures, and a shorter guide to the other radios the app knows."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 18
---

# Messages Through a Baofeng UV-5R (and Other Radios)

The Baofeng UV-5R (and its siblings UV-5RA, UV-5RE and BF-F8HP) is the cheap two-band (2 m /
70 cm) FM handheld that many people own. EmberMesh turns its voice channel into a data link with the
**sound-card modem**: the phone or computer plays the data as sound into the radio, and listens to
the radio's speaker.

> **What is proven and what is not.** The four connection presets were measured through software
> models of a VOX handheld, a PTT handheld, acoustic coupling and a data port: with the presets every
> test message arrived (for example 8 of 8 single messages through the VOX model, where the old
> defaults lost all 8 because VOX clipped their start). **No real UV-5R, cable, interface or phone
> microphone has been used yet.** Do the two-station bench test (section 7) before you rely on it.

## 1. Legal notes first (not legal advice)

- The UV-5R's main bands, **2 m and 70 cm, are amateur bands**. Transmitting there needs an amateur
  licence; EmberMesh then sends only cleartext with your callsign on every packet, and refuses to
  transmit until your callsign is set.
- The radio can also be tuned to frequencies of licence-free services (FRS, PMR446 and others).
  Whether **this radio** may be used on such a service depends on its certification in your country;
  in many countries radios with a detachable antenna and wide tuning range are **not** approved for
  licence-free services, and many services restrict or forbid data. Check your national rules.
- In a genuine emergency where life is in immediate danger, most jurisdictions permit any means of
  radio communication to summon help (the note in **Tools → Band plans**). Return to normal rules
  afterwards.
- Start with **low power** and the shortest transmissions that work.

## 2. Choose a method

| Method | You need | Speed | Notes |
|---|---|---|---|
| **A. Audio cable, radio keys itself (VOX)** | A K1 two-pin radio audio cable to a 4-pole (TRRS) headset plug; on phones without a jack a USB-C/Lightning headset adapter with microphone support | 1200 bit/s (AFSK 1200) | Simplest with a cable. Works on every platform |
| **B. Cable or interface with a PTT line** | A USB sound-card + PTT interface for K1 radios (AIOC / Digirig class) with its K1 lead; on Android a USB-C/micro-USB OTG adapter | 1200 bit/s | Most reliable. Android, Chrome/Edge on a computer, emberd. **Not iPhone** |
| **C. No cable: hold the device to the radio** | Nothing | Robust 125 (125 bit/s, slow) | You press PTT. Works with anything, including iPhones |

The radio's accessory socket is the Kenwood-style **K1** jack: a 3.5 mm and a 2.5 mm plug side by
side. Easiest are ready-made "APRS-K1" / "K1 audio" cables, or a USB interface made for K1 radios.
Keep the device's output **low (start around 30 %)**: the radio's microphone input is far more
sensitive than a line input. Hum or RF feedback: use the interface's isolation, ferrites and lower
power. Full parts lists: [What you need](what-you-need.md).

## 3. Set up the radio

These are the settings from the app's **verified** UV-5R profile (checked against the UV-5R series
menu definitions; the numbering can differ on other firmware versions and clones, so check the name
the display shows):

| Setting | Value | Menu |
|---|---|---|
| Squelch | 1-3 (closed with no signal) | MENU 0 SQL |
| Transmit power | LOW to start (BF-F8HP: LOW/MID) | MENU 2 TXP |
| Battery save | OFF | MENU 3 SAVE |
| VOX (method A only) | 1-3 to start (OFF disables) | MENU 4 VOX: raise if it keys by itself, lower if packets start late |
| Bandwidth | WIDE (or NARR if everyone else uses narrow) | MENU 5 WN |
| Dual watch | OFF | MENU 7 TDR |
| Keypad beep | OFF | MENU 8 BEEP |
| Transmit time-out | 60 s or more | MENU 9 TOT |
| Tone squelch / tones | OFF for simplex data | MENU 10 R-DCS, 11 R-CTCS, 12 T-DCS, 13 T-CTCS |
| Voice prompts | OFF | MENU 14 VOICE |
| PTT ID | OFF | MENU 19 PTT-ID |
| Roger beep | OFF | MENU 39 ROGER |

For methods B and C set **VOX OFF**. With VOX, keep the phone/computer volume constant and do not
use the radio for voice while VOX is on.

**Why these:** battery save and dual watch clip or mute the start of received packets; beeps,
voice prompts, roger beeps and PTT ID mix into the data; a short time-out cuts long bursts; tones
would block stations that do not use them.

**The frequency.** Put both radios on the **same simplex frequency** agreed in your net plan
([Running a net](running-a-net.md)), entered in frequency mode on the keypad or stored in a memory
channel. The key sequence for entering frequencies and storing channels is in the radio's manual
(or use the CHIRP programming software); the app's profile does not cover it.

## 4. Set it up in EmberMesh: the "Connect a radio" wizard

Open **Links & radios → Add connection → Connect a radio (guided)**, or **Instructions** (the **Help** tab on
phones) **→ Connect a radio** (in Simple mode: **Connections** / **How to use**). Eight steps:

1. **Connect a radio:** type `UV-5R` in **Search radios** and choose **Baofeng UV-5R / UV-5RA /
   UV-5RE / BF-F8HP**.
2. **How is it connected?** **Audio cable, radio keys itself (VOX)** (marked **recommended** for this
   radio), **Cable or interface with a PTT line**, or **No cable — hold the device to the radio**.
   Methods this device cannot do are marked **not here** with the reason.
3. **You will need:** the parts list for that method on this device, plus the cable notes. Tap **I
   have these**.
4. **Set up the radio:** the settings above as a checklist, tagged **checked against the manual**.
   Tick them off on the radio.
5. **Band and licence:** **Amateur (ham) band — I have a licence** (enter your callsign if asked) or
   **Licence-free / emergency use (e.g. PMR446, FRS/GMRS where allowed)**.
6. **This device:** choose the sound card / port the radio is connected to (plug everything in
   first), then **Create the link**.
7. **Set the audio level:** a live meter (section 5).
8. **On-air test:** **Send a test** and **Ping** (section 7), then **Finish** and **Go to Links**.

The wizard creates a sound-card link with the matching **Radio setup** preset:

| Method | Preset | Modem | Lead-in | Level |
|---|---|---|---|---|
| A (VOX) | Handheld radio keyed by its own VOX (audio cable) | AFSK 1200 | 400 ms wake-up tone + 600 ms TX delay | 0.35 |
| B (PTT interface) | Handheld radio, PTT keyed by a serial line or interface (AIOC, Digirig) | AFSK 1200 | 450 ms TX delay | 0.40 |
| C (no cable) | No cable: device speaker/microphone held to the radio, PTT by hand | Robust 125 | 1500 ms TX delay (time for you to press PTT) | 0.80 |

Any setting you change yourself in the link's **Settings** overrides the preset. The link list shows
the preset and the settings actually in use.

## 5. Set the levels

1. Let the radio receive a busy channel, or have the other station transmit.
2. Turn the **radio's volume knob** until the meter's bar sits in the middle and the hint says
   **Level OK.**; the **CLIP** lamp must stay off.
3. Transmit level: start low (the presets do). If the other station's meter shows **frames bad**
   rising, or your signal sounds distorted on a second radio, lower **Transmit volume** (or the
   interface's level control).

Details: [Handheld radios](handheld-audio-modem.md), "Setting levels".

## 6. Method details

### A. VOX cable

```
   UV-5R K1 socket  <== K1 two-pin cable ==>  4-pole TRRS plug --> phone headset jack
                                                                  (or USB-C headset adapter)
```

The radio transmits whenever it hears the app's audio. If the first part of packets is lost, raise
the radio's VOX level step by step, or the link's **VOX wake-up tone** / **TX delay**. If the radio
keys by itself from noise, lower the VOX sensitivity number.

### B. PTT interface (AIOC / Digirig class)

```
   UV-5R K1 socket <== K1 lead ==> [ interface: sound card + PTT ] --USB--(OTG)--> phone / computer
```

- **Android:** in step 6 pick the interface as input and output, and as **Push-to-talk (PTT)** pick
  its serial line (an AIOC keys on DTR, a Digirig-style interface on RTS; the app suggests the right
  one when it recognises the interface).
- **Computer (emberd):** **Transmit keying (PTT)**: Serial port DTR line (AIOC) or RTS line
  (Digirig), or **USB sound-card GPIO (CM108/CM119 HID, AIOC)**. Quick start from the command line:

  ```sh
  emberd --callsign N0CALL-1 --radio "preset=handheld_ptt,in=AIOC,out=AIOC,ptt=serial_dtr,ptt_port=/dev/ttyACM0"
  ```

  (Replace `N0CALL-1` with your own callsign. The CM108 GPIO path is compiled but untested on real
  hardware.)
- **Browser:** Chrome or Edge on a computer, opened from the single file (or `https`/`localhost`).
- **iPhone/iPad:** not possible (no serial lines). Use method A or C.

### C. No cable

Hold the phone's microphone and speaker (usually its bottom edge) 1-3 cm from the radio's grille, in
a quiet place, radio volume moderate. When the link starts to transmit (**TX ON AIR** on the link
card, the **TX** lamp on the meter), **press and hold the radio's PTT** until it stops. The long
lead-in gives you time to react. It is slow: send short messages.

## 7. First on-air test (two stations)

1. Two stations side by side, same frequency, low power, both set up as above.
2. On both, open the link's meter and Statistics. On station 1, wizard step 8 **Send a test** (a
   short Public message "Radio test from ...").
3. Station 2 should show **DCD** lighting, **frames ok** counting up and the message in **Public**.
4. Add each other as contacts, then **Ping**: "Pong from ... — the radio link works".
5. Swap roles, then move apart and repeat to learn the real range.

The two stations need not be switched on together: each announces itself when it first hears the
other. In simulated tests two AFSK 1200 stations held each other's keys after a median of 18 s
(worst 49 s). The link shows when it cannot transmit because no callsign is set (amateur band) and
its error line says "Set your callsign to transmit on this link (amateur band)".

## 8. Running it on each platform

| Platform | Method A (VOX) | Method B (PTT interface) | Method C (no cable) |
|---|---|---|---|
| Android | Headset jack or USB-C headset adapter, or a USB sound card via OTG | Yes, OTG adapter; serial PTT or "USB serial adapter ..., PTT only" | Yes |
| iPhone / iPad | Lightning/USB-C headset adapter with mic, or a USB audio interface | **No** | Yes (**Transmitter keying: I press PTT myself**) |
| Browser | Single file in Chrome/Edge/Firefox/Safari on a computer (not on a plain-http hub page) | Chrome/Edge on a computer only | Yes (secure page) |
| Computer (emberd) | USB sound card, `--radio "preset=handheld_vox,in=USB Audio,out=USB Audio"` | Yes (serial, CM108 GPIO, CAT) | Yes |

## 9. Common failures

| Symptom | Fix |
|---|---|
| The link refuses to transmit (amateur) | Set your callsign (wizard step 5, or Settings → Profile) |
| Radio never transmits with VOX | VOX off or level wrong (MENU 4), output volume too low |
| Other station misses the start of every packet | Battery save on (MENU 3), dual watch on (MENU 7), squelch too high (MENU 0), VOX slow: raise the lead-in |
| Radio keys up on its own | VOX too sensitive; lower the number, keep the room quiet |
| Distorted, nothing decodes | Transmit level too high: lower it to about 30 % or below |
| Constant hiss holds the channel busy | Squelch fully open: set 1-3 so it closes with no signal |
| Packets cut off at the end | Transmit time-out too short (MENU 9), or raise **TX tail** |
| Beeps or voice in the audio | Keypad beep, voice prompt, roger beep, PTT ID off (MENU 8, 14, 39, 19) |
| Nothing heard at all | Different frequency, wide/narrow mismatch (MENU 5), tones set on one radio (MENU 10-13) |
| Hum, or the radio misbehaves while transmitting | RF or ground loop: lower power, add ferrites, use an isolated interface |

## 10. Other radios

The wizard knows 22 radio entries. **Only the UV-5R family's settings are verified against the
maker's manual**; for every other radio it shows the same kind of checklist in generic words (use
the menu **names** on your radio, not numbers). Its recommendations:

| Radio | Connector | Recommended setup |
|---|---|---|
| Baofeng UV-82 / UV-82HP / GT-3, BF-888S, Quansheng UV-K5 / UV-K6 / UV-5R Plus, AnyTone AT-D878UV, generic K1 handhelds (Retevis, TYT, Wouxun, Radtel ...) | K1 two-pin | VOX cable (handheld_vox). BF-888S has no keypad: program it with software. UV-K5 menu numbers differ between firmwares. AnyTone: use an analog channel; its Bluetooth is for headsets, not data |
| Baofeng UV-9R family | often a model-specific sealed connector | VOX cable for your exact model, or no cable |
| Yaesu FT-60R, FT-65R / FT-4XR / FT-4XE | Yaesu accessory jack (not K1) | VOX cable made for the model |
| Kenwood TH-K20 | Kenwood two-pin | PTT interface (handheld_ptt) |
| Icom ID-52, IC-T70 | Icom accessory jack | PTT interface |
| Kenwood TH-D72, TH-D74 / TH-D75, TM-D710 | built-in KISS TNC (USB; Bluetooth on TH-D74/D75; serial on TM-D710) | **KISS TNC** link, not audio ([KISS TNCs](kiss-tnc-packet.md)) |
| Mobile/base radios with a 6-pin mini-DIN data port, Yaesu FT-817/818/857/897 | data port | sound_card preset with a sound-card interface on the data port |
| Yaesu FT-991A, Icom IC-705 / IC-7100 / IC-7300 | USB (built-in sound card + CAT/CI-V) | sound_card preset, one USB cable |
| Generic FRS / GMRS / PMR446 blister-pack radios, generic CB | none / model-specific mic connector | No cable (acoustic), licence-free; check whether data is allowed |
| Any handheld with VOX + audio cable; any radio + USB interface (AIOC, Digirig); any radio with no cable | — | handheld_vox / handheld_ptt / acoustic |

Universal settings for any FM radio, from the generic profile: squelch low but closed (radio silent
with no signal; fully open lets noise keep the channel busy), low power, same wide/narrow as the
others, dual watch off, battery save off, roger and key beeps and voice prompts off, transmit
time-out 60 s or longer, tones off for simplex data, and VOX on with a low sensitivity number when
using method A.
