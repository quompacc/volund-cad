if(NOT DEFINED VOELUND_SOURCE_FILE OR VOELUND_SOURCE_FILE STREQUAL "")
    message(FATAL_ERROR "VOELUND_SOURCE_FILE is required for cyclomatic-complexity checking")
endif()
if(NOT EXISTS "${VOELUND_SOURCE_FILE}")
    message(FATAL_ERROR "VOELUND_SOURCE_FILE does not exist: ${VOELUND_SOURCE_FILE}")
endif()
if(NOT DEFINED VOELUND_CLANGXX OR VOELUND_CLANGXX STREQUAL "")
    message(FATAL_ERROR
        "Cyclomatic-complexity checking requires an LLVM clang++ frontend (version 22 or newer). "
        "clang-tidy's readability-function-cognitive-complexity is not a substitute."
    )
endif()
if(NOT EXISTS "${VOELUND_CLANGXX}")
    message(FATAL_ERROR "VOELUND_CLANGXX does not exist: ${VOELUND_CLANGXX}")
endif()

execute_process(
    COMMAND "${VOELUND_CLANGXX}" --version
    RESULT_VARIABLE clang_version_result
    OUTPUT_VARIABLE clang_version_output
    ERROR_VARIABLE clang_version_error
)
if(NOT clang_version_result EQUAL 0)
    message(FATAL_ERROR "Unable to query VOELUND_CLANGXX: ${clang_version_error}")
endif()
if(NOT clang_version_output MATCHES "clang version ([0-9]+)")
    message(FATAL_ERROR "VOELUND_CLANGXX is not an LLVM clang frontend: ${clang_version_output}")
endif()
if(CMAKE_MATCH_1 LESS 22)
    message(FATAL_ERROR "VOELUND_CLANGXX must be version 22 or newer: ${clang_version_output}")
endif()

# A Clang AST/CFG consumer is intentionally required here.  CMake's text
# processing cannot distinguish C++ control flow from comments, strings,
# macros, templates, or unevaluated expressions and must not claim to measure
# McCabe complexity.  A02 records the missing consumer; A04 wires the selected
# consumer into the mandatory quality target after the toolchain is available.
message(FATAL_ERROR
    "No approved Clang CFG consumer is configured. Cyclomatic-complexity checking remains mandatory; "
    "see docs/quality-tooling.md for the required tool contract."
)
