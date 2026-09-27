# LightMediaBrowser

**LightMediaBrowser** は、Linux（およびQt 6サポート環境）向けの高速かつモダンなデスクトップ向け動画ライブラリ管理アプリケーションです。  
動画ファイル自体を変更（破壊）せず、管理データやメタデータをSQLiteデータベースに一元保存することで、大量のローカル動画ファイルをスマートに整理・閲覧できます。

---

## 🌟 主な機能 (Features)

- **フォルダのインポートと管理**: 動画が保存されているフォルダを複数登録・一括スキャンし、ライブラリへ自動的かつ効率的に取り込みます。
- **非同期サムネイル生成**: 快適なブラウジングのため、QtConcurrentやFFmpegを活用してバックグラウンドでサムネイル画像を自動生成・キャッシュします。
- **メタデータ管理 & 抽出**: 動画ファイルから解像度、コーデック、再生時間などの詳細情報を解析・表示します。
- **検索とフィルタリング**: キーワード検索、タグツリーによる階層的な絞り込み、お気に入り・再生回数管理など充実したライブラリ整理機能。
- **外部ファイル非依存**: 元の動画ファイルを直接書き換えないため、安全にコレクションを管理できます。

## 💾 保存データについて (Data Storage)

動画情報は SQLite データベースに、登録フォルダや画面設定はアプリ設定に保存されます。Linux では通常、データベースは `~/.local/share/LightMediaBrowser/database.sqlite`、設定は `~/.config/LightMediaBrowser/LightMediaBrowser.conf` にあります。

データベースが見つからない状態で起動すると、新しいライブラリとして扱い、以前の登録フォルダ一覧を空にします。画面設定はそのまま保持され、初回起動時の標準 `Videos` フォルダ登録も、登録フォルダ一覧を明示的に保持していない場合に限って行われます。

---

## 🛠 技術スタック (Tech Stack)

- **言語**: C++20
- **フレームワーク**: Qt 6 (Core, Gui, Widgets, Sql, Concurrent, Network)
- **データベース**: SQLite 3
- **ビルドシステム**: CMake (3.21以上)
- **マルチメディア処理** (任意): FFmpeg (libavformat, libavcodec, libavutil)

---

## 📦 必要要件 (Prerequisites)

ビルドと実行には以下のライブラリおよびツールが必要です：

- **CMake** (3.21以上)
- **C++20対応コンパイラ** (GCC / Clang など)
- **Qt 6** 開発パッケージ (`Qt6Core`, `Qt6Gui`, `Qt6Widgets`, `Qt6Sql`, `Qt6Concurrent`, `Qt6Network`)
- **SQLite 3** 開発パッケージ
- **FFmpeg** 開発パッケージ (`libavformat`, `libavcodec`, `libavutil`) ※高度なメタデータ・サムネイル抽出用（推奨）

### Ubuntu / Debian の場合のインストール例
```sh
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libsqlite3-dev libavformat-dev libavcodec-dev libavutil-dev pkg-config
```

---

## 🚀 ビルドと実行 (Build & Run)

ターミナルで以下のコマンドを実行します：

```sh
# リポジトリクローン後、ビルドディレクトリを作成
cmake -S . -B build

# コンパイルの実行
cmake --build build -j$(nproc)

# アプリケーションの起動
./build/lightmediabrowser
```

---

## 📂 プロジェクト構成 (Project Structure)

```text
lightmediabrowser/
├── CMakeLists.txt          # CMakeビルド設定
├── resources/              # アイコン・SQLマイグレーション等のリソース
│   ├── migrations/         # SQLite用スキーママイグレーション
│   └── resources.qrc       # Qtリソースファイル
├── src/                    # ソースコード
│   ├── database/           # SQLiteデータベース接続・操作
│   ├── domain/             # ドメインモデル (Video等)
│   ├── media/              # サムネイル生成・メディア処理
│   ├── scanner/            # メディアファイルスキャン
│   ├── ui/                 # Qt Widgets UI (MainWindow等)
│   └── main.cpp            # エントリーポイント
└── README.md
```

---

## 📄 ライセンス (License)

本プロジェクトのライセンスについては、プロジェクト内のファイルまたは作者へお問い合わせください。
