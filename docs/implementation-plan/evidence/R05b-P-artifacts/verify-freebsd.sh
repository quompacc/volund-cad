set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r05bp-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r05bp-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r05bp-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r05bp-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r05bp-ctest.xml" > r05bp-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_projected_edge_tests > r05bp-projected-edge.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_projected_edge_tests read r05bp-windows.volund > r05bp-exchange.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_projected_edge_tests write r05bp-freebsd.volund >> r05bp-exchange.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r05bp-source-paths.txt > r05bp-source-hashes.txt
