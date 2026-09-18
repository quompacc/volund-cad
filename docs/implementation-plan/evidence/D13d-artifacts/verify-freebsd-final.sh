set -eu
cd /home/eduard/src/voelund-p04-20260913
cmake --build --preset freebsd-clang-lean-occt --target voelund_safe_persistence_tests -j4 > d13d-reference-build.log 2>&1
ctest --test-dir build/freebsd-clang-lean-occt -R application.safe_persistence --output-on-failure --output-junit "$PWD/d13d-reference.xml" > d13d-reference.log 2>&1
build/freebsd-clang-lean-occt/tests/voelund_product_exchange_tests read d13d-exchange/freebsd-via-windows > d13d-return.log 2>&1
while IFS= read -r path; do sha256 -q "$path"; done < d13d-source-paths.txt > d13d-final-source-hashes.txt
find d13d-exchange -type f -name '*.volund' -exec sha256 {} \; > d13d-exchange-hashes.txt
