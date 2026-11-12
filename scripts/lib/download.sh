# shellcheck shell=bash
# Shared by install-deb.sh and run-appimage.sh: finds and downloads a package.
#
#   download_package <deb|appimage> <source> <empty directory>
#
# <source> is "latest" (newest GitHub release) or a release version ("0.1.0"
# or "v0.1.0"). Prints the downloaded file.

KDETODO_REPO="${KDETODO_REPO:-anjankow/todo-ubuntu-kde}"
KDETODO_API="${KDETODO_API:-https://api.github.com}"

die() {
    echo "error: $*" >&2
    exit 1
}

have_gh() {
    command -v gh >/dev/null && gh auth status >/dev/null 2>&1
}

download_package() {
    local kind="$1" source="$2" dir="$3" pattern
    case "$kind" in
        deb) pattern='*.deb' ;;
        appimage) pattern='*.AppImage' ;;
        *) die "unknown package kind: $kind" ;;
    esac
    mkdir -p "$dir"

    local tag=""
    if [ "$source" != "latest" ]; then
        tag="v${source#v}"
    fi
    echo "Downloading the ${tag:-latest} release of $KDETODO_REPO ..." >&2
    if have_gh; then
        # without a tag, gh takes the latest release
        gh release download ${tag:+"$tag"} --repo "$KDETODO_REPO" --pattern "$pattern" --dir "$dir" --clobber >&2 \
            || die "no ${tag:-latest} release with a $kind package found"
    else
        command -v curl >/dev/null || die "curl is required (or the GitHub CLI)"
        local api="$KDETODO_API/repos/$KDETODO_REPO/releases/${tag:+tags/$tag}"
        [ -n "$tag" ] || api="${api}latest"
        local url
        url="$(curl -fsSL "$api" | grep -o '"browser_download_url": *"[^"]*"' | cut -d'"' -f4 \
            | grep -E "${pattern/\*/.*}\$" | head -n 1)" \
            || die "no ${tag:-latest} release found (a private repository needs the GitHub CLI)"
        [ -n "$url" ] || die "the ${tag:-latest} release has no $kind package"
        curl -fL --progress-bar -o "$dir/$(basename "$url")" "$url" || die "download failed: $url"
    fi

    local file
    file="$(find "$dir" -maxdepth 1 -name "$pattern" | head -n 1)"
    [ -n "$file" ] || die "the download contains no $kind package"
    echo "$file"
}
