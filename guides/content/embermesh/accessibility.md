---
id: embermesh/accessibility
title: "EmberMesh 8: Simple Mode and Accessibility"
category: embermesh
tags: [embermesh, simple mode, accessibility, text size, high contrast, motion, animation, screen reader, talkback, voiceover, keyboard, vibration, dyslexia, plain words]
summary: "Simple mode (bigger text and buttons, fewer choices, plain words) and every accessibility setting: text size, high contrast, motion, the start-up animation, extra reading space, vibration, keyboard shortcuts and screen readers, plus a table of normal names and their Simple-mode names so every chapter can be followed in either mode."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 8
---

# Simple Mode and Accessibility

All of these are in **Settings → Accessibility** ("Simple mode, text size, contrast, motion").
They are preferences of this device's app and take effect at once.

## Simple mode

**Simple mode** makes the app easier for someone who is stressed, tired, unfamiliar with phones, or
has poor eyesight:

- **Bigger text and buttons.**
- **Fewer choices on each screen.** Less-used controls are tucked away behind a **More options**
  button on that screen (tap it again, **Fewer options**, to hide them). Nothing is removed: every
  feature stays reachable through **More options** and the **More** screen.
- **Plain words** instead of technical names (table below).
- **Only the essential places in the bottom bar:** **Messages**, **Send for help**, **Guides**,
  **How to use**, **Settings** and **More**. The **More** screen lists everything else, including
  **People**.
- **Quick buttons** in a conversation: **Send my location** and **I'm OK**.
- An emergency shortcuts card on the **Guides** screen, next to search and asking the library.
- A big **Back** bar on screens that have no back button of their own.
- No decorative motion.

**Turn it on:**

- At first run, the welcome screen offers **Make it simpler: bigger text and fewer buttons**. ("You
  can change this any time in Settings → Accessibility.")
- Any time: **Settings → Accessibility → Simple mode**.

### Normal names and Simple-mode names

The manual uses the normal names. With Simple mode on, the same things are called:

| Normal name | Simple-mode name |
|---|---|
| Chats | Messages |
| SOS | Send for help |
| Emergency | Send for help |
| Links | Connections |
| Links & radios | Connections |
| Bulletins | Notice board |
| Instructions (the Help tab on phones) | How to use |
| Survival library | Guides |
| People & mesh | People |
| Tools | Radio tools |
| Safety check-in | Tell people how you are |
| SOS alerts received | Calls for help nearby |
| Roll call (latest check-in per person) | Who has said how they are |
| Smart search | Search by meaning |
| Region & display | Region and screen |
| Mesh & relaying | Network and passing on messages |
| Sneakernet bundles | Carry messages by hand |
| Fingerprint words | Safety words |
| Low-power mode (no animation, fewer refreshes) | Battery saver (no animation, fewer updates) |
| Share this device | Let others connect through this device |
| Net plan (where to tune) | Where to tune the radio |
| Mesh statistics | Network numbers |
| Peers | Nearby devices |
| Radar | Map of nearby devices |
| Contacts | People I know |
| Post | Write a notice |
| Show my card | Show my contact code |
| Back up identity | Save a backup of my identity |
| Set passphrase | Set a password |
| Change passphrase | Change password |
| Panic wipe | Erase everything |
| Duress passphrase | Emergency password |
| I am safe — stop SOS | I am safe now — stop calling for help |
| Send emergency | Yes, send as emergency |
| Stop SOS | Yes, stop |
| Send as emergency? | Send as an emergency? |
| Stop SOS? | Stop calling for help? |
| Remove passphrase? | Remove the password? |
| Replace identity? | Replace who you are on this device? |

A few explanatory texts are also reworded (for example the SOS instructions read "Press and hold the
red button for 3 seconds. Every phone and radio in reach is asked for help, again and again, with
your position.").

### The most common tasks in Simple mode

| Task | Simple mode |
|---|---|
| Send a message | **Messages** → the conversation (or **Public**) → type → send |
| Call for help | **Send for help** → press and hold the red button 3 seconds |
| Say you are all right | **Send for help → Tell people how you are → I'm OK**, or the **I'm OK** button in a conversation |
| Find a guide | **Guides** → type in the search box, or ask a question (see [Finding guides](library-search.md)) |
| Connect a radio | **More** → **Connections** → **Add connection → Connect a radio (guided)**, or **How to use → Connect a radio** |

## Text size

**Text size**: a slider (from 0.85 to 1.6 times) and the presets **Normal**, **Larger**, **Largest**,
with a **Preview**. It comes on top of the phone's own font-size setting, which also applies (on
Android the system font size, on iPhone Dynamic Type).

## Contrast

**High contrast**: **High contrast**, **Follow system** or **Off**. Stronger text, borders and focus
outlines in every theme, including the red night theme. (The theme itself is in **Settings → Region
& display**.)

## Motion

- **Animation**: **Follow system** (the phone's or computer's reduce-motion setting; the default),
  **Full**, **Reduced**, **Off**. Low-power mode and Simple mode allow at most **Reduced**.
- **Start-up animation**: **Full**, **Short** or **Off**.

**The start-up animation** is the short logo animation when the app opens. It **never delays the
app**: the app starts underneath it, and the animation ends as soon as the app is ready, at once on
any tap or key press, and after a fixed time limit (under 3 seconds) at the latest. Choose
**Short** or **Off** to shorten or remove it; with reduced motion, Simple mode or low-power mode it
shows a calm, still version.

## Reading, touch and keys

- **Extra reading space**: more space between letters, words and lines; can help with dyslexia. No
  extra fonts are downloaded.
- **Vibrate on touch**: a short vibration confirms each button press, a pattern for SOS and for
  errors. Only on devices that can vibrate (Android phones, iPhones; in a browser only Chrome on
  Android). Elsewhere the setting is not shown.
- **Keyboard shortcuts** (computers and keyboards): press `g` and a letter to jump to a screen:
  `c` Chats, `p` People, `s` SOS, `b` Bulletins, `l` Links, `g` Guides, `i` Instructions, `t`
  Tools, `n` Notes, `o` Settings, `a` Accessibility, `m` More; `?` lists them; `/` jumps to the
  screen's search box. **Show keyboard shortcuts** lists them. Turn them off if they clash with your
  screen reader or switch control.

## Screen readers

EmberMesh works with **TalkBack** (Android), **VoiceOver** (iPhone, iPad, Mac), **NVDA**, **JAWS** and
**Narrator** (Windows). The app reads its whole interface to them; when many things happen at once
it says "more updates" instead of reading every one; after moving to another screen the screen's
title is read first; "Skip to content" is the first thing on every page. The Accessibility page
says when a screen reader is running.

- **Android:** the whole app is navigable with TalkBack; the system font size, "Remove animations",
  high-contrast text and bold text (Android 12+) are followed.
- **iPhone/iPad:** VoiceOver reads the app directly; Dynamic Type text size, reduce motion and high
  contrast are followed.
