---
id: embermesh/sdr-listening
title: "EmberMesh 21: Listening with an SDR Dongle (rtl_tcp)"
category: embermesh
tags: [embermesh, sdr, rtl-sdr, rtl_tcp, receive only, monitor, aprs, scanner, dongle]
summary: "Using a cheap RTL-SDR dongle with the rtl_tcp server as a receive-only EmberMesh link to monitor a net, APRS or keyboard modes without transmitting: setup, settings and troubleshooting."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 21
---

# Listening with an SDR Dongle (rtl_tcp)

An **RTL-SDR** is a cheap USB radio receiver (originally a TV dongle). With the **SDR receiver
(rtl_tcp)** link EmberMesh tunes it to one frequency, demodulates narrow FM and feeds the sound
to the same modems as the audio link. You can:

- **monitor a net** and receive its mesh messages, SOS beacons and bulletins;
- read **APRS** traffic and messages;
- decode Morse, RTTY, PSK31 or DTMF text;
- all **without transmitting**, so no licence is needed to listen.

**It never transmits.** It cannot acknowledge or reply. Pair it with another link if you need to
answer.

> **Hardware status:** tested with synthetic radio data (a desktop daemon decoded FM I/Q made from
> its own modem), **never with a real dongle**.

## What you need

- An RTL-SDR dongle and an antenna for the frequency (a quarter-wave ground plane from
  [Antennas](antennas.md) works well).
- The `rtl_tcp` program (part of the rtl-sdr tools) on the computer the dongle is plugged into.
- EmberMesh on a **computer (emberd)**, an **iPhone** or an **Android phone** (both over Wi-Fi to
  the computer running `rtl_tcp`). Browsers do not offer this link.

## Steps

1. Plug in the dongle and start the server:

   ```sh
   rtl_tcp -a 127.0.0.1 -p 1234        # only this computer may connect
   rtl_tcp -a 0.0.0.0 -p 1234          # other devices on the network may connect (e.g. an iPhone)
   ```

2. **Add connection →** "RTL-SDR (rtl_tcp server)" (computer), "SDR receiver (rtl_tcp)" (iPhone) or
   "TCP client" (Android). Enter the address of the computer running `rtl_tcp` and port `1234`.
3. **Protocol → SDR receiver (rtl_tcp)**. Set:
   - **Frequency** in **hertz**: e.g. `144390000` for APRS in North America (the default),
     `144800000` for APRS in Europe, or the net's data frequency (146.550 MHz = `146550000`).
   - **Modem**: the same as the net uses (AFSK 1200 for APRS and most packet).
   - **Band / licence**: the same as the stations you listen to (it decides the framing of the
     Robust modes).
4. **Connect.**

The **link's own settings** (frequency, sample rate, gain, correction, channel width) tune the
dongle, and you can change them later with **Settings** on the link card; the link re-tunes the
dongle. The computer's connection form also has tuning fields, but they are used only when its
advanced option **Tune from these settings** is on.

Hub command line:

```sh
rtl_tcp -a 127.0.0.1 -p 1234 &
emberd --link '{"endpoint":{"kind":"rtl_tcp","config":{"host":"127.0.0.1","port":1234}},"link":{"type":"rtl_tcp","config":{"frequency_hz":144390000}}}'
```

(A `--link` given on the command line is not saved; add the link from the operator page to keep it.)

**You should see:** the link goes from **Connecting…** to **Connected** when the server answers.
The card shows the same **RX level** meter as a sound-card link (the level of the demodulated
audio; the **TX** lamp never lights). **Statistics** shows the tuner name (R820T, E4000 ...), frequency, sample rate, gain, channel
power and the meter values in full.
Received messages appear in the normal chats, marked as arriving via this link.

## Settings

| Setting | Default | Notes |
|---|---|---|
| **Frequency** | 144390000 Hz | Channel to listen to, in Hz |
| **SDR sample rate** | 240 kHz (lowest CPU, recommended) | 1.024 MHz or 2.048 MHz also offered |
| **Tuning offset** | 50000 Hz | Keeps the channel away from the dongle's centre spike; rarely needs changing |
| **RF gain** | 0 = automatic | Set a fixed gain if strong nearby signals overload the dongle |
| **Frequency correction** | 0 ppm | Cheap dongles are off by tens of ppm; correct it if decoding fails on a known-good signal |
| **Channel width** | 12.5 kHz narrow FM (most radios) | 25 kHz for wide-FM transmitters |
| **Modem**, **Band / licence**, **APRS messaging** | as the audio link | Must match the stations you hear |

## If it doesn't work

| Symptom | Likely cause | Fix |
|---|---|---|
| **Error** "not an rtl_tcp server" | Something else answers on that port | Check the port and that `rtl_tcp` is running |
| Stays **Connecting…** | Server not reachable | Start `rtl_tcp` with `-a 0.0.0.0` for network use; check the firewall |
| Meter says **No audio arriving** or **Too quiet** | No signal, wrong frequency, poor antenna | Check the frequency in Hz; improve the antenna; listen with another radio to confirm activity |
| Signal present, nothing decodes | Frequency error, wrong modem or channel width | Set **Frequency correction**; try the other **Channel width**; match the modem |
| Decodes stop when a strong station transmits nearby | Overload | Set a fixed lower **RF gain** |
| High CPU on a small computer | Sample rate too high | Use 240 kHz |
