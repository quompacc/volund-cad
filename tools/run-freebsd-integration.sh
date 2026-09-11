#!/bin/sh
set -eu

if [ "$(uname -s)" != "FreeBSD" ]; then
    echo "This integration gate must run on FreeBSD." >&2
    exit 1
fi

environment_name=${1:-unspecified}
build_preset=${2:-freebsd-clang}
case "$environment_name" in
    control|meridian) ;;
    *)
        echo "Usage: sh tools/run-freebsd-integration.sh control|meridian [build-preset]" >&2
        exit 1
        ;;
esac

if [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "WAYLAND_DISPLAY is not set; run this inside the active Wayland session." >&2
    exit 1
fi

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
report_dir="$repository_root/artifacts/freebsd-$environment_name"
mkdir -p "$report_dir"

{
    echo "environment=$environment_name"
    echo "timestamp=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "freebsd_version=$(freebsd-version -kru | tr '\n' ' ')"
    echo "wayland_display=$WAYLAND_DISPLAY"
    echo "build_preset=$build_preset"
    uname -a
} >"$report_dir/host.txt"

cd "$repository_root"
cmake --build --preset "$build_preset"
ldd "build/$build_preset/apps/voelund-cad/voelund-cad" \
    >"$report_dir/linked-libraries.txt"
if command -v eglinfo >/dev/null 2>&1; then
    env -u DISPLAY eglinfo -B >"$report_dir/egl.txt" 2>&1 || true
fi
if env -u DISPLAY QT_QPA_PLATFORM=wayland \
    ctest --preset "$build_preset" -L wayland --output-on-failure \
    >"$report_dir/ctest.txt" 2>&1; then
    cat "$report_dir/ctest.txt"
else
    test_status=$?
    cat "$report_dir/ctest.txt"
    exit "$test_status"
fi
