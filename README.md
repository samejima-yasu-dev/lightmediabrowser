# LightMediaBrowser

**LightMediaBrowser is a fast, lightweight, local-first application for organizing and browsing large collections of local video files.**

<img width="2560" height="1543" alt="Screen" src="https://github.com/user-attachments/assets/880624bd-9cc3-43e8-8ed3-412a1a702df6" />

It is designed specifically for **video management** — scanning, browsing, searching, tagging, and organizing your collection.

**It is not a video player.**
Use the video player you prefer, such as VLC, mpv, or MPC-HC/BE, to play your files.

> **Status: 0.0.1-alpha**
>
> This is an early alpha release. The core workflow is usable, but features and internal architecture may change.

---

## Why LightMediaBrowser?

When you have a large collection of video files, simply opening them in a file manager can become difficult.

LightMediaBrowser provides a lightweight local library for your videos:

* Scan multiple local folders
* Browse videos with thumbnails
* Search and filter your collection
* Organize videos with tags
* View video metadata
* Mark videos as favorites
* Track playback counts
* Open videos with your preferred external player

The application focuses on **managing your video collection**, rather than trying to replace your video player.

---

## Features

### Local Video Library

* Add and manage multiple folders
* Recursively scan directories for video files
* Browse videos in a thumbnail-based library
* Original video files are never modified by the application

### Fast Browsing

* Asynchronous directory scanning
* Asynchronous thumbnail generation
* Thumbnail caching
* Database-backed library
* Designed to handle large local video collections

### Search & Filtering

* Search your video library
* Filter videos
* Find videos by filename and other stored information
* Organize videos with tags

### Tags & Organization

* Create tag categories
* Assign tags to videos
* Use tags to organize large collections
* Add notes to videos
* Mark videos as favorites

### Video Metadata

LightMediaBrowser extracts and displays information such as:

* Duration
* Resolution
* Video codec
* Other available media metadata

### External Video Players

LightMediaBrowser intentionally does **not** include a built-in video player.

When you want to watch a video, the application opens it using the video player associated with your operating system.

This keeps LightMediaBrowser focused on its main purpose:

> **Organize your videos. Use the player you already like.**

---

## Local-first & Non-destructive

LightMediaBrowser is designed to work entirely with your local media collection.

Your original video files remain where they are.

The application stores library information separately in a SQLite database.

It does **not**:

* Upload your videos to a cloud service
* Require an online account
* Modify your original video files
* Convert your videos
* Replace your existing video player

Your media files remain yours.

---

## Data Storage

LightMediaBrowser stores library information in a SQLite database and application settings using Qt's standard settings system.

On Linux, the default locations are:

```text
~/.local/share/LightMediaBrowser/database.sqlite
~/.config/LightMediaBrowser/LightMediaBrowser.conf
```

The application does not modify the original video files.

### About deleted or moved files

The library database is separate from the filesystem.

If a video file is moved or deleted outside LightMediaBrowser, the database may retain information about the previous file until the library is updated.

This behavior may be improved in a future release.

---

## Project Status

**Current version: 0.0.1-alpha**

LightMediaBrowser is currently in early development.

The main video-management workflow is already implemented:

```text
Add folder
    ↓
Scan videos
    ↓
Generate thumbnails
    ↓
Browse library
    ↓
Search / Filter
    ↓
Tag / Organize
    ↓
Open with your preferred player
```

The 0.0.x releases are intended to validate the application's basic design and workflow.

Expect:

* Bugs
* Missing features
* UI changes
* Database/schema changes
* API and internal architecture changes

Feedback and bug reports are welcome.

---

## What LightMediaBrowser is Not

LightMediaBrowser intentionally does not try to be an all-in-one media application.

It is not intended to be:

* A video player
* A video editor
* A video converter
* A media streaming server
* A cloud media service

Its primary purpose is simple:

> **Manage and browse your local video collection quickly.**

---

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

---

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

---

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

| Directory    | Description                                   |
| ------------ | --------------------------------------------- |
| `database/`  | SQLite database access and operations         |
| `domain/`    | Domain models such as video data              |
| `media/`     | Thumbnail generation and media processing     |
| `scanner/`   | Local media file scanning                     |
| `ui/`        | Qt Widgets user interface                     |
| `resources/` | Application resources and database migrations |

---

## Design Goals

LightMediaBrowser is built around a few simple ideas.

### Local-first

Your media library stays on your machine.

### Non-destructive

Original media files are not modified.

### Fast browsing

Scanning and thumbnail generation are performed asynchronously, and thumbnails are cached for subsequent browsing.

### Lightweight

The application is focused on video management rather than trying to provide every possible media feature.

### Use the player you like

Video playback is delegated to an external player.

This allows users to keep using the player and configuration they already prefer.

---

## Roadmap

Possible future improvements include:

* Better handling of moved and deleted files
* Improved search performance for very large libraries
* More flexible tag management
* Library maintenance tools
* Improved cross-platform packaging
* Additional filtering and organization features

The roadmap is intentionally flexible during the 0.0.x development stage.

---

## Contributing

Bug reports, suggestions, and feedback are welcome.

If you find a problem, please open an issue with:

* What you were trying to do
* What you expected to happen
* What actually happened
* Your operating system
* Relevant application or console output

---

## License

MIT License.

See [LICENSE](LICENSE) for details.
