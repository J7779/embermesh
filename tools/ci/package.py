#!/usr/bin/env python3
"""Release packages, reproducible from the same inputs (sorted entries, fixed time stamps).

  package.py desktop --platform ID --version V --emberd FILE --web DIR --guides FILE --out DIR
      -> DIR/embermesh-V-emberd-ID.tar.gz   (.zip for windows-* platforms)
         embermesh-V-emberd-ID/{emberd[.exe], guides.pack, web/, deploy/, LICENSE, licenses/, README.txt, BUILDINFO.txt}
  package.py web --version V --web DIR --out DIR
      -> DIR/embermesh-V-web.zip            embermesh-V-web/<the static site + PWA>

Time stamps come from SOURCE_DATE_EPOCH (CI sets it to the commit time), else 2026-01-01.
BUILDINFO.txt records the version, commit and workflow run from the GITHUB_* environment.
"""
import argparse
import io
import os
import shutil
import sys
import tarfile
import time
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
EPOCH = int(os.environ.get("SOURCE_DATE_EPOCH", "1767225600"))

# Licence texts shipped with the desktop package: (path in the repository, name in licenses/).
DESKTOP_LICENCES = [
    ("core/third_party/monocypher/LICENCE.md", "Monocypher-LICENCE.md"),
    ("host/third_party/miniaudio/LICENSE", "miniaudio-LICENSE.txt"),
]

DESKTOP_README = """EmberMesh {version} - emberd for {platform}

emberd is one self-contained program: a mesh node, the hub that serves the web app on port 8437,
and a radio gateway. This package holds:

  {exe:<14} the daemon
  guides.pack    the offline guide library (found automatically next to the program)
  web/           the web app served to phones and laptops (found automatically next to the program)
  deploy/        service files: Linux systemd, macOS launchd, Windows Task Scheduler
  LICENSE, licenses/   licence texts

Start it:   {run}
Then open:  http://localhost:8437/   (it prints the operator URL and the hub URLs)
Options:    {exe} --help

The program is not code-signed. {platform_note}
Hardware links (radios, TNCs, serial, sound cards) are unverified on every platform; see
guides/content/embermesh/troubleshooting.md in the source repository.
"""

PLATFORM_NOTES = {
    "linux": "Linux builds are built and tested on CI runners only.",
    "macos": "macOS: if Gatekeeper blocks it, run `xattr -d com.apple.quarantine emberd` once.",
    "windows": "Windows: SmartScreen may warn on first start; the build is tested on CI runners only.",
}


def build_info(version, platform):
    env = os.environ
    lines = [f"version: {version}"]
    if platform:
        lines.append(f"platform: {platform}")
    for key, name in (("GITHUB_SHA", "commit"), ("GITHUB_REF", "ref")):
        if env.get(key):
            lines.append(f"{name}: {env[key]}")
    if env.get("GITHUB_SERVER_URL") and env.get("GITHUB_REPOSITORY") and env.get("GITHUB_RUN_ID"):
        lines.append(f"workflow run: {env['GITHUB_SERVER_URL']}/{env['GITHUB_REPOSITORY']}/actions/runs/{env['GITHUB_RUN_ID']}")
    lines.append("source date: " + time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(EPOCH)))
    return "\n".join(lines) + "\n"


class Archive:
    """Collects (archive name, bytes or file path, executable) and writes .zip or .tar.gz."""

    def __init__(self):
        self.entries = {}

    def add_file(self, name, path, executable=None):
        if executable is None:
            executable = os.access(path, os.X_OK) and not path.endswith((".md", ".txt", ".json", ".pack", ".html", ".js", ".mjs", ".css", ".svg", ".wasm", ".webmanifest"))
        self.entries[name] = (path, None, executable)

    def add_bytes(self, name, data, executable=False):
        self.entries[name] = (None, data, executable)

    def add_tree(self, prefix, directory):
        for d, dirs, files in os.walk(directory):
            dirs[:] = sorted(x for x in dirs if not x.startswith("."))
            for f in sorted(files):
                if f.startswith(".") or f in ("Thumbs.db", "desktop.ini"):
                    continue
                full = os.path.join(d, f)
                rel = os.path.relpath(full, directory).replace(os.sep, "/")
                self.add_file(f"{prefix}/{rel}", full)

    def _data(self, name):
        path, data, _ = self.entries[name]
        if data is not None:
            return data
        with open(path, "rb") as fh:
            return fh.read()

    def write(self, out):
        names = sorted(self.entries)
        if out.endswith(".zip"):
            with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
                for n in names:
                    info = zipfile.ZipInfo(n, time.gmtime(max(EPOCH, 315532800))[:6])
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = ((0o755 if self.entries[n][2] else 0o644) | 0o100000) << 16
                    z.writestr(info, self._data(n))
        else:
            with open(out, "wb") as raw:
                import gzip
                with gzip.GzipFile(fileobj=raw, mode="wb", compresslevel=9, mtime=EPOCH, filename="") as gz:
                    with tarfile.open(fileobj=gz, mode="w", format=tarfile.PAX_FORMAT) as tar:
                        dirs = set()
                        for n in names:
                            parts = n.split("/")[:-1]
                            for i in range(1, len(parts) + 1):
                                d = "/".join(parts[:i])
                                if d not in dirs:
                                    dirs.add(d)
                                    ti = tarfile.TarInfo(d)
                                    ti.type, ti.mode, ti.mtime = tarfile.DIRTYPE, 0o755, EPOCH
                                    tar.addfile(ti)
                            data = self._data(n)
                            ti = tarfile.TarInfo(n)
                            ti.size, ti.mtime = len(data), EPOCH
                            ti.mode = 0o755 if self.entries[n][2] else 0o644
                            ti.uid = ti.gid = 0
                            ti.uname = ti.gname = ""
                            tar.addfile(ti, io.BytesIO(data))
        return out


def need(path, what):
    if not path or not os.path.exists(path):
        sys.exit(f"package.py: {what} not found: {path}")
    return path


def cmd_desktop(a):
    windows = a.platform.startswith("windows")
    exe = "emberd.exe" if windows else "emberd"
    top = f"embermesh-{a.version}-emberd-{a.platform}"
    ar = Archive()
    ar.add_file(f"{top}/{exe}", need(a.emberd, "emberd"), executable=True)
    ar.add_file(f"{top}/guides.pack", need(a.guides, "guides.pack"), executable=False)
    web = need(a.web, "web app directory")
    need(os.path.join(web, "index.html"), "web app index.html")
    need(os.path.join(web, "wasm", "ember.wasm"), "web app wasm/ember.wasm")
    ar.add_tree(f"{top}/web", web)
    ar.add_tree(f"{top}/deploy", need(os.path.join(ROOT, "apps", "desktop", "deploy"), "deploy scripts"))
    licence = os.path.join(ROOT, "LICENSE")
    if os.path.isfile(licence):
        ar.add_file(f"{top}/LICENSE", licence, executable=False)
    else:
        print("package.py: warning: no LICENSE at the repository root; the package has none", file=sys.stderr)
    third = os.path.join(ROOT, "THIRD_PARTY_LICENSES.md")
    if os.path.isfile(third):
        ar.add_file(f"{top}/THIRD_PARTY_LICENSES.md", third, executable=False)
    for src, name in DESKTOP_LICENCES:
        ar.add_file(f"{top}/licenses/{name}", need(os.path.join(ROOT, src), "licence file"), executable=False)
    family = a.platform.split("-")[0]
    readme = DESKTOP_README.format(version=a.version, platform=a.platform, exe=exe,
                                   run=("emberd.exe" if windows else "./emberd"),
                                   platform_note=PLATFORM_NOTES.get(family, ""))
    ar.add_bytes(f"{top}/README.txt", readme.replace("\n", "\r\n" if windows else "\n").encode())
    ar.add_bytes(f"{top}/BUILDINFO.txt", build_info(a.version, a.platform).encode())
    os.makedirs(a.out, exist_ok=True)
    print(ar.write(os.path.join(a.out, top + (".zip" if windows else ".tar.gz"))))


def cmd_web(a):
    top = f"embermesh-{a.version}-web"
    web = need(a.web, "web app directory")
    need(os.path.join(web, "index.html"), "web app index.html")
    ar = Archive()
    ar.add_tree(top, web)
    licence = os.path.join(ROOT, "LICENSE")
    if os.path.isfile(licence):
        ar.add_file(f"{top}/LICENSE", licence, executable=False)
    ar.add_bytes(f"{top}/BUILDINFO.txt", build_info(a.version, "web").encode())
    os.makedirs(a.out, exist_ok=True)
    print(ar.write(os.path.join(a.out, top + ".zip")))


def main(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("desktop")
    for k in ("platform", "version", "emberd", "web", "guides", "out"):
        d.add_argument("--" + k, required=True)
    w = sub.add_parser("web")
    for k in ("version", "web", "out"):
        w.add_argument("--" + k, required=True)
    a = p.parse_args(argv)
    {"desktop": cmd_desktop, "web": cmd_web}[a.cmd](a)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
