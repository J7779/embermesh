---
id: embermesh/library-search
title: "EmberMesh 9: Finding Guides - Search, Smart Search and Asking the Library"
category: embermesh
tags: [embermesh, guides, library, search, smart search, semantic search, ask, related guides, offline, keyword]
summary: "How to find what you need in the offline survival library: keyword search, Smart search by meaning, Ask the library with its one-tap emergency questions, Related guides, what 'no good passage found' means, when keywords work better, and the limits of the small on-device search model."
source: original
source_title: EmberMesh Operator Manual
license: CC0-1.0
attribution: EmberMesh contributors
retrieved: 2026-11-13
part_of: embermesh/manual
order: 9
---

# Finding Guides: Search, Smart Search and Asking the Library

Every app carries the whole survival library on the device (several thousand guides; the **Guides**
screen shows the exact numbers in your copy). Everything below works with **no network**.

## How smart search works, and what it is not

The guide library finds guides by what you mean, not only by the exact words you type: "stuff to stop
infection in a cut" finds the wound-care guides even though they never use the word "stuff". It does
this with a small table of word meanings that is part of the guide library on your device; **nothing
is sent anywhere** and it works with no network at all. **It does not write answers and it does not
give advice of its own:** it shows you passages copied from the guides, with the guide they came
from, so you can read the original. If nothing in the library matches what you asked, it says so
instead of guessing. Exact names, part numbers and codes ("UV-5R", "FM 21-76") are still matched
word for word first.

The "table of word meanings" is a small text-embedding model (potion-base-4M, MIT licence, stored
with 4-bit weights, about 2.35 MB) that travels inside the guide library file. It is not a chatbot
and it understands **English only**.

## Searching

**Guides** (**Survival library**; in Simple mode the screen is called **Guides** and Smart search
**Search by meaning**): type in **Search all guides**; results appear as you type.
**All categories** narrows the search to one subject.

- **Smart search** (switch under the search box, shown when your library includes the search
  model): "Also finds guides by meaning: helps most with questions and everyday wording (English).
  Exact words still come first. Runs on this device; nothing is sent anywhere." It is on unless you
  turn it off.
- A result marked **≈ by meaning** was found by meaning, not by the words you typed; its preview
  shows the matching passage.
- Turn Smart search off for strict word-for-word results.

## Ask the library

Under the search box, **Ask the library** takes a plain question ("e.g. how do I stop bleeding?").
Tap **Ask**: it shows up to five **passages quoted from the guides (not generated answers)**, each
with the guide's title and **read the whole guide**.

- **One-tap emergency questions** on the **SOS** screen ("First aid: ask the guides") and in the
  medical category ("Ask the medical guides"): **Bleeding won’t stop**, **Not breathing**, **Burn**,
  **Unsafe water**.
- **Instructions** has its own box, **Ask the instructions**, that searches only this manual.
- **"No good passage found"** means that even the best passage the library has is not a confident
  match. That is deliberate: rather than show a poor match, it tells you to ask another way or search
  by keywords. Rephrase in plain words, try fewer words, or search for a key term.

## Related guides

At the end of a guide, **Related guides** lists the guides closest in meaning to it, so you can
continue from one topic to the next.

## When it helps, and when plain keywords are better

Measured on a set of 195 test queries, combining word matching with meaning found a relevant guide
among the top ten results for 91 % of queries (86 % with words alone), and helped most for questions
and everyday wording. For questions, **Ask the library** put a passage from a relevant guide first in
73 % of the test queries and among the top five in 83 %.

| Use smart search / Ask for | Use plain keywords for |
|---|---|
| Questions ("how do I purify water without fire") | Exact names, model numbers and codes ("UV-5R", "FM 21-76", a callsign) |
| Everyday wording ("my kid ate something bad") | Misspelled words: **Ask the library** often finds no good passage for them (most of its "no good passage" cases in testing were misspellings); the normal search box copes better |
| Paraphrases of what you need | Rare synonyms the guides do not use: try the usual word too |

## Limits

- **Results are only as good as the guides.** The passages are quoted from guides written by many
  sources over many years; some are old or written for other situations. **Check critical facts in
  the guide itself**, and seek trained help when you can (every guide ends with that warning).
- English only.
- A single-file web app may have been built without the library; then **Guides** is empty.
