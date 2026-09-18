set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r04b-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r04b-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r04b-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r04b-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r04b-ctest.xml" > r04b-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_support_binding_tests read r04b-windows-bound.volund r04b-freebsd-roundtrip.volund > r04b-exchange.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_support_binding_tests create r04b-freebsd-bound.volund >> r04b-exchange.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r04b-source-paths.txt > r04b-source-hashes.txt
