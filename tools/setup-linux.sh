#!/usr/bin/env bash
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
vcpkg_root="${repository_root}/.tools/vcpkg"

for command_name in git g++ cmake ninja; do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "Required command is missing: ${command_name}" >&2
        exit 1
    fi
done

if [[ ! -x "${vcpkg_root}/vcpkg" ]]; then
    mkdir -p "${repository_root}/.tools"
    git clone --depth 1 https://github.com/microsoft/vcpkg.git "${vcpkg_root}"
    "${vcpkg_root}/bootstrap-vcpkg.sh" -disableMetrics
fi

cd "${repository_root}"
cmake --preset linux-gcc
