# PCSX2 Modem — ME56PS2 USBモデムエミュレーション

[English](README.md) | [한국어](README_KR.md)

## 概要

このプロジェクトは[PCSX2](https://github.com/PCSX2/pcsx2)に**Omron ME56PS2 USBモデムエミュレーション**を内蔵した改造版です。モデム対戦に対応したPS2ゲーム（例：アーマード・コア2 アナザーエイジ）でオンライン対戦が可能です。

ゲームデータはTCP/IPを介して転送されるため、物理モデムなしで2台のPCSX2がLANまたはインターネット経由で接続できます。

## 使い方

- [English](How_to_use_EN.md)
- [한국어](How_to_use_KR.md)
- [日本語](How_to_use_JP.md)

## デモ動画

[![Demo](https://img.youtube.com/vi/luzijcTlwYk/0.jpg)](https://youtu.be/luzijcTlwYk?si=nV5E8qtBftBgjss6)

## 仕組み

プラグインはPS2のIOPが標準シリアルモデムとして認識する**FTDI FT232ベースUSBモデム**（VID: 0x0590, PID: 0x001A）をエミュレートします。ゲームのATコマンド（ATD、ATA、ATHなど）をインターセプトし、TCPソケット接続に変換します。

- **サーバーモード**: TCP接続の着信を待機（着信側）
- **クライアントモード**: リモートホストにTCP接続（発信側）
- **PPP/ゲームデータ**: TCPトンネルを介して透過的に転送

## クイックスタート

### プレイヤーA（サーバー）

1. **Settings > Controllers > USB > Port 1** → ME56PS2 Modem
2. Settings: **Port** = `10023`、**Server Mode** = 有効
3. ゲーム起動 → モデム対戦メニュー → 待機

### プレイヤーB（クライアント）

1. **Settings > Controllers > USB > Port 1** → ME56PS2 Modem
2. Settings: **Remote Host** = プレイヤーAのIP、**Port** = `10023`、**Server Mode** = 無効
3. ゲーム起動 → モデム対戦メニュー → 任意の番号を入力（例: `0528#0528`）

有効なIPアドレスではない番号を入力すると、設定済みのRemote HostとPortに自動接続されます。

## ネットワーク設定

- **LAN**: ローカルIPアドレスを直接使用します。
- **インターネット**: サーバー側ルーターでポート転送を設定するか、VPN（ZeroTier、Hamachiなど）を使用します。

## ソースからビルド

```bash
cd pcsx2
export MSYS_NO_PATHCONV=1
"/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" \
  PCSX2_qt.sln /t:pcsx2-qt /p:Configuration=Release /p:Platform=x64 /m
```

出力: `pcsx2/bin/pcsx2-qtx64.exe`

## クレジット

- **PCSX2** — [https://github.com/PCSX2/pcsx2](https://github.com/PCSX2/pcsx2) (GPL-3.0+)
- **me56ps2-emulator** — [https://github.com/msawahara/me56ps2-emulator](https://github.com/msawahara/me56ps2-emulator) by Masataka Sawahara (MIT License)
- **参考** — [Qiitaブログ記事](https://qiita.com/msawahara/items/f109b75919ddcf0db05a)
- **PCSX2移植** — ChungSo

## ライセンス

このプロジェクトは**GPL-3.0+**ライセンスのPCSX2をベースにしています。

USBモデムエミュレーションは**MIT License**のme56ps2-emulatorをベースにしています：

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
