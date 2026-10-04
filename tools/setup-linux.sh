#!/usr/bin/env bash
set -euo pipefail

# Usage: tools/setup-linux.sh [desktop|core]
#   desktop (default): native Wayland desktop on Fedora with system Qt 6, Boost and
#                      Eigen plus the project-local lean OCCT 7.9.3 (ADR 0011, P05).
#   core:              CAD core without desktop through the pinned vcpkg toolchain.
mode="${1:-desktop}"
repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require_commands() {
    local command_name
    for command_name in "$@"; do
        if ! command -v "${command_name}" >/dev/null 2>&1; then
            echo "Required command is missing: ${command_name}" >&2
            exit 1
        fi
    done
}

setup_core() {
    local vcpkg_root="${repository_root}/.tools/vcpkg"
    require_commands git g++ cmake ninja
    if [[ ! -x "${vcpkg_root}/vcpkg" ]]; then
        mkdir -p "${repository_root}/.tools"
        git clone --depth 1 https://github.com/microsoft/vcpkg.git "${vcpkg_root}"
        "${vcpkg_root}/bootstrap-vcpkg.sh" -disableMetrics
    fi
    cd "${repository_root}"
    cmake --preset linux-gcc
}

setup_desktop() {
    # Fedora package names; the lean OCCT build needs FreeType and EGL/GLES headers.
    local packages=(
        gcc-c++ cmake ninja-build git-core
        qt6-qtbase-devel qt6-qtwayland
        boost-devel eigen3-devel
        freetype-devel libglvnd-devel mesa-libEGL-devel
    )
    require_commands git g++ cmake ninja rpm
    local missing=()
    local package_name
    for package_name in "${packages[@]}"; do
        if ! rpm -q "${package_name}" >/dev/null 2>&1; then
            missing+=("${package_name}")
        fi
    done
    if ((${#missing[@]} > 0)); then
        echo "Required packages are missing; install them with:" >&2
        echo "  sudo dnf install ${missing[*]}" >&2
        exit 1
    fi

    if [[ ! -f "${repository_root}/.tools/occt-linux/install/lib/cmake/opencascade/OpenCASCADEConfig.cmake" ]]; then
        sh "${repository_root}/tools/build-lean-occt.sh"
    fi
    cd "${repository_root}"
    cmake --preset linux-desktop
}

case "${mode}" in
    desktop) setup_desktop ;;
    core) setup_core ;;
    *)
        echo "Usage: tools/setup-linux.sh [desktop|core]" >&2
        exit 1
        ;;
esac
