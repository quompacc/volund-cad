set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r07-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r07-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r07-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r07-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r07-ctest.xml" > r07-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_contour_graph_tests > r07-contours.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r07-source-paths.txt > r07-source-hashes.txt
printf 'FreeBSD R07 complete\n'
