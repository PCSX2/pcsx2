# PCSX2 ME56PS2 USB Modem — How to Use

## Overview

This is a modified version of PCSX2 with built-in **Omron ME56PS2 USB modem emulation**, enabling online multiplayer for games that support modem connectivity (e.g., Armored Core 2 Another Age).

The modem emulation tunnels game data over TCP/IP, so two PCSX2 instances can connect over a local network or the internet.

## Demo Video

https://youtu.be/luzijcTlwYk?si=nV5E8qtBftBgjss6

## Quick Start

### Player A (Server / Answering side)

1. Open PCSX2 and go to **Settings > Controllers > USB > Port 1**
2. Set Device Type to **ME56PS2 Modem**
3. Click **Settings** and configure:
   - **Port**: Choose a port number (e.g., `10023`)
   - **Server Mode**: Enabled (checked)
4. Start the game and proceed to the modem multiplayer menu
5. Wait for Player B to connect

### Player B (Client / Calling side)

1. Open PCSX2 and go to **Settings > Controllers > USB > Port 1**
2. Set Device Type to **ME56PS2 Modem**
3. Click **Settings** and configure:
   - **Remote Host**: Player A's IP address (e.g., `192.168.1.10`)
   - **Port**: Same port as Player A (e.g., `10023`)
   - **Server Mode**: Disabled (unchecked)
4. Start the game and proceed to the modem multiplayer menu
5. When prompted to dial, enter any number (e.g., `0528#0528`)
6. The game will connect using the configured Remote Host and Port

## In-Game Dialing

- If you enter a valid IP in the dial string (e.g., `192-168-1-10#10023`), it will connect directly to that address.
- If you enter any other number (e.g., `0528#0528`, `1234#5678`), it will use the **Remote Host** and **Port** configured in Settings.

## Network Requirements

- **LAN play**: Both PCs must be on the same network. Use the local IP address.
- **Internet play**: The server player must have the configured port forwarded on their router, or both players can use a VPN (e.g., ZeroTier, Hamachi).

## Troubleshooting

- **Game doesn't detect the modem**: Make sure ME56PS2 Modem is set on **USB Port 1** (not Port 2).
- **Connection fails**: Check firewall settings and ensure the port is open.
- **"BUSY" response**: Verify that the server player has started the game first and is waiting for a connection.

## Credits

- Based on [me56ps2-emulator](https://github.com/msawahara/me56ps2-emulator) by msawahara
- Reference: [Qiita blog post](https://qiita.com/msawahara/items/f109b75919ddcf0db05a)
- PCSX2 port by ChungSo

## License

This software is licensed under **GPL-3.0+**, same as PCSX2.
Source code: https://github.com/PCSX2/pcsx2
