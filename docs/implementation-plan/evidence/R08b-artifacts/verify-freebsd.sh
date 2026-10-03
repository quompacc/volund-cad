set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r08b-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r08b-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r08b-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r08b-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r08b-ctest.xml" > r08b-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_region_tests > r08b-regions.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r08b-source-paths.txt > r08b-source-hashes.txt
printf 'FreeBSD R08b complete\n'
