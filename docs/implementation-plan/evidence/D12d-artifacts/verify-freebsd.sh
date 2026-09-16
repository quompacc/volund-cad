set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf d12d-source-delta.tar
while IFS= read -r path; do touch "$path"; done < d12d-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > d12d-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> d12d-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/d12d-ctest.xml" > d12d-ctest.log 2>&1
