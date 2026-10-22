#!/bin/sh
set -eu

# Project-local OCCT 7.9.3 with EGL/GLES and without Xlib for the native
# Wayland desktop on FreeBSD and Linux (docs/dependencies.md, ADR 0011).
case "$(uname -s)" in
    FreeBSD)
        platform=freebsd
        # FreeBSD's libc exposes POSIX 2008 locales without glibc's extensions.
        occt_cxx_flags=-DOCCT_CLOCALE_POSIX2008
        ;;
    Linux)
        platform=linux
        occt_cxx_flags=
        ;;
    *)
        echo "This OCCT build is intended for FreeBSD or Linux." >&2
        exit 1
        ;;
esac

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
occt_root="${VOELUND_OCCT_WORK_ROOT:-$repository_root/.tools/occt-$platform}"
# The CI shares one OCCT cache between workspaces through a symlink. CMake works on the
# physical path, so the cache is the same from every workspace (M22).
mkdir -p "$occt_root"
occt_root=$(CDPATH= cd -- "$occt_root" && pwd -P)
source_dir="${VOELUND_OCCT_SOURCE_DIR:-$occt_root/src}"
build_dir="$occt_root/build"
install_dir="$occt_root/install"
occt_commit=a016080bf6738d6aeae020badee4e888ad1540a5

if [ ! -d "$source_dir/.git" ]; then
    if [ -n "${VOELUND_OCCT_SOURCE_DIR:-}" ]; then
        echo "The supplied read-only OCCT source must already be a Git checkout." >&2
        exit 1
    fi
    mkdir -p "$occt_root"
    git clone --filter=blob:none --no-checkout \
        https://github.com/Open-Cascade-SAS/OCCT.git "$source_dir"
    git -C "$source_dir" fetch --depth 1 origin "$occt_commit"
    git -C "$source_dir" checkout --detach "$occt_commit"
fi

if [ "$(git -C "$source_dir" rev-parse HEAD)" != "$occt_commit" ]; then
    echo "Unexpected OCCT source revision in $source_dir" >&2
    exit 1
fi

# A build configured through another (workspace) path is built anew: its cache and the
# forwarding headers OCCT generates under build/include name the old source path.
forwarding="$build_dir/include/opencascade/Standard.hxx"
if { [ -f "$build_dir/CMakeCache.txt" ] &&
     ! grep -qx "CMAKE_CACHEFILE_DIR:INTERNAL=$build_dir" "$build_dir/CMakeCache.txt"; } ||
   { [ -f "$forwarding" ] && ! grep -qF "\"$source_dir/" "$forwarding"; }; then
    rm -rf "$build_dir"
fi

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_CXX_FLAGS=$occt_cxx_flags" \
    -DINSTALL_DIR="$install_dir" \
    -DBUILD_MODULE_FoundationClasses=OFF \
    -DBUILD_MODULE_ModelingData=OFF \
    -DBUILD_MODULE_ModelingAlgorithms=OFF \
    -DBUILD_MODULE_Visualization=OFF \
    -DBUILD_MODULE_ApplicationFramework=OFF \
    -DBUILD_MODULE_DataExchange=OFF \
    -DBUILD_MODULE_DETools=OFF \
    -DBUILD_MODULE_Draw=OFF \
    "-DBUILD_ADDITIONAL_TOOLKITS=TKBO;TKPrim;TKFillet;TKOffset;TKV3d;TKMesh;TKShHealing;TKHLR;TKOpenGles;TKBin;TKDESTEP;TKDEIGES;TKXCAF" \
    -DBUILD_SAMPLES_QT=OFF \
    -DBUILD_Inspector=OFF \
    -DBUILD_DOC_Overview=OFF \
    -DBUILD_RESOURCES=OFF \
    -DINSTALL_SAMPLES=OFF \
    -DINSTALL_TEST_CASES=OFF \
    -DUSE_XLIB=OFF \
    -DUSE_OPENGL=OFF \
    -DUSE_GLES2=ON \
    -DUSE_TBB=OFF \
    -DUSE_TCL=OFF \
    -DUSE_TK=OFF \
    -DUSE_FREETYPE=ON \
    -DUSE_FREEIMAGE=OFF \
    -DUSE_FFMPEG=OFF \
    -DUSE_RAPIDJSON=OFF \
    -DUSE_DRACO=OFF \
    -DUSE_VTK=OFF

cmake --build "$build_dir" --parallel "${VOELUND_BUILD_JOBS:-4}"
cmake --install "$build_dir"
