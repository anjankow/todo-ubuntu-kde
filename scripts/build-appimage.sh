#!/usr/bin/env bash
# Builds kdetodo-<version>-x86_64.AppImage from a configured and built
# CMake build directory. The AppImage carries Qt 5 and the KDE Frameworks
# libraries the application uses, so it runs without installing anything.
#
#   scripts/build-appimage.sh <build-dir> <version>
#
# Tools: linuxdeploy and its Qt plugin, downloaded into <build-dir>/appimage-tools
# (or $KDETODO_APPIMAGE_TOOLS) unless they are already there.
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <build-dir> <version>" >&2
    exit 2
fi
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$(cd "$1" && pwd)"
version="$2"
tools="${KDETODO_APPIMAGE_TOOLS:-$build/appimage-tools}"
appdir="$build/AppDir"
output="kdetodo-${version}-x86_64.AppImage"

mkdir -p "$tools"
fetch() {
    if [ ! -x "$tools/$1" ]; then
        curl -fsSL --retry 3 -o "$tools/$1" "$2"
        chmod +x "$tools/$1"
    fi
}
fetch linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage

rm -rf "$appdir" "${build:?}/$output"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr
mkdir -p "$appdir/apprun-hooks"
cp "$repo/packaging/appimage/apprun-hook.sh" "$appdir/apprun-hooks/kdetodo.sh"

# The tools are AppImages themselves; CI machines have no FUSE.
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$tools:$PATH" # linuxdeploy finds its plugins in PATH
export QMAKE="${QMAKE:-/usr/lib/qt5/bin/qmake}"
# Next to xcb: the smoke test below starts the application without a display.
export EXTRA_PLATFORM_PLUGINS="libqoffscreen.so"
# The application icon is an SVG file.
export EXTRA_QT_PLUGINS="svg"
export LDAI_OUTPUT="$output"
export LINUXDEPLOY_OUTPUT_VERSION="$version"

cd "$build"
linuxdeploy-x86_64.AppImage \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/kdetodo" \
    --desktop-file "$appdir/usr/share/applications/org.anjankow.kdetodo.desktop" \
    --icon-file "$appdir/usr/share/icons/hicolor/scalable/apps/org.anjankow.kdetodo.svg" \
    --plugin qt \
    --output appimage

test -f "$build/$output"
echo "Built $build/$output"

# Smoke test: the bundle holds the KDE libraries and data, and the binary starts.
check="$build/appimage-check"
rm -rf "$check" && mkdir -p "$check"
(cd "$check" && "$build/$output" --appimage-extract >/dev/null)
for f in usr/bin/kdetodo apprun-hooks/kdetodo.sh \
    usr/share/icons/hicolor/scalable/apps/org.anjankow.kdetodo.svg \
    usr/plugins/platforms/libqxcb.so usr/plugins/platforms/libqoffscreen.so; do
    test -e "$check/squashfs-root/$f" || {
        echo "missing in the AppImage: $f" >&2
        exit 1
    }
done
for lib in libKF5XmlGui libKF5WidgetsAddons libKF5ConfigCore libKF5I18n libQt5Widgets; do
    ls "$check/squashfs-root/usr/lib/$lib".so* >/dev/null 2>&1 || {
        echo "not bundled: $lib" >&2
        exit 1
    }
done
QT_QPA_PLATFORM=offscreen "$build/$output" --appimage-extract-and-run --version
rm -rf "$check"
