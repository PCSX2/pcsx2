# PCSX2 Modem — ME56PS2 USB 모뎀 에뮬레이션

[English](README.md) | [日本語](README_JP.md)

## 개요

이 프로젝트는 [PCSX2](https://github.com/PCSX2/pcsx2)에 **Omron ME56PS2 USB 모뎀 에뮬레이션**을 내장한 수정판입니다. 모뎀 멀티플레이를 지원하는 PS2 게임(예: 아머드 코어 2 어나더 에이지)에서 온라인 대전이 가능합니다.

게임 데이터는 TCP/IP를 통해 전송되므로, 물리적 모뎀 없이 두 대의 PCSX2가 로컬 네트워크 또는 인터넷을 통해 연결할 수 있습니다.

## 사용법

- [English](How_to_use_EN.md)
- [한국어](How_to_use_KR.md)
- [日本語](How_to_use_JP.md)

## 시연 영상

[![Demo](https://img.youtube.com/vi/luzijcTlwYk/0.jpg)](https://youtu.be/luzijcTlwYk?si=nV5E8qtBftBgjss6)

## 작동 원리

플러그인은 PS2의 IOP가 표준 시리얼 모뎀으로 인식하는 **FTDI FT232 기반 USB 모뎀**(VID: 0x0590, PID: 0x001A)을 에뮬레이트합니다. 게임의 AT 명령(ATD, ATA, ATH 등)을 가로채 TCP 소켓 연결로 변환합니다.

- **서버 모드**: TCP 연결 수신 대기 (수신 측)
- **클라이언트 모드**: 원격 호스트에 TCP 연결 (발신 측)
- **PPP/게임 데이터**: TCP 터널을 통해 투명하게 전달

## 빠른 시작

### 플레이어 A (서버)

1. **설정 > 컨트롤러 > USB > Port 1** → ME56PS2 Modem
2. Settings: **Port** = `10023`, **Server Mode** = 활성화
3. 게임 시작 → 모뎀 멀티플레이 메뉴 → 대기

### 플레이어 B (클라이언트)

1. **설정 > 컨트롤러 > USB > Port 1** → ME56PS2 Modem
2. Settings: **Remote Host** = 플레이어 A의 IP, **Port** = `10023`, **Server Mode** = 비활성화
3. 게임 시작 → 모뎀 멀티플레이 메뉴 → 아무 번호나 입력 (예: `0528#0528`)

유효한 IP 주소가 아닌 번호를 입력하면, 설정된 Remote Host와 Port로 자동 연결됩니다.

## 네트워크 설정

- **LAN**: 로컬 IP 주소를 직접 사용합니다.
- **인터넷**: 서버 측 라우터에서 포트포워딩을 설정하거나, VPN(ZeroTier, Hamachi 등)을 사용합니다.

## 소스에서 빌드

```bash
cd pcsx2
export MSYS_NO_PATHCONV=1
"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" \
  PCSX2_qt.sln /t:pcsx2-qt /p:Configuration=Release /p:Platform=x64 /m
```

출력: `pcsx2/bin/pcsx2-qtx64.exe`

## 크레딧

- **PCSX2** — [https://github.com/PCSX2/pcsx2](https://github.com/PCSX2/pcsx2) (GPL-3.0+)
- **me56ps2-emulator** — [https://github.com/msawahara/me56ps2-emulator](https://github.com/msawahara/me56ps2-emulator) by Masataka Sawahara (MIT License)
- **참고** — [Qiita 블로그](https://qiita.com/msawahara/items/f109b75919ddcf0db05a)
- **PCSX2 이식** — ChungSo

## 라이센스

이 프로젝트는 **GPL-3.0+** 라이센스의 PCSX2를 기반으로 합니다.

USB 모뎀 에뮬레이션은 **MIT License**의 me56ps2-emulator를 기반으로 합니다:

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
