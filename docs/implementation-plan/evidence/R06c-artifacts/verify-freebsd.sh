set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r06c-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r06c-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r06c-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r06c-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland VOELUND_REFERENCE_REPAIR_ARTIFACT_DIR="$PWD/r06c-ui" ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r06c-ctest.xml" > r06c-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_repair_tests read-pending r06c-windows-source.volund > r06c-exchange.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_repair_tests read-fixed r06c-windows-fixed.volund >> r06c-exchange.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_repair_tests read-fixed r06c-windows-multiple-fixed.volund >> r06c-exchange.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r06c-source-paths.txt > r06c-source-hashes.txt
printf 'FreeBSD R06c complete\n'
