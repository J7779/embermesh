"""Unit tests of the CI helper scripts in tools/ci/ against outputs captured from real runs
(samples/; identifiers removed). Run: python3 -m unittest discover -s tools/ci/tests -v"""
import json
import os
import subprocess
import sys
import tarfile
import tempfile
import unittest
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
CI = os.path.dirname(HERE)
SAMPLES = os.path.join(HERE, "samples")
sys.path.insert(0, CI)

import report  # noqa: E402


def counts(kind, *names):
    c = report.collect(kind, [os.path.join(SAMPLES, n) for n in names])
    return (c.total, c.passed, c.failed, c.skipped) if c.found else None


def slurp(path, mode="r"):
    with open(path, mode) as f:
        return f.read()


def run(script, *args, env=None, cwd=None):
    e = dict(os.environ)
    e.update(env or {})
    return subprocess.run([sys.executable, os.path.join(CI, script), *args], capture_output=True, text=True, env=e, cwd=cwd)


class ReportParsers(unittest.TestCase):
    def test_doctest(self):
        self.assertEqual(counts("doctest", "doctest.txt"), (557, 557, 0, 28))
        self.assertEqual(counts("doctest", "doctest-failed.txt"), (412, 410, 2, 3))
        self.assertEqual(counts("doctest", "doctest.txt", "doctest-failed.txt"), (969, 967, 2, 31))

    def test_ctest_console_both_formats(self):
        self.assertEqual(counts("ctest", "ctest4.txt"), (23, 23, 0, 0))   # CTest 4: no "failed" clause
        self.assertEqual(counts("ctest", "ctest3.txt"), (23, 22, 1, 0))

    def test_ctest_junit(self):
        self.assertEqual(counts("ctest", "ctest.xml"), (3, 1, 1, 1))

    def test_node_spec_and_tap(self):
        self.assertEqual(counts("node", "node-spec.txt"), (88, 88, 0, 0))
        self.assertEqual(counts("node", "node-tap.txt"), (3, 1, 1, 1))

    def test_gradle_junit_directory(self):
        self.assertEqual(counts("junit", "gradle"), (10, 7, 2, 1))

    def test_xcresult_json_and_log(self):
        self.assertEqual(counts("xcresult", "xcresult-summary.json"), (87, 81, 1, 5))
        self.assertEqual(counts("xcresult", "xcodebuild.txt"), (87, 81, 1, 5))

    def test_missing_input_is_reported_not_fatal(self):
        r = run("report.py", "tests", "doctest", "suite", os.path.join(SAMPLES, "nope.log"))
        self.assertEqual(r.returncode, 0)
        self.assertIn("no results found", r.stdout)

    def test_row_marks_failures(self):
        r = run("report.py", "tests", "ctest", "native", os.path.join(SAMPLES, "ctest3.txt"))
        self.assertIn("| native ❌ | 23 | 22 | 1 | 0 |", r.stdout)

    def test_xcresult_failures(self):
        r = run("report.py", "xcresult-failures", os.path.join(SAMPLES, "xcresult-summary.json"))
        self.assertIn("CaptureTests/testMicPcmDecodesWithRealCore()", r.stdout)

    def test_files_table(self):
        r = run("report.py", "files", os.path.join(SAMPLES, "gradle"))
        self.assertIn("TEST-a.xml", r.stdout)
        self.assertIn("| File | Size | SHA-256 |", r.stdout)


class NeedsSummary(unittest.TestCase):
    def needs(self, **results):
        return {"NEEDS": json.dumps({k: {"result": v, "outputs": {}} for k, v in results.items()}), "GITHUB_STEP_SUMMARY": ""}

    def test_all_success(self):
        self.assertEqual(run("needs-summary.py", "X", env=self.needs(a="success", b="success")).returncode, 0)

    def test_required_failure_fails(self):
        r = run("needs-summary.py", "X", env=self.needs(a="success", b="failure"))
        self.assertEqual(r.returncode, 1)
        self.assertIn("b (failure)", r.stdout)

    def test_skipped_or_cancelled_required_fails(self):
        self.assertEqual(run("needs-summary.py", "X", env=self.needs(a="skipped")).returncode, 1)
        self.assertEqual(run("needs-summary.py", "X", env=self.needs(a="cancelled")).returncode, 1)

    def test_optional_failure_passes(self):
        r = run("needs-summary.py", "X", "--optional", "emulator", env=self.needs(build="success", emulator="failure"))
        self.assertEqual(r.returncode, 0)

    def test_empty_needs_fails(self):
        self.assertEqual(run("needs-summary.py", "X", env={"NEEDS": "{}", "GITHUB_STEP_SUMMARY": ""}).returncode, 1)


class RelativeUrls(unittest.TestCase):
    def site(self, files):
        d = tempfile.mkdtemp()
        for name, text in files.items():
            p = os.path.join(d, name)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "w", encoding="utf-8") as f:
                f.write(text)
        return d

    def test_relative_site_passes(self):
        d = self.site({"index.html": '<link rel="manifest" href="manifest.webmanifest"><script src="js/app.js"></script>',
                       "manifest.webmanifest": json.dumps({"start_url": "./", "scope": "./", "icons": [{"src": "icon.svg"}]}),
                       "js/app.js": "new URL('../wasm/ember.mjs', import.meta.url); new WebSocket(base + '/ws');",
                       "sw.js": "const FILES = ['./', 'index.html', 'wasm/ember.wasm'];", "wasm/ember.mjs": ""})
        self.assertEqual(run("check-relative-urls.py", d).returncode, 0)

    def test_root_absolute_urls_fail(self):
        d = self.site({"index.html": '<script src="/js/app.js"></script>',
                       "manifest.webmanifest": json.dumps({"start_url": "/", "scope": "/", "icons": [{"src": "/icon.svg"}]}),
                       "sw.js": "self.addEventListener('install', () => caches.open('x').then(c => c.addAll(['/index.html', '/wasm/ember.wasm'])));",
                       "js/app.js": "", "wasm/ember.wasm": "", "icon.svg": ""})
        r = run("check-relative-urls.py", d)
        self.assertEqual(r.returncode, 1)
        for want in ('src="/js/app.js"', "manifest start_url", "manifest scope", "manifest icons[].src", "/index.html", "/wasm"):
            self.assertIn(want, r.stdout)


class PagesLimits(unittest.TestCase):
    def test_small_site_passes_with_margins(self):
        d = tempfile.mkdtemp()
        with open(os.path.join(d, "index.html"), "w") as f:
            f.write("x" * 1000)
        r = run("pages-limits.py", d)
        self.assertEqual(r.returncode, 0)
        self.assertIn("| Site (1 files)", r.stdout)


class Packaging(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.web = os.path.join(self.tmp, "web")
        os.makedirs(os.path.join(self.web, "wasm"))
        for n in ("index.html", "wasm/ember.wasm"):
            with open(os.path.join(self.web, n), "w") as f:
                f.write("x")
        self.bin = os.path.join(self.tmp, "emberd")
        with open(self.bin, "w") as f:
            f.write("#!/bin/sh\n")
        os.chmod(self.bin, 0o755)
        self.pack = os.path.join(self.tmp, "guides.pack")
        with open(self.pack, "wb") as f:
            f.write(b"EMGUIDE1")

    def package(self, platform):
        out = os.path.join(self.tmp, "out-" + platform)
        r = run("package.py", "desktop", "--platform", platform, "--version", "9.9.9", "--emberd", self.bin,
                "--web", self.web, "--guides", self.pack, "--out", out, env={"SOURCE_DATE_EPOCH": "1767225600"})
        self.assertEqual(r.returncode, 0, r.stderr)
        return r.stdout.strip()

    def test_tarball_layout_modes_and_reproducibility(self):
        a = self.package("linux-x86_64")
        first = slurp(a, "rb")
        self.assertEqual(slurp(self.package("linux-x86_64"), "rb"), first)   # byte-identical rebuild
        with tarfile.open(a) as t:
            members = {m.name: m for m in t.getmembers()}
        top = "embermesh-9.9.9-emberd-linux-x86_64/"
        for n in ("emberd", "guides.pack", "web/index.html", "web/wasm/ember.wasm", "README.txt", "BUILDINFO.txt",
                  "licenses/Monocypher-LICENCE.md", "licenses/miniaudio-LICENSE.txt", "deploy/linux/emberd.service"):
            self.assertIn(top + n, members)
        self.assertEqual(members[top + "emberd"].mode, 0o755)
        self.assertEqual(members[top + "guides.pack"].mode, 0o644)

    def test_windows_zip(self):
        a = self.package("windows-x86_64")
        self.assertTrue(a.endswith(".zip"))
        with zipfile.ZipFile(a) as z:
            names = z.namelist()
        self.assertIn("embermesh-9.9.9-emberd-windows-x86_64/emberd.exe", names)


class ReleaseNotes(unittest.TestCase):
    def test_checksums_and_notes(self):
        d = tempfile.mkdtemp()
        for n in ("embermesh-1.0.0-emberd-linux-x86_64.tar.gz", "guides.pack"):
            with open(os.path.join(d, n), "wb") as f:
                f.write(n.encode())
        notes = os.path.join(d, "..", os.path.basename(d) + "-notes.md")
        r = run("release-notes.py", "--dir", d, "--version", "1.0.0", "--kind", "tag", "--out-notes", notes,
                env={"GITHUB_SHA": "a" * 40, "GITHUB_REPOSITORY": "o/r", "SOURCE_DATE_EPOCH": "1767225600"})
        self.assertEqual(r.returncode, 0, r.stderr)
        sums = slurp(os.path.join(d, "SHA256SUMS.txt")).splitlines()
        self.assertEqual(len(sums), 2)
        self.assertTrue(all(len(line.split("  ")[0]) == 64 for line in sums))
        text = slurp(notes)
        self.assertIn("EmberMesh 1.0.0", text)
        self.assertIn("emberd for Linux x86-64", text)


class AndroidSigning(unittest.TestCase):
    SCRIPT = os.path.join(CI, "android-signing.sh")

    def sh(self, *args, env=None):
        tmp = tempfile.mkdtemp()
        out = os.path.join(tmp, "out")
        e = {k: v for k, v in os.environ.items() if not k.startswith("ANDROID_KEY")}
        e.update({"RUNNER_TEMP": tmp, "GITHUB_OUTPUT": out})
        e.update(env or {})
        r = subprocess.run(["bash", self.SCRIPT, *args], capture_output=True, text=True, env=e)
        outputs = dict(line.split("=", 1) for line in slurp(out).splitlines()) if os.path.exists(out) else {}
        return r, outputs, tmp

    def test_no_secrets_means_throwaway_key(self):
        r, o, _ = self.sh("prepare")
        self.assertEqual(r.returncode, 0)
        self.assertEqual(o.get("present"), "false")

    def test_partial_secrets_fail(self):
        r, _, _ = self.sh("prepare", env={"ANDROID_KEYSTORE_BASE64": "eA==", "ANDROID_KEY_ALIAS": "a"})
        self.assertEqual(r.returncode, 1)
        self.assertIn("only 2 of the 4", r.stdout)

    def test_all_secrets_decode_into_runner_temp_and_cleanup(self):
        secret = "pw-" + "x" * 10
        env = {"ANDROID_KEYSTORE_BASE64": "a2V5c3RvcmUtYnl0ZXM=\n", "ANDROID_KEYSTORE_PASSWORD": secret,
               "ANDROID_KEY_ALIAS": "embermesh", "ANDROID_KEY_PASSWORD": secret}
        r, o, tmp = self.sh("prepare", env=env)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(o.get("present"), "true")
        ks = o["keystore"]
        self.assertTrue(ks.startswith(tmp))
        self.assertEqual(slurp(ks, "rb"), b"keystore-bytes")
        self.assertEqual(os.stat(ks).st_mode & 0o077, 0)
        self.assertNotIn(secret, r.stdout + r.stderr)
        r2 = subprocess.run(["bash", self.SCRIPT, "cleanup"], capture_output=True, text=True, env={**os.environ, "RUNNER_TEMP": tmp})
        self.assertEqual(r2.returncode, 0)
        self.assertFalse(os.path.exists(ks))


class ReleaseNotesSigner(unittest.TestCase):
    def notes(self, files, *extra):
        d = tempfile.mkdtemp()
        for n in files:
            with open(os.path.join(d, n), "wb") as f:
                f.write(b"x")
        out = d + "-notes.md"
        r = run("release-notes.py", "--dir", d, "--version", "1.0.0", "--kind", "rolling", "--out-notes", out, *extra)
        self.assertEqual(r.returncode, 0, r.stderr)
        return slurp(out)

    def test_release_key(self):
        t = self.notes(["embermesh-1.0.0-android-release.apk"], "--android-signer", "release", "--android-cert", "ab" * 32)
        self.assertIn("signed with the project's release key", t)
        self.assertIn("ab" * 32, t)

    def test_ci_debug_key(self):
        t = self.notes(["embermesh-1.0.0-android-release-ci-debug-signed.apk"])
        self.assertIn("throw-away debug key", t)
        self.assertNotIn("project's release key", t)

    def test_site_address_opens_the_notes(self):
        t = self.notes(["guides.pack"], "--site", "https://owner.example/repo/")
        self.assertTrue(t.startswith("**Web app, nothing to install:** <https://owner.example/repo/>"), t[:120])

    def test_no_site_line_without_a_usable_address(self):
        for site in ("", "null", "http://owner.example/repo/", "https://owner.example/) [x](https://evil.example"):
            t = self.notes(["guides.pack"], "--site", site)
            self.assertNotIn("Web app, nothing to install", t, site)
            self.assertNotIn("evil.example", t, site)


if __name__ == "__main__":
    unittest.main()
