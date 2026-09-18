set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r05bu-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r05bu-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r05bu-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r05bu-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland VOELUND_EDGE_FLOW_ARTIFACT_DIR="$PWD/r05bu-ui" ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r05bu-ctest.xml" > r05bu-ctest.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r05bu-source-paths.txt > r05bu-source-hashes.txt
tar -cf r05bu-ui.tar r05bu-ui
