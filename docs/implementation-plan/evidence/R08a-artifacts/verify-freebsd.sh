set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r08a-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r08a-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r08a-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r08a-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r08a-ctest.xml" > r08a-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_intersection_tests > r08a-intersections.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r08a-source-paths.txt > r08a-source-hashes.txt
printf 'FreeBSD R08a complete\n'
