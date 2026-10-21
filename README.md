# LightMediaBrowser

**LightMediaBrowser is a fast, lightweight, local-first application for organizing and browsing large collections of local video files.**

<img width="2560" height="1543" alt="Screen" src="https://github.com/user-attachments/assets/880624bd-9cc3-43e8-8ed3-412a1a702df6" />

It is designed specifically for **video management** — scanning, browsing, searching, tagging, and organizing your collection.

**It is not a video player.** Play your files with the external player you prefer, such as VLC, mpv, or MPC-HC/BE.

> **Status: 0.0.10-alpha**
>
> This is an early alpha release. The core workflow is usable, but features and internal architecture may change.

---

## Why LightMediaBrowser?

When you have a large collection of video files, simply opening them in a file manager can become difficult.

LightMediaBrowser provides a lightweight local library for your video collection:

* Scan multiple local folders
* Browse videos in a thumbnail-based library
* View three large, evenly spaced scene previews for the selected video
* Search and filter your collection
* Organize videos with tags
* View video metadata
* Mark favorites and track playback counts
* Open videos with your preferred external player

The application focuses on **managing your video collection**, rather than trying to replace your video player.

---

## Features

### Local Video Library

* Add and manage multiple folders
* Recursively scan folders for video files
* Browse videos in a thumbnail-based library
* Original video files are never modified by the application

### Fast Browsing

* Asynchronous directory scanning
* Asynchronous thumbnail generation
* Thumbnail caching
* Additional scene previews are generated only for the selected video and cached for later
* Database-backed library
* Designed to handle large local video collections

### Search & Filtering

* Search your video library
* Filter videos
* Find videos by filename and other stored information
* Filter by favorites, playback count, and tags
* Find same-size video candidates

Matching file sizes do not confirm that video contents are identical.

### Tags & Organization

* Create tag categories
* Assign tags to videos
* Use tags and notes to organize large collections
* Mark videos as favorites

### Video Metadata

LightMediaBrowser retrieves and displays information such as:

* Duration
* Resolution
* Video and audio codecs
* Other available media metadata

### External Video Players

LightMediaBrowser intentionally does **not** include a built-in video player.

When you want to watch a video, the application opens it using the video player associated with your operating system.

This keeps LightMediaBrowser focused on its main purpose:

> **Organize your videos. Use the player you already like.**

---

## Local-first & Non-destructive

LightMediaBrowser is designed to manage media stored locally. Library information is stored separately in a SQLite database, and your original video files remain in place.

It does **not**:

* Upload your videos to a cloud service
* Require an online account
* Modify your original video files
* Convert your videos
* Replace your existing video player

Your media files remain yours.

---

## Data Storage

Library information is stored in a SQLite database, and application preferences use Qt's standard settings system.

Use **Thumbnail Threads: Limited / Unlimited** in the toolbar to toggle the parallel thread limit for thumbnail generation. The limit is enabled by default to reduce CPU load. The button label shows the active setting, and your preference is restored the next time you launch the application.

On Linux, the default locations are:

```text
~/.local/share/LightMediaBrowser/database.sqlite
~/.config/LightMediaBrowser/LightMediaBrowser.conf
```

### About deleted or moved files

The library database is separate from the filesystem.

After a complete scan of an available registered folder, records and thumbnails for video files no longer found in that folder are deleted. If any part of a scan is unavailable or unreadable, missing-file cleanup is skipped to avoid deleting records based on incomplete results. If a registered folder itself is unavailable, its records and thumbnails are kept in case an external drive has only been temporarily disconnected.

---

## Project Status

**Current version: 0.0.10-alpha**

LightMediaBrowser is in early development.

The core video-management workflow is implemented:

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

The 0.0.x releases are intended to validate the application's basic design and workflow. Changes may include:

* Bugs and missing features
* UI changes
* Database schema, API, and internal architecture changes

Feedback and bug reports are welcome.

---

## What LightMediaBrowser is Not

LightMediaBrowser is not intended to be an all-in-one media application.

It is not intended to be:

* A video player
* A video editor or converter
* A media streaming server
* A cloud media service

Its purpose is simple: **manage and browse your local video collection quickly.**

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
* **Media processing:** FFmpeg command-line tools (`ffmpeg` and `ffprobe`)

---

## Requirements

To build and run LightMediaBrowser, you need:

* CMake 3.21 or later
* A C++20-compatible compiler such as GCC or Clang
* Qt 6 development packages
* SQLite 3 development packages
* `ffmpeg` and `ffprobe` command-line tools at runtime

### Ubuntu / Debian

```bash
sudo apt update

sudo apt install \
  build-essential \
  cmake \
  qt6-base-dev \
  libsqlite3-dev \
  ffmpeg
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

Thumbnail generation and video metadata retrieval require `ffmpeg` and `ffprobe` to be available at runtime.

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
├── README.md
└── README.jp.md
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

The roadmap may change as the 0.0.x series develops.

---

## Contributing

Bug reports, suggestions, and feedback are welcome. When reporting an issue, please include as much of the following information as possible:

* Steps to reproduce
* Expected and actual results
* Operating system
* Relevant application or console output

---

## License

The application is licensed under the MIT License; see [LICENSE](LICENSE) for details.

The application uses Qt, SQLite, and FFmpeg, which are subject to their respective license terms. Release AppImages include available copyright notices from the bundled Qt, SQLite, and FFmpeg packages under `usr/share/doc/lightmediabrowser/third-party/`. Before redistributing, review the bundled components and their license terms. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the dependency and packaging scope.
