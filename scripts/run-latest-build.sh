#!/usr/bin/env bash
# Downloads the kdetodo binary built by CI for the latest merge to main and
# runs it. Nothing is installed.
#
#   scripts/run-latest-build.sh [--download-only] [-- <kdetodo arguments>]
#
# Requirements: the GitHub CLI, signed in (gh auth login), and the Qt 5 / KDE
# Frameworks 5 runtime libraries (the script tells you which are missing).
#
# The binary is saved in ~/.cache/kdetodo-test and started from there. Your
# normal configuration and todo file are used, exactly as an installed copy
# would.
set -euo pipefail

REPO="${KDETODO_REPO:-anjankow/todo-ubuntu-kde}"
WORKFLOW="ci.yml"
ARTIFACT="kdetodo-bin"
BRANCH="main"
DEST="${KDETODO_TEST_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/kdetodo-test}"

download_only=0
if [ "${1:-}" = "--download-only" ]; then
    download_only=1
    shift
fi
if [ "${1:-}" = "--" ]; then
    shift
fi

die() {
    echo "error: $*" >&2
    exit 1
}

command -v gh >/dev/null || die "the GitHub CLI is required: sudo apt install gh && gh auth login"
gh auth status >/dev/null 2>&1 || die "not signed in to GitHub: run 'gh auth login'"

echo "Looking for the latest successful build of $BRANCH in $REPO ..."
run_id="$(gh run list --repo "$REPO" --workflow "$WORKFLOW" --branch "$BRANCH" --event push \
    --status success --limit 1 --json databaseId --jq '.[0].databaseId // empty')"
[ -n "$run_id" ] || die "no successful CI run on $BRANCH found"

rm -rf "$DEST"
mkdir -p "$DEST"
gh run download "$run_id" --repo "$REPO" --name "$ARTIFACT" --dir "$DEST" \
    || die "run $run_id has no artifact named $ARTIFACT (artifacts expire after 90 days)"
binary="$DEST/kdetodo"
[ -f "$binary" ] || die "the artifact does not contain kdetodo"
chmod +x "$binary" # artifacts do not keep the executable bit
echo "Downloaded $binary (CI run $run_id)"

# Report runtime libraries that are not installed on this machine.
missing="$(ldd "$binary" | awk '/not found/ {print $1}')"
if [ -n "$missing" ]; then
    echo "These libraries are missing on this system:" >&2
    awk '{print "  " $0}' <<<"$missing" >&2
    die "install them: sudo apt install libqt5quickcontrols2-5 libqt5widgets5t64 libkf5i18n5 libkf5configcore5 libkf5coreaddons5"
fi

# The interface needs these QML modules at run time.
qmldir="$(qmake -query QT_INSTALL_QML 2>/dev/null || echo /usr/lib/x86_64-linux-gnu/qt5/qml)"
for module in org/kde/kirigami.2 org/kde/qqc2desktopstyle QtQuick/Controls.2 QtQuick/Layouts Qt/labs/platform; do
    [ -d "$qmldir/$module" ] || die "QML module $module is missing: sudo apt install qml-module-org-kde-kirigami2 qml-module-org-kde-qqc2desktopstyle qml-module-qtquick-controls2 qml-module-qtquick-layouts qml-module-qt-labs-platform"
done

if [ "$download_only" -eq 1 ]; then
    exit 0
fi

echo "Starting $binary"
exec "$binary" "$@"
