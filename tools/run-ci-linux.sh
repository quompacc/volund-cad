#!/bin/sh
set -eu

# C01: CI entry on the Fedora runner. Reuses the cached project-local OCCT,
# configures linux-desktop, checks source limits and runs the P05 desktop gate.
# The Wayland session is a headless `kwin_wayland --virtual` on the host GPU
# unless VOELUND_CI_SESSION=desktop selects the logged-in WAYLAND_DISPLAY.
if [ "$(uname -s)" != "Linux" ]; then
    echo "This CI entry must run on Linux." >&2
    exit 1
fi

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
occt_cache="${VOELUND_OCCT_CACHE:-$HOME/.cache/voelund/occt-linux}"
report_dir="$repository_root/artifacts/linux-desktop"
session_kind="${VOELUND_CI_SESSION:-headless}"
mkdir -p "$occt_cache" "$repository_root/.tools" "$report_dir"
if [ ! -e "$repository_root/.tools/occt-linux" ]; then
    ln -s "$occt_cache" "$repository_root/.tools/occt-linux"
fi

cd "$repository_root"
bash tools/setup-linux.sh desktop
cmake --build --preset linux-desktop --target check-source-limits

compositor_pid=
runtime_dir=
cleanup() {
    if [ -n "$compositor_pid" ]; then
        # dbus-run-session does not forward TERM; stop the compositor's whole group.
        kill -TERM -- "-$compositor_pid" 2>/dev/null || true
        wait "$compositor_pid" 2>/dev/null || true
    fi
    if [ -n "$runtime_dir" ]; then
        rm -rf "$runtime_dir"
    fi
}
trap cleanup EXIT INT TERM

case "$session_kind" in
    headless)
        runtime_dir=$(mktemp -d)
        chmod 700 "$runtime_dir"
        socket="voelund-ci-$$"
        env -u DISPLAY -u WAYLAND_DISPLAY XDG_RUNTIME_DIR="$runtime_dir" \
            setsid dbus-run-session -- kwin_wayland --virtual --socket "$socket" \
            --width 1600 --height 1000 >"$report_dir/compositor.txt" 2>&1 &
        compositor_pid=$!
        attempts=0
        while [ ! -S "$runtime_dir/$socket" ]; do
            attempts=$((attempts + 1))
            if [ "$attempts" -gt 50 ] || ! kill -0 "$compositor_pid" 2>/dev/null; then
                echo "Headless kwin_wayland did not provide $socket." >&2
                cat "$report_dir/compositor.txt" >&2
                exit 1
            fi
            sleep 0.2
        done
        export XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$socket"
        ;;
    desktop)
        if [ -z "${WAYLAND_DISPLAY:-}" ]; then
            echo "VOELUND_CI_SESSION=desktop needs WAYLAND_DISPLAY." >&2
            exit 1
        fi
        ;;
    *)
        echo "Unknown VOELUND_CI_SESSION: $session_kind" >&2
        exit 1
        ;;
esac
{
    echo "session=$session_kind"
    echo "wayland_display=$WAYLAND_DISPLAY"
    if [ "$session_kind" = headless ]; then
        echo "compositor=kwin_wayland --virtual (headless; no physical display or compositor proof)"
    fi
} >"$report_dir/session.txt"

sh tools/run-linux-desktop.sh

# A04: function limits (clang-tidy readability-function-size + cyclomatic plugin) against the
# visible exception list; a missing tool or analysis error fails like a violation.
sh tools/run-static-analysis.sh build/linux-desktop "$report_dir/static-analysis"
