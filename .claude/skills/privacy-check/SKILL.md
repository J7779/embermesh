---
name: privacy-check
description: Run and act on the EmberMesh publishing gate (tools/check-publishable.sh) and check git identity before every commit, push, release or fork, so that nothing identifying the builder, their machine or other people is published. Use before committing or pushing, when the gate reports findings, when adding fixtures, screenshots or binaries, or when deciding whether an allow-list entry is legitimate.
---

# Privacy check before publishing

Everything git would publish is checked against the identifiers of **the machine and person
running the check** — yours. Run it in normal (local) mode on your own machine; CI runs it in
`--ci` mode, which cannot know your identifiers.

## 1. Run the gate

```sh
tools/check-publishable.sh                 # or ./build.sh privacy; must end with "OK"
tools/check-publishable.sh --self-test     # after changing the gate or its allow-lists
EMBER_PUBLISH_EXTRA_IDS="word1 word2" tools/check-publishable.sh   # your callsign, real name, street...
tools/check-publishable.sh --strict-names  # also warn on your bare first/user name (noisy)
```

It scans exactly `git ls-files -co --exclude-standard` (tracked + untracked-not-ignored), so run
it **after** `git add` of new files and before `git commit`/`git push`. Findings print as
`FAIL [rule] file:line: detail`; machine values are never printed (only their kind).

## 2. What it looks for

| Rule | Fails on |
|---|---|
| `machine` | your user name, home directory, host name (and labels), git `user.name`/`user.email`, GitHub user, local IPv4/MAC, Wi-Fi name, iOS simulator UDIDs, Android AVD names, code-signing team ids, emberd admin tokens, `EMBER_PUBLISH_EXTRA_IDS` |
| `home-path`, `temp-path` | absolute home paths of any user, per-user temp and session paths |
| `email`, `credential` | e-mail addresses not allow-listed; private keys, token shapes, `password=`/`token=`/`secret=` with long values |
| `uuid`, `mac`, `ip` | device ids, MAC addresses, public IPv4 addresses |
| `residue` | words that describe how or by what tools the code was produced |
| `filename` | keys, keystores, `local.properties`, `.env*`, logs, dumps, OS/IDE/build directories, tool settings |
| `binary`, `image-meta`, `large` | unapproved binaries, PNG/JPEG/SVG metadata, files over 1 MiB |
| warnings | callsign-shaped literals, precise coordinates not near a listed landmark, IMEI-shaped numbers |

## 3. Fix findings (default) — do not silence them

- Your identifier, a path or a host name in text: replace it with a placeholder or a relative path
  (`<repo>/…`, `~/…`, `/path/to/…`). Placeholders: callsign `N0CALL`/`N0CALL-n`/`QX0ABC`, e-mail
  `someone@example.org`, IPv4 `192.0.2.x`/`198.51.100.x`/`203.0.113.x`, MAC `00:00:5E:00:53:xx`,
  positions from `tools/publishable-landmarks.txt`, names that are obviously fictional.
- A file that must never be published: `git rm --cached <file>` and make sure `.gitignore`
  covers it. If it was ever pushed, it is public: rotate the secret or key; rewriting history does
  not undo a publication.
- A log, dump, `adb bugreport`, crash report, emberd data directory or test output: delete it from
  the tree; attach only the relevant, scrubbed lines to an issue.
- A screenshot: retake it from the demo backend (`node ui/tests/smoke_chrome.mjs --shots DIR` or
  `ui/?mock=1`) and save a PNG without text, time or EXIF chunks.
- A fixture: regenerate it with invented values; record how it was made next to it.

## 4. When an allow-list entry is legitimate

Only for values that identify nobody and must stay as they are: a vendored author's public
address in an unmodified licence header, a protocol or platform UUID constant, a public
reference station, a reviewed binary or a large vendored file, a public contact in a guide source.
One line in `tools/publishable-allow.txt` (`<rule> path <glob>` or `<rule> match <ERE>`, anchored to
one exact value where possible), with a `#` comment saying why. Never allow-list your own
identifiers, tokens or keys — the `machine` rule cannot be allow-listed by design. Binaries also
need the review noted in the comment (source, generator, no metadata). Run `--self-test` after.

## 5. Git identity hygiene

```sh
git config user.name                      # the name every commit publishes
git config user.email                     # use your GitHub no-reply address (GitHub -> Settings -> Emails)
git log --format='%an <%ae> %ad' origin/main..HEAD   # what this push publishes, incl. time zone
TZ=UTC git commit ...                      # optional: commit times without your local offset
```

Commit messages, branch names and tags are published too: no host names, paths or real names.

## 6. Never commit

`local.properties`, keystores (`*.jks`, `*.p12`, `*.keystore`), `keystore.properties`, signing
or provisioning files, `.env*`, tokens, the emberd `admin_token`, node data (`*.ekv`, `/data/`,
`/emberd-data/`, `*.emberbackup`), logs (`*.log`), crash reports and dumps (`bugreport-*`,
`*.crash`, `*.dmp`), `build/`, `dist/`, `.toolchains/`, tool local state (everything under
`.claude/` except `.claude/skills/` and the shared `.claude/settings.json` — never put personal
rules, paths or tokens in that shared file; they belong in `.claude/settings.local.json`, which is
ignored — and `CLAUDE.local.md`, `.idea/`, `.vscode/`, …), and screenshots or fixtures with
real names, callsigns, positions, contacts or messages. `git add -f` past `.gitignore` is still
refused by the gate.

## 7. Done when

`tools/check-publishable.sh` prints OK on your machine, `git status --short` shows only files you
mean to publish, and the commit author is the identity you intend to make public.
