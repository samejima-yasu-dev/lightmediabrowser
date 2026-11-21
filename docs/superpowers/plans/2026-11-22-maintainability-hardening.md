# Maintainability and Reliability Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or superpowers:subagent-driven-development to implement this plan task-by-task.

**Goal:** Harden LightMediaBrowser by splitting the largest UI responsibilities and expanding migration, UI, external-tool, and diagnostics coverage.

**Architecture:** Preserve the existing Qt/SQLite design. Add focused UI and diagnostics helpers, keep database ownership in `Database`, and verify behavior with QtTest and CTest.

**Tech Stack:** C++20, Qt 6 Widgets/Core/Sql/Concurrent/Test, SQLite, CMake 3.21+, FFmpeg/FFprobe.

**Spec:** `docs/superpowers/specs/2026-11-22-maintainability-hardening-design.md`

## Global Constraints

- Preserve C++20 and Qt 6 APIs.
- Preserve external-player, local-only, and non-destructive behavior.
- Keep database schema and cache names backward-compatible.
- UI smoke tests must run with `QT_QPA_PLATFORM=offscreen`.
- Missing tools must produce actionable local errors, not exceptions or crashes.
- Run the smallest relevant test first, then the complete CTest suite.

## Review Focus

- A scan interrupted by a disconnected folder must not remove existing records. Test by opening the existing MediaScanner and MainWindow cleanup paths.
- A database from the future must remain unopened with a clear error. Test migration version 6 rejection.
- Missing FFmpeg/FFprobe must not prevent application startup. Test `MediaToolLocator` on an empty path and `main` startup remains reachable.
- A log line containing Qt file paths must remain local and be bounded. Test log path creation and one-file writes.
- Offscreen UI startup must not require a display or leave modal dialogs open. Test `MainWindow` object names and list refresh.

---

### Task 1: Split Tag UI from MainWindow

**Files:**
- Create: `src/ui/TagManager.h`, `src/ui/TagManager.cpp`
- Modify: `src/ui/MainWindow.h`, `src/ui/MainWindow.cpp`, `CMakeLists.txt`
- Test: `tests/TagManagerTest.cpp`

**Interfaces:**
- `TagManager(Database *database, QTreeWidget *tree, QWidget *parent = nullptr)`
- `void refresh()`
- `void showContextMenu(const QPoint &position)`
- `void assignToVideo(qint64 videoId, const QString &videoName)`
- signals `tagSelectionChanged(qint64 tagId)` and `libraryChanged()`

- [ ] **Step 1: Add failing test** for refresh, category/tag creation, tag selection, and deletion using an isolated Qt test database.
- [ ] **Step 2: Run `ctest --test-dir build -R tag_manager_test --output-on-failure` and confirm it fails because `TagManager` is missing.**
- [ ] **Step 3: Implement `TagManager` and move tag tree refresh/context/assignment code from `MainWindow` into it.**
- [ ] **Step 4: Wire `MainWindow::refreshTags`, `showTagContextMenu`, `selectTag`, and `assignTagsToVideo` to `TagManager` without changing behavior.**
- [ ] **Step 5: Run the TagManager test and full CTest suite.**

### Task 2: Extract Testable Library Filter State

**Files:**
- Create: `src/ui/LibraryFilterState.h`
- Modify: `src/ui/MainWindow.cpp`, `CMakeLists.txt`
- Test: `tests/LibraryFilterStateTest.cpp`

**Interfaces:**
- `struct LibraryFilterState { QString query; qint64 tagId; bool favoritesOnly; bool duplicatesOnly; int minPlayCount; QString sortKey; }`
- `QString normalizedSortKey() const`

- [ ] **Step 1: Add failing test** for default values, sort-key normalization, and state copying.
- [ ] **Step 2: Run `ctest --test-dir build -R library_filter_state_test --output-on-failure` and confirm the missing-header failure.**
- [ ] **Step 3: Implement `LibraryFilterState` and make `refreshVideos/loadMoreVideos` consume one state snapshot per refresh.**
- [ ] **Step 4: Run the targeted test and full CTest suite.**

### Task 3: Add MainWindow Offscreen UI Smoke Coverage

**Files:**
- Create: `tests/MainWindowSmokeTest.cpp`
- Modify: `src/ui/MainWindow.cpp`, `src/ui/MainWindow.h`, `CMakeLists.txt`
- Test: `tests/MainWindowSmokeTest.cpp`

- [ ] **Step 1: Add object names** to the search, folder, filter, sort, play-count, tag tree, and video list widgets.
- [ ] **Step 2: Add a failing offscreen smoke test** that constructs `MainWindow`, finds required controls, changes search text, changes favorite filter, and waits for the list refresh timer.
- [ ] **Step 3: Run `ctest --test-dir build -R mainwindow_smoke_test --output-on-failure` and confirm red before implementation.**
- [ ] **Step 4: Make only the smallest UI wiring changes needed for the test; preserve existing behavior.**
- [ ] **Step 5: Run the smoke test and full CTest suite.**

### Task 4: Detect and Report Missing FFmpeg Tools

**Files:**
- Create: `src/media/MediaToolLocator.h`, `src/media/MediaToolLocator.cpp`
- Modify: `src/media/ThumbnailService.cpp`, `src/media/VideoMetadataProbe.cpp`, `src/ui/MainWindow.cpp`, `CMakeLists.txt`
- Test: `tests/MediaToolLocatorTest.cpp`

**Interfaces:**
- `struct MediaToolAvailability { bool ffmpegAvailable; bool ffprobeAvailable; QString ffmpegPath; QString ffprobePath; }`
- `MediaToolAvailability locateMediaTools()`
- `QString missingMediaToolsMessage(const MediaToolAvailability &availability)`

- [ ] **Step 1: Add failing tests** for empty executable names, Linux/macOS/Windows guidance, and both tools present.
- [ ] **Step 2: Run `ctest --test-dir build -R media_tool_locator_test --output-on-failure` and confirm missing implementation failure.**
- [ ] **Step 3: Implement tool discovery and startup/status use for clear user-facing errors.**
- [ ] **Step 4: Update FFmpeg process startup paths to use discovered executables while retaining `ffmpeg`/`ffprobe` fallback behavior for bundled Linux packages.**
- [ ] **Step 5: Run targeted and full tests.**

### Task 5: Add Local Rotating Diagnostics

**Files:**
- Create: `src/diagnostics/DiagnosticLogger.h`, `src/diagnostics/DiagnosticLogger.cpp`
- Modify: `src/main.cpp`, `CMakeLists.txt`, `.gitignore` if needed
- Test: `tests/DiagnosticLoggerTest.cpp`

**Interfaces:**
- `bool installDiagnosticLogger(QString *logPath = nullptr, QString *error = nullptr)`
- `QString diagnosticLogPath()`

- [ ] **Step 1: Add failing tests** for AppData log path, log file creation, and bounded rotation constants.
- [ ] **Step 2: Run `ctest --test-dir build -R diagnostic_logger_test --output-on-failure` and confirm missing implementation failure.**
- [ ] **Step 3: Implement Qt message-handler installation, 5 MB max and 3 backups, and startup log header.**
- [ ] **Step 4: Record only local application diagnostics; do not include database contents or media payload.**
- [ ] **Step 5: Run targeted and full tests.**

### Task 6: Expand Database Migration and Interrupted-Scan Coverage

**Files:**
- Modify: `tests/DatabaseTest.cpp`, `tests/MediaScannerTest.cpp`, `CMakeLists.txt`

- [ ] **Step 1: Add migration test setup that creates a temporary database, writes `PRAGMA user_version = 6`, and confirms `Database::open` rejects it with the current support message.**
- [ ] **Step 2: Add migration test for initial schema through current version and key tables/foreign-key/unique constraints.**
- [ ] **Step 3: Add MediaScanner test for unreadable/missing root or directory leaves `complete=false` and emits no cleanup-confirming result.**
- [ ] **Step 4: Run targeted tests to verify red/green behavior.**
- [ ] **Step 5: Fix any migration or scanner defect found and rerun full CTest suite.**

### Task 7: Finalize Compatibility Documentation and Pipeline Validation

**Files:**
- Modify: `README.md`, `README.jp.md`, `docs/INSTALLATION.md`, optionally existing workflow files if validation exposes defects

- [ ] **Step 1: Document FFmpeg detection, diagnostic log location, and schema/cache compatibility behavior.**
- [ ] **Step 2: Run `cmake --build build --parallel`, `ctest --test-dir build --output-on-failure`, and release script syntax checks available locally.**
- [ ] **Step 3: Correct only defects directly exposed by these checks.**
- [ ] **Step 4: Review the final diff for unrelated changes and confirm git status is intentional.**
- [ ] **Step 5: Report per-stage completion and remaining platform-only validation.**
