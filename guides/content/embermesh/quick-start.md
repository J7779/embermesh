---
id: embermesh/quick-start
title: "EmberMesh 1: Quick Start - Two Devices Talking in Five Minutes"
category: embermesh
tags: [embermesh, quick start, setup, first run, bluetooth, wifi, getting started]
summary: "The fastest way to get two devices exchanging messages with EmberMesh, plus a table that sends you to the right chapter for your situation."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 1
---

# Quick Start: Two Devices Talking in Five Minutes

EmberMesh is a messenger that needs **no internet, no phone network, no accounts and no
servers**. Every phone, laptop or small computer running it talks directly to nearby devices and
radios, and carries messages for other people until they can be delivered.

This chapter gets two devices talking as fast as possible. Read the rest of the manual later.

## Pick your situation

| What you have right now | Fastest way | Read |
|---|---|---|
| Two phones with EmberMesh, within a few tens of metres | Bluetooth | This chapter, then [Phone to phone](phone-to-phone.md) |
| Phones on the same Wi-Fi, or one Android phone running a hotspot | Wi-Fi (UDP) | This chapter, then [Phone to phone](phone-to-phone.md) |
| Two iPhones or iPads | Apple peer-to-peer | [Phone to phone](phone-to-phone.md) |
| Only one person has the app | Copy the app to the others | [Getting the app onto other devices](spreading-the-app.md) |
| You still have internet | Install the hosted web app; it then works offline | [Operating the web app](app-web.md) |
| Large text, fewer buttons, plain words | Turn on Simple mode | [Simple mode and accessibility](accessibility.md) |
| Looking for a guide or an answer | Search or ask the library | [Finding guides](library-search.md) |
| A laptop, Raspberry Pi or a phone, and many people with phones | Run a hub; guests just open a web page | [Setting up a hub](hub-setup.md) |
| Handheld radios (walkie-talkies, ham handhelds) | Sound-card modem | [Handheld radios](handheld-audio-modem.md) |
| A packet TNC, Direwolf or an APRS radio | KISS link | [KISS TNCs and packet radio](kiss-tnc-packet.md) |
| Meshtastic, RNode or LoRa modules | LoRa link | [LoRa](lora.md) |
| Two groups with no link at all | Carry messages on a USB stick or QR codes | [Sneakernet](sneakernet.md) |
| Someone is in danger now | SOS beacon | [Emergency use](emergency-use.md) |
| Something does not work | Symptom tables | [Troubleshooting and known limits](troubleshooting.md) |

## Guides for your device

| You use | Full guide |
|---|---|
| An Android phone | [Operating the Android app](app-android.md) |
| An iPhone or iPad | [Operating the iPhone and iPad app](app-ios.md) |
| A browser (single file, hub page) | [Operating the web app](app-web.md) |
| A computer or Raspberry Pi | [Operating the computer hub](app-desktop.md) |
| Which cable or adapter to get | [What you need](what-you-need.md) |
| What a screen or setting does | [Screens tour](screens-tour.md), [Settings reference](settings-reference.md) |

## Where things are in the app

- **On a phone** the bottom bar shows **Chats**, **People**, **SOS**, **Guides**, **Help** (the
  **Instructions** screen: this manual, the radio wizard and the hardware list) and **More**.
  Tap **More** to reach **Bulletins**, **Links & radios**, **Tools**, **Notes & lists**,
  **Settings** and **Mesh statistics**.
- **On a wide screen** (tablet, laptop) all of these are shown in the side bar directly
  (**Links**, **Tools**, **Settings** and so on).
- The strip at the top shows `links up / links total`, how many devices were **heard** in the
  last 15 minutes, and how many messages are **queued**. Tap the links count to open the Links
  screen.

**Simple mode** uses plain names: **Chats** is **Messages**, **SOS** is **Send for help**, **Links &
radios** is **Connections**, **Help** / **Instructions** is **How to use**. The full list is in [Simple mode and
accessibility](accessibility.md).

## The five-minute path

Do steps 1 to 4 on **both** devices.

### Step 1. First start

1. Open EmberMesh. A short logo animation may play while the app starts; it never delays the app,
   and any tap skips it (shorten or turn it off in Settings → Accessibility). You see **Welcome to
   EmberMesh**. If you want bigger text and fewer buttons, tick **Make it simpler: bigger text and
   fewer buttons** there ([Simple mode](accessibility.md)).
2. Type **Your name** (something others will recognise, for example "Sam at Oak St shelter").
3. Leave **Amateur radio callsign** empty unless you hold an amateur licence.
4. Choose your **Region**. It selects the legal frequencies and power limits shown in the tools.
5. Tap **Continue**. The **Your identity** page shows your node ID and eight **fingerprint words**.
   Leave them for now (see step 7).
6. Tap **Start using EmberMesh**. You arrive in **Chats**.

### Step 2. Add a connection

1. Open **More → Links & radios** (wide screen: **Links**).
2. Tap **Add connection**. A list asks "How do you want to reach other devices?"
3. Pick one:
   - **Same Wi-Fi or hotspot (Android, desktop):** "Wi-Fi LAN / hotspot (UDP)" on Android,
     "LAN multicast (Wi-Fi / Ethernet)" on a computer. Zero setup.
   - **Bluetooth:** "Bluetooth LE (phone to phone)" on Android, "Bluetooth (nearby phones)" on
     iPhone.
   - **Two iPhones:** "Apple peer-to-peer (iPhone, iPad, Mac)".
   Greyed-out entries marked **unavailable** do not work on this device; the line under each one
   says why.
4. Leave every field at its default. Make sure **Reconnect automatically when the app starts**
   is ticked.
5. Tap **Connect**. You return to **Links & radios**.

**You should see:** a card for the new link with a coloured dot and the word **Connected**.

### Step 3. Allow the permission prompts

The phone will ask for permissions the first time:

- Android: **Nearby devices** (Bluetooth), and on Android 11 or older **Location** (Android needs
  it for Bluetooth and Wi-Fi scans; Location must also be switched on).
- Android 17 and newer: **local network** access (Android describes it as finding and connecting
  to nearby devices). Without it every Wi-Fi link and the hub silently time out.
- iPhone: **Bluetooth** and **Local Network**.

Say **Allow**. If you refused by mistake, allow it in the phone's system settings for EmberMesh.

### Step 4. Keep the app open

For this first test keep EmberMesh **open on the screen** on both devices. (Background operation
is covered in [Power and field operation](power-and-field.md).)

### Step 5. See each other

1. Open **People**, then the **Nearby** tab.
2. Tap **Announce myself** on both devices.
3. Within about a minute the other device appears with its name and the word **Direct**.

### Step 6. Send a message

1. Open **Chats**. The **Public** channel is always at the top ("Open channel for everyone
   nearby").
2. Type a message and tap the send arrow.
3. It appears on the other device.

Your own message shows a status icon. Tap a message to see **Copy**, **Details** and **Delete**;
**Details** shows the status in words: Queued, Sent, Relayed, Delivered, Read or Failed. See
[How EmberMesh works](how-it-works.md).

### Step 7. Make it private and verified

1. In **People → Nearby**, tap the **+** next to the other device ("Add to contacts").
2. Open **People → Contacts** and tap the new contact.
3. Under **Verify identity**, ask the other person to read their eight fingerprint words aloud
   (they are in **Settings → Identity**). If every word matches, tap **Words match — verify**.
4. Tap **Message**. Direct messages are end-to-end encrypted.

**Do this before an emergency.** Messages, SOS alerts and bulletins from people you have not
added and verified are marked **⚠ unverified sender**: anyone can invent a name. Strangers' SOS
alerts are still shown, but grouped below verified ones and without a notification. Add and
verify your family, neighbours and group now, while you can stand next to them.

## If it doesn't work

| What you see | Check |
|---|---|
| The link card says **Error** | Read the red line on the card. Tap **Reconnect**. Check that Bluetooth or Wi-Fi is switched on. |
| The link says **Connected** but nobody appears in **Nearby** | Both devices must have a link of the *same kind*. Keep both apps open and tap **Announce myself** again. |
| Bluetooth finds nobody (Android) | Bluetooth on; on Android 11 or older Location on too; both phones have the app open. |
| Wi-Fi finds nobody | Some hotspots and guest networks block devices from seeing each other. Try Bluetooth, or have the phone that makes the hotspot also run EmberMesh. |
| An iPhone and an Android phone cannot see each other | Keep the iPhone app in the foreground. See "Android and iPhone together" in [Phone to phone](phone-to-phone.md). |
| No app on the second device | [Getting the app onto other devices](spreading-the-app.md). |

## Test the link: ping

Once a contact exists, open it in **People → Contacts** and tap **Ping**. A line such as
"Pong: 0.4 s round trip, 0 hops" proves that messages travel both ways. You can also ping from
**People → Nearby** (the small ping button next to each device).

## What others can see about you

Your **name** and everything in **Public** are visible to anyone nearby; direct messages are not.
Pick a name you are comfortable sharing. Details: [What other people can see](security.md#what-other-people-can-see).

## Back up your identity now

Your identity (your keys) lives only on this device. Go to **Settings → Identity → Back up
identity**, choose a passphrase of at least 8 characters and save or copy the backup somewhere
safe (paper, USB stick). See [Security and safety of your data](security.md).
