<p align="center">
  <img src="images/EH3D_BANNER.svg" alt="EH3D-NODE-BUNGARD banner" width="100%">
</p>

# 📡 EH3D-NODE-BUNGARD

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 中文</a> | 🇯🇵 <b>日本語</b> | <a href="README_por.md">🇵🇹 Português</a></p>

### Bungard CCD マシンの筐体内に置き、制御ユニットの RS-232 ポートに接続して、EH3D のプログラムがイーサネットまたは Wi-Fi でマシンを動かせるようにする ESP32-S3 ノード。マシンのランプとスピンドルのドライブも同じ基板で扱います

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0--or--later-blue.svg" alt="GPL 3.0 or later">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B17%20%2B%20ESP--IDF-00599C.svg" alt="C++17 and ESP-IDF">
  <img src="https://img.shields.io/badge/Languages-8-00E5FF.svg" alt="Languages">
  <img src="https://img.shields.io/badge/Version-0.0.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-FFB020.svg" alt="Maturity">
</p>

---

**正直さの確認 - 今日実際に動くもの：** ESP32-S3（ESP-IDF 5.5）用の C++17 ファームウェアで、Bungard CCD マシンの制御ユニットの RS-232 ポートをネットワークポートにします。プログラムはアクセストークン付きで TCP 接続し、マシンをアームして動きのバイト列を送ると、ノードはそれをキューに入れ、回線自身のペースで送り出します。さらにマシンのランプ（HOME X・Y・Z、ENABLE、TOOL）を読み、0-10 V の速度信号と運転・方向ラインでスピンドルを制御し、セットアップ、ユーザー、ネットワーク、更新、マシン固有の設定のための Web パネル（8 言語）を備えます。通信の規則と設定はパソコン上でテストされ、ファームウェアは両方の基板で警告なしにビルドできます。**まだ基板に書き込んでも実行してもいません**。RS-232 変換、ランプ入力、スピンドルのドライブをまとめるキャリア基板は、紙の上の設計です。

---

> **安全性とセキュリティの現状：** これは安全システムではありません。配線を誤ったノードや、設定を誤ったスピンドルドライブは、マシンを動かしたり工具を回したりすることがあります。ノードはアクセストークンがなければ制御を拒み、プログラムがアームしている間だけマシン向けのバイトを受け付け、セッションが無言になるか接続が切れると停止します（キューを空にし、ステッピングドライバとスピンドルをオフ。保持している作業を完了させる任意の設定がオンの場合を除く）。マシンのリミットスイッチはランプ経由でしか見えず、独自の非常停止はまだありません（キャリア基板に押しボタン入力の場所があります）。スピンドルのドライブは商用電源電圧です。メーカーのマニュアルに従って配線・設定してください。パネルはマシンを動かすことはなく、プログラムがマシンをアームしている間は設定の保存を拒否します。このプロジェクトは独立しており、Bungard Elektronik が製造・推奨・サポートするものではありません。製品名は、対応するマシンとドライブを示すためだけに使っています。

## 📋 概要

| | |
|---|---|
| バージョン | `0.0.2` |
| 成熟度 | 足場段階 |
| テスト | ホストテスト 9 スイート（約 2200 項目） |
| 技術スタック | C++17、ESP-IDF 5.5、ESP32-S3 |
| 実機での動作 | いいえ：2 つのファームウェアイメージはビルドできますが、基板上ではまだ何も動かしていません |

## 🎯 概要説明

* **制御リンク**（TCP、既定のポートは 5800）：CRC 付きフレームのプロトコル、アクセストークンによるログイン、アーム、制御ユニット向けバイトのキュー、セッションが無言または切断されたときの停止。プログラムは間隔をあけたバイト列を送り、ノードの UART がそれを正確に回線へ出します。USB シリアルアダプターには高速でこれができません。
* **制御ユニットへのシリアルライン**：MAX3232 を介した 38400 ボー 8N1 と、ハンドシェイクの DTR、DCD、RI ライン。ピン、およびトランシーバーがラインを反転するかどうかは設定です。
* **ランプ**：HOME X、HOME Y、HOME Z、ENABLE、TOOL をバッファトランジスタ経由で読み、ステータスとパネルに表示します。要求により、軸が原点に着いた瞬間にノードが探索の残りを破棄できるため、プログラムがストリームの途中で原点ラインを読む必要がなくなります。
* **スピンドル**：0-10 V コンバーター向けの速度 PWM（既定はフル信号で 24000 rpm、Huanyang HY02D223B のようなインバーター向け）と正転・逆転の運転ライン。回転中や停止直後の方向切り替えはせず、マシンが停止するたびに止まります。
* **パネル**（HTTP と HTTPS、8 言語）：USB コンソールに表示されるコードによる初回セットアップ、ユーザー、イーサネットまたは Wi-Fi、制御リンク、RS-232 ピン、ランプ、スピンドル、ログ、2 つのファームウェアスロットへのアップロード更新とロールバック。イメージに秘密情報は組み込まれません。
* **自己保護**：タスクとメモリを見張るガード、メモリ不足時のパネルの入口のゲート、ノードに到達できるようになってからの新イメージの確定。
* **基板**：Waveshare ESP32-S3-ETH（W5500 イーサネット）と ESP32-S3-WROOM-1 N16R8（Wi-Fi）。それぞれ 1 つのイメージです。キャリア基板（100 x 100 mm）の設計は `docs/BOARD.md` にあります。
* **対象機械**：DOS ソフトウェア 4.6 のシリアルポート制御ユニットを持つ Bungard CCD MTC と CCD/2 MTC。ATC の 2 モデルは対象外で、XL モデルとそれ以降の機械は独自の最新制御ユニットを備えています。
* **自律的な完了**（既定はオフ、設定項目）：プログラムまたはサーバーがいなくなると、ノードはキューにあるバイト（間に入れられたスピンドル命令を含む）を最後まで実行し、その後スピンドルとドライバをオフにします。キューは外部 RAM にあり（最大 4 MB、数分間の動作）、時間制限、Abort、パネルの停止ボタンで終了します。**予定（未実装）**：物理的な停止のための押しボタン入力。

## 🧩 EH3D での役割

EH3D-NODE-BUNGARD は EH3D のプログラムとマシンの間の最終段です。現在は EH3D-DRILLPRO と EH3D-ROUTEPRO が接続し、その前に EH3D-SERVER が入る予定です。EH3D ファミリー最初のノードで、設定、パネル、更新の仕組みは今後のノードと共有できるように作られています（共通ベースは EH3D-COMMON にあります）。

## 📂 リポジトリ構成

```text
EH3D-NODE-BUNGARD/
├── core/         リンクのフレームとセッション規則、バイトキュー、設定：ハードウェア不要、パソコンでテスト
├── main/         ESP-IDF ファームウェア：UART、制御リンク、ランプ、スピンドル、ネットワーク、設定ストア、Web サーバー
├── panel/        ファームウェアに組み込まれる Web パネル
├── tests/        ホストテストとパネルの文言の確認
├── tools/        build_node.sh、pack_panel.py、check_sdkconfig.py、ci_validate.py、panel_mock.mjs
├── docs/         PROTOCOL、NODE_FIRMWARE、WIRING、BOARD
├── images/       バナー
└── eh3d.project.json
```

## 🛠️ ビルドと実行

ホストテストには C++17 コンパイラと CMake、ファームウェアには ESP-IDF 5.5（または Docker）が必要です。

```bash
build.bat            # the host tests (Windows)  -  ./build.sh on Linux
tools/build_node.sh s3-eth      # or s3-wifi: the firmware, in the ESP-IDF container
python -I tools/ci_validate.py
```

`tools/build_node.sh s3-eth`（または `s3-wifi`）で ESP-IDF コンテナ内にイメージをビルドします。Windows ではインストール済みの ESP-IDF 5.5.5 でも同じです。イメージ `dist/eh3d-node-bungard-<バージョン>-<基板>-complete.bin` は 1 つの基板専用です。`python -m esptool --chip esp32s3 -p COMx write_flash 0x0 <イメージ>` でアドレス 0 に書き込みます。その後、ノードが開く Wi-Fi `EH3D-SETUP-xxxxxx` に接続し、`http://192.168.4.1/` を開いて、ノードが USB コンソールに表示するセットアップコードを入力します。アクセストークン、ネットワーク、ピンはパネルから設定します。

## 🔗 関連プロジェクト

* [EH3D-DRILLPRO](https://github.com/JuanenRac/EH3D-DRILLPRO): 穴あけプログラム。`connection = node` でこのノード経由でマシンを動かします
* [EH3D-ROUTEPRO](https://github.com/JuanenRac/EH3D-ROUTEPRO): ルーティングプログラム。プロトコルライブラリと機械ファイルを共有します
* [EH3D-COMMON](https://github.com/JuanenRac/EH3D-COMMON): EH3D ファミリーの共通部分。ノードのファームウェアのベースはそこにあります
* [EH3D-SERVER](https://github.com/JuanenRac/EH3D-SERVER): EH3D ファミリーの中心。プログラムや他のクライアントの作業を、このノード経由で機械上で実行します

## 📚 ドキュメントとコミュニティ

* [制御リンク](docs/PROTOCOL.md): フレーム、ログイン、アーム、キュー、スピンドル、ランプ、マシンを止める規則.
* [ファームウェア](docs/NODE_FIRMWARE.md): セットアップ、設定、パネル、ロールバック付き更新、メモリ、自己保護.
* [配線](docs/WIRING.md): RS-232 ポート、ランプ、スピンドルのドライブ.
* [キャリア基板](docs/BOARD.md): 回路図を描くための設計.
* [変更履歴](CHANGELOG.md)
* [貢献方法](CONTRIBUTING.md)
* [セキュリティポリシー](SECURITY.md)
* [サポート](SUPPORT.md)

## 👤 作者

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 ライセンス

ソフトウェア：GPL-3.0-or-later。[LICENSE](LICENSE) を参照。スイートのハードウェア設計には CERN OHL-S を使います。
