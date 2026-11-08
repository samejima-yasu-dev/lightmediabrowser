#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "$script_dir/.." && pwd)"
build_directory="${1:-$repository_root/build}"
architecture="${2:-}"
output_directory="${3:-$repository_root/dist}"
qt_root="${QT_ROOT_DIR:-}"

if [[ -z "$architecture" ]]; then
    echo "Usage: $0 <build-directory> <arm64|x86_64> [output-directory]" >&2
    exit 2
fi
if [[ "$architecture" != "arm64" && "$architecture" != "x86_64" ]]; then
    echo "Unsupported macOS architecture: $architecture" >&2
    exit 2
fi
if [[ -z "$qt_root" || ! -d "$qt_root" ]]; then
    echo "QT_ROOT_DIR must point to the installed Qt prefix." >&2
    exit 2
fi

app_bundle="$build_directory/LightMediaBrowser.app"
executable="$app_bundle/Contents/MacOS/LightMediaBrowser"
deploy_tool="$qt_root/bin/macdeployqt"
sqlite_plugin="$qt_root/plugins/sqldrivers/libqsqlite.dylib"

if [[ ! -d "$app_bundle" ]]; then
    echo "Application bundle not found: $app_bundle" >&2
    exit 1
fi
if [[ ! -x "$deploy_tool" ]]; then
    echo "macdeployqt not found: $deploy_tool" >&2
    exit 1
fi
if [[ ! -f "$sqlite_plugin" ]]; then
    echo "Qt QSQLITE plugin not found: $sqlite_plugin" >&2
    exit 1
fi
if [[ ! -x "$executable" ]]; then
    echo "Application executable not found: $executable" >&2
    exit 1
fi

license_directories=()
if [[ -n "${QT_LICENSE_DIR:-}" && -d "$QT_LICENSE_DIR" ]]; then
    license_directories+=("$QT_LICENSE_DIR")
fi
for candidate in \
    "$qt_root/licenses" \
    "$qt_root/Licenses" \
    "$(dirname "$qt_root")/licenses" \
    "$(dirname "$qt_root")/Licenses" \
    "$(dirname "$(dirname "$qt_root")")/licenses" \
    "$(dirname "$(dirname "$qt_root")")/Licenses"; do
    if [[ -d "$candidate" ]]; then
        license_directories+=("$candidate")
    fi
done
if [[ ${#license_directories[@]} -eq 0 ]]; then
    echo "Qt/SQLite license directory was not found under the Qt installation: $qt_root" >&2
    exit 1
fi

license_files=()
for directory in "${license_directories[@]}"; do
    while IFS= read -r -d '' license_file; do
        license_files+=("$license_file")
    done < <(find "$directory" -type f \
        \( -iname '*license*' -o -iname '*copying*' -o -iname '*copyright*' \
           -o -iname '*notice*' -o -iname '*lgpl*' -o -iname '*gpl*' -o -iname '*sqlite*' \) \
        -print0)
done
if [[ ${#license_files[@]} -eq 0 ]]; then
    echo "Qt/SQLite license texts were not found under the Qt installation: $qt_root" >&2
    exit 1
fi

mkdir -p "$output_directory"
package_name="LightMediaBrowser-macos-$architecture"
package_path="$output_directory/$package_name.dmg"
staging_directory="$(mktemp -d "${TMPDIR:-/tmp}/lightmediabrowser-package.XXXXXX")"
trap 'rm -rf "$staging_directory"' EXIT

plugin_directory="$app_bundle/Contents/PlugIns/sqldrivers"
mkdir -p "$plugin_directory"
cp "$sqlite_plugin" "$plugin_directory/"
"$deploy_tool" "$app_bundle" -always-overwrite

actual_architectures="$(lipo -archs "$executable")"
if [[ "$actual_architectures" != "$architecture" ]]; then
    echo "Expected $architecture app binary, got: $actual_architectures" >&2
    exit 1
fi
if ! otool -l "$executable" | grep -Eq 'minos[[:space:]]+12\.0'; then
    echo "The app binary must target macOS 12.0 or later." >&2
    exit 1
fi
if [[ ! -f "$plugin_directory/libqsqlite.dylib" ]]; then
    echo "The QSQLITE plugin was not deployed into the app bundle." >&2
    exit 1
fi

staged_app="$staging_directory/LightMediaBrowser.app"
ditto "$app_bundle" "$staged_app"
cp "$repository_root/LICENSE" "$staging_directory/"
cp "$repository_root/THIRD_PARTY_NOTICES.md" "$staging_directory/"
cp "$repository_root/docs/INSTALLATION.md" "$staging_directory/README.md"
third_party_directory="$staging_directory/third-party-licenses/qt"
mkdir -p "$third_party_directory"
license_index=0
for license_file in "${license_files[@]}"; do
    destination_name="$(printf '%03d' "$license_index")-$(basename "$(dirname "$license_file")")-$(basename "$license_file")"
    cp "$license_file" "$third_party_directory/$destination_name"
    license_index=$((license_index + 1))
done

if find "$staging_directory" -type f \( -iname 'ffmpeg' -o -iname 'ffprobe' \) -print -quit | grep -q .; then
    echo "FFmpeg must not be included in the macOS package." >&2
    exit 1
fi
if [[ ! -d "$staged_app/Contents/Frameworks" || ! -d "$staged_app/Contents/PlugIns/sqldrivers" ]]; then
    echo "Qt runtime frameworks or SQL plugins are missing from the app bundle." >&2
    exit 1
fi
if [[ -z "$(find "$third_party_directory" -type f -print -quit)" ]]; then
    echo "Qt license texts were not copied into the macOS package." >&2
    exit 1
fi

if [[ -e "$package_path" ]]; then
    rm -f "$package_path"
fi
hdiutil create -volname "LightMediaBrowser $architecture" \
    -srcfolder "$staging_directory" -ov -format UDZO "$package_path"
if [[ ! -s "$package_path" ]]; then
    echo "macOS package was not created: $package_path" >&2
    exit 1
fi
echo "Created $package_path"
