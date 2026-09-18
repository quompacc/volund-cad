set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r06b-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r06b-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r06b-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r06b-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland VOELUND_FEATURE_STATUS_ARTIFACT_DIR="$PWD/r06b-ui" ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r06b-ctest.xml" > r06b-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_repair_tests build/freebsd-clang-lean-occt/tests > r06b-repair.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_repair_tests read-pending r06b-windows-pending.volund > r06b-exchange.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_repair_tests read-fixed r06b-windows-fixed.volund >> r06b-exchange.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r06b-source-paths.txt > r06b-source-hashes.txt
printf 'FreeBSD R06b complete\n'
