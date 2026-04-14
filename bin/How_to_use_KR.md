# PCSX2 ME56PS2 USB Modem — 사용법

## 개요

이 버전은 **Omron ME56PS2 USB 모뎀 에뮬레이션**이 내장된 수정판 PCSX2입니다. 모뎀 멀티플레이를 지원하는 게임(예: 아머드 코어 2 어나더 에이지)에서 온라인 대전이 가능합니다.

게임 데이터는 TCP/IP를 통해 전송되므로, 두 대의 PCSX2가 로컬 네트워크 또는 인터넷을 통해 연결할 수 있습니다.

## 시연 영상

https://youtu.be/luzijcTlwYk?si=nV5E8qtBftBgjss6

## 빠른 시작

### 플레이어 A (서버 / 수신 측)

1. PCSX2를 열고 **설정 > 컨트롤러 > USB > Port 1**로 이동
2. 장치 유형을 **ME56PS2 Modem**으로 설정
3. **Settings** 클릭 후 설정:
   - **Port**: 포트 번호 입력 (예: `10023`)
   - **Server Mode**: 활성화 (체크)
4. 게임을 시작하고 모뎀 멀티플레이 메뉴로 진입
5. 플레이어 B의 접속을 대기

### 플레이어 B (클라이언트 / 발신 측)

1. PCSX2를 열고 **설정 > 컨트롤러 > USB > Port 1**로 이동
2. 장치 유형을 **ME56PS2 Modem**으로 설정
3. **Settings** 클릭 후 설정:
   - **Remote Host**: 플레이어 A의 IP 주소 (예: `192.168.1.10`)
   - **Port**: 플레이어 A와 동일한 포트 (예: `10023`)
   - **Server Mode**: 비활성화 (체크 해제)
4. 게임을 시작하고 모뎀 멀티플레이 메뉴로 진입
5. 전화번호 입력 시 아무 번호나 입력 (예: `0528#0528`)
6. 설정된 Remote Host와 Port로 자동 연결됩니다

## 호환성 모드 (SubType)

모뎀 장치는 세 가지 **서브타입(SubType)**을 제공합니다. 설정 > 컨트롤러 > USB > Port 1 > Device Subtype에서 선택하세요.

| SubType | 사용 시점 |
|---|---|
| **Balanced** (기본값) | PCSX2 ↔ PCSX2 대전, LAN 또는 합리적인 RTT의 인터넷. 지연과 안정성의 균형. |
| **Compatible (stable)** | **실기 PS2 + ME56PS2 하드웨어**(원본 me56ps2-emulator 경유)와 대전할 때. 데이터 무결성 우선. 원본과 동일한 ~40ms IN 페이싱·OS 기본 소켓 버퍼. |
| **Fast (low latency)** | 양쪽 모두 PCSX2이고 같은 LAN에서 RTT가 매우 낮아 지연을 최소화하고 싶을 때. 가장 공격적 튜닝. 네트워크가 깨끗하지 않으면 프레임을 흘릴 수 있음. |

실기 PS2와 대전 시 AP/HP 동기 ズレ가 보이면 **Compatible**로 전환하세요. 렉은 심하지만 안정적이라면 **Balanced**를 먼저 시도하고, 신뢰할 수 있는 LAN에서만 **Fast**로 이동하는 것을 권장합니다.

## 게임 내 다이얼 입력

- 유효한 IP 형식으로 입력하면 (예: `192-168-1-10#10023`) 해당 주소로 직접 연결됩니다.
- 그 외 아무 번호를 입력하면 (예: `0528#0528`, `1234#5678`) 설정에서 지정한 **Remote Host**와 **Port**로 연결됩니다.

## 네트워크 요구사항

- **LAN 플레이**: 두 PC가 같은 네트워크에 있어야 합니다. 로컬 IP 주소를 사용하세요.
- **인터넷 플레이**: 서버 측 플레이어는 라우터에서 포트포워딩이 필요하거나, 양쪽 모두 VPN(예: ZeroTier, Hamachi)을 사용할 수 있습니다.

## 문제 해결

- **게임이 모뎀을 인식하지 않음**: ME56PS2 Modem이 **USB Port 1**에 설정되어 있는지 확인하세요.
- **연결 실패**: 방화벽 설정을 확인하고 포트가 열려 있는지 확인하세요.
- **"BUSY" 응답**: 서버 측 플레이어가 먼저 게임을 시작하고 대기 중인지 확인하세요.

## 크레딧

- [me56ps2-emulator](https://github.com/msawahara/me56ps2-emulator) by msawahara 기반
- 참고: [Qiita 블로그](https://qiita.com/msawahara/items/f109b75919ddcf0db05a)
- PCSX2 이식: ChungSo

## 라이센스

이 소프트웨어는 PCSX2와 동일한 **GPL-3.0+** 라이센스를 따릅니다.
소스코드: https://github.com/PCSX2/pcsx2
