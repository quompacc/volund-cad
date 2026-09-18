set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r04c-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r04c-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r04c-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r04c-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland VOELUND_SUPPORT_FLOW_ARTIFACT_DIR="$PWD/r04c-ui" ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r04c-ctest.xml" > r04c-ctest.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r04c-source-paths.txt > r04c-source-hashes.txt
tar -cf r04c-ui.tar r04c-ui
