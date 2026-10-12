# Third-party notices

LightMediaBrowser's own source code is licensed under the MIT License; see
[LICENSE](LICENSE). This notice concerns separately licensed components used
or packaged with the application and does not change their licenses.

## Components

| Component | How it is used | License information |
| --- | --- | --- |
| Qt 6 | Application framework; Qt libraries are included in release AppImages. | Qt modules and builds may be offered under LGPL, GPL, or commercial terms. The copyright and license text for the Qt packages used by each release is included in that AppImage. |
| SQLite 3 | Database library; the application links to SQLite and release AppImages include it when deployment resolves it as a runtime dependency. | SQLite is in the public domain. See the SQLite project for details. |
| FFmpeg | The `ffmpeg` and `ffprobe` executables are invoked as separate processes and included in release AppImages. | FFmpeg's licensing depends on the source and build configuration. The copyright and license text for the Ubuntu packages used by each release is included in that AppImage. |

The release workflow copies available package copyright files for Qt, FFmpeg,
and SQLite to:

```text
usr/share/doc/lightmediabrowser/third-party/
```

Those package files are the authoritative notices for the binaries in that
release. The set of enabled codecs and linked components can affect applicable
terms; verify the exact package build and AppImage contents before redistributing.
The application icon and other visual assets should also be checked against
their original source and license before redistribution.

This summary is informational, not legal advice, and is not a substitute for
the license texts shipped with each release.
