set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r06a-source-delta.tar
while IFS= read -r path; do touch "$path"; done < r06a-source-delta.txt
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > r06a-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> r06a-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/r06a-ctest.xml" > r06a-ctest.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_sketch_reference_review_tests build/freebsd-clang-lean-occt/tests tests/fixtures/documents > r06a-review.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < r06a-source-paths.txt > r06a-source-hashes.txt
