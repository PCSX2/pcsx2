# PCSX2 ME56PS2 USB Modem — 使い方

## 概要

これは**Omron ME56PS2 USBモデムエミュレーション**を内蔵した改造版PCSX2です。モデム対戦に対応したゲーム（例：アーマード・コア2 アナザーエイジ）でオンライン対戦が可能です。

ゲームデータはTCP/IPを介して転送されるため、2台のPCSX2がLANまたはインターネット経由で接続できます。

## デモ動画

https://youtu.be/luzijcTlwYk?si=nV5E8qtBftBgjss6

## クイックスタート

### プレイヤーA（サーバー / 着信側）

1. PCSX2を開き、**Settings > Controllers > USB > Port 1** に移動
2. Device Typeを **ME56PS2 Modem** に設定
3. **Settings** をクリックして設定:
   - **Port**: ポート番号を入力（例: `10023`）
   - **Server Mode**: 有効（チェック）
4. ゲームを起動し、モデム対戦メニューに進む
5. プレイヤーBの接続を待機

### プレイヤーB（クライアント / 発信側）

1. PCSX2を開き、**Settings > Controllers > USB > Port 1** に移動
2. Device Typeを **ME56PS2 Modem** に設定
3. **Settings** をクリックして設定:
   - **Remote Host**: プレイヤーAのIPアドレス（例: `192.168.1.10`）
   - **Port**: プレイヤーAと同じポート（例: `10023`）
   - **Server Mode**: 無効（チェックなし）
4. ゲームを起動し、モデム対戦メニューに進む
5. 電話番号入力画面で任意の番号を入力（例: `0528#0528`）
6. 設定済みのRemote HostとPortに自動接続されます

## 互換性モード（SubType）

モデムデバイスは3種類の **SubType** を提供します。Settings > Controllers > USB > Port 1 > Device Subtype から選択してください。

| SubType | 推奨用途 |
|---|---|
| **Balanced** (既定値) | PCSX2 ↔ PCSX2 対戦、LAN または妥当な RTT のインターネット。遅延と安定性の中間。 |
| **Compatible (stable)** | **実機PS2 + ME56PS2ハードウェア**（オリジナル me56ps2-emulator 経由）との対戦時。データ完全性を優先し、オリジナルと同じ ~40ms の IN ペーシング・OSデフォルトのソケットバッファを使用。 |
| **Fast (low latency)** | 両方とも PCSX2 で、同一LAN上で RTT が非常に低く、遅延を最小化したい場合。最も攻撃的なチューニング。ネットワークが綺麗でないとフレームを取りこぼす可能性あり。 |

実機PS2との対戦で AP / HP の同期ズレが見られる場合は **Compatible** に切り替えてください。ラグが大きいが安定している場合はまず **Balanced** を試し、信頼できる LAN のみで **Fast** に移行することを推奨します。

## ゲーム内ダイヤル入力

- 有効なIP形式で入力した場合（例: `192-168-1-10#10023`）、そのアドレスに直接接続します。
- それ以外の番号を入力した場合（例: `0528#0528`、`1234#5678`）、設定画面の **Remote Host** と **Port** を使用して接続します。

## ネットワーク要件

- **LAN対戦**: 2台のPCが同じネットワーク上にある必要があります。ローカルIPアドレスを使用してください。
- **インターネット対戦**: サーバー側プレイヤーはルーターでポート転送を設定するか、両方のプレイヤーがVPN（例: ZeroTier、Hamachi）を使用できます。

## トラブルシューティング

- **ゲームがモデムを認識しない**: ME56PS2 Modemが **USB Port 1** に設定されているか確認してください。
- **接続失敗**: ファイアウォール設定を確認し、ポートが開いているか確認してください。
- **「BUSY」応答**: サーバー側プレイヤーが先にゲームを起動し、待機中であるか確認してください。

## クレジット

- [me56ps2-emulator](https://github.com/msawahara/me56ps2-emulator) by msawahara をベースに開発
- 参考: [Qiitaブログ記事](https://qiita.com/msawahara/items/f109b75919ddcf0db05a)
- PCSX2移植: ChungSo

## ライセンス

本ソフトウェアはPCSX2と同じ **GPL-3.0+** ライセンスに従います。
ソースコード: https://github.com/PCSX2/pcsx2
