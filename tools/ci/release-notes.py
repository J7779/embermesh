#!/usr/bin/env python3
"""Checksums and the notes body of a GitHub release.

  release-notes.py --dir DIR --version V --kind rolling|tag [--toolchains FILE] [--out-notes FILE]
                   [--android-signer release|ci-debug] [--android-cert SHA256]

Writes DIR/SHA256SUMS.txt (sha256sum format, sorted, over every other file in DIR) and the
Markdown notes (stdout, or --out-notes). Commit, ref, run URL and date come from the GITHUB_*
environment and SOURCE_DATE_EPOCH. --toolchains is a "name: version" list (one per line) that is
copied into the notes.
"""
import argparse
import hashlib
import os
import sys
import time

DESCRIPTIONS = [
    ("-emberd-linux-x86_64.tar.gz", "emberd for Linux x86-64 (glibc), with the web app, guides.pack and deploy scripts"),
    ("-emberd-linux-arm64.tar.gz", "emberd for Linux arm64 (Raspberry Pi OS 64-bit and other aarch64 glibc systems)"),
    ("-emberd-macos-arm64.tar.gz", "emberd for macOS on Apple silicon"),
    ("-emberd-macos-x86_64.tar.gz", "emberd for macOS on Intel"),
    ("-emberd-windows-x86_64.zip", "emberd.exe for Windows x86-64"),
    ("-web.zip", "the web app as a static site + PWA (serve the folder with any static HTTP server)"),
    ("embermesh.html", "the whole app in one HTML file without the guide library (open it from disk)"),
    ("embermesh-with-library.html", "the single HTML file with the whole guide library inside"),
    ("-android-debug.apk", "Android debug APK (all ABIs: arm64-v8a, armeabi-v7a, x86_64)"),
    ("-android-release-ci-debug-signed.apk", "Android release APK (minified), signed with the CI runner's throw-away debug key"),
    ("-android-release.apk", "Android release APK (minified, all ABIs), signed with the project's release key"),
    ("guides.pack", "the offline guide library on its own (bundled in every package above)"),
]


def describe(name):
    for suffix, text in DESCRIPTIONS:
        if name.endswith(suffix):
            return text
    return ""


def human(n):
    for unit in ("B", "KiB", "MiB", "GiB"):
        if n < 1024 or unit == "GiB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1024.0
    return str(n)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dir", required=True)
    p.add_argument("--version", required=True)
    p.add_argument("--kind", choices=("rolling", "tag"), required=True)
    p.add_argument("--toolchains")
    p.add_argument("--out-notes")
    p.add_argument("--android-signer", choices=("release", "ci-debug", ""), default="")
    p.add_argument("--android-cert", default="")
    a = p.parse_args(argv)

    files = sorted(f for f in os.listdir(a.dir)
                   if os.path.isfile(os.path.join(a.dir, f)) and f != "SHA256SUMS.txt" and not f.startswith("."))
    if not files:
        print("release-notes.py: no files to release", file=sys.stderr)
        return 1
    sums = [(sha256(os.path.join(a.dir, f)), f) for f in files]
    with open(os.path.join(a.dir, "SHA256SUMS.txt"), "w", encoding="utf-8", newline="\n") as out:
        for h, f in sums:
            out.write(f"{h}  {f}\n")

    env = os.environ
    sha = env.get("GITHUB_SHA", "unknown")
    repo_url = f"{env.get('GITHUB_SERVER_URL', 'https://github.com')}/{env.get('GITHUB_REPOSITORY', '')}"
    run_url = f"{repo_url}/actions/runs/{env['GITHUB_RUN_ID']}" if env.get("GITHUB_RUN_ID") else ""
    epoch = int(env.get("SOURCE_DATE_EPOCH", time.time()))
    date = time.strftime("%Y-%m-%d %H:%M UTC", time.gmtime(epoch))

    n = []
    if a.kind == "rolling":
        n += [f"Rolling pre-release built from `main` at [`{sha[:12]}`]({repo_url}/commit/{sha}) "
              f"(commit date {date}). It is replaced by every new commit on `main`; for a fixed version use "
              f"a tagged release.", ""]
    else:
        n += [f"EmberMesh {a.version}, built from [`{sha[:12]}`]({repo_url}/commit/{sha}) (commit date {date}).", ""]
    if run_url:
        n += [f"Built by [this workflow run]({run_url}); every file below is also attached to that run as a workflow artifact.", ""]
    n += ["| File | Size | What it is |", "|---|---:|---|"]
    for f in files:
        n.append(f"| `{f}` | {human(os.path.getsize(os.path.join(a.dir, f)))} | {describe(f)} |")
    n += ["", "Verify a download: `sha256sum -c SHA256SUMS.txt --ignore-missing` (macOS: `shasum -a 256 -c ...`).", ""]
    signer = a.android_signer or ("release" if any(f.endswith("-android-release.apk") for f in files) else "ci-debug")
    cert = f" Signing certificate SHA-256: `{a.android_cert}`." if a.android_cert else ""
    if signer == "release":
        ANDROID_NOTE = ("- The Android release APK is signed with the project's release key, so it installs as an update over "
                        "earlier copies signed with that key." + cert + " Check it with `apksigner verify --print-certs`.")
    else:
        ANDROID_NOTE = ("- The Android release APK is signed with a debug key generated on the CI runner for this build only." + cert +
                        " It installs anywhere, but a later build is signed with a different key, so Android will not install it "
                        "over this one: uninstall first (this deletes the app's data; make an identity backup in the app).")
    n += ["### Before you install", "",
          "- Nothing here is code-signed. macOS may need `xattr -d com.apple.quarantine emberd`; Windows SmartScreen may warn.",
          ANDROID_NOTE,
          "- iOS is not distributed here (it needs Apple signing); build it from source, see `apps/ios/README.md`.",
          "- Radios, TNCs, LoRa boards, serial adapters and sound cards are unverified on every platform; read "
          "`guides/content/embermesh/troubleshooting.md` before relying on a link.", ""]
    if a.toolchains and os.path.isfile(a.toolchains):
        with open(a.toolchains, encoding="utf-8") as f:
            tc = [line.strip() for line in f if line.strip()]
        if tc:
            n += ["<details><summary>Toolchains</summary>", ""] + [f"- {t}" for t in tc] + ["", "</details>", ""]
    n += ["<details><summary>SHA-256</summary>", "", "```"] + [f"{h}  {f}" for h, f in sums] + ["```", "", "</details>"]
    text = "\n".join(n) + "\n"
    if a.out_notes:
        with open(a.out_notes, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
