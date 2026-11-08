# LightMediaBrowser Installation

## English

- Windows package: Windows 10 or later, x64.
- macOS packages: macOS 12 Monterey or later; use the `arm64` package for
  Apple Silicon and the `x86_64` package for Intel Macs.
- Extract the Windows ZIP or copy `LightMediaBrowser.app` from the mounted DMG
  to Applications.
- Install FFmpeg separately. On macOS, Homebrew can install it with
  `brew install ffmpeg`. On Windows, add the folder containing `ffmpeg.exe`
  and `ffprobe.exe` to `PATH`.
- macOS applications launched from Finder may not inherit Homebrew's `PATH`.
  To launch from Terminal with the Homebrew tools available, run:

  ```sh
  PATH="$(brew --prefix)/bin:$PATH" \
    "/Applications/LightMediaBrowser.app/Contents/MacOS/LightMediaBrowser"
  ```

- These packages are not code-signed or notarized. Windows SmartScreen or
  macOS Gatekeeper may show a warning when launching for the first time.

## 日本語

- Windows版の対応環境はWindows 10以降（x64）です。
- macOS版の対応環境はmacOS 12 Monterey以降です。Apple Siliconでは
  `arm64`、Intel Macでは`x86_64`のパッケージを使用してください。
- WindowsではZIPを展開します。macOSではマウントしたDMGから
  `LightMediaBrowser.app`を「アプリケーション」へコピーしてください。
- FFmpegは別途インストールしてください。macOSではHomebrewの
  `brew install ffmpeg`で導入できます。Windowsでは`ffmpeg.exe`と
  `ffprobe.exe`があるフォルダーを`PATH`に追加してください。
- Finderから起動したmacOSアプリはHomebrewの`PATH`を引き継がない場合が
  あります。HomebrewのFFmpegを使ってTerminalから起動する場合は、次を
  実行してください。

  ```sh
  PATH="$(brew --prefix)/bin:$PATH" \
    "/Applications/LightMediaBrowser.app/Contents/MacOS/LightMediaBrowser"
  ```

- 配布パッケージはコード署名・公証されていません。初回起動時に
  Windows SmartScreenやmacOS Gatekeeperの警告が表示される場合があります。
