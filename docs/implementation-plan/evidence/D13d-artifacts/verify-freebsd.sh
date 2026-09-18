set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf d13d-source-delta.tar
touch tests/CMakeLists.txt tests/integration/ProductFileExchangeTests.cpp tests/integration/CadSessionSafePersistenceTests.cpp
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > d13d-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> d13d-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/d13d-ctest.xml" > d13d-ctest.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < d13d-source-paths.txt > d13d-source-hashes.txt
testbin="$PWD/build/freebsd-clang-lean-occt/tests/voelund_product_exchange_tests"
"$testbin" create "$PWD/d13d-exchange/freebsd"
"$testbin" read "$PWD/d13d-exchange/windows" "$PWD/d13d-exchange/windows-via-freebsd"
ldd "$testbin" > d13d-ldd.txt
uname -a > d13d-environment.txt
grep OCC_VERSION_COMPLETE "$prefix/include/opencascade/Standard_Version.hxx" >> d13d-environment.txt
