#!/bin/sh
set -eu

if [ "$(uname -s)" != "FreeBSD" ]; then
    echo "This OCCT build is intended for FreeBSD." >&2
    exit 1
fi

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
occt_root="$repository_root/.tools/occt-freebsd"
source_dir="$occt_root/src"
build_dir="$occt_root/build"
install_dir="$occt_root/install"
occt_commit=a016080bf6738d6aeae020badee4e888ad1540a5

if [ ! -d "$source_dir/.git" ]; then
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

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS=-DOCCT_CLOCALE_POSIX2008 \
    -DINSTALL_DIR="$install_dir" \
    -DBUILD_MODULE_FoundationClasses=OFF \
    -DBUILD_MODULE_ModelingData=OFF \
    -DBUILD_MODULE_ModelingAlgorithms=OFF \
    -DBUILD_MODULE_Visualization=OFF \
    -DBUILD_MODULE_ApplicationFramework=OFF \
    -DBUILD_MODULE_DataExchange=OFF \
    -DBUILD_MODULE_DETools=OFF \
    -DBUILD_MODULE_Draw=OFF \
    "-DBUILD_ADDITIONAL_TOOLKITS=TKBO;TKPrim;TKV3d;TKMesh;TKShHealing;TKHLR;TKOpenGles" \
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
    -DUSE_FREETYPE=OFF \
    -DUSE_FREEIMAGE=OFF \
    -DUSE_FFMPEG=OFF \
    -DUSE_RAPIDJSON=OFF \
    -DUSE_DRACO=OFF \
    -DUSE_VTK=OFF

cmake --build "$build_dir" --parallel "${VOELUND_BUILD_JOBS:-4}"
cmake --install "$build_dir"
