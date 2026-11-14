#!/usr/bin/env python3
"""Cross-platform end-to-end test: the Android app (emulator/phone) <-> emberd (this computer).

Starts dist/desktop/emberd on spare ports with a temporary data dir, then for each transport runs
EmberdInteropTest on the device (am instrument) while injecting emberd's side through its POST /rpc:
  * Public channel both ways, DM both ways, delivery receipt for the app's DM;
  * transports: websocket (emberd hub /ws), tcp (emberd tcp_server link), optionally udp multicast.
The emulator reaches the host at 10.0.2.2 (a real phone: pass --host-ip <this computer's LAN IP>).

Usage:  apps/android/scripts/e2e_emberd.py [--serial emulator-5554] [--transports ws,tcp,udp]
        [--attach-port N] [--nonce TEXT]
  --attach-port N  use an emberd that is already running on 127.0.0.1:N (started with --local-no-token)
                   instead of starting one; it is left running and its profile is not changed.
                   tcp then needs that emberd to have a tcp_server link on --tcp-port.
  --nonce TEXT     use TEXT as the run's nonce (single transport) instead of "<transport>-<time>".
  --expect-public TEXT  the device test also waits (20 s) for this exact text from another node in
                   the app's Public channel and fails without it; reported as "expect_public".
Prereq: the debug APK and the test APK are installed (./gradlew installDebug installDebugAndroidTest).
"""
import argparse, base64, json, os, re, shlex, shutil, signal, subprocess, sys, tempfile, threading, time, urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SDK = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT") or os.path.expanduser("~/Library/Android/sdk")
ADB = os.path.join(SDK, "platform-tools", "adb")
if not os.path.exists(ADB):
    ADB = shutil.which("adb") or "adb"


def rpc(port, method, params=None):
    req = urllib.request.Request(f"http://127.0.0.1:{port}/rpc", data=json.dumps({"method": method, "params": params or {}}).encode(),
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        env = json.loads(r.read())
    if not env.get("ok"):
        raise RuntimeError(f"{method}: {env}")
    return env["result"]


def msgs(port, conv):
    return rpc(port, "messages.list", {"conv": conv, "limit": 200})["messages"]


def run_transport(a, port, url, nonce, log):
    card = rpc(port, "identity.card")["card"]
    cmd = [ADB, "-s", a.serial, "shell", "am", "instrument", "-w", "-r",
           "-e", "class", "org.embermesh.app.EmberdInteropTest",
           "-e", "emberd_url", url, "-e", "nonce", nonce,
           "-e", "emberd_card", base64.b64encode(card.encode()).decode()]
    if getattr(a, "expect_public", None):
        cmd += ["-e", "expect_public", shlex.quote(a.expect_public)]  # adb shell re-splits on spaces
    cmd += ["org.embermesh.app.test/androidx.test.runner.AndroidJUnitRunner"]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    out = []
    threading.Thread(target=lambda: out.extend(proc.stdout), daemon=True).start()
    app_id, replied, t0 = None, False, time.time()
    while proc.poll() is None and time.time() - t0 < 240:
        try:
            rpc(port, "messages.send", {"conv": "ch:public", "text": f"emberd->android public {nonce}"})
            for c in rpc(port, "conversations.list")["conversations"]:
                if c["id"].startswith("dm:"):
                    for m in msgs(port, c["id"]):
                        if m["text"] == f"android->emberd dm {nonce}":
                            app_id = c["id"][3:]
            if app_id and not replied:
                rpc(port, "messages.send", {"conv": f"dm:{app_id}", "text": f"emberd->android dm {nonce}"})
                replied = True
        except Exception as e:  # emberd busy / not yet
            log(f"  (rpc: {e})")
        time.sleep(3)
    proc.wait(timeout=30)
    time.sleep(1)
    text = "".join(out)
    passed = "OK (1 test)" in text
    pub = [m for m in msgs(port, "ch:public") if m["text"] == f"android->emberd public {nonce}"]
    dm = [m for m in msgs(port, f"dm:{app_id}") if m["text"] == f"android->emberd dm {nonce}"] if app_id else []
    reply = [m for m in msgs(port, f"dm:{app_id}") if m["text"] == f"emberd->android dm {nonce}"] if app_id else []
    res = {
        "device_test": "pass" if passed else "FAIL",
        "emberd_got_public": bool(pub) and not pub[0]["outgoing"],
        "emberd_got_dm": bool(dm),
        "emberd_dm_to_app_status": reply[0]["status"] if reply else None,
        "app_node_id": app_id,
        "via_link_on_emberd": pub[0].get("via_link") if pub else None,
    }
    if getattr(a, "expect_public", None):
        seen = re.search(r"interop expect-public: [^\n]*", text)
        res["expect_public"] = seen.group(0).strip() if seen else None
    if not passed:
        res["instrument_output"] = text[-1500:]
    return res


def run_all(a, port, log):
    urls = {"ws": f"ws://{a.host_ip}:{port}/ws", "tcp": f"tcp://{a.host_ip}:{a.tcp_port}", "udp": "udp://multicast"}
    results = {}
    for t in a.transports.split(","):
        nonce = a.nonce if a.nonce and "," not in a.transports else f"{t}-{int(time.time())}"
        log(f"== {t}: {urls[t]} nonce {nonce}")
        results[t] = run_transport(a, port, urls[t], nonce, log)
        log(json.dumps(results[t], indent=1))
    return results


def summarize(results):
    ok = bool(results) and all(r["device_test"] == "pass" and r["emberd_got_public"] and r["emberd_got_dm"] for r in results.values())
    print("RESULT", "PASS" if ok else "FAIL", json.dumps({k: v["device_test"] for k, v in results.items()}), flush=True)
    return ok


def attach(a):
    """--attach-port: run against an emberd somebody else started and owns (left as it is)."""
    log = lambda s: print(s, flush=True)
    try:
        info = rpc(a.attach_port, "node.info")
    except Exception as e:
        print(f"RESULT FAIL no emberd /rpc on 127.0.0.1:{a.attach_port} without a token ({e})", flush=True)
        return 1
    log(f"attached to emberd {info['node_id']} on :{a.attach_port}")
    return 0 if summarize(run_all(a, a.attach_port, log)) else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial", default="emulator-5554")
    ap.add_argument("--host-ip", default="10.0.2.2")
    ap.add_argument("--port", type=int, default=18437)
    ap.add_argument("--tcp-port", type=int, default=18440)
    ap.add_argument("--transports", default="ws,tcp")
    ap.add_argument("--emberd", default=os.path.join(ROOT, "dist/desktop/emberd"))
    ap.add_argument("--attach-port", type=int, default=None)
    ap.add_argument("--nonce", default=None)
    ap.add_argument("--expect-public", default=None)
    a = ap.parse_args()
    if a.attach_port:
        sys.exit(attach(a))
    data = tempfile.mkdtemp(prefix="emberd-e2e-")
    logf = open(os.path.join(data, "emberd.log"), "w")
    link = json.dumps({"name": "e2e tcp", "endpoint": {"kind": "tcp_server", "config": {"port": a.tcp_port}},
                       "link": {"type": "stream", "config": {"preset": "tcp"}}})
    args = [a.emberd, "--data-dir", data, "--port", str(a.port), "--name", "DesktopEmberd", "--local-no-token", "--link", link, "--log-level", "info"]
    if "udp" not in a.transports.split(","):
        args.append("--no-udp")
    p = subprocess.Popen(args, stdout=logf, stderr=subprocess.STDOUT)
    log = lambda s: print(s, flush=True)
    results = {}
    try:
        for _ in range(50):
            try:
                rpc(a.port, "node.info"); break
            except Exception:
                time.sleep(0.2)
        rpc(a.port, "node.set_profile", {"name": "DesktopEmberd"})
        log(f"emberd {rpc(a.port, 'node.info')['node_id']} on :{a.port} (tcp_server :{a.tcp_port}), data {data}")
        results = run_all(a, a.port, log)
    finally:
        p.send_signal(signal.SIGTERM)
        try:
            p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            p.kill()
        logf.close()
    ok = all(r["device_test"] == "pass" and r["emberd_got_public"] and r["emberd_got_dm"] for r in results.values())
    print("RESULT", "PASS" if ok else "FAIL", json.dumps({k: v["device_test"] for k, v in results.items()}))
    shutil.rmtree(data, ignore_errors=True) if ok else print("emberd data/log kept in", data)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
