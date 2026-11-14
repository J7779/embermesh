# emberd — EmberMesh desktop node, hub and radio gateway

`emberd` is a single self-contained executable for Linux (including Raspberry Pi), macOS and
Windows. It contains:

- **a full mesh node**: the shared C++ core, with its own identity, storage, store-and-forward and relaying;
- **a hub** on port `8437`: it serves the web app to any phone or laptop on the same network (typically
  its own Wi-Fi hotspot) and connects all of them to the mesh over a WebSocket;
- **a radio gateway**: KISS TNCs (serial / TCP / Direwolf), Meshtastic, sound-card soft modems with
  PTT, and rtl_tcp SDR receive. Everyone on the hotspot can reach distant nodes through the radio.

The mesh engine bridges between every link automatically, so you don't have to configure any routing.

> Status: tested against the real core, including multi-process mesh tests over every
> IP/serial/audio-pipe medium (see *Tests* and *What is proven*). The **Desktop** workflow builds
> emberd for Linux, macOS and Windows on every commit to `main`; its run summary shows which test
> suites ran on which system (see the badge in the top-level README for the current result). It has not been run on a Raspberry Pi. Nothing has
> been tested against real radios, TNCs or SDR dongles.

## Build

From the repository root:

```sh
./build.sh guides desktop web     # emberd -> dist/desktop/ (+ guides.pack), web app -> dist/web/
./build.sh test                   # every suite, including the emberd mesh tests
# or by hand:
cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop -j
./build/desktop/apps/desktop/emberd --help
ctest --test-dir build/desktop -R "ember_host|emberd"   # host + daemon tests (-L mesh: multi-process only)
```

Hub guests need `dist/web` (the UI **plus** the WebAssembly core, built by `./build.sh web`). The plain
`ui/` folder has no `wasm/` and guests would see "EmberMesh could not start"; emberd warns at start
when the web root it uses has no `wasm/ember.wasm`.

| CMake option | Default | Meaning |
|---|---|---|
| `EMBER_HOST_AUDIO` | ON (desktop) | sound-card endpoint (vendored miniaudio, no system deps) and the `audio_pipe` virtual sound card |
| `EMBER_HOST_SERIAL` | ON (desktop) | serial-port endpoint + RTS/DTR PTT |

With both OFF (Android/iOS embedding) no miniaudio, termios/IOKit or serial code is compiled and
`serial_test.cpp` is skipped.

There are no dependencies beyond the OS and a C++17 compiler (CMake ≥ 3.20). No system libraries
are needed at build time: sound (miniaudio) loads ALSA/PulseAudio/JACK at run time with `dlopen`.
`CMakePresets.json` has ready configurations for each platform.

### Linux (and Raspberry Pi)

```sh
sudo apt install cmake ninja-build g++            # Debian / Ubuntu / Pi OS; clang works too
cmake --preset linux-gcc                          # or: linux-clang, linux-asan (ASan + UBSan, Debug)
cmake --build --preset linux-gcc
ctest --preset linux-gcc                          # core, host, emberd and multi-process mesh tests
# without presets:
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build/linux
```

The mesh tests use ptys (`libutil`), FIFOs and loopback multicast, so they run in an ordinary container
or CI VM without extra privileges. They read daemon CPU/RSS with `ps` (procps); without it those
figures show as 0. Raspberry Pi: add the user that runs emberd to `dialout` (serial ports) and `audio`
(sound cards).

### Windows

Visual Studio 2022 or the Build Tools ("Desktop development with C++"), from any shell. The Visual
Studio generator finds the compiler itself, so no Developer Prompt is needed:

```powershell
cmake --preset windows-msvc                       # x64; windows-msvc-arm64 for ARM64
cmake --build --preset windows-msvc               # Release
ctest --preset windows-msvc                       # core, host and emberd tests (no mesh tests)
# without presets (build.ps1 does the same):
cmake -S . -B build/msvc -G "Visual Studio 17 2022" -A x64
cmake --build build/msvc --config Release
ctest --test-dir build/msvc -C Release --output-on-failure
.\build\msvc\apps\desktop\Release\emberd.exe --help
```

The Visual Studio generator is multi-config: pass `--config` / `-C`, not `CMAKE_BUILD_TYPE`. The
multi-process mesh tests are POSIX-only and are not built on Windows. MinGW-w64 is optional (MSYS2
UCRT64: `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja`,
then `cmake --preset windows-mingw`).

### Runtime dependencies (self-contained `emberd`)

`EMBER_STATIC_RUNTIME` (default ON on Windows and Linux) links the C++ runtime into the executables:

| Platform | `emberd` links against |
|---|---|
| Linux (glibc) | `libc`, `libm` and the dynamic loader only. libstdc++ and libgcc are static. glibc stays dynamic (NSS / `getaddrinfo`), so build on the oldest distribution you want to support. ALSA/PulseAudio/JACK are `dlopen`ed when a sound card is used and are optional. |
| Linux (musl, e.g. Alpine) | add `-DCMAKE_EXE_LINKER_FLAGS=-static` for a fully static binary with no dependencies |
| Windows (MSVC) | static CRT (`/MT`). Only DLLs that ship with Windows 10/11: `kernel32`, `ws2_32`, `iphlpapi`, `bcrypt`, `advapi32`, `shell32`, `crypt32` (the system certificate store, for `wss://`). No Visual C++ redistributable. miniaudio loads WASAPI/DirectSound/WinMM at run time. |
| Windows (MinGW) | `-static`: no `libstdc++-6.dll`, `libgcc_s_seh-1.dll` or `libwinpthread-1.dll`. The UCRT (part of Windows 10/11) is used. |
| macOS | system `libc++` and frameworks (CoreFoundation, CoreAudio, AudioToolbox, IOKit, Security) |

Pass `-DEMBER_STATIC_RUNTIME=OFF` to use the shared runtimes, for example for distribution packages.
Sanitizer builds always use the shared runtime.

### Windows packaging

A portable folder (zip) is enough. emberd needs no installer and writes only to `%APPDATA%\EmberMesh`:

```
EmberMesh\
  emberd.exe          build\msvc\apps\desktop\Release\emberd.exe
  guides.pack         build\msvc\guides\guides.pack
  web\                dist/web (index.html + wasm\ember.wasm) from a `./build.sh web` build
  deploy\windows\     install-task.ps1, emberd-start.bat (autostart), firewall notes
```

emberd finds `<exe>\guides.pack` and `<exe>\web` by itself. The executable is unsigned, so
SmartScreen warns on first start. Windows Firewall asks once when the hub starts listening (see
[`deploy/windows/README.md`](deploy/windows/README.md)).

## Quick start

```sh
emberd                      # node + hub on 0.0.0.0:8437 + LAN multicast link
```

On startup emberd prints the hub URLs for every local interface, for example `http://192.168.4.1:8437/`,
a terminal QR code of the first URL (from the core's `tools.qr`; `--no-qr` hides it) and the operator
URL with the admin token:

```
http://localhost:8437/?remote=1&token=<admin token>
```

- **Guests** (phones and laptops on the same Wi-Fi) open `http://<hub-ip>:8437/`. Their browser runs
  its own WASM node, with its own identity and end-to-end crypto, and joins the mesh through `/ws`.
- **The operator** opens the `?remote=1&token=…` URL (`emberd --open` does it for you) and controls
  the daemon's own node: links, radios, messages and settings.

Headless box over SSH: `emberd --console`, or attach to a running service with `emberd --attach`.

## Command-line flags

| Flag | Meaning |
|---|---|
| `--config FILE` | JSON config file (default `<data-dir>/emberd.json` if it exists). CLI flags override the file. |
| `--data-dir DIR` | State directory. Defaults: Linux `$XDG_DATA_HOME/embermesh` or `~/.local/share/embermesh`; macOS `~/Library/Application Support/EmberMesh`; Windows `%APPDATA%\EmberMesh`. |
| `--web-root DIR` | Web app served at `/`. Default: the first of `$EMBER_WEB_ROOT`, `./dist/web`, `<exe>/../share/embermesh/web`, `<exe>/web`, `<exe>/../web`, `<repo>/dist/web` that contains `index.html` **and** `wasm/ember.wasm`; failing that a plain `ui/` (with a warning); failing that a built-in fallback page. |
| `--download-dir DIR` | Served at `/download/` (default `<data-dir>/download`, plus `dist/android/` and the single-file `<web-root>/embermesh.html` if present). |
| `--port N` | Hub port (default `8437`). |
| `--bind ADDR` | Bind address (default `0.0.0.0`; use `127.0.0.1` for local only). |
| `--name NAME` | This node's display name (set on the profile). Default for a fresh identity: "EmberMesh hub" plus the first 4 hex digits of its node id — emberd never announces the computer's user or host name. |
| `--callsign CALL` | Amateur callsign for the profile. `kiss`/`audio` links in amateur mode refuse to transmit as `N0CALL`. |
| `--passphrase-file FILE` | Unlock an encrypted store at start with the passphrase in FILE (first line). |
| `--no-qr` | Don't print the QR code. |
| `--no-hub` | Don't serve HTTP/WS (pure radio gateway / relay). |
| `--token TOKEN` | Use this admin token for `/rpc`. |
| `--new-token` | Generate and store a new admin token. |
| `--print-token` | Print the admin token and exit. |
| `--local-no-token` | Allow `/rpc` from 127.0.0.1 / ::1 without a token. |
| `--no-udp` | Disable the LAN multicast link (default on: `239.83.77.69:47474`, interoperates with the phone apps on the same Wi-Fi). |
| `--no-nearby` | Do not advertise/browse `_embermesh._tcp` (mDNS) or link up with nearby devices automatically (see *Nearby devices*). |
| `--nearby-service NAME` | Use another mDNS service type (testing). |
| `--relay-broker [PORT]` | Also run the `/mqtt` relay broker on its own port (default 8438); see *A private internet relay*. |
| `--setup-done` | Acknowledge the Nearby/internet notices for a headless node (what first-run setup does in the app). |
| `--link JSON` | Bring up a link for this run (repeatable). Same shape as `host.link.create` params. Not saved: the core keeps it with `persist:false`; drop the flag and it is gone. |
| `--kiss-tcp HOST:PORT` | Shorthand: KISS over TCP (Direwolf: `127.0.0.1:8001`). |
| `--kiss-serial PORT[:BAUD]` | Shorthand: KISS TNC on a serial port. |
| `--console` | Interactive terminal chat in the same process (node + hub keep running). |
| `--attach [URL]` | Terminal chat against a running emberd (default `ws://127.0.0.1:8437/rpc`; token from `<data-dir>/admin_token` or `?token=` in the URL). |
| `--open` | Open the operator UI (`http://localhost:8437/?remote=1&token=…`) in the default browser. |
| `--list-serial` / `--list-audio` | Print devices and exit. |
| `--log-level debug\|info\|warn\|error` | Log verbosity (default `info`). |
| `--log-file FILE` | Log to a file instead of stderr. |
| `--in-memory` | Persist nothing (testing). |
| `--guides FILE` | `guides.pack` to load. Default lookup: `<exe>/guides.pack` (dist/desktop), `<data-dir>/guides.pack`, `<exe>/../share/embermesh/guides.pack`, `<web-root>/guides.pack`, `./build/guides.pack`. Loaded zero-copy. |
| `--version`, `--help` | |

### Data directory

| Path | Contents |
|---|---|
| `node/` | core storage (identity, messages, contacts… **and the link list**) |
| `admin_token` | remote-control token (mode 0600) |
| `saved/` | files written by `host.file.save` (bundles, backups) |
| `inbox/` | `host.file.pick` returns the newest matching file from here (there is no GUI file picker) |
| `download/` | files offered at `/download/` |

### Config file

Keys mirror the long flags, with `_` instead of `-`:

```json
{
  "name": "camp-north-hub",
  "port": 8437,
  "bind": "0.0.0.0",
  "hub": true,
  "udp": true,
  "local_no_token": false,
  "web_root": "/usr/local/share/embermesh/ui",
  "download_dir": "/var/lib/embermesh/download",
  "log_level": "info",
  "captive_portal": false,
  "links": [
    { "name": "Direwolf",
      "endpoint": { "kind": "tcp", "config": { "host": "127.0.0.1", "port": 8001 } },
      "link": { "type": "kiss", "config": {} } }
  ]
}
```

### Links and persistence

The core is the single source of truth for links (docs/RPC_API.md, "Link persistence"). Links added
live (operator page, console `link add`, `host.link.create`) are persisted by the core with stable
ids. At start, and again on every `node.ready` (after unlocking, a duress unlock or a wipe), emberd
calls `links.list` and reopens the endpoint of every link whose `endpoint.kind` it provides, then
reports `links.endpoint_state`. emberd keeps no link file of its own. `--link`/config-file links are added with `persist:false` and re-added
after unlock/wipe. The hub is the core link with endpoint kind `ws_server` (raw, preset `websocket`):
it is reopened at start unless `--no-hub`; `host.hub.stop` removes it so it stays off.

`host.link.create` checks the link type against the endpoint kind using the core's `links.types`
(`endpoint_kinds`), and fills a fitting driver preset when none is given: raw over `udp` → `udp_lan`,
over `websocket`/`ws_server` → `websocket`, `loopback` → `loopback`; stream over `tcp`/`tcp_server` →
`tcp`, over `serial` → `cable`. `autostart:false` (or `persist:false`) means "this run only".
Optional `host_key` makes create idempotent (an existing link with the same key is reused).

### Locked (encrypted) store

A node whose storage has a passphrase starts locked. emberd then: unlocks with `--passphrase-file`;
otherwise, on a terminal, asks for the passphrase (no echo, 3 tries; empty = later); otherwise prints
"storage is LOCKED". While locked the hub serves the app and `/rpc`, so the operator page (Settings →
Security) or the console (`call security.unlock {"passphrase":"…"}`) can unlock; guests join the mesh
and the saved links come back as soon as the node is unlocked (`node.ready`).

## Hub URLs (port 8437)

| Path | What |
|---|---|
| `/` | the web app (from `--web-root`, or the built-in fallback page) |
| `/ws` | mesh bus: binary WebSocket, one mesh frame per message (≤ 4096 bytes = the raw `websocket` preset mtu; larger closes with 1009). All guests share one core `raw` link; the mesh engine relays. |
| `/rpc` | remote-control JSON-RPC WebSocket for the operator, protected by the token (`?token=`) |
| `POST /rpc` | the same over plain HTTP for scripts: body `{"method","params"}`, with `Authorization: Bearer <token>` or `?token=` |
| `/download/` | listing and files for spreading the app (single-file HTML build, APK, source archive) |
| `/hub.json` | hub probe: `{"embermesh_hub":true,"name":…,"ws":"/ws",…}` |

Quick check with curl:

```sh
curl http://localhost:8437/hub.json
curl -H "Authorization: Bearer $(emberd --print-token)" \
     -d '{"method":"node.info","params":{}}' http://localhost:8437/rpc
```

To spread the app, copy the single-file HTML build, the Android APK and a source archive into
`<data-dir>/download/`. Guests can then download them from `http://<hub-ip>:8437/download/`
and pass them on.

## Console (`--console`, `--attach`)

The console is a line-based REPL that works over plain SSH or a serial console. Incoming messages,
SOS alerts and link-state changes print asynchronously.

| Command | |
|---|---|
| `help` | list commands |
| `info` | node info (id, name, fingerprint) |
| `peers`, `contacts`, `channels`, `convs` | lists |
| `read <conv> [n]` | last n messages (`conv` = `dm:<node_id>` or `ch:<channel_id>`) |
| `send <conv> <text>` | send to a conversation |
| `say <text>` | send to the Public channel |
| `dm <node_id> <text>` | direct message |
| `sos [text]` / `sos stop` / `sos status` / `sos list` | emergency beacon |
| `checkin <ok\|need_help\|injured\|moving\|sheltering> [note]` | status check-in |
| `links`, `link add <json>`, `link rm <id>` | link management (`link add` takes `host.link.create` params) |
| `hub` | hub status, URLs, client count |
| `unlock <passphrase>`, `lock` | unlock / lock an encrypted store (the passphrase is visible as you type; prefer `--passphrase-file` or the start-up prompt) |
| `call <method> [json]` | raw RPC call |
| `quit` | exit (with `--console` this also stops the daemon) |

## Radio gateway recipes

Links can be added in three ways: at start with `--link` / `--kiss-*`, in the config file, or live
from the operator UI or console (`link add …`). Links added live are saved and restored at the next
start. The link types listed here are the core drivers. The operator UI shows the authoritative list
from `links.types`.

**Direwolf (sound-card TNC) via KISS over TCP.** In `direwolf.conf` set `KISSPORT 8001`, then run:

```sh
emberd --kiss-tcp 127.0.0.1:8001
```

**Hardware KISS TNC on USB serial** (Mobilinkd over USB, TNC-Pi, radios with a built-in TNC):

```sh
emberd --kiss-serial /dev/ttyUSB0:9600        # Linux
emberd --kiss-serial /dev/cu.usbserial-A1:9600 # macOS: use /dev/cu.*, not /dev/tty.*
emberd --kiss-serial COM3:9600                # Windows (untested)
emberd --list-serial                          # find the port
```

**Meshtastic** over TCP (Wi-Fi-enabled devices, API port 4403) or USB serial:

```sh
emberd --link '{"endpoint":{"kind":"tcp","config":{"host":"192.168.1.50","port":4403}},"link":{"type":"meshtastic","config":{}}}'
emberd --link '{"endpoint":{"kind":"serial","config":{"port":"/dev/ttyACM0","baud":115200}},"link":{"type":"meshtastic","config":{}}}'
```

**Sound-card soft modem / "Connect a radio".** Connect the radio's audio to a sound card (a USB
interface such as an AIOC or Digirig, a CM108 dongle, or the computer's own jacks), or use acoustic
coupling. Pick a core preset (`links.types` → `audio` → `preset`; `rf.radios` has per-radio
settings): `handheld_vox`, `handheld_ptt`, `acoustic`, `sound_card`. Transmitter keying (`ptt` on
the audio endpoint):

| `ptt` | Keys with | Extra keys |
|---|---|---|
| `vox` | the radio's own VOX (use `handheld_vox`) | |
| `serial_rts` / `serial_dtr` | a serial port's RTS/DTR line: Digirig (its CP210x port, RTS), AIOC (its CDC port, DTR), any USB-serial adapter + transistor | `ptt_port`, `ptt_invert` |
| `cm108` | the USB sound chip's HID GPIO (CM108/CM119 dongles, AIOC in CM108 mode); Linux `/dev/hidrawN` (needs write access: udev rule or `plugdev`), macOS IOKit HID | `ptt_device` (`auto`, `/dev/hidrawN`, or `VID:PID`), `ptt_gpio` (3) |
| `rigctld` | CAT through Hamlib `rigctld -m <model> -r <port>` (TCP 4532) | `rig_host`, `rig_port`, `rig_freq_hz`, `rig_mode` |
| `none` | receive only | |

`host.endpoints.scan {kind:"audio"}` lists input/output devices **and** `ptt_devices`: VOX, every
serial port (with a hint and ready-made config for AIOC/Digirig/USB-serial), CM108-compatible HID
devices and rigctld, each with the endpoint `config` to merge. A named device that is missing (an
unplugged USB interface) is an error, retried with backoff until it is back; a device that stops
mid-session drops the link the same way (`fallback_default:true` uses the system default instead).

Quick start with `--radio` (or `"radio": {...}` / `"radio": "k=v,..."` in the config file):

```sh
emberd --list-audio                     # device names (a part of the name is enough)
emberd --callsign N0CALL-1 --radio "preset=handheld_vox,in=USB Audio,out=USB Audio"
emberd --callsign N0CALL-1 --radio "preset=handheld_ptt,in=AIOC,out=AIOC,ptt=serial_dtr,ptt_port=/dev/ttyACM0"
emberd --callsign N0CALL-1 --radio "preset=handheld_ptt,in=USB Audio,out=USB Audio,ptt=cm108"
# the same as a full link spec:
emberd --link '{"endpoint":{"kind":"audio","config":{"input_device":"USB Audio","output_device":"USB Audio","ptt":"serial_rts","ptt_port":"/dev/ttyUSB0"}},"link":{"type":"audio","config":{"preset":"handheld_ptt"}}}'
```

(Replace `N0CALL-1` with your own callsign: amateur links refuse to transmit as N0CALL.) On macOS
the first open of a microphone needs the permission prompt answered: run emberd once from Terminal.

**RTL-SDR receive via rtl_tcp.** Run `rtl_tcp -a 127.0.0.1 -p 1234`. The **link** settings tune the
dongle (`frequency_hz`, `sample_rate`, `gain_db`, `ppm`); the connection only needs host and port.
(The connection's own `freq_hz`/`sample_rate` fields are used only with `"tune":true`.)

```sh
emberd --link '{"endpoint":{"kind":"rtl_tcp","config":{"host":"127.0.0.1","port":1234}},"link":{"type":"rtl_tcp","config":{"frequency_hz":144390000,"sample_rate":"240000"}}}'
```

**Morse (CW) reading.** The core decodes Morse from light (camera) or audio (RPC_API §6). On the
operator page the browser captures camera and microphone itself (`getUserMedia`) and feeds
`morse.rx.feed` over `/rpc`. The daemon adds a native source: `host.capture.start
{kind:"mic_pcm", session, device?|input_path?, sample_rate?}` reads this computer's sound input (or a
raw s16le PCM file/FIFO) and feeds the session every ~33 ms; `host.info.features` has
`capture_mic_pcm` and `host.info.capture` says that camera capture stays in the browser. Console:

```
> morse listen                    # default input; or: morse listen USB Audio | morse listen file:/tmp/cw.pcm
~~ morse [18 wpm]: CQ CQ DE N0CALL K
> morse status
> morse stop                      # prints the transcript
```

**Hub-to-hub bridging** over any IP network: use a `websocket` endpoint to `ws://<other-hub>:8437/ws`
with a `raw` link, or `wss://` when the other hub sits behind a TLS reverse proxy (see *TLS*).

## Turning a machine into a Wi-Fi hotspot hub (no internet needed)

| OS | Recipe |
|---|---|
| Linux with NetworkManager (Pi OS bookworm, Ubuntu, Fedora) | [`deploy/linux/hotspot-nmcli.sh`](deploy/linux/hotspot-nmcli.sh): `sudo ./hotspot-nmcli.sh --ssid EmberMesh` |
| Linux without NetworkManager (older Pi OS, minimal installs) | [`deploy/linux/hostapd/`](deploy/linux/hostapd/): hostapd + dnsmasq, static 10.42.0.1 |
| macOS | Internet Sharing to Wi-Fi; see [`deploy/macos/README.md`](deploy/macos/README.md) |
| Windows 10/11 | Mobile hotspot; see [`deploy/windows/README.md`](deploy/windows/README.md) |

Guests join the Wi-Fi network and open `http://<hub-ip>:8437/`. With the nmcli and hostapd recipes
that is `http://10.42.0.1:8437/`.

Optional captive-portal behaviour: set `"captive_portal": true` in the config. The hub then answers
the OS connectivity probes (`/generate_204`, `/hotspot-detect.html`, `/ncsi.txt`, `/connecttest.txt`)
with a redirect to itself. Those probes go to port 80, so you also need:

- DNS that resolves every name to the hub (dnsmasq `address=/#/10.42.0.1`);
- a port redirect 80 → 8437 (`iptables -t nat -A PREROUTING -i wlan0 -p tcp --dport 80 -j REDIRECT --to-ports 8437`).

Captive "sign-in" mini-browsers often lack service workers and storage. Tell users to open the page
in their real browser.

## Running as a service

| OS | Files |
|---|---|
| Linux (systemd) | [`deploy/linux/emberd.service`](deploy/linux/emberd.service), [`deploy/linux/install.sh`](deploy/linux/install.sh) |
| macOS (launchd) | [`deploy/macos/org.embermesh.emberd.plist`](deploy/macos/org.embermesh.emberd.plist) |
| Windows | [`deploy/windows/`](deploy/windows/): Task Scheduler (`install-task.ps1`) or NSSM |

On Linux the service runs as user `ember` with data in `/var/lib/embermesh`:

```sh
sudo -u ember emberd --data-dir /var/lib/embermesh --print-token   # operator token
emberd --attach "ws://127.0.0.1:8437/rpc?token=<token>"             # console against the service
```

## Privacy notes

emberd never puts the computer's user name, host name or device names on the network unless the
operator types them in. Without `--name`, a fresh identity is called "EmberMesh hub" plus the first
4 hex digits of its node id; the computer's name is never used.

What anyone on the network can learn **without the admin token**:

| Where | What |
|---|---|
| `GET /hub.json` | `name` (the hub node's display name), `node_id`, emberd `version`, number of `guests` connected, and the fixed paths `/ws`, `/rpc`, `/download/` |
| HTTP | the hub's LAN address and port; header `Server: emberd` |
| `/download/` | the names and sizes of the files offered there |
| `/ws` (the mesh bus) | everything any mesh node broadcasts: Public-channel messages, SOS, check-ins, bulletins, and every node's announcement (name, callsign, status, public keys). Direct messages and private channels cross it end-to-end encrypted. |
| mDNS (Nearby devices) | see below |
| relay broker (`--relay-broker`, its own port) | that this machine relays; nothing else is readable (topics are random hex, payloads sealed). The broker sees connecting clients' addresses, like any relay. |

**What a LAN observer learns from Nearby devices.** emberd answers and sends multicast DNS on
224.0.0.251 / ff02::fb port 5353 with exactly these records, nothing else:

| Record | Content |
|---|---|
| PTR `_embermesh._tcp.local` | `EmberMesh-<token>._embermesh._tcp.local` |
| SRV | port of the Nearby TCP listener (47475 if free), host `EmberMesh-<token>.local` (never the computer's host name) |
| TXT | `ember=1`, `v=1`, `link=stream`, `t=<token>` |
| A / AAAA `EmberMesh-<token>.local` | the private (RFC 1918 / 169.254) IPv4 addresses of the local-network interfaces and their IPv6 link-local addresses; never public, global or EUI-64 addresses |

`<token>` is 8 random hex digits from the core, new at every start; it is not derived from the node
id, the computer or the user, and it changes when the network changes. So a passive observer
learns that a device at this address runs EmberMesh, its listening port, and a token that changes
at every start and network change. Anyone who connects (and sends the Nearby hello) then sees what
any mesh neighbour sees: the node's id, display name, callsign, public keys and Public traffic —
stable identifiers, which is why the app discloses Nearby before it starts. No host name, user
name, model or version is advertised. Turn it off with `--no-nearby` or Settings → Nearby devices
(`connectivity.nearby`).

The operator page, `/rpc` and console need the admin token. To rename the hub node later, use
`--name NEW` (applied at start), console `name NEW`, or the operator page (Settings → Profile).

## Nearby devices (mDNS, automatic links)

While the core reports `nearby.active` (Nearby on, first-run setup done, node unlocked;
`--no-nearby` turns the host side off), emberd listens for Nearby TCP connections, advertises
`_embermesh._tcp` with its own small mDNS responder (no Avahi/Bonjour library; it shares port 5353
with the OS responder through address/port reuse), browses for other instances and links up with
them by itself (docs/CONNECTIVITY.md §3). Nothing starts before the first-run notice has been
acknowledged: by the operator page's setup screen, or `--setup-done` for a headless node (emberd
prints the notice text at start while it is pending). On lock, wipe or duress everything stops.

- the node with the smaller token dials; the other dials too if no connection from that peer
  arrives within 8 s (one side may not see the other's announcements);
- addresses are tried IPv6 link-local first, then IPv4, 250 ms apart (first to connect wins);
- each connection is a core `stream` link with `preset:"nearby"` (the core sends and requires the
  8-byte hello, so a browser request cannot inject frames) created with `links.add {auto:"nearby",
  auto_role, auto_key}` (`"<token>@<address>"` dialing, `"@<address>"` accepting; not persisted); a
  nearby link whose connection ends is removed and rediscovery makes a new one; when the core removes
  a link (`link.removed`: duplicate, hello failed, evicted, Nearby off, locked) emberd closes it;
- the listener is bound only to the local-network interface addresses (private IPv4, IPv6
  link-local), never to every interface; a change of those addresses is a network change:
  `connectivity.network_changed` gives a new token and emberd re-advertises;
- limits: inbound only from on-link sources, at most 2 connections per source IPv4 address or IPv6
  /64 and 4 new per minute per source; outbound only to on-link addresses from the mDNS answer, at
  most 4 per instance and 4 new dials per minute; the core caps nearby links at 16; the mDNS parser
  drops malformed packets (bounded names, no compression loops, ≤ 64 records per section, TXT ≤ 1300
  bytes, valid UTF-8), ignores responses not from port 5353 and processes at most 40 packets/s per
  sender and 500/s in total.

`host.info` has feature `nearby` and a `nearby` status object (port, links, devices seen, mDNS
counters). `--nearby-service NAME` uses another service type (tests use a private one so they
never meet real devices).

## A private internet relay (`--relay-broker`)

`emberd --relay-broker [PORT]` also runs a minimal MQTT 3.1.1 broker over WebSocket (path `/mqtt`,
subprotocol `mqtt`) on **its own listener** (default port 8438; nothing else is served there — not
the web app, `/ws` or `/rpc`), so a machine reachable from the internet is a private relay with no
third party (CONNECTIVITY §4.12). Off by default. Topics and subscriptions must be exactly 32
lowercase hex characters (what relay links use): `#`, `+`, `$…` and anything else are refused
(SUBACK 0x80), PUBLISHes elsewhere are denied, retain/will/QoS > 0 are ignored, nothing is stored.
Limits: ≤ 200 clients, ≤ 4 per source address, ≤ 256 subscriptions and 100 messages/s per client,
≤ 50 messages/s per topic, packets ≤ 4400 bytes (a sealed relay frame is ≤ 4137), slow subscribers
dropped after 15 s. It never logs topics, client ids or payloads.

Phones, browsers and emberd connect to relays only with `wss://`, and the broker has no TLS of its
own, so put it behind a TLS-terminating reverse proxy that **forwards only `/mqtt`**. Example with Caddy
(automatic certificates) on a machine reachable as `relay.example.org`:

```
relay.example.org {
  reverse_proxy /mqtt 127.0.0.1:8438
}
```

or nginx:

```
location = /mqtt {
  proxy_pass http://127.0.0.1:8438;
  proxy_http_version 1.1;
  proxy_set_header Upgrade $http_upgrade;
  proxy_set_header Connection "upgrade";
  proxy_set_header X-Forwarded-For $remote_addr;
  proxy_read_timeout 600s;
}
```

Run `emberd --bind 127.0.0.1 --relay-broker` (hub and broker reachable only through the proxy),
forward only TCP 443 to the proxy, and add `wss://relay.example.org/mqtt` as a relay in Settings →
Internet relay on every device. The proxy must set `X-Forwarded-For` (Caddy does by default): for
connections from this machine the broker takes the last entry as the client address for its
per-source limit. Never expose the hub port (8437: `/`, `/ws`, `/rpc`) to the internet. The relay sees
what any relay sees (CONNECTIVITY §4.4: client addresses, timing, sizes, which addresses share a
topic) and nothing else; anyone who can reach the URL can use it, within the limits above.

## Security notes

- `/rpc` controls the hub's own node, so it is admin access. It requires the token from
  `<data-dir>/admin_token`, which is generated once with mode 0600. `--new-token` rotates it.
  Guests never get it. They only use `/`, `/ws`, `/download/` and `/hub.json`.
- `--local-no-token` trusts anything that can connect from localhost. Don't use it on multi-user machines.
- The hub uses plain HTTP/WebSocket without TLS. That is fine on an isolated local hotspot, but
  anyone on the network can see bus traffic. Direct messages and private channels are end-to-end
  encrypted by the core anyway; the Public channel is signed cleartext. The admin token travels in
  cleartext too, so use the operator UI from the hub machine itself (`localhost`) or over a network
  you trust.
- Use `--bind 127.0.0.1` to serve only the local machine, or a specific interface address to
  serve only the hotspot.
- Guests on `/ws` share one core link, and the core rate-limits per link, so the hub does fair
  queuing: each guest has a bounded queue and its own budget (100 frames/s sustained, 256 KiB/s),
  all guests together feed the core at most 300 frames/s, served round-robin. A guest that overflows
  its budget by more than 2000 frames within 10 s is disconnected (1008). Outbound frames to a guest
  that stopped reading are dropped (bounded buffer) and the guest is disconnected after 15 s of drops.
- The HTTP server is hardened against path traversal, oversized headers and slow clients; at most 48
  connections per LAN address (loopback is exempt), `max_guests` (200) on the bus.

## TLS (`wss://` and the internet relay)

The `websocket` endpoint speaks `wss://` through a TLS client built on the vendored Mbed TLS 3.6
(`host/third_party/mbedtls`, Apache-2.0; build option `EMBER_HOST_TLS`, ON by default on desktop).
TLS 1.2 with ECDHE and AEAD ciphers; the certificate chain and the host name are **always**
verified (there is no switch to turn that off) against the operating system's trust anchors: the
Security framework's anchors on macOS, the `ROOT` system store on Windows, the distribution's CA
bundle on Linux (`/etc/ssl/certs/ca-certificates.crt` and the other usual paths). Setting
`SSL_CERT_FILE` to a PEM file **replaces** those anchors on every platform, for a private CA.
`host.info` reports `websocket_tls:true` and the `relay` feature.

Internet relay links are created automatically, never by hand: once the user has agreed to the
internet relay (Settings → Internet relay), emberd opens one `wss://` connection per enabled relay
while the node is unlocked, follows changes to the list, and closes them all when the relay is
turned off or the node is locked. Relay sockets send no `User-Agent`, carry messages of at most
8 KiB, accept `ws://` only for a relay added with `allow_plaintext` to a `.local`, private or
link-local address, and reconnect with a backoff of 2 s doubling to 5 min (±20 %), reset after 2
minutes connected. TLS 1.3 is not enabled yet (it needs the PSA crypto layer); every public broker
used by the defaults speaks TLS 1.2.

## Limitations

- No `https://` client and no TLS on the hub itself (put a reverse proxy in front for that).
- Windows code paths (Winsock, Win32 serial, WASAPI through miniaudio) are compiled in CI;
  serial ports and sound cards on Windows have not been exercised.
- Not verified with real radios, TNCs, sound cards or SDR dongles (see *Tests* for what was).
- macOS: the first open of a capture device triggers the microphone-permission prompt. If nobody
  answers it, the audio link goes to `error` after `open_timeout_ms` (8 s) with an explanation, and
  further opens/scans fail fast until the OS returns; nothing else in emberd waits on it.
- `host.file.pick` has no GUI: drop files into `<data-dir>/inbox/`.

## Guide library

emberd memory-maps `guides.pack` (40–60 MB packs are expected) and hands it to the core zero-copy:
pages load on demand and are shared with the OS page cache (`--guides-copy` reads it into RAM
instead). Replace the file by rename (`install`, `mv`), not by rewriting it in place, while emberd runs.
Guests download the pack from the web root (`/data/guides.pack` in `dist/web`) with `Range`/`ETag`
support, so a phone can resume an interrupted download; static files are streamed, so memory does
not grow with the number of concurrent downloads.

## Embedding only the hub (`ember_hub.h`)

Apps that drive the core node themselves (the Android and iOS shells) cannot use `ember_host.h`,
which must be the only consumer of `ember_poll()`. `host/include/ember_hub.h` runs just the hub
(HTTP, web root, `/hub.json`, `/download/`, the `/ws` bus with the same fair queuing and limits) on
its own thread and hands guest frames to a callback; the app feeds them to `ember_link_rx` for its
own `ws_server` raw link and calls `ember_hub_broadcast` for that link's TX. No `/rpc`.

## Virtual sound card (`audio_pipe`)

Endpoint kind `audio_pipe` replaces the sound card with named pipes or files carrying raw s16le mono
PCM (`input_path`, `output_path`, `sample_rate`, created as FIFOs if missing). It runs in real time:
every 20 ms it writes exactly the due samples (silence when idle), so two nodes can be wired
sample-to-sample (A.out → B.in, B.out → A.in) for tests, or connected to sox/aplay/arecord/Direwolf.
A little noise (`rx_noise_dbfs`, default −70) is added to the input like a real sound card. POSIX only.

An optional channel model on the output simulates the radio path: `vox_attack_ms` (the radio's VOX
loses the start of every transmission), `vox_hang_ms`, `vox_threshold_dbfs`, `tx_highpass_hz`,
`tx_lowpass_hz`, `tx_gain_db`, `tx_noise_dbfs`. Status reports sample counts, levels, drops and VOX
key-ups.

```sh
emberd --link '{"endpoint":{"kind":"audio_pipe","config":{"input_path":"/tmp/rx.pcm","output_path":"/tmp/tx.pcm"}},"link":{"type":"audio"}}'
sox -t raw -e signed -b 16 -c 1 -r 48000 /tmp/tx.pcm -d    # listen to what it transmits
```

## Tests

| Suite | What |
|---|---|
| `ember_host_tests` | host library against a fake core, the C APIs (`ember_host.h`, `ember_hub.h`) against the real core |
| `emberd_tests` | console rendering |
| `emberd_mesh.*` (label `mesh`, one CTest entry per scenario) | real emberd processes and the real core: udp multicast pair; tcp_server/tcp pair; websocket→hub pair (Public both ways, DM with delivered receipt, SOS); chain A–B–C relay + store-carry-forward with A and C never online together; gateway hub ⟷ kiss (TCP "air" relay) and hub ⟷ AFSK1200 over `audio_pipe` (DMs must not cross the amateur link); `handheld_vox` preset through a simulated VOX radio; `acoustic` preset through a band-limited, attenuated, noisy path; serial over a pty cable with `stream` and `kiss`; `rtl_tcp` receive of FM-modulated I/Q made from the core's own AFSK; native Morse capture from a PCM pipe + console `morse listen`; 30 Morse feeds/s over `/rpc` with latency of concurrent calls; 48 MiB file to 40 concurrent downloads + Range/ETag; garbage on every endpoint; /ws flood + frozen guest eviction; 100 guests; restart persistence; locked store; console `--attach`/`--console`; anonymous default name |
| `apps/desktop/tests/hub_guests_chrome.mjs` | manual: two headless-Chrome guests (real WASM nodes) + operator page against a running emberd (Node ≥ 22; Chrome must be able to launch — it will not in some locked-down CI containers). `--operator-only` skips the guests. |

Set `EMBER_TEST_LOGS=1` to print daemon logs, `EMBER_KEEP_TEST_DIRS=1` to keep their data dirs.
Scenarios whose core feature is missing (e.g. a preset or `morse.rx.*` in a core build without them)
report that and skip.

## What is proven, and what is not

Proven by the automated tests above: every IP medium, serial over ptys, the kiss and audio
link drivers through emulated channels, rtl_tcp receive of synthetic I/Q, hub robustness and limits,
persistence, locked stores, the console, `/download/`, native Morse capture into the core decoder,
and (`hub_guests_chrome.mjs`) two headless-Chrome WASM guests exchanging Public messages and
encrypted DMs with delivered receipts with each other and the hub node, plus the operator page.
Real hardware actually exercised: enumeration of the computer's sound devices, and opening a real
**output** device (the modem's audio was played). Capture from a real microphone has not been
exercised (on macOS it waits on the OS permission prompt, which emberd times out on with an
explanation).

Not proven: Raspberry Pi, and Linux or Windows outside the CI runners; any real radio, TNC,
LoRa device or SDR dongle; real AIOC / Digirig / CM108 interfaces (the CM108 HID code is compiled for
macOS and Linux but has not driven a device; there is no CM108 PTT on Windows); Linux non-standard
serial baud rates (termios2) on a real adapter; real VOX timing of a handheld; Morse decoding quality on
real signals (that is the core's decoder); memory-mapping a 40–60 MB guide pack in emberd (48 MiB
was tested as a static download).
