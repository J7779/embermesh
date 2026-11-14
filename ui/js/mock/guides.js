// Sample guide corpus for the mock backend. All text is original (CC0-1.0).

const BASE = { source: 'original', source_title: 'EmberMesh sample guide', license: 'CC0-1.0', attribution: 'EmberMesh contributors' };

export const CATEGORY_TITLES = {
  medical: 'Medical', hygiene: 'Hygiene', water: 'Water', food: 'Food', agriculture: 'Agriculture',
  shelter: 'Shelter', fire: 'Fire', navigation: 'Navigation', signaling: 'Signaling', survival: 'Survival',
  radio: 'Radio', electronics: 'Electronics', power: 'Power', repair: 'Repair', weather: 'Weather',
  hazards: 'Hazards', security: 'Security', community: 'Community', reference: 'Reference', embermesh: 'EmberMesh manuals',
};

export const GUIDES = [
  {
    id: 'water/purification-boiling', title: 'Purifying Water by Boiling', category: 'water',
    tags: ['water', 'purification', 'boiling'], summary: 'Kill germs in water with heat: how long, how to handle cloudy water, and storage.',
    markdown: `# Purifying Water by Boiling

Boiling is the **most reliable** way to kill bacteria, viruses and parasites in water. It does not remove chemicals, salt or fuel.

## Steps

1. If the water is cloudy, let it settle, then pour it through a clean cloth or coffee filter.
2. Bring the water to a *rolling* boil (big bubbles that do not stop when stirred).
3. Keep it boiling for **1 minute**. Above 2,000 m (6,500 ft) boil for **3 minutes**.
4. Let it cool with a lid on. Do not add ice unless it was made from safe water.

## Storage

- Use clean containers with tight lids, ideally food-grade plastic or glass.
- Label with the date. Use within a few days, or re-treat.
- Pour, do not dip cups into the container.

## When you cannot boil

| Method | Contact time | Notes |
|:--|:-:|--:|
| Unscented bleach (5-9 %) | 30 min | 2 drops per litre, 4 if cloudy |
| Chlorine dioxide tablets | 30 min - 4 h | Follow packet |
| Solar (SODIS) | 6 h sun | Clear PET bottles, clear water only |

> Fuel is precious. A pot lid cuts boiling time and fuel use by about a third.
`,
  },
  {
    id: 'medical/bleeding-control', title: 'Stopping Severe Bleeding', category: 'medical',
    tags: ['bleeding', 'first aid', 'tourniquet', 'trauma'], summary: 'Direct pressure, wound packing and tourniquets for life-threatening bleeding.',
    markdown: `# Stopping Severe Bleeding

Severe bleeding can kill in minutes. Act first, then call for help over the mesh with an **SOS**.

## 1. Direct pressure

- Put on gloves or use a plastic bag if you have one.
- Press hard on the wound with a clean cloth. Use your body weight.
- Do not lift the cloth to look. Add more on top if it soaks through.

## 2. Pack deep wounds

For wounds in the armpit, groin or neck where a tourniquet cannot go, push gauze or clean cloth *into* the wound until it is full, then press for at least **3 minutes**.

## 3. Tourniquet (arms and legs)

1. Place it 5-8 cm above the wound, not on a joint.
2. Tighten until the bleeding **stops**. It will hurt.
3. Write the time on the casualty's forehead or the tourniquet.
4. Do not loosen it. Get the person to medical care.

> If you have no commercial tourniquet, a strip of cloth at least 4 cm wide and a stick as a windlass can work. Thin cords cut skin and fail.
`,
  },
  {
    id: 'medical/cpr-basics', title: 'Rescue Breathing and Chest Compressions', category: 'medical',
    tags: ['cpr', 'resuscitation', 'first aid'], summary: 'What to do when an adult is unresponsive and has no normal breathing.',
    markdown: `# Rescue Breathing and Chest Compressions

Sample text for the demo library (original, CC0). Learn CPR from a trained instructor.

## Check

- Make the scene safe. Tap the shoulders and shout.
- Tilt the head back and look, listen and feel for normal breaths for up to 10 seconds.
- Gasping is not normal breathing.

## Compress

Kneel beside the chest. Push hard and fast in the centre of the chest, about 5 cm deep and 100 to 120 times a minute. Let the chest come back up between pushes.

## Breathe

After 30 compressions, give 2 rescue breaths if you are trained and willing, then continue. Do not stop until help takes over or the person starts breathing.
`,
  },
  {
    id: 'medical/burns-first-aid', title: 'First Aid for Burns and Scalds', category: 'medical',
    tags: ['burns', 'scalds', 'first aid'], summary: 'Cool the burn, cover it, and know when it is serious.',
    markdown: `# First Aid for Burns and Scalds

Sample text for the demo library (original, CC0).

## Cool it

Hold the burn under cool running water for 20 minutes. Do not use ice, butter or creams. Remove rings and tight items before swelling starts.

## Cover it

Cover loosely with clean plastic film or a clean non-fluffy cloth. Do not burst blisters.

## When it is serious

Burns larger than the casualty's hand, burns on the face, hands, feet or groin, and deep white or charred burns need medical help. Watch for shock.
`,
  },
  {
    id: 'shelter/emergency-shelter-basics', title: 'Emergency Shelter Basics', category: 'shelter',
    tags: ['shelter', 'cold', 'tarp', 'insulation'], summary: 'Stay dry, get off the ground and block the wind: priorities for an improvised shelter.',
    markdown: `# Emergency Shelter Basics

Cold and wet kill faster than hunger. Shelter priorities, in order:

1. **Insulation from the ground** - a layer of dry leaves, pine boughs or foam at least 10 cm thick.
2. **Roof against rain** - a tarp, poncho or plastic sheet, sloped so water runs off.
3. **Wind block** - put the low side of the shelter towards the wind.
4. **Small size** - a small space is easier to keep warm with body heat.

## Tarp lean-to

- Tie a ridge line between two trees at about hip height.
- Drape the tarp over it and stake the windward edge to the ground.
- Pile leaves or snow along the edges to stop drafts.

## Site selection

- Avoid dry stream beds (flash floods) and dead trees overhead ("widow makers").
- Stay near water but at least 60 m away from it.
`,
  },
  {
    id: 'navigation/compass-and-map', title: 'Using a Map and Compass', category: 'navigation',
    tags: ['navigation', 'compass', 'bearing', 'declination'], summary: 'Take and follow a bearing, correct for declination, and find yourself by triangulation.',
    markdown: `# Using a Map and Compass

## Take a bearing from the map

1. Put the compass edge on the map from where you are to where you want to go.
2. Turn the dial until the orienting lines match the map's north-south grid lines.
3. Read the bearing at the index line.

## Correct for declination

Magnetic north is not true north. Declination is printed on most maps.

- **East** declination: *subtract* it from the map bearing.
- **West** declination: *add* it to the map bearing.

Remember: "East is least, West is best" (when going from map to compass).

## Triangulation

Take bearings to two or three landmarks you can identify on the map, convert them to back bearings (add or subtract 180), and draw the lines. You are where they cross.

The EmberMesh **Tools** tab has a bearing and distance calculator for GPS coordinates.
`,
  },
  {
    id: 'radio/handheld-basics-1', title: 'Handheld Radio Basics, Part 1: Channels and Squelch', category: 'radio',
    tags: ['radio', 'handheld', 'squelch', 'frs', 'gmrs'], part_of: 'radio/handheld-basics', order: 1,
    summary: 'What the knobs do: frequency, channels, squelch, power and the push-to-talk button.',
    markdown: `# Handheld Radio Basics, Part 1: Channels and Squelch

A handheld radio ("HT", "walkie-talkie") sends and receives on one frequency at a time. Only one person can talk at once.

## Key controls

- **PTT** (push-to-talk): hold it, wait half a second, then speak. Release when done.
- **Squelch**: mutes the hiss when nobody is talking. Set it just above the point where the noise stops.
- **Power**: Low (0.5-1 W) saves battery; High (4-5 W) reaches further.
- **Channel / frequency**: both sides must use the same one, and the same *tone* (CTCSS/DCS) if set.

## Licence-free channels (examples)

| Region | Service | Example |
|:--|:--|--:|
| US | FRS ch 1 | 462.5625 MHz |
| EU/UK | PMR446 ch 1 | 446.00625 MHz |
| AU | UHF CB ch 11 (calling) | 476.6750 MHz |

Next: Part 2 covers talking procedure.
`,
  },
  {
    id: 'radio/handheld-basics-2', title: 'Handheld Radio Basics, Part 2: Voice Procedure', category: 'radio',
    tags: ['radio', 'procedure', 'prowords', 'phonetic'], part_of: 'radio/handheld-basics', order: 2,
    summary: 'Prowords, the phonetic alphabet and how to pass a message clearly.',
    markdown: `# Handheld Radio Basics, Part 2: Voice Procedure

## Calling

Say who you are calling, then who you are: *"Ridge Camp, this is Alex."* Wait for an answer.

## Prowords

| Proword | Meaning |
|:--|:--|
| OVER | I have finished, your turn |
| OUT | Conversation ended (never "over and out") |
| SAY AGAIN | Repeat your last message |
| ROGER | Message received |
| WILCO | Will comply |
| MAYDAY | Life-threatening emergency (say it three times) |

## Phonetic alphabet

Alfa, Bravo, Charlie, Delta, Echo, Foxtrot, Golf, Hotel, India, Juliett, Kilo, Lima, Mike, November, Oscar, Papa, Quebec, Romeo, Sierra, Tango, Uniform, Victor, Whiskey, X-ray, Yankee, Zulu.

> Think before you key up. Short, planned messages save batteries and airtime.
`,
  },
  {
    id: 'radio/handheld-basics-3', title: 'Handheld Radio Basics, Part 3: Connecting to EmberMesh', category: 'radio',
    tags: ['radio', 'embermesh', 'audio', 'cable', 'vox'], part_of: 'radio/handheld-basics', order: 3,
    summary: 'Send mesh messages through a handheld radio with an audio cable or by holding the phone to it.',
    markdown: `# Handheld Radio Basics, Part 3: Connecting to EmberMesh

EmberMesh can use any FM handheld as a data radio with its built-in **sound-card modem**.

## Option A: audio cable

1. Use a cable from the radio's speaker/mic jack to the phone headset jack (or a USB sound card).
2. Turn VOX on in the radio, or use a serial cable for PTT.
3. In **Links -> Add connection**, choose *Sound card*, then modem *AFSK 1200*.

## Option B: acoustic coupling

Hold the phone's microphone next to the radio speaker, and its speaker next to the radio microphone. Slower and noisier, but needs no cable.

## Legal note

On amateur frequencies, encryption is **not allowed**. EmberMesh marks such links as cleartext-only and sends only signed Public messages with your callsign.

\`\`\`
 Phone            Radio
 [mic ] <------- [spkr]
 [spkr] -------> [mic ]   (VOX keys the transmitter)
\`\`\`
`,
  },
  {
    id: 'embermesh/getting-started', title: 'EmberMesh: Getting Started', category: 'embermesh',
    tags: ['embermesh', 'setup', 'identity', 'mesh', 'hub'], summary: 'Your identity, verifying contacts, and the quickest ways to connect devices.',
    markdown: `# EmberMesh: Getting Started

EmberMesh works with **no internet**. Every device stores and forwards messages for others.

## Your identity

On first run your device makes a key pair. Your *fingerprint* is six words. Read them aloud to a friend over voice radio, or show your QR card, to **verify** each other. Verified contacts are protected against impersonation.

## Fastest ways to connect

1. **Same Wi-Fi or hotspot** - nothing to do, devices find each other.
2. **Hub** - one device starts a hub; others open its address in a browser.
3. **Bluetooth** - phone to phone, about 10-50 m.
4. **Radio** - see *Handheld Radio Basics, Part 3*.
5. **Sneakernet** - export a bundle to a USB stick or QR codes and carry it.

## Priorities

SOS beats everything. Then acknowledgements, direct messages, channels, and bulk sync last.

See also the [radio guide](radio/handheld-basics-1) and the [shelter guide](shelter/emergency-shelter-basics).
`,
  },
  {
    id: 'reference/markdown-sampler', title: 'Markdown Sampler (renderer test)', category: 'reference',
    tags: ['test', 'markdown', 'reference'], summary: 'Exercises every Markdown feature the reader supports, plus hostile input that must stay inert.',
    markdown: `# Markdown Sampler

This page tests the guide renderer. Everything below must display as **text**, never run.

## Emphasis

Plain, *italic*, **bold**, ***bold italic***, \`inline code\`, and _underscored italic_.

### Lists

- Level one item
  - Nested item
    - Deeper item
- Back to level one

1. First
2. Second
   1. Second, sub-step a
   2. Second, sub-step b
3. Third

### Table with alignment

| Left | Centre | Right |
|:-----|:------:|------:|
| a | b | 1.00 |
| longer cell | **bold** | 22.50 |

### Code

\`\`\`
for (let i = 0; i < 3; i++) {
  console.log("<b>not bold</b>");
}
\`\`\`

> A blockquote with *emphasis*.
> Second line of the quote.

### Links

- External: [Example site](https://example.org/page) (shown, not followed off-app)
- Relative guide link: [Boiling water](water/purification-boiling)

#### Hostile content (must render as text)

<script>alert('xss-script')</script>

<img src=x onerror="alert('xss-img')">

[click me](javascript:alert('xss-link'))

[data link](data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg==)

**<iframe src="javascript:alert(1)"></iframe>**

\`<svg onload=alert(1)>\`

"quotes" & 'apostrophes' & <angle brackets> & &amp; entities

---

End of sampler.
`,
  },
];

for (const g of GUIDES) Object.assign(g, BASE, { bytes: new TextEncoder().encode(g.markdown).length });
