# LightMediaBrowser

A fast, lightweight, local-first media browser for managing and browsing large collections of local video files.

LightMediaBrowser is built with **C++20 and Qt 6**. It keeps your original media files untouched and stores library information and metadata separately in a SQLite database.

## Demo

> Demo videos coming soon.

<!--
<img width="2560" height="1543" alt="Screen" src="https://github.com/user-attachments/assets/3ae520c6-77af-4cc9-bb7a-9b75858520f1" />
-->

## Features

* **Local video library**

  * Import and manage multiple folders
  * Scan local directories for video files
  * Keep your original media files untouched

* **Asynchronous thumbnail generation**

  * Generate video thumbnails in the background
  * Use FFmpeg for thumbnail extraction
  * Cache generated thumbnails for faster browsing

* **Media metadata**

  * Extract and display video information such as:

    * Duration
    * Resolution
    * Video codec
    * Other media metadata

* **Search and filtering**

  * Search your media library
  * Filter videos to quickly find what you are looking for
  * Organize videos with tags

* **Local-first storage**

  * Library data is stored locally
  * No cloud service or external media management service is required
  * Original video files are never modified by the application

## Data Storage

LightMediaBrowser stores library information in a SQLite database and application settings using Qt's standard settings system.

On Linux, the default locations are:

```text
~/.local/share/LightMediaBrowser/database.sqlite
~/.config/LightMediaBrowser/LightMediaBrowser.conf
```

The application does not modify the original video files.

## Tech Stack

* **Language:** C++20
* **Framework:** Qt 6

  * Qt Core
  * Qt GUI
  * Qt Widgets
  * Qt SQL
  * Qt Concurrent
  * Qt Network
* **Database:** SQLite 3
* **Build system:** CMake 3.21+
* **Media processing:** FFmpeg

## Requirements

To build and run LightMediaBrowser, you need:

* CMake 3.21 or later
* A C++20-compatible compiler such as GCC or Clang
* Qt 6 development packages
* SQLite 3 development packages
* FFmpeg development libraries

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

## Build

Clone the repository:

```bash
git clone https://github.com/samejima-yasu-dev/lightmediabrowser.git
cd lightmediabrowser
```

Configure the project:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build -j$(nproc)
```

Run:

```bash
./build/lightmediabrowser
```

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

| Directory    | Description                                   |
| ------------ | --------------------------------------------- |
| `database/`  | SQLite database access and operations         |
| `domain/`    | Domain models such as video data              |
| `media/`     | Thumbnail generation and media processing     |
| `scanner/`   | Local media file scanning                     |
| `ui/`        | Qt Widgets user interface                     |
| `resources/` | Application resources and database migrations |

## Design Goals

LightMediaBrowser is designed around a few simple ideas:

* **Local-first** — your media library stays on your machine.
* **Non-destructive** — original media files are not modified.
* **Fast browsing** — thumbnails are generated asynchronously and cached.
* **Simple architecture** — keep the application lightweight and easy to understand.
* **Practical media management** — focus on browsing, searching, and organizing local video collections.

## Project Status

LightMediaBrowser is currently a personal project under active development.

The application is primarily designed for local use, but the project is also published on GitHub as an example of a small desktop application built with modern C++ and Qt.

Features and internal architecture may change as the project evolves.

## License

MIT License.

See [LICENSE](LICENSE) for details.
