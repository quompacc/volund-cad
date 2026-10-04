#!/bin/sh
set -eu

# P05 gate: host inventory, build and the complete CTest under native Wayland.
# Run inside the active Wayland session of the Fedora test host.
if [ "$(uname -s)" != "Linux" ]; then
    echo "This desktop gate must run on Linux." >&2
    exit 1
fi
if [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "WAYLAND_DISPLAY is not set; run this inside the active Wayland session." >&2
    exit 1
fi

build_preset=${1:-linux-desktop}
repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
report_dir="$repository_root/artifacts/linux-desktop"
mkdir -p "$report_dir"
smoke_dir="${VOELUND_SMOKE_ARTIFACT_DIR:-$report_dir/smoke}"
mkdir -p "$smoke_dir"
export VOELUND_SMOKE_ARTIFACT_DIR="$smoke_dir"

{
    echo "timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "build_preset=$build_preset"
    echo "wayland_display=$WAYLAND_DISPLAY"
    echo "xdg_session_type=${XDG_SESSION_TYPE:-}"
    echo "xdg_current_desktop=${XDG_CURRENT_DESKTOP:-}"
    echo "display_unset_for_tests=yes"
    uname -a
    sed -n 's/^PRETTY_NAME=//p' /etc/os-release
    g++ --version | head -n 1
    cmake --version | head -n 1
    if command -v qtpaths6 >/dev/null 2>&1; then
        echo "qt=$(qtpaths6 --qt-version)"
    fi
    rpm -q qt6-qtbase qt6-qtwayland mesa-dri-drivers 2>/dev/null || true
} >"$report_dir/host.txt"
if command -v eglinfo >/dev/null 2>&1; then
    env -u DISPLAY eglinfo -B >"$report_dir/egl.txt" 2>&1 || true
fi

cd "$repository_root"
cmake --build --preset "$build_preset"
ldd "build/$build_preset/apps/voelund-cad/voelund-cad" >"$report_dir/linked-libraries.txt"
# Recorded for review only: system libraries may pull X11 in transitively. The gate
# itself is the smoke tests' QPA check with DISPLAY unset.
grep -E 'libX11|libxcb' "$report_dir/linked-libraries.txt" \
    >"$report_dir/x11-linkage.txt" || true

if env -u DISPLAY QT_QPA_PLATFORM=wayland \
    ctest --preset "$build_preset" --output-junit "$report_dir/ctest.junit.xml" \
    >"$report_dir/ctest.txt" 2>&1; then
    cat "$report_dir/ctest.txt"
else
    test_status=$?
    cat "$report_dir/ctest.txt"
    exit "$test_status"
fi
