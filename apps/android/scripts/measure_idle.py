#!/usr/bin/env python3
"""Idle cost of the running app: CPU time and wakeups (voluntary context switches) per thread over N
seconds, read from /proc of the app process. Usage: measure_idle.py [--seconds 60] [--serial emulator-5554]"""
import argparse, os, shutil, subprocess, time

SDK = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or os.path.expanduser("~/Library/Android/sdk")
ADB = os.path.join(SDK, "platform-tools", "adb")
if not os.path.exists(ADB):
    ADB = shutil.which("adb") or "adb"


def sh(a, cmd):
    return subprocess.run([ADB, "-s", a.serial, "shell", cmd], capture_output=True, text=True).stdout


def snapshot(a, pid):
    out = sh(a, f"for t in /proc/{pid}/task/*; do echo \"$(cat $t/comm)|$(cut -d' ' -f14,15 $t/stat)|$(grep '^voluntary_ctxt' $t/status)\"; done")
    r = {}
    for line in out.splitlines():
        try:
            name, cpu, vol = line.split("|")
            u, s = cpu.split()
            r[name] = r.get(name, (0, 0))
            r[name] = (r[name][0] + int(u) + int(s), r[name][1] + int(vol.split()[-1]))
        except ValueError:
            pass
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial", default="emulator-5554")
    ap.add_argument("--seconds", type=int, default=60)
    a = ap.parse_args()
    pid = sh(a, "pidof org.embermesh.app").strip()
    if not pid:
        raise SystemExit("app not running")
    s0 = snapshot(a, pid)
    time.sleep(a.seconds)
    s1 = snapshot(a, pid)
    tot_cpu = tot_wake = 0
    rows = []
    for k, (c1, w1) in s1.items():
        c0, w0 = s0.get(k, (0, 0))
        dc, dw = c1 - c0, w1 - w0
        tot_cpu += dc; tot_wake += dw
        if dc or dw:
            rows.append((dw, dc, k))
    print(f"{a.seconds}s: CPU {tot_cpu * 10} ms total ({tot_cpu * 10 / a.seconds / 10:.3f}% of one core), {tot_wake} wakeups ({tot_wake / a.seconds:.1f}/s)")
    for dw, dc, k in sorted(rows, reverse=True)[:12]:
        print(f"  {k:<24} wakeups {dw:>5}  cpu {dc * 10:>5} ms")


if __name__ == "__main__":
    main()
