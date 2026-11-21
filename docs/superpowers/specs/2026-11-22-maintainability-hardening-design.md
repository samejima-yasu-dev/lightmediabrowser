# LightMediaBrowser Maintainability Hardening Design

## Intent and Why

LightMediaBrowser is a working 0.0.x local video library. The next work increases maintainability and user-visible reliability without changing the core local-first workflow.

## Goals

1. Reduce `MainWindow` responsibility while preserving all existing behavior.
2. Add test coverage for database migrations and an offscreen UI smoke workflow.
3. Detect missing `ffmpeg`/`ffprobe` tools before metadata or thumbnail work and show a platform-specific installation hint.
4. Centralize application diagnostics in a rotating local log file.
5. Verify database/cache compatibility across supported schema versions.
6. Keep the existing cross-platform build and release pipeline green.

## Non-goals

- No cloud library.
- No built-in video player.
- No conversion or modification of original video files.
- No dependency framework migration.
- No replacement of Qt/SQLite/FFmpeg.

## Constraints

- C++20, Qt 6, CMake 3.21+.
- Existing public behavior and keyboard/navigation behavior remain unchanged.
- Database migrations must reject a schema newer than the application supports.
- Missing external tools must not crash the application or block UI startup.
- Logs stay local and avoid copying media filenames, tags, or file contents.
- All changes must pass the existing CTest suite plus new targeted tests.

## Architecture Decisions

- Extract tag UI/actions from `MainWindow` into a focused `TagManager` Qt object.
- Extract library filter state into a testable `LibraryFilterState` data structure; `MainWindow` remains responsible for Qt widgets and database paging.
- Add a `MediaToolLocator` responsible only for executable discovery and concise error text.
- Add a `DiagnosticLogger` that installs the Qt message handler and writes a bounded local log file.
- Keep database access in `Database`; add migration tests without changing the schema API.
- Keep packaging scripts unchanged unless diagnostics or tool detection reveals a packaging defect.

## Remaining Builder Decisions

- The exact log size/rotation constants are implementation details; choose 5 MB and 3 rotated files.
- TagManager may use modal dialogs while preserving the current copy and behavior.
- UI smoke tests use `QT_QPA_PLATFORM=offscreen` and test only Qt object wiring and listing/filter refresh.
