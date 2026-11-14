#!/usr/bin/env bash
# Hotspot for an EmberMesh hub on Linux WITHOUT NetworkManager (older Raspberry Pi OS with dhcpcd,
# minimal Debian). Pi OS bookworm uses NetworkManager by default: prefer ../hotspot-nmcli.sh there.
#
#   sudo ./setup-hostapd.sh [--iface wlan0] [--ssid EmberMesh] [--password SECRET]
#
# Gives wlan0 the static address 10.42.0.1/24, runs hostapd (access point) and dnsmasq (DHCP + DNS).
# No internet uplink is needed or configured. Idempotent.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
iface="wlan0"
ssid="EmberMesh"
password=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --iface) iface="$2"; shift 2 ;;
    --ssid) ssid="$2"; shift 2 ;;
    --password) password="$2"; shift 2 ;;
    -h|--help) sed -n '2,9p' "$0"; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 1 ;;
  esac
done

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo "error: run as root (sudo $0 ...)" >&2
  exit 1
fi
if systemctl is-active --quiet NetworkManager 2>/dev/null; then
  echo "warning: NetworkManager is active; it may fight hostapd over $iface."
  echo "         Prefer ../hotspot-nmcli.sh, or mark $iface unmanaged in NetworkManager."
fi
if [[ -n "$password" && ( ${#password} -lt 8 || ${#password} -gt 63 ) ]]; then
  echo "error: WPA2 password must be 8..63 characters" >&2
  exit 1
fi

say() { echo "==> $*"; }

if ! command -v hostapd >/dev/null || ! command -v dnsmasq >/dev/null; then
  say "installing hostapd and dnsmasq (needs packages: from the internet once, or a local apt mirror)"
  apt-get install -y hostapd dnsmasq
fi

say "writing /etc/hostapd/hostapd.conf (SSID '$ssid' on $iface)"
mkdir -p /etc/hostapd
sed -e "s/^interface=.*/interface=$iface/" -e "s/^ssid=.*/ssid=$ssid/" "$here/hostapd.conf" > /etc/hostapd/hostapd.conf
if [[ -n "$password" ]]; then
  cat >> /etc/hostapd/hostapd.conf <<EOF
wpa=2
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
wpa_passphrase=$password
EOF
fi
chmod 600 /etc/hostapd/hostapd.conf
if [[ -f /etc/default/hostapd ]]; then
  sed -i 's|^#\?DAEMON_CONF=.*|DAEMON_CONF="/etc/hostapd/hostapd.conf"|' /etc/default/hostapd
fi

say "writing /etc/dnsmasq.d/embermesh.conf"
mkdir -p /etc/dnsmasq.d
sed -e "s/^interface=.*/interface=$iface/" "$here/dnsmasq.conf" > /etc/dnsmasq.d/embermesh.conf

if [[ -f /etc/dhcpcd.conf ]]; then
  if ! grep -q "# embermesh-hotspot" /etc/dhcpcd.conf; then
    say "adding static 10.42.0.1/24 for $iface to /etc/dhcpcd.conf"
    cat >> /etc/dhcpcd.conf <<EOF

# embermesh-hotspot
interface $iface
    static ip_address=10.42.0.1/24
    nohook wpa_supplicant
EOF
  fi
  systemctl restart dhcpcd || true
else
  say "no dhcpcd; assigning 10.42.0.1/24 to $iface now (add it to your network config to persist)"
  ip link set "$iface" up
  ip addr replace 10.42.0.1/24 dev "$iface"
fi

command -v rfkill >/dev/null && rfkill unblock wifi || true
say "enabling hostapd + dnsmasq"
systemctl unmask hostapd 2>/dev/null || true
systemctl enable hostapd dnsmasq
systemctl restart hostapd
systemctl restart dnsmasq

echo
echo "Hotspot '$ssid' is up. Guests: join it and open http://10.42.0.1:8437/ (or http://ember.mesh:8437/)"
