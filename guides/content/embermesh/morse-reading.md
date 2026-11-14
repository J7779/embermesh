---
id: embermesh/morse-reading
title: "EmberMesh 26: Sending and Reading Morse by Light and Sound"
category: embermesh
tags: [embermesh, morse, cw, flashlight, lamp, camera, microphone, signalling, decoder, reader, prosigns, practice, sos]
summary: "Using the Morse screen to send Morse by flashlight, screen or tone and to read it in real time with the camera (a flashlight, lamp or blinking screen) or the microphone (a radio speaker, whistle or tone), with the speeds that work, aiming and the watched box, platform notes, working with people who have no app, a Morse table with prosigns, and practice drills."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 26
---

# Sending and Reading Morse by Light and Sound

Morse needs no network, no radio data mode and no app on the other side: a flashlight, a lamp, a
mirror, a whistle or a voice radio is enough. EmberMesh can **send** Morse with the flashlight, the
screen or a tone, and **read** Morse in real time through the **camera** (a blinking light) or the
**microphone** (a tone), turning it into text.

> **What is proven:** the decoder read generated light patterns (with camera jitter, dropped frames,
> auto-exposure drift, flicker and noise) and generated tones (with noise, drift and fading), also
> through each platform's capture code (Android, iPhone, browser). **Not proven:** no real lamp,
> camera, radio or cable has been used yet. Practise with a friend before you depend on it.

## 1. Where it is

**Tools → Morse send & read** opens the **Morse** screen with three tabs: **Send**, **Read by
camera**, **Read by sound**. The **?** button opens this chapter.

## 2. Sending

On **Send**:

1. **Text** (default `SOS`), **Speed (words per minute)** (default 10).
2. Tick **Repeat until stopped** for a beacon.
3. **Flashlight** (the camera torch; on devices without one this is **Screen flash**), **Screen
   only** (the whole screen flashes white), or **Tone** (a 700 Hz beep through the speaker).
4. **Stop** ends it.

SOS is · · · — — — · · · sent as one sign. The app's own advice: signal from high ground at night; a
phone screen is visible for about 1 km in darkness, a flashlight much further.

## 3. Reading with the camera

1. **Read by camera**. Choose **Back camera** or **Front camera**.
2. Tap **Start reading** and allow camera access.
3. **Aim:** hold the device still (rest it on something), with the other light in the picture.
4. **The watched box:** the reader only looks at a small box in the picture. **Tap the light, or
   drag the box onto it.** Or tick **Find the blinking light automatically** and let it search.
5. Watch the status card: the **lamp** lights on each flash, the **level** bar shows brightness with
   its threshold mark, the dots and dashes of the character being received appear next to it, and
   below it the estimated **WPM** and **quality**. Text appears in the transcript as characters are
   recognised.

Hints the reader gives, and what to do:

| Hint | Do |
|---|---|
| Too dark to see a light. Point the camera at the light, or move closer. | Aim at the light; get closer |
| The watched area is fully bright. Tap a darker spot next to the light, or place the box exactly on it. | Move the box, or step back; avoid pointing at the sun or a bright sky |
| Nothing is blinking in the box. Tap the light, or turn on "Find the blinking light". | Move the box onto the light |

**Speeds that work** (measured on simulated phone cameras): the reader needs about **two camera
frames per dot**. That is roughly **9 WPM at 15 frames per second, 15 WPM at 24 fps, 20 WPM at
30 fps**, and 25 WPM or more at 60 fps. Below about 1.8 frames per dot it fails quickly. Most phone
cameras run at 24-30 fps, so **ask senders to keep to 10-15 WPM**; hand keying is fine.

**Distance and daylight:** the screen says it works "from a few metres up to as far as the camera
can see the light". That has not been measured with real lights. Darkness, a bright point light and
a steady device help; in daylight a dim light may not stand out. **Try it** at the distance you need.

## 4. Reading by sound

1. **Read by sound**, **Start reading**, allow the microphone.
2. Hold the device close to the sound: a radio's speaker, a whistle or tone sender. Quiet
   surroundings help.
3. The status shows the detected **tone** (searched between 300 and 1500 Hz) and the **SNR**, plus
   WPM and quality.

Measured on simulated audio: readable from about **5 to 40 WPM**, and still mostly correct down to a
signal only a few dB above the noise; at a clear signal it copies exactly, except sometimes the
first character while it learns the speed. Hand keying with uneven dots and dashes works; very
uneven keying (dashes only twice a dot) costs accuracy.

A radio receiving Morse (or a Morse-sending EmberMesh link) can be read this way by holding the phone
to its speaker.

## 5. The transcript

- Prosigns are shown in angle brackets, for example `<SOS>`, `<AR>`. Characters that share a pattern
  with a prosign are written as the prosign: `+` as `<AR>`, `=` as `<BT>`, `(` as `<KN>`, `&` as
  `<AS>`.
- `□` marks a group of dots and dashes the reader could not decode.
- Buttons: **Clear**, **Copy**, **Send as message…** (send the text into a chat), **Save to notes**
  (a note titled "Morse received ...").
- Reading stops when the app leaves the screen. A reading session that gets no input for a minute
  ends ("Press Start again").

## 6. Per platform

| Platform | Camera | Microphone |
|---|---|---|
| Android | The app's own camera capture (exposure and white balance locked after a moment, so auto exposure cannot swamp the flashes), or the in-page camera | Yes, also while a sound-card link uses the microphone |
| iPhone / iPad | The app's own camera capture with exposure, focus and white balance locked; the in-page camera as an option (**Use the in-page camera instead of the app’s**) | Yes |
| Browser | Only on a secure page: the single file, `https` or `localhost`. **Not on a plain-http hub page** | Same |
| Computer hub | On the operator page the browser captures camera and microphone | Also from the console: `morse listen` (default input, or `morse listen USB Audio`), `morse status`, `morse stop` |

## 7. With people who have no app

- They can **send** with any flashlight, lamp, mirror flash or whistle; you read with the camera or
  microphone.
- You can **send** to them with **Flashlight**, **Screen only** or **Tone**, and they read by eye or
  ear, using the table below. Send slowly (5-10 WPM) with clear gaps.
- A voice radio works too: they key the microphone and whistle or hum Morse; you read the radio's
  speaker by sound. A sound-card link set to the **Morse code (CW)** modem sends text as Morse over a
  radio ([Handheld radios](handheld-audio-modem.md)).
- Agree on a calling routine: `CQ` (anyone), `DE` (from), `K` (over), `<AR>` (end of message), `<SK>`
  (end of contact), `R` (received).

## 8. Morse table

| Letter | Code | Letter | Code | Digit | Code |
|---|---|---|---|---|---|
| A | · — | N | — · | 1 | · — — — — |
| B | — · · · | O | — — — | 2 | · · — — — |
| C | — · — · | P | · — — · | 3 | · · · — — |
| D | — · · | Q | — — · — | 4 | · · · · — |
| E | · | R | · — · | 5 | · · · · · |
| F | · · — · | S | · · · | 6 | — · · · · |
| G | — — · | T | — | 7 | — — · · · |
| H | · · · · | U | · · — | 8 | — — — · · |
| I | · · | V | · · · — | 9 | — — — — · |
| J | · — — — | W | · — — | 0 | — — — — — |
| K | — · — | X | — · · — | | |
| L | · — · · | Y | — · — — | | |
| M | — — | Z | — — · · | | |

| Sign | Code | Meaning |
|---|---|---|
| `.` | · — · — · — | full stop |
| `,` | — — · · — — | comma |
| `?` | · · — — · · | question / please repeat |
| `/` | — · · — · | slash |
| `<AR>` (also `+`) | · — · — · | end of message |
| `<BT>` (also `=`) | — · · · — | break / new paragraph |
| `<KN>` (also `(`) | — · — — · | over, only the station called |
| `<AS>` (also `&`) | · — · · · | wait |
| `<SK>` | · · · — · — | end of contact |
| `<SOS>` | · · · — — — · · · | distress, sent as one sign |

Timing: a dash is three dots long; one dot of silence between the parts of a letter, three between
letters, seven between words. **Tools → Signalling → Morse code** converts any text to Morse.

## 9. Practice drills

1. **Self-test:** two devices. One sends `SOS SOS` with **Screen only** at 10 WPM; the other reads
   with **Read by camera** from 2 m, then 10 m, then across a dark room or yard.
2. **Speed ladder:** send `PARIS PARIS` at 8, 12, 15 and 20 WPM by flashlight and note where reading
   fails on your camera.
3. **Sound:** send with **Tone**, read with **Read by sound**; then through two voice radios (one
   keyed while the phone beeps into it, the other's speaker read by the second phone).
4. **By eye and ear:** learn the letters of your callsign or name, `SOS`, `OK`, `K`, `R` and the
   digits; copy the app's slow flashes on paper and check against the transcript.
5. **Hand keying:** key a flashlight by hand to a reader and check the **quality** figure; aim for
   steady rhythm rather than speed.
