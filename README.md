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

## Installation & Usage
1. Download or build the `VS.SCR` file.
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
  * `R` キー: 現在の動画の頭出し（最初から再生）
  * `S` キー: 消音のON/OFF切り替え

## インストールと使い方
1. ビルド済み、またはご自身でビルドした `VS.SCR` を任意のフォルダーに配置します。
2. `VS.SCR` を右クリックし、**「インストール」** を選択します。
3. Windowsのスクリーンセーバー設定画面から「設定」を開き、再生したい動画ファイルやURLを登録してください。

詳細な設定方法や注意事項については、公式マニュアルをご覧ください。
👉 **[オンラインマニュアル（日本語）](https://bearbeetle.github.io/bb-labo/vs_help_j.html)**

## ビルド方法（開発者向け）
1. Visual Studio 2022 で `VS.sln` を開きます。
2. ソリューションをビルドします。
3. 出力された `VS.SCR` を右クリックしてインストールしてください。

## ライセンス
このプロジェクトは MIT ライセンスの下で公開されています。詳細は [LICENSE](LICENSE) ファイルをご覧ください。  
Copyright (c) 1997-2026 BearBeetle.