set -eu
cd /home/eduard/src/voelund-p04-20260913
prefix="$PWD/build/d11d-occt/install"
cmake -S tools/ocaf-probe -B build/d11d-ocaf-freebsd -G Ninja -DCMAKE_BUILD_TYPE=Release -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > d11d-probe-build.log 2>&1
cmake --build build/d11d-ocaf-freebsd -j4 >> d11d-probe-build.log 2>&1
ctest --test-dir build/d11d-ocaf-freebsd --output-on-failure --output-junit "$PWD/d11d-probe-ctest.xml" > d11d-probe-ctest.log 2>&1
touch include/voelund/document/DocumentId.hpp src/document/DocumentId.cpp src/document/CMakeLists.txt
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > d11d-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> d11d-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/d11d-ctest.xml" > d11d-ctest.log 2>&1
