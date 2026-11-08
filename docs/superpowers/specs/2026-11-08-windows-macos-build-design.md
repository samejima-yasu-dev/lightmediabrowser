# Windows and macOS Build Design

## Intent

Make Windows and macOS builds available automatically when changes are pushed
to GitHub, and attach ready-to-download builds to versioned GitHub Releases.
The project already builds a Linux AppImage on `v*` tags; preserve that release
path while adding the other desktop platforms.

## Goals

- Build and run the existing test suite on Windows and macOS in GitHub Actions
  for pushes and pull requests.
- Make platform build artifacts downloadable from each applicable Actions run.
- Publish Windows and macOS packages alongside the existing Linux AppImage on
  `v*` tags.
- Package the Qt runtime and required Qt plugins so users do not need a separate
  Qt installation.
- Keep `ffmpeg` and `ffprobe` out of the Windows and macOS packages to avoid
  increasing their size; document that users must install both and make them
  available on `PATH`.
- Include the applicable third-party license notices for components shipped in
  distributed packages.
- Update the English and Japanese README build/run and distribution guidance.

## Recommended defaults

- Windows: x64 ZIP package.
- macOS: separate Apple Silicon (arm64) and Intel (x86_64) DMG packages.
- Minimum supported versions: Windows 10 and macOS 12 Monterey.
- Regular push and pull request: compile and test on Windows and macOS, then
  upload build packages as Actions artifacts.
- Version tag matching `v*`: also publish the platform packages to the GitHub
  Release, preserving the Linux AppImage.
- Require users to install `ffmpeg` and `ffprobe` separately and put them on
  `PATH`; do not bundle them with Windows or macOS packages.
- Code signing and notarization: out of scope because no signing environment is
  available. Document that Windows may show a SmartScreen warning and macOS may
  block or warn on first launch.

These are recommendations selected because the user asked for a recommended
approach; they can be changed before implementation.

## Current state

- The application uses C++20, CMake, Qt 6, and SQLite.
- Most application code uses Qt APIs for paths, file access, and opening media,
  and no obvious Linux-only API was found during inspection.
- Thumbnail generation and metadata probing launch `ffmpeg` and `ffprobe` by
  name. Users can continue to provide these tools through `PATH`.
- The current `.github/workflows/release.yml` runs only for `v*` tags and
  builds/releases a Linux AppImage.
- The current README documents Linux development and the external FFmpeg
  runtime dependency, but not Windows or macOS installation.

## Constraints

- Use native GitHub Actions runners for each target OS rather than cross
  compiling.
- Keep the existing Linux AppImage release behavior.
- Do not bundle FFmpeg or add a new FFmpeg lookup mechanism for Windows/macOS.
  Update user documentation with platform-appropriate installation guidance.
- Do not add code-signing secrets or ask the user to upload certificates.
- Verify packaging against the actual Qt deployment; include applicable Qt and
  SQLite license texts and notices for shipped components.
- Preserve existing application behavior and run the existing tests on each
  supported runner.

## Non-goals

- Windows ARM64 support or a universal macOS binary.
- Bundling FFmpeg or ffprobe.
- Code signing, notarization, installers, auto-updates, or a new release
  management process.
- Changes to the Linux AppImage packaging beyond any workflow changes required
  to coordinate release uploads.
- Broad UI redesign or unrelated application fixes.

## Left to the builder

- Choose and pin maintained GitHub Actions and dependency versions.
- Choose reliable Qt distribution and deployment tools for each platform and
  package the required Qt SQL plugin and runtime dependencies.
- Decide the precise ZIP/DMG directory layout and artifact naming.
- Add Windows and macOS FFmpeg installation guidance while preserving the
  application's current PATH-based lookup.
- Configure CMake app packaging for macOS if required by the deployment tools.
- Structure workflow jobs so regular builds upload artifacts while tagged
  builds publish all platform packages and the existing AppImage exactly once.
- Test that each package starts, finds its runtime dependencies, can invoke
  FFmpeg when the user has installed it, and includes the required notices.
