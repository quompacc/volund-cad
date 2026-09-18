set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r04a-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r04a-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r04a-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r04a-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r04a-ctest.xml" > r04a-ctest.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r04a-source-paths.txt > r04a-source-hashes.txt
