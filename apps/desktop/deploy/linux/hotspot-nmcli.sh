#!/usr/bin/env bash
# Turns a Linux machine / Raspberry Pi (NetworkManager, e.g. Pi OS bookworm) into a Wi-Fi hotspot
# for an EmberMesh hub. No internet connection is needed: NetworkManager's "shared" mode runs DHCP
# and DNS for the clients itself (hub address 10.42.0.1 by default).
#
#   sudo ./hotspot-nmcli.sh [--ssid EmberMesh] [--password SECRET] [--iface wlan0]
#                           [--band bg|a] [--channel 6] [--down]
#
# No --password => open network (easiest for strangers in an emergency).
# --password    => WPA2-PSK (8..63 characters).
# --down        => stop and delete the hotspot connection.
set -euo pipefail

ssid="EmberMesh"
password=""
iface="wlan0"
band="bg"
channel="6"
con="embermesh-hotspot"
down=0

usage() { sed -n '2,13p' "$0"; exit "${1:-0}"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ssid) ssid="$2"; shift 2 ;;
    --password) password="$2"; shift 2 ;;
    --iface) iface="$2"; shift 2 ;;
    --band) band="$2"; shift 2 ;;
    --channel) channel="$2"; shift 2 ;;
    --down) down=1; shift ;;
    -h|--help) usage 0 ;;
    *) echo "unknown argument: $1" >&2; usage 1 ;;
  esac
done

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "error: run as root (sudo $0 ...)" >&2
  exit 1
fi
command -v nmcli >/dev/null || { echo "error: nmcli not found (use deploy/linux/hostapd/ instead)" >&2; exit 1; }

if [[ $down -eq 1 ]]; then
  echo "==> stopping hotspot $con"
  nmcli connection down "$con" 2>/dev/null || true
  nmcli connection delete "$con" 2>/dev/null || true
  exit 0
fi

if [[ -n "$password" && ( ${#password} -lt 8 || ${#password} -gt 63 ) ]]; then
  echo "error: WPA2 password must be 8..63 characters" >&2
  exit 1
fi

# Make sure Wi-Fi is not blocked and the radio is on.
command -v rfkill >/dev/null && rfkill unblock wifi || true
nmcli radio wifi on

echo "==> (re)creating connection $con: SSID '$ssid' on $iface"
nmcli connection delete "$con" 2>/dev/null || true
nmcli connection add type wifi ifname "$iface" con-name "$con" autoconnect yes ssid "$ssid"
nmcli connection modify "$con" \
  802-11-wireless.mode ap \
  802-11-wireless.band "$band" \
  802-11-wireless.channel "$channel" \
  ipv4.method shared \
  ipv6.method disabled \
  connection.autoconnect-priority 100
if [[ -n "$password" ]]; then
  nmcli connection modify "$con" wifi-sec.key-mgmt wpa-psk wifi-sec.proto rsn \
    wifi-sec.pairwise ccmp wifi-sec.group ccmp wifi-sec.psk "$password"
fi

echo "==> bringing hotspot up"
nmcli connection up "$con"

ip="$(ip -4 -o addr show dev "$iface" | awk '{print $4}' | cut -d/ -f1 | head -n1)"
echo
echo "Hotspot '$ssid' is up on $iface (${password:+WPA2}${password:-open})."
echo "Guests: join '$ssid' and open  http://${ip:-10.42.0.1}:8437/"
echo "Make sure emberd is running (systemctl status emberd)."
echo "The hotspot starts automatically at boot; remove it with: sudo $0 --down"
