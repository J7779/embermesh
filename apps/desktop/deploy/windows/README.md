# emberd on Windows (UNTESTED)

> The Windows code paths (Winsock sockets, Win32 serial ports, miniaudio WASAPI audio) compile and
> link, but serial ports and sound cards have **not been exercised** on Windows. Please report problems.
> Build options, runtime dependencies and a portable-folder layout: see [../../README.md](../../README.md#windows).

## Build

Use Visual Studio 2022 (or the Build Tools) from a "Developer PowerShell":

```powershell
cmake -S . -B build/desktop -DCMAKE_BUILD_TYPE=Release
cmake --build build/desktop --config Release -j
.\build\desktop\apps\desktop\Release\emberd.exe --help
```

Data dir: `%APPDATA%\EmberMesh`. Operator token: `emberd --print-token`.

## Firewall

From an elevated PowerShell, allow guests to reach the hub and the LAN multicast link:

```powershell
New-NetFirewallRule -DisplayName EmberMesh -Direction Inbound -Protocol TCP -LocalPort 8437 -Action Allow
New-NetFirewallRule -DisplayName "EmberMesh LAN" -Direction Inbound -Protocol UDP -LocalPort 47474 -Action Allow
```

Windows may also show its own "allow access" dialog the first time emberd listens. Allow it on
**Private** networks, and make sure the hotspot/LAN is set to Private.

## Running automatically

`emberd` is a console program, not a native Windows service. You have two options:

- **Task Scheduler** (built in): `install-task.ps1` registers a task that starts
  `emberd-start.bat` at logon (default) or at system startup (`-AtStartup`, which runs as SYSTEM,
  so its data dir is the SYSTEM profile; pass `-DataDir` to choose one). The task restarts emberd if it fails.

  ```powershell
  powershell -ExecutionPolicy Bypass -File install-task.ps1 -EmberdPath C:\EmberMesh\emberd.exe
  powershell -ExecutionPolicy Bypass -File install-task.ps1 -Remove
  ```

- **NSSM** (the "Non-Sucking Service Manager", a third-party tool) wraps any console app as a real
  service: `nssm install EmberMesh C:\EmberMesh\emberd.exe --data-dir C:\EmberMesh\data`, then
  `nssm start EmberMesh`. emberd shuts down cleanly on Ctrl+C / console close events.

`emberd-start.bat` can also simply be double-clicked. Edit the paths and links at its top.

## Serial ports

Serial ports are named `COM1`, `COM3`, … (`emberd --list-serial`, or Device Manager > Ports).
emberd opens them as `\\.\COMn`, so COM10 and higher work too. Example:

```bat
emberd --kiss-serial COM3:9600
```

## Wi-Fi hotspot (Mobile hotspot)

Windows 10/11: Settings > Network & internet > **Mobile hotspot**.

1. "Share my internet connection from": pick Wi-Fi or Ethernet. Recent builds let you turn the
   hotspot on even when that connection has no internet. Some builds refuse when no adapter is
   connected at all. If so, connect Ethernet to anything, such as a switch or another PC.
2. Edit the network name, password and band, then turn Mobile hotspot on.
3. The PC is usually `192.168.137.1` on the hotspot network. Guests open `http://192.168.137.1:8437/`.
4. Turn off "power saving" (turn off the hotspot when no devices are connected) so it stays up.

To share and host on the same Wi-Fi card, the driver must support it. Check
`netsh wlan show drivers`: "Hosted network supported : Yes", or Wi-Fi Direct support on newer drivers.
If it doesn't, a cheap USB Wi-Fi adapter or an ordinary Wi-Fi router (no internet needed) works as well.
