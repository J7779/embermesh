# emberd on macOS

## Install as a launchd user agent

```sh
./build.sh guides desktop web            # emberd, the guide library and dist/web (UI + WASM core)
sudo install -m 0755 dist/desktop/emberd /usr/local/bin/emberd
sudo mkdir -p /usr/local/share/embermesh && sudo cp -R dist/web /usr/local/share/embermesh/web
sudo cp build/guides.pack /usr/local/share/embermesh/guides.pack
sed "s/USERNAME/$(whoami)/g" apps/desktop/deploy/macos/org.embermesh.emberd.plist \
  > ~/Library/LaunchAgents/org.embermesh.emberd.plist
launchctl bootstrap gui/$(id -u) ~/Library/LaunchAgents/org.embermesh.emberd.plist
launchctl kickstart -k gui/$(id -u)/org.embermesh.emberd   # restart after changes
launchctl bootout gui/$(id -u)/org.embermesh.emberd        # stop / uninstall
tail -f ~/Library/Logs/emberd.log
emberd --print-token                                        # operator token
```

The agent runs while you are logged in. Data lives in `~/Library/Application Support/EmberMesh`.
Put links and other options in `emberd.json` there, or add them from the operator UI at
`http://localhost:8437/?remote=1&token=…`.

## Permissions prompts

- **Firewall:** on the first start macOS may ask whether `emberd` may accept incoming network
  connections. Allow it, or guests can't reach port 8437 or the UDP multicast link. Check this in
  System Settings > Network > Firewall > Options.
- **Microphone (audio endpoint):** opening a sound-card input triggers the microphone permission
  prompt. A launchd agent can't answer it. Run `emberd` once from Terminal, add the audio link,
  and grant access (the grant belongs to Terminal / the binary). Until access is granted, the audio
  link stays in "opening".
- **Serial ports:** use `/dev/cu.*` devices (for example `/dev/cu.usbserial-A10K`), not `/dev/tty.*`.
  `emberd --list-serial` lists them. USB-serial chips may need the vendor's driver on older macOS.

## Wi-Fi hotspot with Internet Sharing

Modern macOS can't create a stand-alone ad-hoc ("computer-to-computer") network any more. Use
Internet Sharing, which turns the Mac's Wi-Fi into an access point:

1. System Settings > General > Sharing > **Internet Sharing** > (i).
2. "Share your connection from": pick an interface other than Wi-Fi, such as Ethernet, a USB
   Ethernet adapter, or Thunderbolt Bridge. macOS requires a source interface, but it doesn't
   have to have internet. An unplugged Ethernet port or Thunderbolt Bridge usually works.
3. "To devices using": **Wi-Fi**. Under Wi-Fi Options, set the network name (e.g. `EmberMesh`),
   the channel, and WPA2/WPA3 security with a password. Open networks aren't offered.
4. Turn Internet Sharing on. The Mac becomes `192.168.2.1` on that network, and macOS runs DHCP for guests.
5. Guests join the network and open `http://192.168.2.1:8437/`. emberd also prints the URL at startup.

If Internet Sharing is greyed out, the Wi-Fi card is busy. Disconnect from other Wi-Fi networks first.
Alternatively, put the Mac and the phones on any existing Wi-Fi router, even one with no internet:
guests open `http://<mac-ip>:8437/`, and the LAN multicast link connects to the phone apps on that network.
