#!/bin/sh
set -eu

# L06 (A-PERF-01): runs every load stage of voelund_perf_bench, each in its own process, and
# writes the result lines together with the host into one report.
#
#   sh tools/run-perf.sh [<build-dir>] [<report>]
#
# Run on an otherwise idle machine (no CI job, no other build): the values are timings.
repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-$repository_root/build/linux-desktop-release}
report=${2:-$repository_root/artifacts/perf/perf-$(date -u +%Y%m%dT%H%M%SZ).txt}
bench="$build/tests/voelund_perf_bench"
[ -x "$bench" ] || { echo "voelund_perf_bench not built in $build" >&2; exit 1; }
mkdir -p "$(dirname -- "$report")"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

{
    echo "# A-PERF-01 $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# commit $(git -C "$repository_root" rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "# build $(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$build/CMakeCache.txt")"
    echo "# cpu $(sed -n 's/^model name[^:]*: //p' /proc/cpuinfo | head -n 1), $(nproc) threads"
    echo "# memory $(awk '/MemTotal/ {printf "%.1f GiB", $2 / 1048576}' /proc/meminfo)"
    echo "# os $(sed -n 's/^PRETTY_NAME=//p' /etc/os-release), load $(cut -d' ' -f1-3 /proc/loadavg)"
} >"$report"
for stage in sketch-100 sketch-1000 features-10 features-100 features-500 bodies-1 bodies-10 \
    bodies-100; do
    echo "running $stage" >&2
    "$bench" "$stage" "$work" 2>&1 | grep -E '^stage=|failed' >>"$report" ||
        echo "stage=$stage failed" >>"$report"
done
cat "$report"
