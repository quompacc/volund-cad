#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

set -eu

# A04: mandatory static analysis on Linux. One clang-tidy pass over compile_commands.json runs
# readability-function-size (80 lines, 5 parameters, nesting 4) and the plugin check
# voelund-cyclomatic-complexity (McCabe 10, tools/cyclomatic); CheckFunctionLimits.cmake then
# compares the result with the visible exception list tools/quality/function-limits.txt.
#
#   sh tools/run-static-analysis.sh <build-dir> [<output-dir>]
#
# Any missing tool, a translation unit that does not parse or a mismatch with the list fails.
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "${1:?usage: run-static-analysis.sh <build-dir> [<output-dir>]}" && pwd)
out=${2:-$build/static-analysis}
jobs=${VOELUND_ANALYSIS_JOBS:-$(nproc)}
clang_tidy=${VOELUND_CLANG_TIDY:-clang-tidy}

fail() {
    echo "static analysis: $*" >&2
    exit 1
}
command -v "$clang_tidy" >/dev/null 2>&1 || fail "requires clang-tidy (LLVM 22)"
command -v jq >/dev/null 2>&1 || fail "requires jq to read compile_commands.json"
"$clang_tidy" --version | grep -q "LLVM version 22\." || fail "requires clang-tidy of LLVM 22"
[ -f "$build/compile_commands.json" ] || fail "no compile_commands.json in $build"

rm -rf "$out"
mkdir -p "$out/db" "$out/reports"
cmake -S "$root/tools/cyclomatic" -B "$out/tool" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    >"$out/tool-configure.log" 2>&1 || fail "configuring tools/cyclomatic failed, see $out/tool-configure.log"
cmake --build "$out/tool" >"$out/tool-build.log" 2>&1 || fail "building tools/cyclomatic failed"
plugin="$out/tool/libVoelundTidyPlugin.so"
[ -f "$plugin" ] || fail "VoelundTidyPlugin was not built (clang-tidy headers: clang-tools-extra-devel)"

# CMake adds GCC's C++20 module scanning flags, which clang does not know. Compiler warnings
# (GCC's -Werror set as clang sees it) are not what this measures: --extra-arg=-w below.
sed -e 's/ -fmodules-ts//g' -e 's/ -fmodule-mapper=[^ "]*//g' -e 's/ -fdeps-[a-z]*=[^ "]*//g' \
    "$build/compile_commands.json" >"$out/db/compile_commands.json"
jq -r '.[].file' "$out/db/compile_commands.json" | sort -u |
    grep -v -e "^$root/build/" -e "^$build/" -e "^$root/third_party/planegcs/upstream/" |
    grep "^$root/" >"$out/files.txt" || true
count=$(wc -l <"$out/files.txt")
[ "$count" -gt 0 ] || fail "no own translation units in compile_commands.json"
echo "static analysis: $count translation units, $jobs jobs"

export VOELUND_ANALYSIS_TIDY="$clang_tidy" VOELUND_ANALYSIS_PLUGIN="$plugin" \
    VOELUND_ANALYSIS_DB="$out/db" VOELUND_ANALYSIS_CONFIG="$root/.clang-tidy" \
    VOELUND_ANALYSIS_REPORTS="$out/reports"
xargs -P "$jobs" -n 6 sh -c '
    report="$VOELUND_ANALYSIS_REPORTS/batch-$$"
    if "$VOELUND_ANALYSIS_TIDY" -p "$VOELUND_ANALYSIS_DB" --quiet \
        --config-file="$VOELUND_ANALYSIS_CONFIG" --load="$VOELUND_ANALYSIS_PLUGIN" \
        --extra-arg=-w \
        "--checks=-*,readability-function-size,voelund-cyclomatic-complexity" \
        "$@" >"$report.txt" 2>"$report.err"; then
        :
    else
        echo "$*" >"$report.failed"
    fi' _ <"$out/files.txt"

if ls "$out/reports/"*.failed >/dev/null 2>&1; then
    cat "$out/reports/"*.err >&2
    fail "clang-tidy could not analyse: $(cat "$out/reports/"*.failed | tr '\n' ' ')"
fi
cat "$out/reports/"*.txt >"$out/report.txt"
cmake "-DVOELUND_SOURCE_ROOT=$root" "-DVOELUND_TIDY_REPORT=$out/report.txt" \
    "-DVOELUND_ANALYSED_FILES=$count" \
    "-DVOELUND_FUNCTION_LIMITS=$root/tools/quality/function-limits.txt" \
    "-DVOELUND_CURRENT_LIMITS=$out/function-limits-current.txt" \
    -P "$root/tools/cmake/CheckFunctionLimits.cmake"
