#!/usr/bin/env python3
"""Markdown reports for CI job summaries ($GITHUB_STEP_SUMMARY). Standard library only.

  report.py tests-header
  report.py tests KIND NAME PATH...      one table row: suite, total, passed, failed, skipped
  report.py files PATH...                table of files: name, size, SHA-256 (directories: their files)
  report.py counts KIND PATH...          print "total passed failed skipped" (for scripts)
  report.py xcresult-failures PATH       Markdown list of the failed tests in an .xcresult bundle

KIND says how to read PATH (a log file, a report file or a directory of reports):
  doctest   doctest console output ("[doctest] test cases: ..."); several runs are added up
  ctest     CTest JUnit XML (--output-junit) or CTest console output ("NN% tests passed, ...")
  node      node --test output, spec ("ℹ tests 12") or TAP ("# tests 12") reporter
  junit     JUnit XML files, e.g. Gradle's build/test-results/** (directories are searched)
  xcresult  an Xcode .xcresult bundle (xcrun xcresulttool), or xcodebuild console output

A missing or unreadable input gives a row that says so; the exit status is 0 unless the
arguments are wrong, so a report never hides the real failure of the step before it.
"""
import glob
import hashlib
import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET


class Counts:
    def __init__(self, total=0, passed=0, failed=0, skipped=0):
        self.total, self.passed, self.failed, self.skipped = total, passed, failed, skipped
        self.found = False

    def add(self, total, passed, failed, skipped):
        self.total += total
        self.passed += passed
        self.failed += failed
        self.skipped += skipped
        self.found = True


def read_text(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8", "replace")


def parse_doctest(text, c):
    for m in re.finditer(r"\[doctest\] test cases:\s*(\d+)\s*\|\s*(\d+) passed\s*\|\s*(\d+) failed"
                         r"(?:\s*\|\s*(\d+) skipped)?", text):
        total, passed, failed = int(m.group(1)), int(m.group(2)), int(m.group(3))
        # doctest's "test cases" count excludes skipped ones; report them separately.
        c.add(total, passed, failed, int(m.group(4) or 0))


def parse_junit_xml(path, c):
    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError):
        return
    suites = [root] if root.tag == "testsuite" else root.findall(".//testsuite")
    if root.tag == "testsuites" and not suites:
        suites = [root]
    for s in suites:
        if s.find("testsuite") is not None:
            continue  # nested container: its children are counted
        total = int(float(s.get("tests", 0)))
        failed = int(float(s.get("failures", 0))) + int(float(s.get("errors", 0)))
        skipped = int(float(s.get("skipped", s.get("disabled", 0)) or 0))
        c.add(total, total - failed - skipped, failed, skipped)


def parse_ctest(text, c):
    # CTest < 4: "96% tests passed, 1 tests failed out of 23"; CTest 4: "100% tests passed out of 23"
    m = re.search(r"(\d+)% tests passed(?:,\s*(\d+) tests? failed)? out of (\d+)", text)
    if m:
        failed, total = int(m.group(2) or 0), int(m.group(3))
        skipped = len(re.findall(r"\*\*\*(?:Skipped|Not Run \(Disabled\))", text))
        c.add(total, total - failed - skipped, failed, skipped)


def parse_node(text, c):
    vals = {}
    for key in ("tests", "pass", "fail", "cancelled", "skipped", "todo"):
        found = re.findall(r"^(?:ℹ|#)\s+%s\s+(\d+)\s*$" % key, text, re.M)
        if found:
            vals[key] = sum(int(x) for x in found)
    if "tests" in vals:
        failed = vals.get("fail", 0) + vals.get("cancelled", 0)
        c.add(vals["tests"], vals.get("pass", 0), failed, vals.get("skipped", 0) + vals.get("todo", 0))


def parse_xcodebuild_log(text, c):
    # "Executed 85 tests, with 2 tests skipped and 0 failures (0 unexpected) in ..." (last = all tests)
    found = re.findall(r"Executed (\d+) tests?, with (?:(\d+) tests? skipped and )?(\d+) failures?", text)
    if found:
        total, skipped, failed = int(found[-1][0]), int(found[-1][1] or 0), int(found[-1][2])
        c.add(total, total - failed - skipped, failed, skipped)


def parse_xcresult_summary(data, c):
    passed = int(data.get("passedTests", 0))
    failed = int(data.get("failedTests", 0))
    skipped = int(data.get("skippedTests", 0)) + int(data.get("expectedFailures", 0))
    total = int(data.get("totalTestCount", 0)) or passed + failed + skipped
    c.add(total, passed, failed, skipped)


def xcresult_json(path):
    if path.endswith(".json"):
        return json.loads(read_text(path))
    out = subprocess.run(["xcrun", "xcresulttool", "get", "test-results", "summary", "--path", path, "--compact"],
                         capture_output=True, text=True, timeout=120)
    if out.returncode != 0:
        raise ValueError(out.stderr.strip() or "xcresulttool failed")
    return json.loads(out.stdout)


def collect(kind, paths):
    c = Counts()
    for p in paths:
        if kind == "xcresult" and ((p.endswith(".xcresult") and os.path.isdir(p)) or (p.endswith(".json") and os.path.isfile(p))):
            try:
                parse_xcresult_summary(xcresult_json(p), c)
            except (OSError, ValueError, subprocess.SubprocessError):
                pass
            continue
        files = []
        if os.path.isdir(p):
            files = sorted(glob.glob(os.path.join(p, "**", "*.xml"), recursive=True))
        elif os.path.isfile(p):
            files = [p]
        for f in files:
            if kind in ("junit",) or (kind == "ctest" and f.endswith(".xml")):
                parse_junit_xml(f, c)
                continue
            try:
                text = read_text(f)
            except OSError:
                continue
            {"doctest": parse_doctest, "ctest": parse_ctest, "node": parse_node,
             "xcresult": parse_xcodebuild_log}[kind](text, c)
    return c


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
    kinds = ("doctest", "ctest", "node", "junit", "xcresult")
    if len(argv) >= 1 and argv[0] == "tests-header":
        print("| Suite | Total | Passed | Failed | Skipped |\n|---|---:|---:|---:|---:|")
        return 0
    if len(argv) >= 3 and argv[0] == "tests" and argv[1] in kinds:
        c = collect(argv[1], argv[3:])
        name = argv[2].replace("|", "/")
        if not c.found:
            print(f"| {name} | – | – | – | – (no results found) |")
        else:
            mark = "" if c.failed == 0 else " ❌"
            print(f"| {name}{mark} | {c.total} | {c.passed} | {c.failed} | {c.skipped} |")
        return 0
    if len(argv) >= 2 and argv[0] == "counts" and argv[1] in kinds:
        c = collect(argv[1], argv[2:])
        print(f"{c.total} {c.passed} {c.failed} {c.skipped}" if c.found else "none")
        return 0
    if len(argv) == 2 and argv[0] == "xcresult-failures":
        try:
            data = xcresult_json(argv[1])
        except (OSError, ValueError, subprocess.SubprocessError) as e:
            print(f"_could not read {argv[1]}: {e}_")
            return 0
        for f in data.get("testFailures", []):
            text = " ".join(str(f.get("failureText", "")).split())
            print(f"- FAILED `{f.get('testIdentifierString', '?')}`: {text}")
        return 0
    if len(argv) >= 1 and argv[0] == "files":
        rows = []
        for p in argv[1:]:
            if os.path.isdir(p):
                for d, _, fs in os.walk(p):
                    rows += [os.path.join(d, f) for f in sorted(fs)]
            elif os.path.isfile(p):
                rows.append(p)
        if not rows:
            print("_no files_")
            return 0
        print("| File | Size | SHA-256 |\n|---|---:|---|")
        for f in sorted(rows):
            print(f"| `{f.replace(os.sep, '/')}` | {human(os.path.getsize(f))} | `{sha256(f)[:16]}…` |")
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")   # Windows consoles default to a legacy code page
    sys.exit(main(sys.argv[1:]))
