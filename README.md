# VideoScreen (ヴィデオ・スクリーン)

**English** | [日本語](#日本語-japanese)

VideoScreen is a video playback screensaver for Windows. 
Originally developed in 1997, it has been continuously maintained and was recently modernized (v3.00) to support **Windows 11** and modern multi-monitor environments using the Windows Media Foundation (MFPlay) API.

## Features
* **Video Playback:** Plays any video format supported by Windows Media Player.
* **Smart Multi-Monitor Support:** Automatically detects the primary monitor and plays the video there, while blacking out secondary monitors.
* **Customizable Playback:** Supports random playback, resume playback (start from where you left off), and mute options.
* **Dynamic Display:** The video position changes periodically to prevent screen burn-in. Custom scaling (50% to 300% or custom percentage) is available.
* **Interactive Controls:** While the screensaver is running, you can use keyboard shortcuts:
  * `N`: Skip to the next video
  * `R`: Restart the current video
  * `S`: Toggle mute on/off

## Download
You can download the latest pre-compiled package (`Vs300.zip`) from our official website:
👉 **[BB Programming Lab. Official Website](https://bearbeetle.github.io/bb-labo/)**

## Installation & Usage
1. Extract the downloaded zip file or build the `VS.SCR` file from source.
2. Copy `VS.SCR` to any folder (or `C:\Windows\System32`).
3. Right-click `VS.SCR` and select **"Install"**.
4. Configure your video list and preferences via the standard Windows Screen Saver settings.

For detailed instructions, please refer to the official manual:
👉 **[Online Manual (English)](https://bearbeetle.github.io/bb-labo/vs_help_e.html)**

## Build Instructions (For Developers)
1. Open `VS.sln` with Visual Studio 2022.
2. Build the solution.
3. The resulting `.scr` file can be installed as described above.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.  
Copyright (c) 1997-2026 BearBeetle.

---

# 日本語 (Japanese)

「ヴィデオ・スクリーン」は、登録した動画を次々と再生するWindows用のスクリーンセーバーです。
1997年に誕生した歴史あるソフトウェアですが、最新バージョン（v3.00）にて動画再生APIを刷新（Media Foundation / MFPlayを採用）し、**Windows 11** および最新のマルチモニター環境に完全対応しました。

## 主な機能
* **動画再生:** Windows Media Playerで再生可能な幅広い動画フォーマットに対応。
* **マルチモニター完全対応:** 複雑なディスプレイ設定（拡張画面など）でも自動的にメインディスプレイを正確に検出し、サブディスプレイを黒く塗りつぶしてメイン画面のみで動画を再生します。
* **柔軟な再生オプション:** ランダム再生、続き再生（レジューム機能）、消音設定などをサポート。
* **焼き付き防止:** 動画の表示位置が定期的に変化します。表示サイズも任意に設定可能です。
* **キーボード操作:** スクリーンセーバー起動中、以下のキー操作が可能です：
  * `N` キー: 次の動画へスキップ
  * `R` キー: 現在の動画を最初から再生
  * `S` キー: 消音（ミュート）のON/OFFを切り替え

## ダウンロード
コンパイル済みの実行ファイルパッケージ（`Vs300.zip`）は、以下の公式サイトからダウンロードできます：
👉 **[ＢＢ研究所 公式サイト](https://bearbeetle.github.io/bb-labo/)**

## インストールと使い方
1. ダウンロードしたzipファイルを解凍するか、ソースから `VS.SCR` をビルドします。
2. `VS.SCR` を任意のフォルダ（または `C:\Windows\System32`）にコピーします。
3. `VS.SCR` を右クリックして **「インストール」** を選択します。
4. Windows標準のスクリーンセーバー設定画面から、再生したい動画リストや各種設定を行います。

詳細な設定方法や仕様については、公式オンラインマニュアルをご参照ください：
👉 **[オンラインマニュアル (日本語)](https://bearbeetle.github.io/bb-labo/vs_help_j.html)**

## ビルド手順（開発者向け）
1. Visual Studio 2022 で `VS.sln` を開きます。
2. ソリューションをビルドします。
3. 生成された `.scr` ファイルを上記の手順でインストールできます。

## ライセンス
本プロジェクトは MIT ライセンスの下で公開されています。詳細については [LICENSE](LICENSE) ファイルをご覧ください。  
Copyright (c) 1997-2026 BearBeetle.
