set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r06d-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r06d-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r06d-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r06d-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland VOELUND_BATCH_REPAIR_ARTIFACT_DIR="$PWD/r06d-ui" ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r06d-ctest.xml" > r06d-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_repair_tests read-fixed r06d-windows-fixed.volund > r06d-exchange.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r06d-source-paths.txt > r06d-source-hashes.txt
printf 'FreeBSD R06d complete\n'
