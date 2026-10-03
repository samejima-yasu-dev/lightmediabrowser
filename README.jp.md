# LightMediaBrowser

**LightMediaBrowserは、大量のローカル動画ファイルを整理・閲覧するための、高速で軽量なローカルファーストアプリケーションです。**

<img width="2560" height="1543" alt="Screen" src="https://github.com/user-attachments/assets/880624bd-9cc3-43e8-8ed3-412a1a702df6" />

動画の**管理**に特化しており、動画コレクションのスキャン、閲覧、検索、タグ付け、整理を行えます。

**動画プレイヤーではありません。**

動画の再生には、VLC、mpv、MPC-HC/BEなど、お好みの動画プレイヤーを使用してください。

> **Status: 0.0.2-alpha**
>
> 初期アルファ版です。基本的なワークフローは利用できますが、今後機能や内部構造が変更される可能性があります。

---

## なぜLightMediaBrowser？

大量の動画ファイルを持っていると、単純にファイルマネージャーから探すだけでは管理が難しくなることがあります。

LightMediaBrowserは、動画コレクションのための軽量なローカルライブラリを提供します。

* 複数のローカルフォルダをスキャン
* サムネイル付きで動画を閲覧
* 動画コレクションを検索・フィルタリング
* タグによる動画の整理
* 動画のメタデータを表示
* お気に入り登録
* 再生回数を記録
* 好みの外部動画プレイヤーで動画を開く

LightMediaBrowserは、動画プレイヤーそのものを置き換えるのではなく、**動画コレクションの管理**に重点を置いています。

---

## Features

### ローカル動画ライブラリ

* 複数のフォルダを追加・管理
* ディレクトリを再帰的にスキャンして動画ファイルを検出
* サムネイルベースのライブラリで動画を閲覧
* アプリケーションが元の動画ファイルを変更することはありません

### 高速なブラウジング

* 非同期ディレクトリスキャン
* 非同期サムネイル生成
* サムネイルキャッシュ
* データベースによるライブラリ管理
* 大規模なローカル動画コレクションを想定した設計

### 検索・フィルタリング

* 動画ライブラリを検索
* 動画をフィルタリング
* ファイル名などの保存された情報から動画を検索
* タグを使って動画を整理

### タグ・整理

* タグカテゴリを作成
* 動画にタグを割り当て
* タグを使って大量の動画を整理
* 動画にメモを追加
* 動画をお気に入りに登録

### 動画メタデータ

LightMediaBrowserは、以下のような情報を取得・表示します。

* 再生時間
* 解像度
* ビデオコーデック
* その他取得可能なメディアメタデータ

### 外部動画プレイヤー

LightMediaBrowserには、意図的に**内蔵動画プレイヤーを搭載していません**。

動画を再生するときは、OSに関連付けられている動画プレイヤーを使用します。

これにより、LightMediaBrowserは本来の目的に集中できます。

> **動画は整理する。再生は、いつものプレイヤーで。**

---

## Local-first & Non-destructive

LightMediaBrowserは、ローカルに保存されたメディアコレクションだけで動作することを基本設計としています。

元の動画ファイルは、そのまま元の場所に残ります。

アプリケーションは、ライブラリ情報をSQLiteデータベースに別途保存します。

LightMediaBrowserは以下のことを行いません。

* 動画をクラウドサービスへアップロード
* オンラインアカウントを要求
* 元の動画ファイルを変更
* 動画を変換
* 既存の動画プレイヤーを置き換える

**あなたのメディアファイルは、あなたのものです。**

---

## Data Storage

LightMediaBrowserは、ライブラリ情報をSQLiteデータベースに保存し、アプリケーション設定にはQtの標準設定システムを使用します。

Linuxでは、デフォルトで以下の場所に保存されます。

```text
~/.local/share/LightMediaBrowser/database.sqlite
~/.config/LightMediaBrowser/LightMediaBrowser.conf
```

アプリケーションが元の動画ファイルを変更することはありません。

### 削除・移動されたファイルについて

ライブラリデータベースは、ファイルシステムとは別に管理されています。

LightMediaBrowserの外部で動画ファイルが移動または削除された場合、ライブラリを更新するまで、データベースに以前のファイル情報が残る場合があります。

この動作は、今後のリリースで改善される可能性があります。

---

## Project Status

**Current version: 0.0.2-alpha**

LightMediaBrowserは現在、初期開発段階です。

動画管理に必要な基本的なワークフローはすでに実装されています。

```text
フォルダを追加
    ↓
動画をスキャン
    ↓
サムネイルを生成
    ↓
ライブラリを閲覧
    ↓
検索 / フィルター
    ↓
タグ付け / 整理
    ↓
好みのプレイヤーで開く
```

0.0.xシリーズは、アプリケーションの基本設計とワークフローを検証するための開発版です。

以下のような変更が発生する可能性があります。

* バグ
* 未実装の機能
* UIの変更
* データベース・スキーマの変更
* APIや内部アーキテクチャの変更

フィードバックやバグ報告を歓迎します。

---

## What LightMediaBrowser is Not

LightMediaBrowserは、すべてのメディア機能を1つにまとめたオールインワンアプリケーションを目指していません。

以下を目的としたアプリケーションではありません。

* 動画プレイヤー
* 動画編集ソフト
* 動画変換ソフト
* メディアストリーミングサーバー
* クラウドメディアサービス

目的はシンプルです。

> **ローカルにある動画コレクションを、素早く管理・閲覧する。**

---

## Tech Stack

* **言語:** C++20
* **フレームワーク:** Qt 6

  * Qt Core
  * Qt GUI
  * Qt Widgets
  * Qt SQL
  * Qt Concurrent
  * Qt Network
* **データベース:** SQLite 3
* **ビルドシステム:** CMake 3.21+
* **メディア処理:** FFmpeg

---

## Requirements

LightMediaBrowserをビルド・実行するには、以下が必要です。

* CMake 3.21以降
* GCCやClangなどのC++20対応コンパイラ
* Qt 6開発パッケージ
* SQLite 3開発パッケージ
* FFmpeg開発ライブラリ

### Ubuntu / Debian

```bash
sudo apt update

sudo apt install \
  build-essential \
  cmake \
  qt6-base-dev \
  libsqlite3-dev \
  libavformat-dev \
  libavcodec-dev \
  libavutil-dev \
  pkg-config
```

---

## Build

リポジトリをCloneします。

```bash
git clone https://github.com/samejima-yasu-dev/lightmediabrowser.git
cd lightmediabrowser
```

プロジェクトを設定します。

```bash
cmake -S . -B build
```

ビルドします。

```bash
cmake --build build -j$(nproc)
```

実行します。

```bash
./build/lightmediabrowser
```

---

## Project Structure

```text
lightmediabrowser/
├── CMakeLists.txt
├── resources/
│   ├── migrations/
│   └── resources.qrc
├── src/
│   ├── database/
│   ├── domain/
│   ├── media/
│   ├── scanner/
│   ├── ui/
│   └── main.cpp
└── README.md
```

### Main Components

| ディレクトリ       | 説明                          |
| ------------ | --------------------------- |
| `database/`  | SQLiteデータベースへのアクセスと操作       |
| `domain/`    | 動画データなどのドメインモデル             |
| `media/`     | サムネイル生成とメディア処理              |
| `scanner/`   | ローカルメディアファイルのスキャン           |
| `ui/`        | Qt Widgetsによるユーザーインターフェース   |
| `resources/` | アプリケーションリソースとデータベースマイグレーション |

---

## Design Goals

LightMediaBrowserは、いくつかのシンプルな考え方を中心に設計されています。

### Local-first

メディアライブラリはあなたのマシン上に保存されます。

### Non-destructive

元のメディアファイルは変更しません。

### Fast browsing

スキャンとサムネイル生成は非同期で行われ、サムネイルはキャッシュされるため、次回以降の閲覧を高速化できます。

### Lightweight

あらゆるメディア機能を搭載するのではなく、動画管理に必要な機能に集中します。

### Use the player you like

動画再生は外部プレイヤーに任せます。

普段使っているプレイヤーや、その設定をそのまま使い続けることができます。

---

## Roadmap

今後、以下のような改善を予定しています。

* 移動・削除されたファイルのより良い処理
* 非常に大規模なライブラリでの検索性能向上
* より柔軟なタグ管理
* ライブラリのメンテナンス機能
* クロスプラットフォーム向けパッケージングの改善
* 追加のフィルタリング・整理機能

0.0.xの開発段階では、ロードマップは柔軟に変更される予定です。

---

## Contributing

バグ報告、提案、フィードバックを歓迎します。

問題を見つけた場合は、Issueを作成する際に以下の情報を含めてください。

* 何をしようとしていたか
