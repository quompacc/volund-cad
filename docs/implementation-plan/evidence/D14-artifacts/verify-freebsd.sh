set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf d14-source-delta.tar
touch tests/CMakeLists.txt tests/integration/MigrationAcceptanceTests.cpp tests/integration/LegacyConstraintMigrationTests.cpp
prefix="$PWD/build/d11d-occt/install"
cmake --preset freebsd-clang-lean-occt -DCMAKE_PREFIX_PATH="$prefix" -DOpenCASCADE_DIR="$prefix/lib/cmake/opencascade" > d14-build.log 2>&1
cmake --build --preset freebsd-clang-lean-occt -j4 >> d14-build.log 2>&1
env -u DISPLAY XDG_RUNTIME_DIR=/var/run/xdg/eduard WAYLAND_DISPLAY=wayland-1 QT_QPA_PLATFORM=wayland ctest --preset freebsd-clang-lean-occt --output-on-failure --output-junit "$PWD/d14-ctest.xml" > d14-ctest.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < d14-source-paths.txt > d14-source-hashes.txt
build/freebsd-clang-lean-occt/tests/voelund_migration_acceptance_tests tests/fixtures/documents > d14-migration-cases.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_legacy_constraint_migration_tests tests/fixtures/documents > d14-constraint-cases.log 2>&1
