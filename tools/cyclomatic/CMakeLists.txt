# Build-only quality tool: McCabe cyclomatic complexity on Clang's CFG (A02).
# Not part of the CAD product; built on its own against the distribution's Clang:
#   cmake -S tools/cyclomatic -B <dir> -G Ninja && cmake --build <dir> && ctest --test-dir <dir>
cmake_minimum_required(VERSION 3.25)
project(voelund_cyclomatic LANGUAGES CXX)

# Clang's C++ API changes between major versions; the tool is tested against 22 only.
find_package(Clang REQUIRED CONFIG)
if(NOT LLVM_VERSION_MAJOR EQUAL 22)
    message(FATAL_ERROR "voelund-cyclomatic is verified against LLVM 22, found ${LLVM_PACKAGE_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Built-in headers (stddef.h, ...) of the same Clang, asked from its driver.
find_program(VOELUND_CLANG clang HINTS "${LLVM_TOOLS_BINARY_DIR}" NO_DEFAULT_PATH REQUIRED)
execute_process(COMMAND "${VOELUND_CLANG}" -print-resource-dir
    OUTPUT_VARIABLE voelund_resource_dir OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)

add_executable(voelund-cyclomatic main.cpp CyclomaticComplexity.cpp)
target_include_directories(voelund-cyclomatic SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(voelund-cyclomatic PRIVATE ${LLVM_DEFINITIONS}
    VOELUND_CLANG_RESOURCE_DIR="${voelund_resource_dir}")
if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(voelund-cyclomatic PRIVATE -fno-rtti)
endif()
if(TARGET clang-cpp)
    target_link_libraries(voelund-cyclomatic PRIVATE clang-cpp LLVM)
else()
    target_link_libraries(voelund-cyclomatic PRIVATE clangTooling clangFrontend clangAnalysis
        clangAST clangBasic)
endif()

enable_testing()
get_filename_component(voelund_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_test(NAME quality.cyclomatic_fixtures
    COMMAND "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${voelund_source_root}"
        "-DVOELUND_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/fixtures"
        "-DVOELUND_CYCLOMATIC_TOOL=$<TARGET_FILE:voelund-cyclomatic>"
        -P "${voelund_source_root}/tests/cmake/CyclomaticComplexityTests.cmake")
