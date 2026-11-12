# shellcheck shell=sh
# Sourced by the AppRun of the AppImage before kdetodo starts.
# The application icon is looked up in XDG_DATA_DIRS; the host's directories
# stay behind it so that the desktop's icon theme is still found.
export XDG_DATA_DIRS="${APPDIR}/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
