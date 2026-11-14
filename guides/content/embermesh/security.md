---
id: embermesh/security
title: "EmberMesh 28: Security and Safety of Your Data"
category: embermesh
tags: [embermesh, security, privacy, passphrase, lock, duress, panic wipe, backup, identity, verification, encryption, eavesdropping, metadata, forward secrecy]
summary: "Protecting EmberMesh on your device and on the air: storage passphrase and lock, duress passphrase, panic wipe, identity backup and restore, verifying contacts, what an eavesdropper can and cannot see, and the system's limits."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 28
---

# Security and Safety of Your Data

## Five things to do today

1. **Set a storage passphrase:** **Settings → Security → Set passphrase**.
2. **Back up your identity:** **Settings → Identity → Back up identity**, keep it offline.
3. **Verify the people you rely on** by fingerprint words or QR card.
4. **Use direct messages or private channels** for anything sensitive, never Public or bulletins.
5. Decide **when you would wipe**, and know where **Panic wipe** is.

## Storage passphrase and lock

Without a passphrase, anyone holding your unlocked device can read your messages, contacts and
notes. The Security section warns: "Storage is not protected by a passphrase."

**Set it:**

1. **Settings → Security → Set passphrase.**
2. Type a new passphrase (at least 6 characters; longer is much better) twice. **Save.**
3. The section now says "Storage is encrypted with a passphrase."

- All messages, keys and notes are encrypted on the device with a key derived from the passphrase.
- **There is no recovery** if you forget it. Keep an identity backup.
- **Lock now** locks immediately. The app opens on the screen **EmberMesh is locked**; type the
  passphrase and tap **Unlock**.
- After **Lock now**, the device **keeps relaying and storing** messages for others and for you.
- But after a **restart** (phone reboot, app killed, power cut on a hub) a protected device starts
  locked **with no connections at all** until someone unlocks it. For an unattended relay or hub,
  either do not set a passphrase or use emberd's `--passphrase-file` ([Setting up a
  hub](hub-setup.md)).
- To change or remove it: **Change passphrase** (leave the new one empty to remove; you will be
  asked to confirm).

## Duress passphrase

If someone forces you to unlock the app, you can type a **different** passphrase that silently
erases your secrets first. To an onlooker it is indistinguishable from a normal unlock of a
lightly used phone:

- **Kept:** your profile name, callsign and status, your settings (region, units, theme, relaying,
  position sharing ...) and your connections (links). The app looks like your app.
- **Erased:** your identity (keys), contacts, messages, channels other than Public, notes, lists
  and inventory. A fresh identity is created in their place.
- It takes the same time as a normal unlock, gives the same result, and the duress passphrase then
  works as the normal storage passphrase. **Settings → Security** still says **Duress passphrase
  (set)**, just as before.

1. A storage passphrase must be set first.
2. **Settings → Security → Duress passphrase → Set duress passphrase.** Use something different
   from the real passphrase (at least 4 characters) that you can say under pressure.
3. The summary line then reads **Duress passphrase (set)**.
4. The copy of your name and settings that a duress unlock keeps is updated automatically when
   they change. If your duress passphrase was set with an older version of EmberMesh, set it again
   once: until then a duress unlock leaves an empty profile.

Limits, honestly:

- After a duress unlock there are no contacts, no messages and new fingerprint words. Someone who
  knew your app or your words before may notice. Your contacts will see a new, unverified identity
  with your name.
- It erases only **this device**. Copies of your messages remain on the devices of the people you
  wrote to, and stored on relays until they expire.
- Do not enter the duress passphrase by mistake. It cannot be undone.

## Panic wipe

**Settings → Security → Panic wipe**:

1. Type `ERASE` in the box.
2. **Hold** the red button "Hold 3 s to erase everything" for 3 seconds.

It destroys your identity, keys, messages, contacts, notes, settings and connections on this
device. It cannot
be undone. Without a backup your identity is gone for good.

## Identity backup and restore

Your identity (secret keys) exists only on your device. On iPhone it is deliberately excluded from
iCloud and computer backups.

**Back up:**

1. **Settings → Identity → Back up identity.**
2. Enter a backup passphrase (at least 8 characters) twice, tap **Create backup**.
3. **Copy**, **Save file** (`embermesh-identity-backup.txt`) or **QR**. Store it offline: on
   paper, on a USB stick kept somewhere safe.

The backup contains your secret keys, encrypted with that passphrase. **Anyone with both the backup
and the passphrase can pretend to be you.** Keep them apart.

**Restore** (new phone, after a wipe):

1. **Settings → Identity → Restore identity.**
2. Paste the backup text, or **From file**, or **Scan QR**.
3. Enter the backup passphrase, tap **Restore**, confirm **Replace identity?**.

Contacts will then see you as the restored identity, already verified. The restore replaces the
identity currently on the device.

## Verifying contacts

Encryption protects you only if you are talking to the right key. When you add someone from the
**Nearby** list or by node ID, the app learns their key from the mesh, which an impostor could
fake.

- Compare the **eight fingerprint words**: on their contact page under **Verify identity**, and on
  their device under **Settings → Identity**. Read them over voice radio if needed. All eight
  must match. (Older versions showed six words; they are the first six of the eight.) Then **Words match — verify**.
- Or scan their card face to face: **Verify by scanning their card**.
- If words do not match: do not trust that contact; remove it and add them again from their card.
- A contact's **name** is chosen by them and proves nothing.

## What other people can see

**Your name.** EmberMesh never takes a name from your phone or computer:

- On phones the profile name is empty until **you** type one at first run.
- A new computer hub (emberd) is called "EmberMesh hub" plus the first four characters of its node
  id, unless you set a name.
- Names shown to nearby devices before any profile exists are anonymous too: an iPhone appears as
  "EmberMesh-" plus four random hex digits (Apple peer-to-peer and the local network listener) and
  as "Ember" over Bluetooth; Android phones appear as "EmberMesh phone" over Bluetooth and as
  "EmberMesh-" plus four random hex digits for their TCP server.
- **Check:** a data folder made by an older version of emberd may still hold a profile named after
  the computer. Look at **Settings → Profile → Name** (on a hub: the operator page), and change it
  there, with `emberd --name NEW`, or with the console command `name NEW`.

**What is visible to whom:**

| Who | Sees |
|---|---|
| Anyone you show your **contact card or QR code** | Your node id, name and public keys |
| Anyone **in radio range or on the same network** | The Public channel, open channels, SOS, check-ins, bulletins, net plans, and every node's announcement (name, callsign, status, public keys, and position if sharing is Automatic) |
| **Hub guests** | The hub's name, address and node id (from `/hub.json`), and the files offered at `/download/` |
| Devices on the same Wi-Fi | With **Nearby devices** on (the default after setup): that a device at your address runs EmberMesh (service `_embermesh._tcp`, named "EmberMesh-" plus a random token that changes at every start and network change); once connected, your **name and EmberMesh identity**, like any mesh neighbour. Phones also announce their own network host name through the system, which on some phones is derived from the device name. A TCP server / local network listener link advertises the same service type with the anonymous name above |
| A **relay operator** (only with **Internet relay** on) | Your IP address (so roughly where you are), when you are online, roughly how many contacts you have, which other users you exchange messages with, across days and networks; message sizes and timing. **Not** content, names, node ids or keys |
| Anyone watching a public relay | Topic names, sizes and timing on relays that allow it (those are listed but off by default); not your address or content |
| Your **contacts** (with Internet relay on) | When you come online |
| Anyone who sees a **WebRTC pairing code** | This device's local network addresses (and, only if a STUN server was configured, its public internet address); the code is useless after pairing |
| Outside the app's control | Android itself shows the phone's own Wi-Fi Direct and Bluetooth names to devices it pairs with (system settings) |

**What the apps never send:** no analytics, no tracking, no requests to any internet server of
ours. The interface talks only to its own app or hub; the phone apps block every outside web
address. The only traffic is mesh traffic over the links you add, Nearby devices on your local
network, and, **only if you turn on the Internet relay**, encrypted connections to the relay servers
listed in Settings → Connections ([Connections](connections.md)).

## What an eavesdropper can and cannot see

Anyone with a receiver on your frequency, a device on your hub's Wi-Fi, or a stolen relay sees
the traffic. What they get:

| Traffic | Content readable? | What is always visible |
|---|---|---|
| Public channel, open channels | **Yes** | Everything |
| SOS, check-ins, bulletins, net plans | **Yes** (including positions attached) | Everything |
| Node announcements | **Yes**: your name, callsign, status text, public keys (and position with automatic position sharing) | Everything |
| Direct messages, files, shared positions to one person | **No** | Sender's and recipient's node IDs, time, size, kind of packet, priority, hop count |
| Private channels | **No** | Sender's node ID, channel ID, time, size |
| Amateur-radio links | Only cleartext ever goes there | Your **callsign** on every packet |
| Meshtastic links | Additionally wrapped in the Meshtastic channel's own encryption | As above for people with the channel key |
| Hub Wi-Fi | Plain HTTP: the same as above, plus the web pages | The operator token, if you use the operator page across the network |

Also remember:

- **Transmitting reveals where you are.** Anyone with a direction-finding antenna can locate a
  transmitter. A message, a beacon or an SOS all transmit.
- **Who talks to whom, when, and how much** is visible even when content is not.
- **The internet relay is a third party.** The default relay is a free service run by another
  company. If being seen online, or being linked to the people you write to, could put you in
  danger, leave the internet relay off or use only a relay on your own hub.
- **Public Wi-Fi with Nearby devices on:** strangers' devices on that network become mesh
  neighbours and see your name and identity. Turn **Nearby devices** off on networks you do not
  trust.

## Limits of the system

- **No forward secrecy.** Direct messages are encrypted with keys derived from both people's
  long-term identity keys. If someone records your traffic now and later obtains your identity
  (a seized unlocked phone, or your backup and its passphrase), they can decrypt the recorded
  direct messages. Wipe devices you lose control of; protect backups.
- **Channels: use a random key.** A channel created with **Random key (recommended)** cannot be
  guessed; it can only be joined with its invite. A **Passphrase** channel's key is derived from the
  channel name and passphrase: anyone in radio range can record the traffic and try passphrases
  offline, without limit. Names, dates and short words fall quickly. If you must use a passphrase,
  use **Generate strong passphrase** or four or more random words; the app rates it Weak / Fair /
  Strong and offers to re-create a weak channel with a random key. Anyone with the invite can read
  the channel; leaving does not change the key.
- **Unverified senders.** Anyone can create an identity and give it any name. Messages from keys
  your device cannot check are tagged **⚠ unverified sender**; strangers' traffic is rate-limited and
  relayed only a hop or two, and can never cancel a verified SOS or delete someone's bulletin.
  Verify the people you rely on in advance.
- **Time.** A device with a sane clock never takes its time from the mesh, so nobody can push a
  false time onto it; only a device that has lost its clock adopts the time several nodes (or one of
  your contacts) agree on.
- **The device is the weak point.** Malware, an unlocked screen, or a shoulder-surfer defeat
  encryption. Lock the app; keep the screen private.
- **Blocking hides, it does not stop.** A blocked contact's messages are hidden from you, but your
  device still relays their traffic.
- **Bulletins and Public are permanent-ish.** Deleting sends a notice, but copies survive on
  devices out of range until they expire.
- The cryptography uses a well-known library (Monocypher). The project's documentation records no
  independent security audit of EmberMesh itself.
