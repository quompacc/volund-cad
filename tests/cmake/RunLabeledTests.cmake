# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

if(NOT DEFINED VOELUND_CTEST_COMMAND OR VOELUND_CTEST_COMMAND STREQUAL "")
    message(FATAL_ERROR "VOELUND_CTEST_COMMAND is required")
endif()
if(NOT DEFINED VOELUND_BINARY_DIR OR VOELUND_BINARY_DIR STREQUAL "")
    message(FATAL_ERROR "VOELUND_BINARY_DIR is required")
endif()
if(NOT DEFINED VOELUND_TEST_LABEL OR VOELUND_TEST_LABEL STREQUAL "")
    message(FATAL_ERROR "VOELUND_TEST_LABEL is required")
endif()

execute_process(
    COMMAND "${VOELUND_CTEST_COMMAND}" -N -L "${VOELUND_TEST_LABEL}"
    WORKING_DIRECTORY "${VOELUND_BINARY_DIR}"
    RESULT_VARIABLE list_result
    OUTPUT_VARIABLE list_output
    ERROR_VARIABLE list_error
)
if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "Unable to list CTest label '${VOELUND_TEST_LABEL}':\n${list_output}\n${list_error}")
endif()
if(NOT list_output MATCHES "Total Tests: ([0-9]+)")
    message(FATAL_ERROR "CTest did not report a test count for label '${VOELUND_TEST_LABEL}':\n${list_output}")
endif()
if(CMAKE_MATCH_1 EQUAL 0)
    message(FATAL_ERROR "Required CTest label '${VOELUND_TEST_LABEL}' selected no tests")
endif()

execute_process(
    COMMAND "${VOELUND_CTEST_COMMAND}" --output-on-failure -L "${VOELUND_TEST_LABEL}"
    WORKING_DIRECTORY "${VOELUND_BINARY_DIR}"
    RESULT_VARIABLE test_result
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)
if(NOT test_result EQUAL 0)
    message(FATAL_ERROR "CTest label '${VOELUND_TEST_LABEL}' failed:\n${test_output}\n${test_error}")
endif()
message(STATUS "CTest label '${VOELUND_TEST_LABEL}' passed with ${CMAKE_MATCH_1} selected tests")
