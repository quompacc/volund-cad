# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED VOELUND_SOURCE_ROOT OR VOELUND_SOURCE_ROOT STREQUAL "")
    message(FATAL_ERROR "VOELUND_SOURCE_ROOT is required")
endif()
if(NOT DEFINED VOELUND_TEST_ROOT OR VOELUND_TEST_ROOT STREQUAL "")
    message(FATAL_ERROR "VOELUND_TEST_ROOT is required")
endif()
if(NOT DEFINED VOELUND_CLANG_TIDY OR VOELUND_CLANG_TIDY STREQUAL "")
    message(FATAL_ERROR "VOELUND_CLANG_TIDY is required; quality checks must not silently skip clang-tidy")
endif()
if(NOT EXISTS "${VOELUND_CLANG_TIDY}")
    message(FATAL_ERROR "VOELUND_CLANG_TIDY does not exist: ${VOELUND_CLANG_TIDY}")
endif()

# The caller owns the parent directory. Keep each run in a fresh child so
# fixtures cannot delete or overwrite unrelated files or another test run.
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef fixture_run_id)
set(VOELUND_TEST_ROOT "${VOELUND_TEST_ROOT}/quality-rules-${fixture_run_id}")
if(EXISTS "${VOELUND_TEST_ROOT}")
    message(FATAL_ERROR "Fixture directory already exists: ${VOELUND_TEST_ROOT}")
endif()
file(MAKE_DIRECTORY "${VOELUND_TEST_ROOT}")
file(MAKE_DIRECTORY "${VOELUND_TEST_ROOT}/source-length/src")

function(write_fixture name contents)
    set(path "${VOELUND_TEST_ROOT}/${name}.cpp")
    file(WRITE "${path}" "${contents}")
    set(${name}_PATH "${path}" PARENT_SCOPE)
endfunction()

function(expect_tidy_failure fixture expected_message)
    execute_process(
        COMMAND "${VOELUND_CLANG_TIDY}"
            "--config-file=${VOELUND_SOURCE_ROOT}/.clang-tidy"
            "--checks=-*,readability-function-size"
            "--warnings-as-errors=readability-function-size"
            "${${fixture}_PATH}"
            -- -std=c++20
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(combined "${output}\n${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "${fixture} unexpectedly passed readability-function-size")
    endif()
    if(NOT combined MATCHES "${expected_message}")
        message(FATAL_ERROR "${fixture} failed without the expected diagnostic '${expected_message}':\n${combined}")
    endif()
endfunction()

set(long_source "")
foreach(line RANGE 1 601)
    string(APPEND long_source "// ${line}\n")
endforeach()
file(WRITE "${VOELUND_TEST_ROOT}/source-length/src/too_long.cpp" "${long_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${VOELUND_TEST_ROOT}/source-length"
        -P "${VOELUND_SOURCE_ROOT}/tools/cmake/CheckSourceLimits.cmake"
    RESULT_VARIABLE source_length_result
    OUTPUT_VARIABLE source_length_output
    ERROR_VARIABLE source_length_error
)
if(source_length_result EQUAL 0)
    message(FATAL_ERROR "601-line source fixture unexpectedly passed")
endif()
set(source_length_combined "${source_length_output}\n${source_length_error}")
if(NOT source_length_combined MATCHES "too_long.cpp: 601 lines")
    message(FATAL_ERROR "601-line fixture failed without the expected diagnostic:\n${source_length_combined}")
endif()

write_fixture(clean "int clean(int value) {\n    if (value > 0) {\n        return value;\n    }\n    return 0;\n}\n")
execute_process(
    COMMAND "${VOELUND_CLANG_TIDY}"
        "--config-file=${VOELUND_SOURCE_ROOT}/.clang-tidy"
        "--checks=-*,readability-function-size"
        "--warnings-as-errors=readability-function-size"
        "${clean_PATH}"
        -- -std=c++20
    RESULT_VARIABLE clean_result
    OUTPUT_VARIABLE clean_output
    ERROR_VARIABLE clean_error
)
if(NOT clean_result EQUAL 0)
    message(FATAL_ERROR "clean fixture failed:\n${clean_output}\n${clean_error}")
endif()

set(long_function "int tooLong() {\n")
foreach(line RANGE 1 81)
    string(APPEND long_function "    int value${line} = ${line};\n")
endforeach()
string(APPEND long_function "    return 0;\n}\n")
write_fixture(function_length "${long_function}")
write_fixture(parameters "int tooManyParameters(int a, int b, int c, int d, int e, int f) {\n    return a + b + c + d + e + f;\n}\n")
write_fixture(nesting "int tooDeep(int value) {\n    if (value > 0) {\n        if (value > 1) {\n            if (value > 2) {\n                if (value > 3) {\n                    if (value > 4) {\n                        return value;\n                    }\n                }\n            }\n        }\n    }\n    return 0;\n}\n")

expect_tidy_failure(function_length "exceeds recommended size/complexity thresholds")
expect_tidy_failure(parameters "6 parameters \\(threshold 5\\)")
expect_tidy_failure(nesting "nesting level 5 starts here \\(threshold 4\\)")

write_fixture(cyclomatic "int tooComplex(int value) {\n    if (value == 1) return 1;\n    if (value == 2) return 2;\n    if (value == 3) return 3;\n    if (value == 4) return 4;\n    if (value == 5) return 5;\n    if (value == 6) return 6;\n    if (value == 7) return 7;\n    if (value == 8) return 8;\n    if (value == 9) return 9;\n    if (value == 10) return 10;\n    return 0;\n}\n")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_FILE=${cyclomatic_PATH}"
        -P "${VOELUND_SOURCE_ROOT}/tools/cmake/CheckCyclomaticComplexity.cmake"
    RESULT_VARIABLE cyclomatic_result
    OUTPUT_VARIABLE cyclomatic_output
    ERROR_VARIABLE cyclomatic_error
)
if(cyclomatic_result EQUAL 0)
    message(FATAL_ERROR "Cyclomatic-complexity check unexpectedly passed without the voelund-cyclomatic tool")
endif()
set(cyclomatic_combined "${cyclomatic_output}\n${cyclomatic_error}")
if(NOT cyclomatic_combined MATCHES "requires the voelund-cyclomatic")
    message(FATAL_ERROR "Cyclomatic-complexity failure did not explain the missing mandatory tool:\n${cyclomatic_combined}")
endif()

message(STATUS "Quality-rule fixtures passed; cyclomatic checking without its tool correctly fails (values: CyclomaticComplexityTests.cmake)")
