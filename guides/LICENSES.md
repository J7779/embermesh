<!-- SPDX-License-Identifier: CC0-1.0 -->
# Licences of the guide library

The EmberMesh **software** is MIT-licensed ([LICENSE](../LICENSE)). The **guide library** in
`guides/content/` is not. It is a collection of texts from many sources, and **each guide keeps
its own licence**, recorded in its front matter:

```yaml
source: https://en.wikipedia.org/w/index.php?title=Water_storage&oldid=1378105779
source_title: "Wikipedia: Water storage"
license: CC BY-SA 4.0
attribution: Wikipedia contributors, "Water storage", ... Converted to Markdown for the EmberMesh
  offline library; references, images ... omitted. Licensed CC BY-SA 4.0 (https://...).
```

`license` names the licence, `attribution` is the credit line the licence asks for (including a
note of what was changed), and `source` / `source_title` say where the text came from. Every
category folder also has one or more `_sources*.md` files listing each source, the evidence for
its licence and the guides taken from it. The apps show source, licence and attribution at the
bottom of every guide, and `tools/guidepack` copies these fields into `guides.pack`, so they
travel with every copy of the library.

Only texts that anyone may copy, modify and redistribute for any purpose are accepted: public
domain, CC0, CC BY and CC BY-SA. No "non-commercial" (NC) or "no derivatives" (ND) material, and
nothing "all rights reserved".

## What is in the library

Counted from the `license:` field of all 5,333 guides (5,301 library guides and the 32-chapter operator manual) (files whose names start with `_` are
not guides):

| Licence | Guides | Main sources |
|---|---:|---|
| **CC BY-SA 4.0** | 2,582 | Wikipedia (2,348), Wikibooks (189), Wikivoyage (45) |
| **Public domain — US Government works** (17 U.S.C. §105) | 1,191 | US Army field manuals and technical publications approved for public release, FEMA / Ready.gov, CDC, ATSDR, NIOSH, EPA, NWS / NOAA, USDA (Farmers' and home-canning bulletins, a 1954 soil-conservation handbook), US Coast Guard, USGS, FCC rules (47 CFR) |
| **Public domain — old books** (published in 1930 or earlier) | 1,204 | Books mostly from 1880–1929 on farming, repair, woodcraft, first aid, radio, food preservation and household skills, transcribed from public-domain scans |
| **CC BY 4.0** | 295 | Open Resources for Nursing (Open RN / WisTech Open) textbooks (282); BCcampus *Clinical Procedures for Safer Patient Care* via LibreTexts (13) |
| **CC0 1.0** | 33 | The EmberMesh operator manual (`guides/content/embermesh/`, 32 chapters) and one original reference table — written for EmberMesh and dedicated to the public domain |
| **CC BY-SA 3.0** | 28 | Appropedia |
| **Total** | **5,333** | |

The `license:` values are written out more precisely in the files themselves (30 distinct
spellings, for example `Public Domain (US Government work, 17 U.S.C. §105; published 1918, United
States)`); the table groups them. To recount:

```sh
grep -rh --include='*.md' '^license:' guides/content | sort | uniq -c | sort -rn
```

In the apps, **Settings → About → Licences** summarises the library's licences; with a core that
provides `guides.info` (docs/RPC_API.md §7) the counts come from the installed pack itself.

## Your obligations, in plain words

You may copy, print, translate, adapt and redistribute the guides, including commercially, as
long as you follow the licence of each guide you use:

- **Keep the attribution.** Every CC BY and CC BY-SA guide must stay credited as its
  `attribution` line says, with the licence named and linked, and with a note that it was changed
  (the line already says how). If you change a guide further, say so. The simplest way to comply
  is to keep the front matter of every file you copy.
- **CC BY-SA stays CC BY-SA.** A guide under CC BY-SA 3.0 or 4.0 — and anything you adapt from it
  — must be shared under the same licence (or a compatible later version). This applies to
  redistributed packs too: a `guides.pack`, a single-file HTML build with the library inlined, a
  printed binder or a USB stick all carry CC BY-SA material and must keep it marked as CC BY-SA
  with its attribution. Bundling CC BY-SA guides next to MIT software or public-domain guides does
  not change either licence.
- **No extra restrictions.** You may not add terms or technical measures that stop others from
  using CC-licensed guides as their licence allows.
- **Public-domain guides** need nothing from you, but please keep their source line so readers
  can find the original. "Public domain" here means **in the United States**: US Government works
  are not protected by US copyright, and the old books were published in 1930 or earlier. Some of
  the old books were written by authors who died less than 70 years ago or were first published
  outside the US; they may still be under copyright in other countries. If you redistribute
  outside the US, check the books' authors and dates (listed in the `_sources*.md` files).
- **CC0** guides (the operator manual) can be used for anything, without conditions.
- **Trademarks and endorsement.** Credit lines name agencies and publishers only as sources. No
  source endorses EmberMesh, and no agency logo or seal is reproduced.

The guides are information, not professional advice. Some sources are old and contain outdated
practice; those passages are marked with editor's notes. Illustrations from the sources are not
reproduced (a few are described in words).

## Adding guides

New guide text must come with an accepted licence and full front matter (see
[docs/ARCHITECTURE.md §7](../docs/ARCHITECTURE.md#7-guide-corpus-format) and
[CONTRIBUTING.md](../CONTRIBUTING.md)). Add the source to the category's `_sources*.md` with the
evidence for its licence. Text you write yourself for the library is contributed under CC0 1.0
unless you state a CC BY or CC BY-SA licence in its front matter.

## Canonical licence texts

| Licence | Deed and legal code |
|---|---|
| CC0 1.0 Universal | <https://creativecommons.org/publicdomain/zero/1.0/> · <https://creativecommons.org/publicdomain/zero/1.0/legalcode> |
| CC BY 4.0 | <https://creativecommons.org/licenses/by/4.0/> · <https://creativecommons.org/licenses/by/4.0/legalcode> |
| CC BY-SA 4.0 | <https://creativecommons.org/licenses/by-sa/4.0/> · <https://creativecommons.org/licenses/by-sa/4.0/legalcode> |
| CC BY-SA 3.0 Unported | <https://creativecommons.org/licenses/by-sa/3.0/> · <https://creativecommons.org/licenses/by-sa/3.0/legalcode> |
| US Government works | 17 U.S.C. §105: <https://www.law.cornell.edu/uscode/text/17/105> |
| Public domain in the US (term of copyright) | <https://www.copyright.gov/help/faq/faq-duration.html> |
