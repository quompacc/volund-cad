set -eu
cd /home/eduard/src/voelund-p04-20260913
tar -xf r02-reference-contract.tar
cmake -S tools/reference-contract -B build/r02-reference-contract -G Ninja -DCMAKE_BUILD_TYPE=Release > r02-build.log 2>&1
cmake --build build/r02-reference-contract >> r02-build.log 2>&1
build/r02-reference-contract/reference_contract > r02-fixtures.log 2>&1
ctest --test-dir build/r02-reference-contract --output-on-failure --output-junit "$PWD/r02-ctest.xml" > r02-ctest.log 2>&1
for path in tools/reference-contract/CMakeLists.txt tools/reference-contract/SubshapeContract.hpp tools/reference-contract/ContractProof.cpp; do
    sha256 -q "$path"
done > r02-source-hashes.txt
