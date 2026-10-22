# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

cmake_minimum_required(VERSION 3.28)

foreach(required IN ITEMS VOELUND_SOURCE_ROOT VOELUND_TEST_ROOT VOELUND_CLANG_TIDY)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef isolation_run_id)
set(fixture_parent "${VOELUND_TEST_ROOT}/isolation-${isolation_run_id}")
if(EXISTS "${fixture_parent}")
    message(FATAL_ERROR "Isolation fixture already exists: ${fixture_parent}")
endif()
file(MAKE_DIRECTORY "${fixture_parent}")
set(sentinel "${fixture_parent}/caller-owned.txt")
file(WRITE "${sentinel}" "Caller-owned content must survive both test runs.\n")
file(SHA256 "${sentinel}" original_hash)

foreach(run RANGE 1 2)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DVOELUND_SOURCE_ROOT=${VOELUND_SOURCE_ROOT}"
            "-DVOELUND_TEST_ROOT=${fixture_parent}"
            "-DVOELUND_CLANG_TIDY=${VOELUND_CLANG_TIDY}"
            -P "${CMAKE_CURRENT_LIST_DIR}/QualityRulesTests.cmake"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Quality fixture run ${run} failed:\n${output}${error}")
    endif()
    if(NOT EXISTS "${sentinel}")
        message(FATAL_ERROR "Quality fixtures deleted caller-owned data")
    endif()
    file(SHA256 "${sentinel}" current_hash)
    if(NOT current_hash STREQUAL original_hash)
        message(FATAL_ERROR "Quality fixtures changed caller-owned data")
    endif()
    file(GLOB fixture_runs LIST_DIRECTORIES true "${fixture_parent}/quality-rules-*")
    list(LENGTH fixture_runs run_count)
    if(NOT run_count EQUAL run)
        message(FATAL_ERROR "Each quality fixture run must retain its own directory")
    endif()
    if(run EQUAL 1)
        list(GET fixture_runs 0 first_run)
        set(previous_fixture "${first_run}/clean.cpp")
        file(SHA256 "${previous_fixture}" previous_hash)
    else()
        if(NOT EXISTS "${previous_fixture}")
            message(FATAL_ERROR "Quality fixtures removed a previous test run")
        endif()
        file(SHA256 "${previous_fixture}" retained_hash)
        if(NOT retained_hash STREQUAL previous_hash)
            message(FATAL_ERROR "Quality fixtures overwrote a previous test run")
        endif()
    endif()
endforeach()

message(STATUS "Quality fixture isolation passed: caller data and previous run preserved")
