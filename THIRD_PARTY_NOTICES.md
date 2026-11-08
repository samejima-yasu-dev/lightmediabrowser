# Third-party notices

LightMediaBrowser's own source code is licensed under the MIT License; see
[LICENSE](LICENSE). This notice concerns separately licensed components used
or packaged with the application and does not change their licenses.

## Components

| Component | How it is used | License information |
| --- | --- | --- |
| Qt 6 | Application framework; Qt runtime libraries are included in the Linux AppImage, Windows ZIP, and macOS DMGs. | Qt modules and builds may be offered under LGPL, GPL, or commercial terms. The applicable Qt license texts are included with each package. |
| SQLite 3 | Database accessed through Qt SQL's QSQLITE driver; the runtime driver is included in platform packages. | SQLite is in the public domain. See the SQLite project for details. |
| FFmpeg | The `ffmpeg` and `ffprobe` executables are included in the Linux AppImage. Windows and macOS users install them separately; they are not included in those packages. | FFmpeg's licensing depends on the source and build configuration. The copyright and license text for the Ubuntu packages used by each Linux release is included in that AppImage. |

The Linux release workflow copies available package copyright files for Qt,
FFmpeg, and SQLite to:

```text
usr/share/doc/lightmediabrowser/third-party/
```

Those package files are the authoritative notices for the binaries in the
AppImage. Windows and macOS packages include Qt license texts from the deployed
Qt distribution under `third-party-licenses/qt/`, together with this notice.
The set of enabled codecs and linked components can affect applicable terms;
verify the exact package build and contents before redistributing. The
application icon and other visual assets should also be checked against their
original source and license before redistribution.

This summary is informational, not legal advice, and is not a substitute for
the license texts shipped with each release.
