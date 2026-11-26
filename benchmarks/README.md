# Performance benchmark

This Linux benchmark records real LightMediaBrowser folder scanning, SQLite API timings, visible-window startup, and the main application process's peak RSS. It writes generated files only under `benchmarks/generated/`, which is ignored by Git.

## Requirements

- CMake and the normal Qt 6 build dependencies
- Python 3
- `ffmpeg` and `ffprobe`
- An X11 or XWayland display and `xwininfo`
- ImageMagick `import` only when capturing a screenshot

## Build and run

From the repository root:

```bash
cmake -S . -B build/performance-release \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DBUILD_BENCHMARKS=ON
cmake --build build/performance-release \
  --target lightmediabrowser lightmediabrowser_database_benchmark \
  --parallel

python3 -m benchmarks.run_benchmark \
  --app build/performance-release/lightmediabrowser \
  --database-benchmark build/performance-release/lightmediabrowser_database_benchmark \
  --counts 100,1000,10000 \
  --screenshot-path benchmarks/images/lightmediabrowser-benchmark.png
```

The runner creates a one-second 640x360 MP4 from the application icon, then hard-links it under unique names in 100 directories. If `benchmarks/generated/` already exists, it validates the requested file count and hard-link layout and refuses to replace or clean it. Use `--dataset-dir` to select a different, new output directory.

Each run gets unique `XDG_DATA_HOME`, `XDG_CONFIG_HOME`, and `XDG_CACHE_HOME` directories. The database benchmark refuses to open if the target database, WAL, or shared-memory file already exists. The app is then launched once against a newly created empty database and five times against the new populated benchmark database. These runs do not use the normal application data or settings directories.

## What is measured

- **Scan:** one elapsed-time measurement from the application's `MediaScanner` over all generated paths.
- **Database insertion:** transactional batches of 100, then 900, then 9,000 video records through `Database::upsertVideos`.
- **Filename search and count:** the actual `Database::searchVideosLight` and `Database::countVideosLight` methods, at 100, 1,000, and 10,000 rows. Each query matches one unique filename. After one warm-up, the reported value is the median of 21 calls.
- **Library page:** the median time to retrieve and materialize an unfiltered page of up to 100 rows, after one warm-up and 21 measured calls.
- **Startup:** time from immediately before process creation to its window becoming visible, so process creation time is included. The runner forces Qt's `xcb` platform so `xwininfo` can observe the window. It reports the first empty-database launch and five relaunch trials for empty and populated databases.
- **Memory:** Linux `VmHWM` (peak RSS) read two seconds after the window becomes visible. It covers the app process, not child processes such as FFmpeg. The OS page cache is not cleared; the populated-database trials share one generated app-data directory, so thumbnail caches can warm between launches.

Each run writes a JSON report, including detected CPU/OS, Qt and SQLite versions, and CMake build type, to a unique folder under `benchmarks/generated/runs/`. The folder also contains the isolated database and application logs.

## Limits

All 10,000 files are hard links to the same MP4 content. This tests directory enumeration and row-count scaling, not 10,000 distinct media files, video decoding, varied codec metadata, or sustained thumbnail generation. Search uses the current filename/notes substring query with a single matching filename; it does not measure every filter combination. Results depend on the machine, build, storage, display server, and cache state and should not be treated as cross-platform guarantees.
