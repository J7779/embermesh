#!/usr/bin/env bash
# Installs emberd as a systemd service on Linux / Raspberry Pi OS. Idempotent.
#
#   sudo ./install.sh [path/to/emberd] [path/to/web] [path/to/guides.pack]
#
# Defaults (relative to the repository root): the first emberd found in dist/desktop/,
# build/host/apps/desktop/, build/desktop/apps/desktop/; the web app dist/web (built by
# `./build.sh web`: the UI *with* the WebAssembly core — the plain ui/ folder has no wasm/ and hub
# guests would see "EmberMesh could not start"); the guide library build/guides.pack.
# Installs:
#   /usr/local/bin/emberd
#   /usr/local/share/embermesh/web/         (web app served at /; emberd finds it by itself)
#   /usr/local/share/embermesh/guides.pack  (offline guide library; emberd finds it by itself)
#   /usr/local/share/embermesh/README.md
#   /etc/systemd/system/emberd.service
#   user/group "ember" (in dialout + audio), data dir /var/lib/embermesh
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "error: run as root (sudo $0 ...)" >&2
  exit 1
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../../../.." && pwd)"
bin="${1:-}"
if [[ -z "$bin" ]]; then
  for c in "$repo/dist/desktop/emberd" "$repo/build/host/apps/desktop/emberd" "$repo/build/desktop/apps/desktop/emberd"; do
    [[ -x "$c" ]] && { bin="$c"; break; }
  done
fi
web="${2:-$repo/dist/web}"
guides="${3:-}"
if [[ -z "$guides" ]]; then
  for c in "$(dirname "${bin:-/nonexistent}")/guides.pack" "$repo/build/guides.pack"; do
    [[ -f "$c" ]] && { guides="$c"; break; }
  done
fi

say() { echo "==> $*"; }

if [[ -z "$bin" || ! -x "$bin" ]]; then
  echo "error: emberd binary not found${bin:+ at $bin} (build it first: ./build.sh desktop)" >&2
  exit 1
fi

say "installing binary $bin -> /usr/local/bin/emberd"
install -D -m 0755 "$bin" /usr/local/bin/emberd

if [[ -d "$web" ]]; then
  if [[ ! -f "$web/wasm/ember.wasm" ]]; then
    echo "warning: $web has no wasm/ember.wasm: hub guests will see 'EmberMesh could not start'." >&2
    echo "         Build the web app first (./build.sh web) and pass dist/web." >&2
  fi
  say "installing web app $web -> /usr/local/share/embermesh/web"
  rm -rf /usr/local/share/embermesh/web.new
  mkdir -p /usr/local/share/embermesh/web.new
  cp -R "$web/." /usr/local/share/embermesh/web.new/
  rm -rf /usr/local/share/embermesh/web
  mv /usr/local/share/embermesh/web.new /usr/local/share/embermesh/web
else
  say "no web app at $web (emberd will serve its built-in fallback page; build it with ./build.sh web)"
fi
if [[ -n "$guides" && -f "$guides" ]]; then
  say "installing guide library $guides -> /usr/local/share/embermesh/guides.pack"
  install -D -m 0644 "$guides" /usr/local/share/embermesh/guides.pack
else
  say "no guides.pack found (build it with ./build.sh guides); the library will be empty"
fi
if [[ -f "$repo/apps/desktop/README.md" ]]; then
  install -D -m 0644 "$repo/apps/desktop/README.md" /usr/local/share/embermesh/README.md
fi

if ! getent group ember >/dev/null; then
  say "creating group ember"
  groupadd --system ember
fi
if ! id -u ember >/dev/null 2>&1; then
  say "creating system user ember"
  useradd --system --gid ember --home-dir /var/lib/embermesh --no-create-home \
    --shell /usr/sbin/nologin ember
fi
for g in dialout audio; do
  if getent group "$g" >/dev/null; then
    say "adding ember to group $g"
    usermod -a -G "$g" ember
  fi
done

say "creating data dir /var/lib/embermesh"
install -d -m 0750 -o ember -g ember /var/lib/embermesh
install -d -m 0750 -o ember -g ember /var/lib/embermesh/download

say "installing systemd unit /etc/systemd/system/emberd.service"
install -D -m 0644 "$here/emberd.service" /etc/systemd/system/emberd.service
systemctl daemon-reload
say "enabling and (re)starting emberd"
systemctl enable emberd >/dev/null
systemctl restart emberd

sleep 1
token="$(sudo -u ember /usr/local/bin/emberd --data-dir /var/lib/embermesh --print-token 2>/dev/null || true)"
say "done. Status: systemctl status emberd   Logs: journalctl -u emberd -f"
say "guests:   http://<this-machine-ip>:8437/"
if [[ -n "$token" ]]; then
  say "operator: http://localhost:8437/?remote=1&token=$token"
fi
