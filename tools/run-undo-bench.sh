#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

set -eu

# UNDO-MESSUNG: runs voelund_undo_bench for the small, medium and large model at 10, 100 and 500
# steps, each in its own process, and writes the result lines with the host into one report.
#
#   sh tools/run-undo-bench.sh [<build-dir>] [<report>]
#
# Run on an otherwise idle machine (no CI job, no other build): the values are timings.
repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-$repository_root/build/linux-desktop-release}
report=${2:-$repository_root/artifacts/perf/undo-$(date -u +%Y%m%dT%H%M%SZ).txt}
bench="$build/tests/voelund_undo_bench"
step_file=${VOELUND_UNDO_STEP_FILE:-$repository_root/tests/differential/nist/nist_stc_08_asme1_ap242-e3.stp}
[ -x "$bench" ] || { echo "voelund_undo_bench not built in $build" >&2; exit 1; }
mkdir -p "$(dirname -- "$report")"

{
    echo "# UNDO-MESSUNG $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# commit ${VOELUND_PERF_COMMIT:-$(git -C "$repository_root" rev-parse HEAD 2>/dev/null || echo unknown)}"
    echo "# build $(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$build/CMakeCache.txt")"
    echo "# cpu $(sed -n 's/^model name[^:]*: //p' /proc/cpuinfo | head -n 1), $(nproc) threads"
    echo "# memory $(awk '/MemTotal/ {printf "%.1f GiB", $2 / 1048576}' /proc/meminfo)"
    echo "# os $(sed -n 's/^PRETTY_NAME=//p' /etc/os-release), load $(cut -d' ' -f1-3 /proc/loadavg)"
    echo "# large model STEP part $(basename -- "$step_file")"
} >"$report"
for model in small medium large; do
    for steps in 10 100 500; do
        echo "running $model $steps" >&2
        if ! timeout "${VOELUND_UNDO_LIMIT:-1800}" "$bench" "$model" "$steps" "$step_file" \
            >>"$report" 2>&1; then
            echo "model=$model steps=$steps metric=aborted value=1 unit=flag" >>"$report"
        fi
    done
done
echo "report: $report" >&2
