#!/usr/bin/env bash
# check-publishable.sh — refuse to publish anything that identifies the person or machine that
# produced this repository.
#
#   tools/check-publishable.sh [--root DIR] [--strict-names] [--max-size BYTES] [--quiet] [--ci|--local]
#   tools/check-publishable.sh --self-test [--ci|--local]
#
# --ci (automatic when CI=true, as on GitHub Actions; --local forces the normal mode) skips the
# identifiers discovered on the machine running the check (user, home, host, git identity, network,
# simulators, signing ids): on a CI runner those are generic words such as "runner" that occur in
# ordinary text. Every other rule, emberd admin tokens and EMBER_PUBLISH_EXTRA_IDS stay active.
#
# Scans exactly what `git ls-files -co --exclude-standard` would publish (tracked files plus
# untracked files that are not ignored) and fails, with file:line findings, on:
#   machine     identifiers of THIS machine and account, discovered at run time (never stored):
#               user name, $HOME, host name and its labels, macOS ComputerName/LocalHostName,
#               git user.name / user.email (and the parts of the e-mail local part), GitHub user,
#               local IPv4 and MAC addresses, Wi-Fi SSID, iOS simulator UDIDs, Android AVD
#               names, code-signing team ids, emberd admin tokens found in data directories.
#               One exception (owner-approved public identity): the git user.name (display name
#               only) may appear in a copyright statement ("Copyright (c) 2026 <name>", "© 2026
#               <name>") in files listed as `identity path <glob>` in publishable-allow.txt, if
#               that line holds no other identifier of this machine
#   home-path  /Users/<name>/, /home/<name>/, C:\Users\<name>\ other than placeholders
#   temp-path   per-user temp and session paths (/private/tmp, /var/folders, assistant scratch)
#   email       e-mail addresses not in the allow-list
#   credential  private keys, cloud/API token shapes, password=/token=/secret= with long values
#   uuid        UUIDs not in the allow-list (simulator UDIDs, captured device ids)
#   mac         MAC addresses outside 00:00:5E:00:53:xx and allow-listed placeholders
#   ip          public IPv4 addresses (outside private, loopback, multicast, documentation ranges)
#   residue     words that reveal how/where the code was produced (assistant names, "coordinator")
#   filename    files that must never be published (keys, keystores, logs, OS/IDE/build residue)
#   binary      binary files that are not explicitly approved in the allow-list
#   image-meta  PNG text/time/ICC/EXIF chunks beyond pixel size, JPEG EXIF/XMP/IPTC/comments, SVG
#               editor metadata
#   large       files over --max-size (default 1 MiB) that are not approved
# and warns (does not fail) on:
#   callsign    callsign-shaped literals outside guides/content (samples: N0CALL-n or Q-prefixed)
#   coord       coordinate pairs with 4+ decimals not near a landmark in publishable-landmarks.txt
#   imei        15-digit numbers that pass the IMEI (Luhn) check
#   name-word   with --strict-names only: the user / first name as a bare word (noisy)
#
# Allow-lists: tools/publishable-allow.txt (format described at its top) and
# tools/publishable-landmarks.txt. Narrow kinds exist for agent instructions (AGENTS.md, the
# project skills under .claude/skills/, the shared .claude/settings.json): `filename dir <prefix>`
# and `filename file <path>` publish one directory or file whose parent the filename rule otherwise
# refuses (everything else under .claude/ stays refused), and `residue term <literal>` accepts a
# tool-convention name such as CLAUDE.md as a literal only.
# Binary files are also searched (strings, and the uncompressed contents of .jar/.zip/.aar
# archives) for the machine, path, e-mail and credential rules.
# Exit status: 0 clean (warnings allowed), 1 findings, 2 usage or environment error.
set -u
export LC_ALL=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPT="$SCRIPT_DIR/$(basename "${BASH_SOURCE[0]}")"
ROOT=""
STRICT_NAMES=0
MAX_SIZE=1048576
QUIET=0
SELF_TEST=0
HITS_OUT="${EMBER_PUBLISH_HITS:-}"
CI_MODE=0; [[ "${CI:-}" == true ]] && CI_MODE=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ci) CI_MODE=1 ;;
    --local) CI_MODE=0 ;;
    --root) shift; ROOT="${1:?--root needs a directory}" ;;
    --root=*) ROOT="${1#*=}" ;;
    --strict-names) STRICT_NAMES=1 ;;
    --max-size) shift; MAX_SIZE="${1:?--max-size needs a number}" ;;
    --quiet|-q) QUIET=1 ;;
    --self-test) SELF_TEST=1 ;;
    -h|--help) sed -n '2,51p' "$SCRIPT" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown option: $1 (try --help)" >&2; exit 2 ;;
  esac
  shift
done

if [[ -t 1 ]]; then B=$'\033[1m'; G=$'\033[32m'; Y=$'\033[33m'; R=$'\033[31m'; N=$'\033[0m'; else B=; G=; Y=; R=; N=; fi

# Time-limited command without GNU timeout (absent on stock macOS). Missing perl: run untimed.
with_timeout() {
  local secs="$1"; shift
  if command -v perl >/dev/null 2>&1; then perl -e 'alarm shift; exec @ARGV or exit 127' "$secs" "$@"; else "$@"; fi
}

# =============================================================================================
# self-test: plant one violation per rule in a scratch repository and prove each rule fires
# =============================================================================================
self_test() {
  local base t fails=0 mode
  mode=--local; [[ $CI_MODE == 1 ]] && mode=--ci
  base="$(mktemp -d "${TMPDIR:-/tmp}/publish-selftest.XXXXXX")" || exit 2
  trap 'rm -rf "$base"' EXIT
  t="$base/repo"
  mkdir -p "$t/tools"
  cp "$SCRIPT" "$t/tools/check-publishable.sh"
  cp "$SCRIPT_DIR/publishable-allow.txt" "$SCRIPT_DIR/publishable-landmarks.txt" "$t/tools/"
  [[ -f "$SCRIPT_DIR/../.gitignore" ]] && cp "$SCRIPT_DIR/../.gitignore" "$t/.gitignore"
  (cd "$t" && git init -q .) || exit 2
  # A synthetic git identity and an explicit extra identifier for the scratch repository. The values
  # are assembled at run time so the literals never appear in this file (the scratch check scans it).
  local sname semail shandle extra
  sname="Tam""sin Quill""feather"; semail="tam""sin.q@quill""mail.net"; shandle="tq""uill77"; extra="zq""xextraid42"
  git -C "$t" config user.name "$sname" && git -C "$t" config user.email "$semail" && git -C "$t" config github.user "$shandle" || exit 2
  export EMBER_PUBLISH_EXTRA_IDS="$extra"

  # Negative control: values that are allowed and must not be reported.
  cat > "$t/control.md" <<'CONTROL'
BLE peer service 8e3b0001-5d1f-4c6a-9b7e-0d2a6f1c4e10, Nordic UART 6e400001-b5a3-f393-e0a9-e50e24dcca9e.
Write to someone@example.com or noreply@example.org. Logs go to /Users/USERNAME/Library/Logs and
/home/<name>/.local/share; on Windows C:\Users\YOU\AppData.
Sample stations N0CALL-1 and QX0ABC; reference station W1AW.
MAC 00:00:5E:00:53:01 or AA:BB:CC:DD:EE:FF. Hub 192.168.43.1, emulator 10.0.2.2, doc 192.0.2.10,
198.51.100.7, 203.0.113.9, multicast 239.83.77.69, DNS 8.8.8.8.
Big Ben is at 51.5007, -0.1246. password=changeme token=<admin token>
CONTROL
  # Owner-approved public identity: the git display name in a copyright statement of an approved file.
  printf 'MIT License\n\nCopyright (c) 2026 %s\n\n\302\251 2024-2026 %s. All rights granted below.\n' "$sname" "$sname" > "$t/LICENSE"
  # Published agent instructions: a project skill under the allow-listed directory .claude/skills/,
  # the shared project settings file and the root import file. The tool-convention names are allow-listed as literal terms only.
  local tool; tool="cla""ude"
  mkdir -p "$t/.$tool/skills/demo"
  printf -- '---\nname: demo\ndescription: Demo playbook. Use when testing the publishing gate.\n---\n\nPlaybooks live in .%s/skills/<name>/SKILL.md; %s.md imports AGENTS.md.\n' "$tool" "$(printf '%s' "$tool" | tr 'a-z' 'A-Z')" > "$t/.$tool/skills/demo/SKILL.md"
  printf '@AGENTS.md\n' > "$t/$(printf '%s' "$tool" | tr 'a-z' 'A-Z').md"
  printf '{\n  "$schema": "https://json.schemastore.org/%s-code-settings.json",\n  "permissions": { "deny": ["Read(.env)", "Read(~/.ssh/**)"] }\n}\n' "$tool" > "$t/.$tool/settings.json"
  local controls=(control.md LICENSE ".$tool/skills/demo/SKILL.md" ".$tool/settings.json" "$(printf '%s' "$tool" | tr 'a-z' 'A-Z').md")
  echo "### pass 1 ($mode): clean tree must pass"
  if ! (cd "$t" && bash tools/check-publishable.sh --root "$t" "$mode"); then
    echo "${R}self-test FAILED: the clean control tree was rejected${N}"; exit 1
  fi

  # Positive plants: file -> rule that must fire. Plants of identifiers discovered on this machine
  # are only expected in the normal (--local) mode; --ci skips that discovery by design.
  local plants=() local_only=() user host
  user="$(id -un 2>/dev/null || echo "${USER:-}")"
  host="$(hostname 2>/dev/null || true)"
  printf 'build id %s\n' "$extra" > "$t/m-extra.txt";                    plants+=("m-extra.txt machine")
  printf 'cache dir: %s/.cache/x\n' "$HOME" > "$t/m-home.txt";            local_only+=("m-home.txt machine")
  if [[ -n "$user" ]]; then printf 'ssh %s@buildbox\n' "$user" > "$t/m-user.txt"; local_only+=("m-user.txt machine"); fi
  if [[ -n "$host" ]]; then printf 'built on %s today\n' "$host" > "$t/m-host.txt"; local_only+=("m-host.txt machine"); fi
  printf 'author: %s\n' "$semail" > "$t/m-email.txt";                     local_only+=("m-email.txt machine")
  # The identity exception is narrow: not in other files, not outside a copyright statement, never
  # for the e-mail or the GitHub handle.
  printf 'Copyright (c) 2026 %s\n' "$sname" > "$t/id-other.txt";          local_only+=("id-other.txt machine")
  printf '# Project\n\nMaintained by %s.\n' "$sname" > "$t/README.md";    local_only+=("README.md machine")
  printf 'Copyright (c) 2026 %s <%s>\n' "$sname" "$semail" > "$t/CONTRIBUTING.md"; local_only+=("CONTRIBUTING.md machine")
  printf 'Copyright (c) 2026 %s\n' "$shandle" > "$t/THIRD_PARTY_LICENSES.md"; local_only+=("THIRD_PARTY_LICENSES.md machine")
  printf 'see /ho%s/jdoe/projects/x\n' 'me' > "$t/p-home.txt";             plants+=("p-home.txt home-path")
  printf 'C:\\Us%s\\jdoe\\Desktop\n' 'ers' > "$t/p-winhome.txt";          plants+=("p-winhome.txt home-path")
  printf 'out: /private/%s/x123/out.log\n' 'tmp' > "$t/p-temp.txt";       plants+=("p-temp.txt temp-path")
  printf 'contact jane.doe@%s.net\n' 'realmail' > "$t/p-email.txt";       plants+=("p-email.txt email")
  printf -- '-----BEGIN RSA PRIV%s KEY-----\nMIIB\n' 'ATE' > "$t/p-key.txt"; plants+=("p-key.txt credential")
  printf 'aws=AKIA%s\n' 'Q3ZQ9KD7LMNB7ZZZ' > "$t/p-aws.txt";            plants+=("p-aws.txt credential")
  printf 'curl ws://h:8437/rpc?tok%s=3f9a0c21d4e5b6a7980a1b2c3d4e5f60\n' 'en' > "$t/p-token.txt"; plants+=("p-token.txt credential")
  printf 'sim 0F27%s-8A11-466B-922C-9EF212388E26\n' '76BE' > "$t/p-udid.txt"; plants+=("p-udid.txt uuid")
  printf 'peer 3c:22:fb:%s:11:9a\n' '0d' > "$t/p-mac.txt";                plants+=("p-mac.txt mac")
  printf 'server 93.184.%s.34\n' '216' > "$t/p-ip.txt";                   plants+=("p-ip.txt ip")
  printf 'written by a sub%s for the coordinator\n' 'agent' > "$t/p-residue.txt"; plants+=("p-residue.txt residue")
  printf 'device 4901542%s\n' '03237518' > "$t/p-imei.txt";               plants+=("p-imei.txt imei")
  printf 'const MY_CALL = "KD9%s";\n' 'ZZQ' > "$t/p-call.js";             plants+=("p-call.js callsign")
  printf 'home = 39.123%s, -105.54321\n' '45' > "$t/p-coord.txt";              plants+=("p-coord.txt coord")
  awk 'BEGIN { for (i = 0; i < 70000; i++) print "0123456789abcdef" }' > "$t/p-large.txt"; plants+=("p-large.txt large")
  printf '\000\001\002binary\000' > "$t/p-blob.bin";                      plants+=("p-blob.bin binary")
  # PNG with a tEXt chunk, JPEG with an EXIF APP1 segment, SVG with editor metadata.
  printf '\211PNG\r\n\032\n\000\000\000\015IHDR\000\000\000\001\000\000\000\001\010\006\000\000\000AAAA\000\000\000\013tEXtAuthor\000jd00\000\000\000\000IEND\256B`\202' > "$t/p-meta.png"; plants+=("p-meta.png image-meta")
  printf '\377\330\377\341\000\016Exif\000\000MM\000*\000\000\377\331' > "$t/p-meta.jpg"; plants+=("p-meta.jpg image-meta")
  printf '<svg xmlns="http://www.w3.org/2000/svg" sodipodi:docname="x.svg"></svg>\n' > "$t/p-meta.svg"; plants+=("p-meta.svg image-meta")
  # Files that must never be published, force-added past .gitignore.
  mkdir -p "$t/apps/x" "$t/.idea"
  printf 'x' > "$t/.DS_Store";               plants+=(".DS_Store filename")
  printf 'x' > "$t/apps/x/release.jks";      plants+=("apps/x/release.jks filename")
  printf 'x' > "$t/apps/x/release.p12";      plants+=("apps/x/release.p12 filename")
  printf 'storePassword=x\n' > "$t/apps/x/keystore.properties"; plants+=("apps/x/keystore.properties filename")
  printf 'x' > "$t/id_ed25519";              plants+=("id_ed25519 filename")
  printf 'sdk.dir=x\n' > "$t/local.properties"; plants+=("local.properties filename")
  printf 'x\n' > "$t/.idea/workspace.xml";   plants+=(".idea/workspace.xml filename")
  printf 'x\n' > "$t/run.log";               plants+=("run.log filename")
  # The published skills directory is narrow: tool settings beside it, the same directory name
  # elsewhere, and forbidden files inside it are still refused; content rules still apply inside it.
  mkdir -p "$t/apps/x/.$tool/skills/y" "$t/.$tool/projects"
  printf '{}\n' > "$t/.$tool/settings.local.json";          plants+=(".$tool/settings.local.json filename")
  printf '{}\n' > "$t/.$tool/projects/x.jsonl";             plants+=(".$tool/projects/x.jsonl filename")
  printf '{}\n' > "$t/apps/x/.$tool/settings.json";         plants+=("apps/x/.$tool/settings.json filename")
  printf 'x\n' > "$t/apps/x/.$tool/skills/y/SKILL.md";      plants+=("apps/x/.$tool/skills/y/SKILL.md filename")
  printf 'x' > "$t/.$tool/skills/demo/release.jks";         plants+=(".$tool/skills/demo/release.jks filename")
  printf 'See .%s/skills/demo/ - drafted by %s for jane.doe@%s.net\n' "$tool" "Cla""ude" 'realmail' > "$t/.$tool/skills/demo/notes.md"
  plants+=(".$tool/skills/demo/notes.md residue" ".$tool/skills/demo/notes.md email")
  (cd "$t" && git add -f .DS_Store apps/x/release.jks apps/x/release.p12 apps/x/keystore.properties id_ed25519 local.properties .idea/workspace.xml run.log \
     ".$tool/settings.local.json" ".$tool/projects/x.jsonl" "apps/x/.$tool/settings.json" "apps/x/.$tool/skills/y/SKILL.md" ".$tool/skills/demo/release.jks") || exit 2

  # expect <hits.tsv> <plant...>: every plant must be reported; the control files must not be.
  expect() {
    local hits="$1" p f r; shift
    for p in "$@"; do
      f="${p% *}"; r="${p##* }"
      if awk -F'\t' -v f="$f" -v r="$r" '$2 == r && $3 == f { found = 1 } END { exit !found }' "$hits"; then
        echo "  ${G}fired${N}   $r  <- $f"
      else
        echo "  ${R}MISSED${N}  $r  <- $f"; fails=$((fails + 1))
      fi
    done
    for f in "${controls[@]}"; do
      if awk -F'\t' -v f="$f" '$3 == f { found = 1 } END { exit !found }' "$hits"; then
        echo "  ${R}false positive in $f:${N}"; awk -F'\t' -v f="$f" '$3 == f' "$hits"; fails=$((fails + 1))
      fi
    done
  }
  local rc=0 all=("${plants[@]}")
  [[ $mode == --local ]] && all+=("${local_only[@]}")
  echo
  echo "### pass 2 ($mode): planted violations must be reported"
  (cd "$t" && EMBER_PUBLISH_HITS="$base/hits.tsv" bash tools/check-publishable.sh --root "$t" --quiet "$mode") || rc=$?
  if [[ $rc -ne 1 ]]; then echo "${R}self-test FAILED: expected exit status 1, got $rc${N}"; fails=$((fails + 1)); fi
  expect "$base/hits.tsv" "${all[@]}"

  if [[ $mode == --local ]]; then
    # CI mode on the same tree: every generic rule still fires; only discovered identifiers are skipped.
    echo
    echo "### pass 3 (--ci): generic rules still fire, discovered identifiers are skipped"
    rc=0
    (cd "$t" && EMBER_PUBLISH_HITS="$base/hits-ci.tsv" bash tools/check-publishable.sh --root "$t" --quiet --ci) || rc=$?
    if [[ $rc -ne 1 ]]; then echo "${R}self-test FAILED: expected exit status 1 with --ci, got $rc${N}"; fails=$((fails + 1)); fi
    expect "$base/hits-ci.tsv" "${plants[@]}"
    if awk -F'\t' '$2 == "machine" && $3 != "m-extra.txt" { found = 1 } END { exit !found }' "$base/hits-ci.tsv"; then
      echo "  ${R}--ci still reported discovered identifiers:${N}"; awk -F'\t' '$2 == "machine" && $3 != "m-extra.txt"' "$base/hits-ci.tsv"; fails=$((fails + 1))
    else
      echo "  ${G}skipped${N} discovered identifiers (${#local_only[@]} plants)"
    fi
  fi
  echo
  if [[ $fails -eq 0 ]]; then echo "${G}self-test passed${N} ($mode: ${#all[@]} planted violations, clean control$([[ $mode == --local ]] && echo ', --ci pass'))"; exit 0; fi
  echo "${R}self-test FAILED${N} ($fails problems)"; exit 1
}
[[ $SELF_TEST == 1 ]] && self_test

# =============================================================================================
# setup
# =============================================================================================
if [[ -z "$ROOT" ]]; then ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null)" || { echo "not in a git checkout; use --root" >&2; exit 2; }; fi
ROOT="$(cd "$ROOT" && pwd)" || exit 2
cd "$ROOT" || exit 2
ALLOW="$ROOT/tools/publishable-allow.txt"; [[ -f "$ALLOW" ]] || ALLOW="$SCRIPT_DIR/publishable-allow.txt"
LAND="$ROOT/tools/publishable-landmarks.txt"; [[ -f "$LAND" ]] || LAND="$SCRIPT_DIR/publishable-landmarks.txt"
[[ -f "$ALLOW" ]] || ALLOW=/dev/null
[[ -f "$LAND" ]] || LAND=/dev/null

WORK="$(mktemp -d "${TMPDIR:-/tmp}/publish-check.XXXXXX")" || exit 2
chmod 700 "$WORK"
if [[ -n "${EMBER_PUBLISH_KEEP:-}" ]]; then echo "work directory kept: $WORK" >&2; else trap 'rm -rf "$WORK"' EXIT; fi
HITS="$WORK/hits.tsv"; : > "$HITS"
T0=$(date +%s)

# --- the publishable file set -------------------------------------------------------------------
git ls-files -co --exclude-standard -z > "$WORK/listed.z" || { echo "git ls-files failed" >&2; exit 2; }
: > "$WORK/all.z"; : > "$WORK/all.txt"
while IFS= read -r -d '' f; do
  [[ -f "$f" && ! -L "$f" ]] || continue
  printf '%s\0' "$f" >> "$WORK/all.z"; printf '%s\n' "$f" >> "$WORK/all.txt"
done < "$WORK/listed.z"
NFILES=$(wc -l < "$WORK/all.txt" | tr -d ' ')
# Text files: grep -I treats files with NUL bytes as binary and lists only text files here.
xargs -0 grep -l -I --null -e '' -- < "$WORK/all.z" > "$WORK/text.z" 2>/dev/null
tr '\0' '\n' < "$WORK/text.z" > "$WORK/text.txt"
# Split for rules that treat the guide library differently.
grep -v '^guides/content/' "$WORK/text.txt" | tr '\n' '\0' > "$WORK/code.z"
grep '^guides/content/' "$WORK/text.txt" | tr '\n' '\0' > "$WORK/guides.z"
# Binary = listed, non-empty, not text.
awk 'NR == FNR { t[$0] = 1; next } !($0 in t)' "$WORK/text.txt" "$WORK/all.txt" | while IFS= read -r f; do
  [[ -s "$f" ]] && printf '%s\n' "$f"
done > "$WORK/binary.txt"

# --- allow-list / finding filter (awk, portable: no regex intervals) --------------------------------
cat > "$WORK/filter.awk" <<'AWK'
function glob2re(g,   r, i, c) {
  r = "^"
  for (i = 1; i <= length(g); i++) {
    c = substr(g, i, 1)
    if (c == "*") r = r ".*"; else if (c == "?") r = r "."
    else if (index(".[]()+^$|\\{}", c)) r = r "\\" c
    else r = r c
  }
  return r "$"
}
function abs(x) { return x < 0 ? -x : x }
function mask(s) { return length(s) <= 4 ? "****" : substr(s, 1, 4) "…(" length(s) " chars)" }
function luhn(s,   i, d, sum, alt) {
  sum = 0; alt = 0
  for (i = length(s); i >= 1; i--) { d = substr(s, i, 1) + 0; if (alt) { d *= 2; if (d > 9) d -= 9 } sum += d; alt = !alt }
  return sum % 10 == 0
}
function private_ip(o1, o2, o3) {
  if (o1 == 0 || o1 == 10 || o1 == 127 || o1 >= 224) return 1
  if (o1 == 169 && o2 == 254) return 1
  if (o1 == 172 && o2 >= 16 && o2 <= 31) return 1
  if (o1 == 192 && o2 == 168) return 1
  if (o1 == 100 && o2 >= 64 && o2 <= 127) return 1
  if (o1 == 192 && o2 == 0 && (o3 == 2 || o3 == 0)) return 1
  if (o1 == 198 && o2 == 51 && o3 == 100) return 1
  if (o1 == 203 && o2 == 0 && o3 == 113) return 1
  if (o1 == 198 && (o2 == 18 || o2 == 19)) return 1
  return 0
}
# Returns the detail text, or "" to drop the finding.
function check(m,   lm, n, o, i, sep, v, local, dom, nums, s, k, lat, lon, comp) {
  lm = tolower(m)
  if (rule == "mac") {
    n = split(lm, o, /[:-]/); if (n != 6) return ""
    sep = substr(lm, 3, 1); if (index(lm, sep == ":" ? "-" : ":")) return ""
    if (lm ~ /^00.00.5e.00.53./) return ""
    return "MAC address " m
  }
  if (rule == "ip") {
    n = split(m, o, "."); if (n != 4) return ""
    for (i = 1; i <= 4; i++) { if (o[i] + 0 > 255 || (length(o[i]) > 1 && substr(o[i], 1, 1) == "0")) return "" }
    if (private_ip(o[1] + 0, o[2] + 0, o[3] + 0)) return ""
    return "public IPv4 address " m
  }
  if (rule == "imei") {
    if (length(m) != 15 || !luhn(m)) return ""
    s = m; gsub(substr(m, 1, 1), "", s); if (s == "") return ""
    if (m ~ /^(0123456789|1234567890)/) return ""
    return "15-digit number that passes the IMEI check: " m
  }
  if (rule == "email") {
    local = substr(lm, 1, index(lm, "@") - 1); dom = substr(lm, index(lm, "@") + 1)
    if (local ~ /^(this|super|return|break|continue|outer|inner|loop)$/) return ""
    if (dom ~ /\.(png|jpe?g|svg|gif|webp|js|mjs|css|ts|kt|swift|cpp|h)$/) return ""
    return "e-mail address " m
  }
  if (rule == "callsign") {
    if (m ~ /^Q/ || m ~ /^N0CALL/) return ""
    return "callsign-shaped literal " m " (samples must be N0CALL-n or Q-prefixed)"
  }
  if (rule == "coord") {
    n = 0; s = m
    while (n < 2 && match(s, /-?[0-9]+\.[0-9]+/)) { nums[++n] = substr(s, RSTART, RLENGTH) + 0; s = substr(s, RSTART + RLENGTH) }
    if (n < 2) return ""
    lat = nums[1]; lon = nums[2]
    if (abs(lat) > 90 || abs(lon) > 180) return ""
    if (abs(lat) < 1 && abs(lon) < 1) return ""
    for (k = 1; k <= nl; k++) if (abs(lat - LA[k]) < 0.02 && abs(lon - LO[k]) < 0.02) return ""
    return "coordinates " lat ", " lon " are not a listed landmark (tools/publishable-landmarks.txt)"
  }
  if (rule == "credential") {
    if (m ~ /^-----BEGIN/ || m ~ /^PuTTY/) return "private key block"
    sub(/^[^A-Za-z0-9-]+/, "", m)
    if (m ~ /^(AKIA|ASIA|gh[pousr]_|github_pat_|glpat-|AIza|xox|sk-|sk_live|rk_live|npm_|eyJ)/) { if (m !~ /[0-9]/) return ""; return "token-shaped string " mask(m) }
    if (match(m, /[:=][ \t"']*[^:= \t"']+$/)) {
      v = substr(m, RSTART + 1); gsub(/^[ \t"']+/, "", v)
      if (v !~ /[0-9]/ || v !~ /[A-Za-z]/) return ""
      if (tolower(v) ~ /example|placeholder|changeme|your|xxxx|redacted|dummy|fake|sample|test/) return ""
      return "credential-like assignment, value " mask(v)
    }
    return "token-shaped string " mask(m)
  }
  if (rule == "home-path") {
    comp = m; sub(/^[A-Za-z]:/, "", comp); gsub(/\\/, "/", comp)
    sub(/^\/(Users|home|users)\//, "", comp); sub(/\/.*$/, "", comp)
    if (tolower(comp) ~ /^(username|user|you|yourname|your-name|your_name|yourusername|me|name|example|shared|guest|pi|ubuntu|runner|admin|someone|alice|bob|web_user|default|public|all users|nobody)$/ || comp == "") return ""
    return "absolute home path " m
  }
  if (rule == "uuid") {
    if (m ~ /[A-F]/ && m !~ /[a-f]/) return "UUID " m " (upper case: the shape of an Apple device/simulator UDID)"
    return "UUID " m " (captured device id? allow-list protocol constants)"
  }
  if (rule == "machine") {
    for (k = 1; k <= ni; k++) if (index(lm, IV[k])) return "identifies this machine/account: " IK[k] " (value hidden)"
    return "identifies this machine/account (value hidden)"
  }
  if (rule == "name-word") return "bare user/first name word (value hidden)"
  return desc " " m
}
BEGIN {
  FS = "\n"; np = 0; nm = 0; nt = 0; nl = 0; ni = 0; nidp = 0; nnm = 0; fullname = ""
  while ((getline ln < allow) > 0) {
    sub(/[ \t]+#.*$/, "", ln); if (ln ~ /^[ \t]*(#|$)/) continue
    n = split(ln, a, /[ \t]+/)
    if (n >= 3 && a[1] == "identity" && a[2] == "path") { if (rule == "machine") IDP[++nidp] = glob2re(a[3]); continue }
    if (n < 3 || (a[1] != rule && a[1] != "*")) continue
    if (a[2] == "path") P[++np] = glob2re(a[3]); else if (a[2] == "match") M[++nm] = tolower(a[3])
    else if (a[2] == "term" && a[1] == rule) T[++nt] = a[3]
  }
  if (land != "") while ((getline ln < land) > 0) {
    sub(/#.*$/, "", ln); n = split(ln, a, /[ \t]+/); k = 0
    for (i = 1; i <= n; i++) if (a[i] != "") b[++k] = a[i]
    if (k >= 2) { nl++; LA[nl] = b[1] + 0; LO[nl] = b[2] + 0 }
  }
  if (ids != "") while ((getline ln < ids) > 0) { i = index(ln, "\t"); if (i) { ni++; IK[ni] = substr(ln, 1, i - 1); IV[ni] = tolower(substr(ln, i + 1)) } }
  # Owner-approved public identity: the git display name (and its words) only.
  for (k = 1; k <= ni; k++) {
    if (IK[k] == "git user.name") { NM[++nnm] = IV[k]; fullname = IV[k] }
    else if (IK[k] == "git user.name part") NM[++nnm] = IV[k]
  }
}
function is_name(s,   k) { for (k = 1; k <= nnm; k++) if (s == NM[k]) return 1; return 0 }
function has_word(s, w,   p, b, e) {
  while ((p = index(s, w)) > 0) {
    b = (p == 1) ? "" : substr(s, p - 1, 1); e = substr(s, p + length(w), 1)
    if (b !~ /[a-z0-9_]/ && e !~ /[a-z0-9_]/) return 1
    s = substr(s, p + 1)
  }
  return 0
}
# 1 if a machine finding is the git display name inside a copyright statement of a file listed as
# `identity path` in the allow-list, and that line holds no other identifier of this machine.
function approved_identity(f, line, m,   k, ok, t, n, ln, ll, rest) {
  if (nidp == 0 || fullname == "" || m ~ /[@\/\\~:=]/) return 0
  ok = 0; for (k = 1; k <= nidp; k++) if (f ~ IDP[k]) ok = 1
  if (!ok) return 0
  t = tolower(m); gsub(/^[^a-z0-9]+/, "", t); gsub(/[^a-z0-9]+$/, "", t)
  if (!is_name(t)) return 0
  n = 0; ln = ""
  while ((getline ln < f) > 0) if (++n == line + 0) break
  close(f)
  if (n != line + 0) return 0
  ll = tolower(ln)
  # "copyright (c) 2026 <name>", "copyright 2024-2026 <name>", "(c) 2026 <name>", "© 2026 <name>"
  if (!match(ll, /(copyright([ \t]*(\(c\)|©))?|\(c\)|©)[ \t]*[12][0-9][0-9][0-9]([ \t]*[-,][ \t]*[12][0-9][0-9][0-9])*[ \t,]+/)) return 0
  rest = substr(ll, RSTART + RLENGTH)
  if (index(rest, fullname) != 1 || substr(rest, length(fullname) + 1, 1) ~ /[a-z0-9_@]/) return 0
  if (index(fullname, t) == 0) return 0
  for (k = 1; k <= ni; k++) if (!is_name(IV[k]) && has_word(ll, IV[k])) return 0
  return 1
}
# 1 if every occurrence of the matched word on that line lies inside one of the literal `term`
# entries of the allow-list (case-sensitive; e.g. a file or directory name a tool convention
# requires). The same word anywhere else on the line still fails.
function only_terms(f, line, m,   n, ln, k, p) {
  n = 0; ln = ""
  while ((getline ln < f) > 0) if (++n == line + 0) break
  close(f)
  if (n != line + 0) return 0
  for (k = 1; k <= nt; k++) while ((p = index(ln, T[k])) > 0) ln = substr(ln, 1, p - 1) " " substr(ln, p + length(T[k]))
  return !has_word(tolower(ln), tolower(m))
}
{
  i = index($0, ":"); f = substr($0, 1, i - 1); r = substr($0, i + 1)
  j = index(r, ":"); line = substr(r, 1, j - 1); m = substr(r, j + 1)
  if (mode == "path") { f = $0; line = 0; m = $0 }
  if (cc != "") {
    while (m != "" && substr(m, 1, 1) !~ cc) m = substr(m, 2)
    while (m != "" && substr(m, length(m), 1) !~ cc) m = substr(m, 1, length(m) - 1)
  }
  shown = f
  if (strip != "" && index(f, strip) == 1) { shown = substr(f, length(strip) + 1); sub(/\.strings$/, "", shown); line = "strings" }
  for (k = 1; k <= np; k++) if (shown ~ P[k]) next
  if (m != "") { lmm = tolower(m); for (k = 1; k <= nm; k++) if (lmm ~ M[k]) next }
  if (rule == "machine" && shown == f && approved_identity(f, line, m)) next
  if (nt > 0 && shown == f && line + 0 > 0 && m != "" && only_terms(f, line, m)) next
  d = check(m); if (d == "") next
  key = rule SUBSEP shown SUBSEP line SUBSEP d; if (key in seen) next; seen[key] = 1
  print level "\t" rule "\t" shown "\t" line "\t" d
}
AWK

# filter <rule> <level> <coreclass> [desc] [extra awk -v ...]  (reads grep -n -o output on stdin)
filter() {
  local rule="$1" level="$2" cc="$3" desc="${4:-}"; shift 4 2>/dev/null || shift $#
  awk -v rule="$rule" -v level="$level" -v cc="$cc" -v desc="$desc" -v allow="$ALLOW" -v land="$LAND" \
      -v ids="${IDS_KINDS:-}" -v strip="$WORK/bin/" -v mode="${FILTER_MODE:-}" "$@" -f "$WORK/filter.awk" >> "$HITS"
}
# Content rules are registered with `rule` and run by `run_rules` in ONE pass over every file
# (perl, which ships with git on every platform) or, without perl, one grep per rule. Patterns use
# the common subset of ERE and Perl syntax. A guard is a comma-separated list of lower-case
# literals: a line is matched against the pattern only if it contains one of them (speed only).
: > "$WORK/rules.tsv"; : > "$WORK/rulemeta.tsv"; mkdir -p "$WORK/raw"
rule() { printf '%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "${4:-:none:}" "$5" >> "$WORK/rules.tsv"; }  # rule <name> <scope> <ci> <guard> <re>
meta() { printf '%s\t%s\t%s\t%s\n' "$1" "$2" "${3:-:none:}" "${4:-:none:}" >> "$WORK/rulemeta.tsv"; }        # meta <name> <level> <coreclass> <desc>
run_rules() {
  if command -v perl >/dev/null 2>&1 && [[ "${EMBER_PUBLISH_ENGINE:-}" != grep ]]; then
    cat > "$WORK/scan.pl" <<'PERL'
      use strict; use warnings; no warnings "uninitialized";
      my ($rules, $out, $binpfx, @lists) = @ARGV;
      my (@R, %fh);
      open(my $rf, "<", $rules) or die "rules: $!";
      while (my $ln = <$rf>) {
        chomp $ln; my ($n, $s, $ci, $g, $re) = split /\t/, $ln, 5; $g = "" if $g eq ":none:";
        # Boundary groups "(^|[^X])" / "([^X]|$)" become look-arounds: same meaning, much faster.
        $re =~ s/\(\^\|\[\^([^\]]+)\]\)/(?<![$1])/g; $re =~ s/\(\[\^([^\]]+)\]\|\$\)/(?![$1])/g;
        my $q = $ci ? qr/$re/mi : qr/$re/m;
        open($fh{$n}, ">>", "$out/$n") or die "$out/$n: $!" unless $fh{$n};
        push @R, [$n, $s, $q, [grep { length } split /,/, $g]];
      }
      my %ok = (allbin => sub { 1 }, text => sub { !$_[0] }, code => sub { !$_[0] && !$_[1] },
                codebin => sub { !$_[1] }, guides => sub { $_[1] }, svg => sub { $_[2] });
      for my $list (@lists) {
        open(my $lh, "<", $list) or next; my @files = do { local $/ = "\0"; <$lh> }; chomp @files;
        for my $f (@files) {
          $f =~ s/\0$//;
          my $bin = index($f, $binpfx) == 0 ? 1 : 0;
          my $gd = (!$bin && $f =~ m{^guides/content/}) ? 1 : 0;
          my $svg = (!$bin && $f =~ /\.svg$/i) ? 1 : 0;
          my @A = grep { $ok{$_->[1]}->($bin, $gd, $svg) } @R; next unless @A;
          open(my $h, "<", $f) or next; binmode $h;
          my $c = do { local $/; <$h> }; close $h;
          next unless defined $c && length $c;
          my $lc;
          RULE: for my $r (@A) {
            my $g = $r->[3];
            if (@$g) { $lc = lc $c unless defined $lc; my $hit = 0; for (@$g) { if (index($lc, $_) >= 0) { $hit = 1; last } } next RULE unless $hit }
            my $q = $r->[2]; my $o = $fh{$r->[0]};
            my ($line, $at) = (1, 0);
            while ($c =~ /$q/g) {
              my ($s, $m) = ($-[0], $&);
              $line += (substr($c, $at, $s - $at) =~ tr/\n//); $at = $s;
              $m =~ s/[\r\n]+$//; $m =~ s/^[\r\n]+//;
              my $ln = $line + ((substr($c, $s, 1) eq "\n") ? 1 : 0);
              print $o "$f:$ln:$m\n";
            }
          }
        }
      }
PERL
    # Split the work over up to 4 processes (round-robin keeps the chunks similar in size).
    local jobs k; jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"; [[ $jobs -gt 4 ]] && jobs=4; [[ $jobs -lt 1 ]] && jobs=1
    cat "$WORK/text.z" "$WORK/binstr.z" | tr '\0' '\n' | awk -v n="$jobs" -v d="$WORK" '{ print > (d "/chunk." (NR % n)) }'
    for ((k = 0; k < jobs; k++)); do
      [[ -f "$WORK/chunk.$k" ]] || continue
      tr '\n' '\0' < "$WORK/chunk.$k" > "$WORK/chunk.$k.z"; mkdir -p "$WORK/raw.$k"
      perl "$WORK/scan.pl" "$WORK/rules.tsv" "$WORK/raw.$k" "$WORK/bin/" "$WORK/chunk.$k.z" &
    done
    wait
    for ((k = 0; k < jobs; k++)); do
      [[ -d "$WORK/raw.$k" ]] || continue
      for f in "$WORK/raw.$k"/*; do [[ -f "$f" ]] && cat "$f" >> "$WORK/raw/$(basename "$f")"; done
    done
  else
    local name scope ci re flags
    while IFS=$'\t' read -r name scope ci _ re; do   # the guard column only speeds up perl
      flags="-E"; [[ $ci == 1 ]] && flags="-E -i"
      # shellcheck disable=SC2086
      [[ -s "$WORK/scope-$scope.z" ]] && xargs -0 grep -H -n -o -I $flags -e "$re" -- < "$WORK/scope-$scope.z" 2>/dev/null >> "$WORK/raw/$name"
    done < "$WORK/rules.tsv"
  fi
  local level cc desc
  while IFS=$'\t' read -r name level cc desc; do
    [[ $cc == :none: ]] && cc=""; [[ $desc == :none: ]] && desc=""
    touch "$WORK/raw/$name"; filter "$name" "$level" "$cc" "$desc" < "$WORK/raw/$name"
  done < "$WORK/rulemeta.tsv"
}

B_ALNUM='(^|[^A-Za-z0-9_])'
E_ALNUM='([^A-Za-z0-9_]|$)'


# =============================================================================================
# 1. identifiers of this machine and account (run time only; written to a private temp file)
# =============================================================================================
IDS_KINDS="$WORK/ids.tsv"; : > "$IDS_KINDS"; : > "$WORK/ctx.tsv"
add_id()  { [[ ${#2} -ge 3 ]] && printf '%s\t%s\n' "$1" "$2" >> "$IDS_KINDS"; }   # match anywhere (word-bounded)
add_ctx() { [[ ${#2} -ge 2 ]] && printf '%s\t%s\n' "$1" "$2" >> "$WORK/ctx.tsv"; } # match only in identifying contexts
add_name() {  # long or multi-word values are distinctive; short single words only in contexts
  local kind="$1" v="$2"
  [[ -z "$v" ]] && return
  if [[ "$v" == *" "* || ${#v} -ge 6 ]]; then add_id "$kind" "$v"; else add_ctx "$kind" "$v"; fi
}
add_ip() { case "$2" in 127.*|0.0.0.0|169.254.*|"") ;; *) add_id "$1" "$2" ;; esac; }

# --ci: skip everything up to "end of the identifiers skipped by --ci" (see the header).
if [[ $CI_MODE == 0 ]]; then
U="$(id -un 2>/dev/null || echo "${USER:-}")"
add_name "local user name" "$U"
[[ -n "${USER:-}" && "${USER:-}" != "$U" ]] && add_name "\$USER" "$USER"
[[ -n "${HOME:-}" && "$HOME" != "/" && ${#HOME} -ge 6 ]] && add_id "home directory" "$HOME"
H="$(hostname 2>/dev/null || uname -n 2>/dev/null || true)"
if [[ -n "$H" ]]; then
  add_id "host name" "$H"
  IFS=. read -r -a labels <<< "$H"
  for l in ${labels[@]+"${labels[@]}"}; do
    case "$(printf '%s' "$l" | tr 'A-Z' 'a-z')" in local|lan|home|localdomain|localhost|internal|corp|domain|router|dhcp|"") continue ;; esac
    [[ ${#l} -ge 4 ]] && add_id "host name label" "$l"
  done
fi
if command -v scutil >/dev/null 2>&1; then
  for k in ComputerName LocalHostName HostName; do
    v="$(with_timeout 5 scutil --get "$k" 2>/dev/null || true)"
    [[ -n "$v" ]] || continue
    add_id "macOS $k" "$v"
    v2="$(printf '%s' "$v" | sed "s/’/'/g")"; [[ "$v2" != "$v" ]] && add_id "macOS $k" "$v2"
  done
fi
GN="$(git config --get user.name 2>/dev/null || true)"
GE="$(git config --get user.email 2>/dev/null || true)"
add_name "git user.name" "$GN"
if [[ -n "$GN" && "$GN" == *" "* ]]; then for w in $GN; do [[ ${#w} -ge 6 ]] && add_id "git user.name part" "$w"; [[ ${#w} -ge 2 && ${#w} -lt 6 ]] && add_ctx "git user.name part" "$w"; done; fi
if [[ -n "$GE" ]]; then
  add_id "git user.email" "$GE"
  lp="${GE%@*}"
  add_name "git e-mail local part" "$lp"
  if [[ "$lp" == *+* ]]; then
    for part in ${lp//+/ }; do
      if [[ "$part" =~ ^[0-9]+$ ]]; then [[ ${#part} -ge 5 ]] && add_id "git e-mail numeric id" "$part"
      else [[ ${#part} -ge 4 ]] && add_id "git e-mail handle" "$part"; [[ ${#part} -lt 4 ]] && add_ctx "git e-mail handle" "$part"; fi
    done
  fi
fi
for v in "$(git config --get github.user 2>/dev/null || true)" \
         "$(sed -n 's/^[[:space:]]*user:[[:space:]]*//p' "${HOME:-/nonexistent}/.config/gh/hosts.yml" 2>/dev/null | head -1)"; do
  [[ -n "$v" ]] && add_name "GitHub user" "$v"
done
# Network: local IPv4 and MAC addresses, Wi-Fi SSID.
if command -v ifconfig >/dev/null 2>&1; then NETINFO="$(with_timeout 5 ifconfig -a 2>/dev/null || true)"
elif command -v ip >/dev/null 2>&1; then NETINFO="$(with_timeout 5 ip addr 2>/dev/null || true)"
else NETINFO=""; fi
for a in $(printf '%s\n' "$NETINFO" | sed -n 's/.*inet \(addr:\)\{0,1\}\([0-9][0-9.]*\).*/\2/p' | sort -u); do add_ip "local IPv4 address" "$a"; done
for a in $(printf '%s\n' "$NETINFO" | grep -Eo '(ether|link/ether|HWaddr) [0-9a-fA-F:]{17}' | awk '{print $2}' | sort -u); do
  case "$a" in 00:00:00:00:00:00|ff:ff:ff:ff:ff:ff) ;; *) add_id "local MAC address" "$a" ;; esac
done
SSID=""
if command -v networksetup >/dev/null 2>&1; then
  for dev in $(with_timeout 5 networksetup -listallhardwareports 2>/dev/null | awk '/Wi-Fi|AirPort/{getline; print $2}'); do
    s="$(with_timeout 5 networksetup -getairportnetwork "$dev" 2>/dev/null | sed -n 's/^Current Wi-Fi Network: //p')"; [[ -n "$s" ]] && SSID="$s"
  done
fi
if [[ -z "$SSID" ]] && command -v iwgetid >/dev/null 2>&1; then SSID="$(with_timeout 5 iwgetid -r 2>/dev/null || true)"; fi
if [[ -z "$SSID" ]] && command -v nmcli >/dev/null 2>&1; then SSID="$(with_timeout 5 nmcli -t -f active,ssid dev wifi 2>/dev/null | sed -n 's/^yes://p' | head -1)"; fi
case "$SSID" in ""|"<redacted>") ;; *) [[ ${#SSID} -ge 4 ]] && add_id "Wi-Fi SSID" "$SSID" ;; esac
# Simulators, emulators, signing identities.
for d in "${HOME:-/nonexistent}/Library/Developer/CoreSimulator/Devices"/*; do
  b="$(basename "$d")"; [[ "$b" =~ ^[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}$ ]] && add_id "iOS simulator UDID" "$b"
done
for d in "${HOME:-/nonexistent}/.android/avd"/*.avd; do [[ -d "$d" ]] && add_id "Android AVD name" "$(basename "$d" .avd)"; done
if command -v security >/dev/null 2>&1; then
  for v in $(with_timeout 5 security find-identity -v -p codesigning 2>/dev/null | grep -Eo '\([A-Z0-9]{10}\)' | tr -d '()' | sort -u); do add_id "Apple team id" "$v"; done
fi
fi  # end of the identifiers skipped by --ci
# emberd admin tokens from default data directories and from builds/test runs inside the checkout.
for f in "${EMBER_DATA_DIR:-/nonexistent}/admin_token" "${HOME:-/nonexistent}/Library/Application Support/EmberMesh/admin_token" \
         "${XDG_DATA_HOME:-${HOME:-/nonexistent}/.local/share}/EmberMesh/admin_token" "${APPDATA:-/nonexistent}/EmberMesh/admin_token" \
         $(find "$ROOT/build" "$ROOT/dist" -maxdepth 6 -name admin_token -type f 2>/dev/null | head -50 | tr '\n' ' '); do
  [[ -f "$f" ]] || continue
  v="$(tr -d ' \r\n' < "$f" 2>/dev/null | head -c 200)"; [[ ${#v} -ge 12 ]] && add_id "emberd admin token" "$v"
done
for v in ${EMBER_PUBLISH_EXTRA_IDS:-}; do add_id "extra identifier (EMBER_PUBLISH_EXTRA_IDS)" "$v"; done

esc_ere() { printf '%s' "$1" | sed 's/[][\.*^$+?(){}|]/\\&/g'; }
build_alt() {  # build_alt <tsv> [kind-filter: only|skip <kind>] -> a|b|c
  local alt="" k v
  while IFS=$'\t' read -r k v; do
    [[ -n "$v" ]] || continue
    [[ "${2:-}" == only && "$k" != "$3" ]] && continue
    [[ "${2:-}" == skip && "$k" == "$3" ]] && continue
    alt="${alt:+$alt|}$(esc_ere "$v")"
  done < "$1"
  printf '%s' "$alt"
}
guard_of() {  # lower-case values joined by commas ("" = no guard, if a value contains a comma)
  local g; g="$(cut -f2 "$1" | tr 'A-Z' 'a-z')"
  case "$g" in *,*) echo "" ;; *) printf '%s' "$g" | tr '\n' ',' ;; esac
}
ALL_ALT="$(build_alt "$IDS_KINDS")"
ID_ALT="$(build_alt "$IDS_KINDS" skip "host name label")"
LABEL_ALT="$(build_alt "$IDS_KINDS" only "host name label")"
CTX_ALT="$(build_alt "$WORK/ctx.tsv")"
G_ALL="$(guard_of "$IDS_KINDS")"; G_CTX="$(guard_of "$WORK/ctx.tsv")"
# context identifiers also resolve to a kind in the output
cat "$WORK/ctx.tsv" >> "$IDS_KINDS"

# --- binary files: extract printable strings so the text rules also see inside them -------------------
mkdir -p "$WORK/bin"; : > "$WORK/binstr.z"
while IFS= read -r f; do
  out="$WORK/bin/$f.strings"; mkdir -p "$(dirname "$out")"
  {
    case "$f" in
      *.jar|*.zip|*.aar|*.apk|*.ipa) if command -v unzip >/dev/null 2>&1; then unzip -Z1 "$f" 2>/dev/null; unzip -z "$f" 2>/dev/null; unzip -p "$f" 2>/dev/null | LC_ALL=C tr -c '[:print:]\n\t' '\n'; fi ;;
    esac
    LC_ALL=C tr -c '[:print:]\t' '\n' < "$f"
  } | grep -E '.{4,}' > "$out" 2>/dev/null || true
  printf '%s\0' "$out" >> "$WORK/binstr.z"
done < "$WORK/binary.txt"
# Scope lists for the grep fallback.
cp "$WORK/text.z" "$WORK/scope-text.z"; cp "$WORK/code.z" "$WORK/scope-code.z"; cp "$WORK/guides.z" "$WORK/scope-guides.z"
cat "$WORK/code.z" "$WORK/binstr.z" > "$WORK/scope-codebin.z"
cat "$WORK/text.z" "$WORK/binstr.z" > "$WORK/scope-allbin.z"
grep -i '\.svg$' "$WORK/text.txt" | tr '\n' '\0' > "$WORK/scope-svg.z"

# --- machine rule ---------------------------------------------------------------------------------------
# Outside the guide library: case-insensitive whole words. Inside it (prose full of ordinary words
# that can collide with a short identifier): case-sensitive, and host-name labels only as words that
# are not part of a slug or path (guide ids such as power/<surname>-electricity-... are fine).
meta machine FAIL '' ''
if [[ -n "$ALL_ALT" ]]; then
  rule machine codebin 1 "$G_ALL" "${B_ALNUM}(${ALL_ALT})${E_ALNUM}"
  [[ -n "$ID_ALT" ]] && rule machine guides 0 "$G_ALL" "${B_ALNUM}(${ID_ALT})${E_ALNUM}"
  [[ -n "$LABEL_ALT" ]] && rule machine guides 0 "$G_ALL" "(^|[^A-Za-z0-9_/-])(${LABEL_ALT})([^A-Za-z0-9_/-]|\$)"
fi
if [[ -n "$CTX_ALT" ]]; then
  rule machine allbin 1 "$G_CTX" "(^|[^A-Za-z0-9_])(([/\\\\~@](${CTX_ALT})|(user|username|owner|author|login)[\"']?[ ]*[=:][ ]*[\"']?(${CTX_ALT}))([^A-Za-z0-9_]|\$)|(${CTX_ALT})@)"
  if [[ $STRICT_NAMES == 1 ]]; then meta name-word WARN '' ''; rule name-word text 0 "" "${B_ALNUM}(${CTX_ALT})${E_ALNUM}"; fi
fi

# =============================================================================================
# 2. generic content rules
# =============================================================================================
meta home-path FAIL '' ''
rule home-path allbin 0 "/users,/home,:\\users,:/users" '(/Users|/home)/[A-Za-z0-9._-]+|[A-Za-z]:(\\\\|\\|/)(Users|users)(\\\\|\\|/)[A-Za-z0-9._-]+'
meta temp-path FAIL '' 'session/temp path'
rule temp-path allbin 1 "/private/,/var/fold,claude-,/tmp/,appdata" '/private/(tmp|var)/[^[:space:]"'"'"']*|/var/fold[e]rs/[^[:space:]"'"'"']*|claude-[0-9]{2,}[^[:space:]"'"'"']*|/tmp/cl[a]ude[^[:space:]"'"'"']*|AppData(\\\\|\\|/)Local(\\\\|\\|/)Temp[^[:space:]"'"'"']*|/tmp/tmp\.[A-Za-z0-9]{6,}'
meta email FAIL '' ''
rule email allbin 0 "@" '[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}'
meta credential FAIL '' ''
rule credential allbin 0 "-----begin,putty,akia,asia,ghp_,gho_,ghu_,ghs_,ghr_,github_pat_,glpat-,aiza,xox,sk-,_live_,npm_,eyj" '-----BEGIN ([A-Z0-9]+ )*PRIVATE KEY( BLOCK)?-----|PuTTY-User-Key-File-[0-9]|(^|[^A-Za-z0-9_-])(AKIA[0-9A-Z]{16}|ASIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{36}|github_pat_[A-Za-z0-9_]{22,}|glpat-[A-Za-z0-9_-]{20}|AIza[0-9A-Za-z_-]{35}|xox[abposr]-[A-Za-z0-9-]{10,}|sk-(ant-)?[A-Za-z0-9_-]{32,}|(sk|rk)_live_[A-Za-z0-9]{16,}|npm_[A-Za-z0-9]{36}|eyJ[A-Za-z0-9_-]{10,}\.eyJ[A-Za-z0-9_-]{10,}\.[A-Za-z0-9_-]{10,})'
rule credential codebin 1 "pass,secret,token,key" '(password|passwd|passphrase|secret|token|api[_-]?key|access[_-]?key|auth[_-]?key|client[_-]?secret|private[_-]?key)["'"'"']?[[:space:]]*[:=][[:space:]]*["'"'"']?[A-Za-z0-9+/_.~-]{16,}'
meta uuid FAIL '[0-9A-Fa-f-]' ''
rule uuid text 0 "-" "${B_ALNUM}[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}${E_ALNUM}"
meta mac FAIL '[0-9A-Fa-f:-]' ''
rule mac text 0 ":,-" '(^|[^0-9A-Fa-f:-])[0-9A-Fa-f]{2}([:-][0-9A-Fa-f]{2}){5,}([^0-9A-Fa-f:-]|$)'
meta ip FAIL '[0-9.]' ''
rule ip text 0 "." '(^|[^0-9.])[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}([^0-9.]|$)'
RESIDUE_WORDS="claude|anthropic|chatgpt|openai|copilot|llm|subagent|sub-agent|subagents|sub-agents|ai assistant|coordinator|another engineer|other engineers|scratchpad|scratch pad|auditor|auditors"
meta residue FAIL '[A-Za-z]' 'process residue:'
rule residue text 1 "$(printf '%s' "$RESIDUE_WORDS" | tr '|' ',')" "${B_ALNUM}(${RESIDUE_WORDS})${E_ALNUM}"
meta imei WARN '[0-9]' ''
rule imei text 0 "" '(^|[^0-9.])[0-9]{15}([^0-9.]|$)'
meta callsign WARN '[A-Z0-9]' ''
rule callsign code 0 "" "${B_ALNUM}[A-Z]{1,2}[0-9][A-Z]{2,3}(-[0-9]{1,2})?${E_ALNUM}"
meta coord WARN '' ''
rule coord code 0 "." '-?[0-9]{1,3}\.[0-9]{4,}[[:space:]]*,[[:space:]]*-?[0-9]{1,3}\.[0-9]{4,}|[Ll]at(itude)?["'"'"']?[[:space:]]*[:=][[:space:]]*-?[0-9]{1,2}\.[0-9]{4,}[^0-9-]{1,16}-?[0-9]{1,3}\.[0-9]{4,}'
meta svg-meta FAIL '' 'SVG editor metadata:'
rule svg-meta svg 1 "sodipodi,inkscape,rdf,dc:creator,xmpmeta,adobe,metadata" 'sodipodi:docname|inkscape:export-filename|inkscape:version|<rdf:RDF|<dc:creator|<x:xmpmeta|adobe:ns:meta|<metadata'
run_rules
# SVG findings are reported under image-meta.
sed 's/^\([A-Z]*\)\tsvg-meta\t/\1\timage-meta\t/' "$HITS" > "$HITS.tmp" && mv "$HITS.tmp" "$HITS"

# =============================================================================================
# 3. file-level rules
# =============================================================================================
FILTER_MODE=path
# Published directories and files (`filename dir <prefix>` / `filename file <path>` in the
# allow-list, e.g. the project skills under .claude/skills/ and the shared .claude/settings.json):
# the directory-name part of the filename rule does not apply to that exact prefix or path at the
# top of the repository. Every other filename pattern (keys, keystores, logs, OS and
# editor files, build directories) still applies to the files inside it, and the same directory
# name anywhere else is still refused. The prefix is masked with a neutral name for this one grep
# and the matching lines are mapped back to the real paths.
awk -v allow="$ALLOW" '
  BEGIN {
    while ((getline ln < allow) > 0) {
      sub(/[ \t]+#.*$/, "", ln); if (ln ~ /^[ \t]*(#|$)/) continue
      n = split(ln, a, /[ \t]+/)
      if (n >= 3 && a[1] == "filename" && a[2] == "dir") { d = a[3]; sub(/\/+$/, "", d); if (d != "") D[++nd] = d "/" }
      if (n >= 3 && a[1] == "filename" && a[2] == "file") F[a[3]] = 1
    }
  }
  {
    m = $0
    if (m in F) { sub(/^.*\//, "", m); m = "published-dir/" m }
    else for (k = 1; k <= nd; k++) if (index(m, D[k]) == 1) { m = "published-dir/" substr(m, length(D[k]) + 1); break }
    print m
  }
' "$WORK/all.txt" > "$WORK/all-masked.txt"
grep -n -E -i '(^|/)(\.DS_Store|\._[^/]*|Thumbs\.db|ehthumbs\.db|desktop\.ini|\.directory|Icon[^[:print:]])$|(^|/)(\.idea|\.vscode|\.vs|\.fleet|\.cursor|\.claude|\.kotlin|\.gradle|\.cxx|\.externalNativeBuild|captures|xcuserdata|DerivedData|node_modules|__pycache__|\.venv|venv|\.pytest_cache|\.mypy_cache|\.cache|\.toolchains|CMakeFiles|\.history|build|dist|out|\.Trash[^/]*|[^/]*\.xcresult|[^/]*\.xcarchive|[^/]*\.dSYM)/|\.(p12|pfx|jks|keystore|pem|key|p8|mobileprovision|provisionprofile|cer|crt|der|csr|kdbx|ovpn|ppk|gpg|log|trace|crash|ips|dmp|hprof|xcuserstate|xccheckout|xcscmblueprint|swp|swo|orig|rej|bak|tmp|apk|aab|ipa|o|obj|a|lib|so|dylib|dll|exe|wasm|class|dex|pyc|pyo|iml|sqlite|sqlite3|db|pack|lnk|sublime-workspace|code-workspace)$|(^|/)(id_rsa|id_dsa|id_ecdsa|id_ed25519)(\.pub)?$|(^|/)\.env(\.[^/]*)?$|(^|/)(\.netrc|\.npmrc|\.pypirc|\.htpasswd|local\.properties|keystore\.properties|signing\.properties|google-services\.json|GoogleService-Info\.plist|ExportOptions\.plist|compile_commands\.json|CMakeCache\.txt|CMakeUserPresets\.json|\.bash_history|\.zsh_history|\.python_history|\.node_repl_history|\.lesshst|\.viminfo|admin_token|emberd\.json|CLAUDE\.local\.md)$|~$|(^|/)#[^/]*#$|(^|/)\.#' "$WORK/all-masked.txt" \
  | cut -d: -f1 | awk 'NR == FNR { hit[$1] = 1; next } FNR in hit' - "$WORK/all.txt" \
  | grep -v -E '(^|/)\.env\.example$' | filter filename FAIL '' 'file must not be published:'
filter binary FAIL '' 'binary file not approved in tools/publishable-allow.txt:' < "$WORK/binary.txt"
xargs -0 wc -c -- < "$WORK/all.z" 2>/dev/null | awk -v max="$MAX_SIZE" '$2 != "total" && $1 > max { sub(/^ *[0-9]+ /, ""); print }' \
  | filter large FAIL '' "larger than $MAX_SIZE bytes:"
FILTER_MODE=

# --- image metadata (PNG chunks, JPEG segments, GIF comments) ---------------------------------------------
image_meta() {  # image_meta <file>  -> prints problems, one per line
  od -An -v -tu1 "$1" 2>/dev/null | awk '
    { for (i = 1; i <= NF; i++) b[n++] = $i + 0 }
    function u32(p) { return ((b[p] * 256 + b[p+1]) * 256 + b[p+2]) * 256 + b[p+3] }
    function u16(p, le) { return le ? b[p] + 256 * b[p+1] : b[p] * 256 + b[p+1] }
    function u32e(p, le) { return le ? b[p] + 256 * (b[p+1] + 256 * (b[p+2] + 256 * b[p+3])) : u32(p) }
    function typ(p) { return sprintf("%c%c%c%c", b[p], b[p+1], b[p+2], b[p+3]) }
    function exif_tags(s, len,   le, ifd, cnt, k, tag, sub_, ok) {   # TIFF structure starting at s
      le = (b[s] == 73); ifd = u32e(s + 4, le); ok = 1
      while (ifd > 0 && ifd < len) {
        cnt = u16(s + ifd, le); sub_ = 0
        for (k = 0; k < cnt; k++) {
          tag = u16(s + ifd + 2 + 12 * k, le)
          if (tag == 34665) sub_ = u32e(s + ifd + 2 + 12 * k + 8, le)
          else if (tag != 40961 && tag != 40962 && tag != 40963 && tag != 274 && tag != 282 && tag != 283 && tag != 296) { printf "EXIF tag 0x%04X beyond pixel size/orientation\n", tag; ok = 0 }
        }
        ifd = sub_
      }
      return ok
    }
    END {
      if (n >= 8 && b[0] == 137 && b[1] == 80 && b[2] == 78 && b[3] == 71) {
        p = 8
        while (p + 8 <= n) {
          len = u32(p); t = typ(p + 4)
          if (t == "tEXt" || t == "zTXt" || t == "iTXt") print "PNG " t " text chunk"
          else if (t == "tIME") print "PNG tIME (modification time) chunk"
          else if (t == "iCCP") print "PNG iCCP (embedded ICC profile with a name) chunk"
          else if (t == "eXIf") exif_tags(p + 8, len)
          else if (t == "caBX" || t == "jumb") print "PNG " t " (content credentials / provenance) chunk"
          else if (t !~ /^(IHDR|PLTE|IDAT|IEND|tRNS|sRGB|gAMA|cHRM|sBIT|bKGD|pHYs|hIST|sPLT|acTL|fcTL|fdAT)$/) print "PNG unknown chunk " t
          if (t == "IEND") break
          p += 12 + len
        }
      } else if (n >= 4 && b[0] == 255 && b[1] == 216) {
        p = 2
        while (p + 4 <= n && b[p] == 255) {
          mk = b[p+1]; len = b[p+2] * 256 + b[p+3]
          if (mk == 218 || mk == 217) break
          if (mk >= 225 && mk <= 239) printf "JPEG APP%d segment (EXIF/XMP/IPTC/vendor metadata)\n", mk - 224
          else if (mk == 254) print "JPEG comment segment"
          p += 2 + len
        }
      } else if (n >= 6 && b[0] == 71 && b[1] == 73 && b[2] == 70) {
        for (i = 13; i + 1 < n; i++) if (b[i] == 33 && (b[i+1] == 254 || b[i+1] == 255)) { print "GIF comment/application extension"; break }
      } else {
        print "image format cannot be checked for metadata (convert to PNG)"
      }
    }'
}
grep -E -i '\.(png|jpe?g|gif|webp|heic|heif|tiff?|bmp|ico|avif)$' "$WORK/all.txt" | while IFS= read -r f; do
  image_meta "$f" | while IFS= read -r why; do printf '%s\n' "$f"$'\t'"$why"; done
done | awk -F'\t' '{ print $1 ":0:" $2 }' | filter image-meta FAIL '' 'image metadata:'

# =============================================================================================
# report
# =============================================================================================
sort -t $'\t' -k1,1 -k2,2 -k3,3 -k4,4n -u "$HITS" -o "$HITS"
# The awk filter prints the description in the last field; for filename/binary/large/image-meta the
# matched text is the path or reason, which is fine.
[[ -n "$HITS_OUT" ]] && cp "$HITS" "$HITS_OUT"
NFAIL=$(awk -F'\t' '$1 == "FAIL"' "$HITS" | wc -l | tr -d ' ')
NWARN=$(awk -F'\t' '$1 == "WARN"' "$HITS" | wc -l | tr -d ' ')
if [[ $QUIET == 0 ]]; then
  awk -F'\t' -v R="$R" -v Y="$Y" -v N="$N" '{
    c = ($1 == "FAIL") ? R : Y
    loc = ($4 == "0") ? $3 : $3 ":" $4
    printf "%s%-4s%s [%s] %s: %s\n", c, $1, N, $2, loc, $5
  }' "$HITS"
fi
T1=$(date +%s)
echo
echo "${B}Publishability check${N}: $NFILES files ($(wc -l < "$WORK/binary.txt" | tr -d ' ') binary) in $((T1 - T0)) s;" \
     "$(wc -l < "$IDS_KINDS" | tr -d ' ') identifiers of this machine checked$([[ $CI_MODE == 1 ]] && echo ' (--ci: identifiers of the CI machine not discovered)')"
if [[ -s "$HITS" ]]; then
  awk -F'\t' '{ k = $1 " " $2; c[k]++ } END { for (k in c) printf "  %-22s %d\n", k, c[k] }' "$HITS" | sort
fi
if [[ $NFAIL -gt 0 ]]; then
  echo "${R}FAILED${N}: $NFAIL finding(s), $NWARN warning(s). Fix them, or allow-list legitimate ones in tools/publishable-allow.txt."
  exit 1
fi
echo "${G}OK${N}: nothing identifying found ($NWARN warning(s))."
exit 0
