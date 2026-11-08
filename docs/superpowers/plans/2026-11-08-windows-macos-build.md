# Windows and macOS Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build downloadable Windows and macOS packages on every push and pull request, and publish those packages with the existing Linux AppImage on `v*` releases.

**Architecture:** Keep the existing Qt/CMake application and deploy its Qt runtime and SQL plugin into platform packages. Use native Windows and macOS runners to build, test, package, and upload artifacts; keep FFmpeg external on Windows/macOS and preserve the existing Linux AppImage packaging. Release publication runs once after all tagged builds complete.

**Tech Stack:** C++20, CMake 3.21+, Qt 6, GitHub Actions, `windeployqt`, `macdeployqt`, PowerShell, shell, CTest.

**Spec:** [docs/superpowers/specs/2026-11-08-windows-macos-build-design.md](../specs/2026-11-08-windows-macos-build-design.md)

## Global Constraints

- Minimum supported versions: Windows 10 and macOS 12 Monterey.
- Windows package: x64 ZIP package.
- macOS packages: separate Apple Silicon (arm64) and Intel (x86_64) DMG packages.
- Regular push and pull request: compile and test on Windows and macOS, then upload build packages as Actions artifacts.
- Version tag matching `v*`: also publish the platform packages to the GitHub Release, preserving the Linux AppImage.
- Require users to install `ffmpeg` and `ffprobe` separately and put them on `PATH`; do not bundle them with Windows or macOS packages.
- Do not add code-signing secrets; document unsigned package launch warnings.
- Use native GitHub Actions runners for each target OS rather than cross compiling.
- Include applicable Qt and SQLite license texts and notices for shipped components.

## Review Focus

- macOS packages must contain only the intended architecture and target macOS 12; inspect `lipo -archs` and the deployment target in the built app.
- Windows and macOS packages must include Qt runtime libraries and the `QSQLITE` plugin; verify deployment output and run the existing database test on each runner.
- FFmpeg must not accidentally enter Windows/macOS packages; inspect package contents and document that both tools must be on `PATH`.
- A push or pull request must upload Actions artifacts but must not publish a GitHub Release; validate workflow event conditions and release job dependencies.
- Unsigned packages must clearly explain first-launch warnings and platform minimums; verify the README and packaged notices contain this user-facing information.

---

### Task 1: Make CMake targets portable to Windows and macOS

**Files:**
- Modify: `CMakeLists.txt`
- Test: existing `tests/DatabaseTest.cpp`

**Interfaces:**
- Consumes: Existing Qt 6 Core, Gui, Widgets, Sql, Concurrent, Network, and Test components.
- Produces: `lightmediabrowser` as a normal Windows executable and a macOS `.app` bundle; CMake no longer requires a separately linked SQLite C library because application and test code access SQLite through Qt SQL only.

- [ ] **Step 1: Verify SQLite C API is not used**

Search `src/` and `tests/` for direct `sqlite3_*` calls or `sqlite3.h` includes. Confirm all database access goes through `QSqlDatabase`/Qt SQL.

- [ ] **Step 2: Remove the redundant SQLite C-library dependency**

Remove `find_package(SQLite3 REQUIRED)` and `SQLite::SQLite3` from application and test link lists in `CMakeLists.txt`. Keep Qt 6 Sql and the runtime `QSQLITE` plugin; do not change database behavior.

- [ ] **Step 3: Configure the macOS application bundle**

Set `MACOSX_BUNDLE` for `lightmediabrowser` when `APPLE` is true, set its macOS output name to `LightMediaBrowser`, and install the bundle with a bundle destination. Preserve the existing Linux runtime, desktop-file, icon, and documentation install behavior.

- [ ] **Step 4: Configure, build, and run tests locally**

Run: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
Run: `cmake --build build --parallel`
Run: `ctest --test-dir build --output-on-failure`
Expected: configure and build succeed; all existing tests pass, including `database_test` using Qt's `QSQLITE` driver.

---

### Task 2: Create deployable Windows and macOS packages

**Files:**
- Create: `tools/package-windows.ps1`
- Create: `tools/package-macos.sh`

**Interfaces:**
- Consumes: the CMake build output from Task 1 and the Qt deployment tools installed by the runner.
- Produces:
  - `LightMediaBrowser-windows-x64.zip`
  - `LightMediaBrowser-macos-arm64.dmg`
  - `LightMediaBrowser-macos-x86_64.dmg`

- [ ] **Step 1: Add Windows deployment and package script**

Use `windeployqt` to deploy Qt runtime libraries and the `QSQLITE` driver beside `lightmediabrowser.exe`. Add `LICENSE`, `THIRD_PARTY_NOTICES.md`, and the applicable Qt/SQLite license texts from the deployed runtime distribution; do not add `ffmpeg` or `ffprobe`; create `LightMediaBrowser-windows-x64.zip`.

- [ ] **Step 2: Add macOS deployment and package script**

Use `macdeployqt` to deploy Qt runtime libraries and the `QSQLITE` driver into `LightMediaBrowser.app`; set `CMAKE_OSX_DEPLOYMENT_TARGET=12.0` and `CMAKE_OSX_ARCHITECTURES` to the runner architecture when configuring each native build. Add `LICENSE`, `THIRD_PARTY_NOTICES.md`, and the applicable Qt/SQLite license texts from the deployed runtime distribution to the DMG contents; do not add `ffmpeg` or `ffprobe`; create the architecture-specific DMG filename.

- [ ] **Step 3: Validate package contents on native runners**

In the Windows runner, assert the ZIP contains `lightmediabrowser.exe`, Qt runtime DLLs, the `sqldrivers/qsqlite` plugin, project and third-party license texts, and contains neither `ffmpeg` nor `ffprobe`.

In each macOS runner, assert the DMG contains `LightMediaBrowser.app`, the Qt SQL driver, project and third-party license texts, and no FFmpeg executable; verify `lipo -archs` matches the artifact architecture and `otool -l` reports a macOS 12 deployment target.

---

### Task 3: Build, test, upload, and publish through GitHub Actions

**Files:**
- Modify: `.github/workflows/release.yml`

**Interfaces:**
- Consumes: package scripts and filenames from Task 2.
- Produces: Actions artifacts for Windows x64, macOS arm64, and macOS x86_64; one GitHub Release containing those three packages and the existing Linux AppImage on `v*` tags.

- [ ] **Step 1: Extend workflow events**

Run Windows/macOS build jobs on `push` and `pull_request`; retain `push` tags matching `v*` for release builds. Keep the Linux AppImage job restricted to `v*` tags.

- [ ] **Step 2: Add native Windows build, test, and artifact job**

Install the project's Qt 6 and CMake dependencies on a native Windows x64 runner using a Qt/toolchain version that supports Windows 10, configure/build with C++20, run `ctest --test-dir build --output-on-failure`, invoke `tools/package-windows.ps1`, and upload `LightMediaBrowser-windows-x64.zip` with `actions/upload-artifact@v4`.

- [ ] **Step 3: Add native macOS build, test, and artifact jobs**

Run separate native arm64 and x86_64 jobs on supported macOS 12+ runners. Configure with `CMAKE_OSX_DEPLOYMENT_TARGET=12.0`, build, run CTest, invoke `tools/package-macos.sh`, and upload the corresponding DMG with `actions/upload-artifact@v4`.

- [ ] **Step 4: Publish release assets once after tagged jobs succeed**

Remove release creation from the Linux-only job. Add a release job gated on `refs/tags/v*`, dependent on successful Linux, Windows, and macOS jobs. Download all package artifacts and use one `softprops/action-gh-release@v2` invocation to publish the AppImage, ZIP, and both DMGs with generated release notes.

- [ ] **Step 5: Validate workflow structure and event behavior**

Run `actionlint .github/workflows/release.yml` when available and inspect its trigger and job conditions. Confirm ordinary branch pushes and pull requests upload Windows/macOS artifacts without running the release job, and `v*` tags run all builds before one release publication.

---

### Task 4: Document platform requirements and downloads

**Files:**
- Modify: `README.md`
- Modify: `README.jp.md`
- Modify: `THIRD_PARTY_NOTICES.md`

**Interfaces:**
- Consumes: artifact names, release behavior, package contents, minimum OS versions, and external FFmpeg requirement from Tasks 2 and 3.
- Produces: matching English and Japanese instructions for downloading, launching, and satisfying runtime prerequisites.

- [ ] **Step 1: Document Windows and macOS requirements**

Add Windows 10+ and macOS 12+ runtime requirements, distinguish Apple Silicon and Intel macOS downloads, and explain that `ffmpeg` and `ffprobe` must be installed separately and available on `PATH`. Remove SQLite development packages from build prerequisites if they are no longer required after Task 1.

- [ ] **Step 2: Document package downloads and unsigned launch warnings**

Explain that ordinary pushes provide temporary GitHub Actions artifacts and `v*` releases provide persistent release assets. Document Windows SmartScreen and macOS Gatekeeper first-launch warnings because packages are unsigned and not notarized.

- [ ] **Step 3: Keep documentation and package notices consistent**

Clarify that the Linux AppImage bundles FFmpeg while Windows/macOS do not. Update `THIRD_PARTY_NOTICES.md` to describe which platform packages ship Qt/SQLite/FFmpeg components, and ensure the package scripts include Qt and SQLite notices applicable to their actual deployed runtime files.

- [ ] **Step 4: Review both language versions**

Compare the relevant sections of `README.md` and `README.jp.md`; verify both state the same OS floors, architecture variants, download paths, FFmpeg prerequisite, and unsigned status.

---

## Verification

- Run the existing CMake build and all CTest tests locally after Task 1.
- Require all existing tests to pass on Windows and macOS in GitHub Actions.
- Require native package-content checks to pass for each Windows/macOS artifact.
- Require `actionlint` to pass when available; inspect release event conditions and artifact dependencies.
- Run `git diff --check` and review README language parity before completion.
