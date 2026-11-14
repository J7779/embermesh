---
name: write-manual-chapter
description: Add or change a chapter of the EmberMesh operator manual (guides/content/embermesh/, CC0, shown in every app under Instructions) - front matter, numbering, plain wording, Simple mode labels, honesty about what was tested, privacy-safe examples, and the README, help-screen and count updates. Use when a feature, screen, setting, link, radio or limit changes and the manual must describe it.
---

# Writing the operator manual

The manual is 32 chapters in `guides/content/embermesh/`, written for people under stress who may
have no other help. It is original text under CC0 1.0 (`guides/content/embermesh/_sources.md`)
and is written from the repository: docs, READMEs, `core/src/link/README.md`, `ui/js/screens/`
(screen names and button labels), `ui/js/lib/core_spec.js` (ranges and defaults).

## 1. Front matter (all chapters share these values)

```markdown
---
id: embermesh/<slug>
title: "EmberMesh <N>: <Title>"
category: embermesh
tags: [embermesh, ...]
summary: "One sentence: what the reader can do after this chapter."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: YYYY-MM-DD
part_of: embermesh/manual
order: <N>
---

# <Title>
```

`<N>` in the title equals `order`. Never copy text from other publications into the manual; quote
library guides by linking to them instead.

## 2. Writing rules

- Plain words, short steps, the exact on-screen labels. Where Simple mode renames a label, use the
  words from the `GLOSSARY` in `ui/js/lib/simple.js` (table in `ui/README.md`) when describing
  Simple mode.
- Every setting, default and range must match the code (`core/src/node/`, `ui/js/lib/core_spec.js`);
  every command must exist (`./build.sh --help`, the platform READMEs).
- **Honesty:** say what was tested in simulation, on an emulator or not at all; never describe
  untested hardware as working. `troubleshooting.md` lists the known limits.
- **Privacy in examples:** callsigns `N0CALL`/`QX0ABC`, names that are obviously invented,
  positions from `tools/publishable-landmarks.txt`, addresses `192.0.2.x` / `192.168.x.x`. Say what a
  feature reveals to others (`security.md` "What other people can see"); never present cleartext,
  hub traffic or light/sound signalling as private.
- Markdown only: no HTML, no images (describe diagrams in words or ASCII), under about 60 KB.

## 3. Where a new or changed feature belongs

`what-you-need.md` (hardware), the medium's chapter (`phone-to-phone.md`, `hub-setup.md`,
`radio-gateway.md`, `handheld-audio-modem.md`, `lora.md`, `morse-reading.md`, …), the platform
chapters `app-android.md`, `app-ios.md`, `app-web.md`, `app-desktop.md`, `screens-tour.md`,
`settings-reference.md`, `accessibility.md`, `security.md`, `troubleshooting.md`,
`quick-reference.md`. Change existing chapters before adding a new one.

## 4. Adding a chapter (also renumbering)

1. Create the file with the next `order`, or renumber `order` and the title number of the
   chapters after it.
2. `ui/js/screens/help.js`: add the slug to the right group in `GROUPS` (unknown chapters land in
   "More").
3. `README.md`: the "Documentation" table, and the chapter count wherever "32-chapter" appears
   (intro, "What it does", "The guide library").
4. `guides/LICENSES.md`: the CC0 row and the totals.
5. Cross-links in other chapters (`embermesh/<slug>` guide links).

## 5. Verify

```sh
./build.sh guides && build/host/tools/guidepack validate guides/content
build/host/tools/guidepack get build/guides.pack embermesh/<slug>      # renders and links
build/host/tools/guidepack search build/guides.pack <words a reader would type>
node --test ui/tests/                                                  # if help.js changed
tools/check-publishable.sh
```

Open it in the app (`cd ui && python3 -m http.server 8000`, then Instructions with the real core
after `./build.sh web`, or `node ui/tests/smoke_chrome.mjs --real auto`). Update `AGENTS.md` and the
playbooks if the chapter list they name changed.
