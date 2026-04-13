# PCSX2 Modem — ME56PS2 USB Modem Emulation

[한국어](README_KR.md) | [日本語](README_JP.md)

## Overview

This is a modified version of [PCSX2](https://github.com/PCSX2/pcsx2) with built-in **Omron ME56PS2 USB modem emulation**, enabling online multiplayer for PS2 games that support modem connectivity (e.g., Armored Core 2 Another Age).

The modem emulation tunnels game data over TCP/IP, allowing two PCSX2 instances to connect over a local network or the internet — no physical modem required.

## Demo Video

[![Demo](https://img.youtube.com/vi/luzijcTlwYk/0.jpg)](https://youtu.be/luzijcTlwYk?si=nV5E8qtBftBgjss6)

## How It Works

The plugin emulates an **FTDI FT232-based USB modem** (VID: 0x0590, PID: 0x001A) that the PS2's IOP recognizes as a standard serial modem. AT commands from the game (ATD, ATA, ATH, etc.) are intercepted and translated into TCP socket operations.

- **Server mode**: Listens for incoming TCP connections (answering side)
- **Client mode**: Connects to a remote host via TCP (calling side)
- **PPP/game data**: Passed through transparently over the TCP tunnel

## Quick Start

### Player A (Server)

1. **Settings > Controllers > USB > Port 1** → ME56PS2 Modem
2. Settings: **Port** = `10023`, **Server Mode** = Enabled
3. Start game → enter modem multiplayer menu → wait

### Player B (Client)

1. **Settings > Controllers > USB > Port 1** → ME56PS2 Modem
2. Settings: **Remote Host** = Player A's IP, **Port** = `10023`, **Server Mode** = Disabled
3. Start game → enter modem multiplayer menu → dial any number (e.g., `0528#0528`)

When you enter a number that is not a valid IP address, the configured Remote Host and Port are used automatically.

## Network Setup

- **LAN**: Use the local IP address directly.
- **Internet**: Port forward the configured port on the server's router, or use a VPN (ZeroTier, Hamachi, etc.).

## Building from Source

```bash
cd pcsx2
export MSYS_NO_PATHCONV=1
"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" \
  PCSX2_qt.sln /t:pcsx2-qt /p:Configuration=Release /p:Platform=x64 /m
```

Output: `pcsx2/bin/pcsx2-qtx64.exe`

## Credits

- **PCSX2** — [https://github.com/PCSX2/pcsx2](https://github.com/PCSX2/pcsx2) (GPL-3.0+)
- **me56ps2-emulator** — [https://github.com/msawahara/me56ps2-emulator](https://github.com/msawahara/me56ps2-emulator) by Masataka Sawahara (MIT License)
- **Reference** — [Qiita blog post](https://qiita.com/msawahara/items/f109b75919ddcf0db05a)
- **PCSX2 port** — ChungSo

## License

This project is based on PCSX2, licensed under **GPL-3.0+**.

The USB modem emulation is based on me56ps2-emulator, licensed under the **MIT License**:

> MIT License
>
> Copyright (c) 2022 Masataka Sawahara
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.
