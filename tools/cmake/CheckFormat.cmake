# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# Mandatory format check: every owned C++ file is formatted as .clang-format says.
#
#   cmake -DVOELUND_SOURCE_ROOT=<repository> -DVOELUND_CLANG_FORMAT=<clang-format> -P CheckFormat.cmake
#
# clang-format output changes between major versions; the check accepts LLVM 22 only. A
# missing tool fails: a skipped check would look like a clean tree.
cmake_policy(SET CMP0007 NEW)
foreach(required VOELUND_SOURCE_ROOT VOELUND_CLANG_FORMAT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "" OR "${${required}}" MATCHES "-NOTFOUND$")
        message(FATAL_ERROR "The format check requires ${required} (clang-format of LLVM 22)")
    endif()
endforeach()
if(NOT EXISTS "${VOELUND_CLANG_FORMAT}")
    message(FATAL_ERROR "VOELUND_CLANG_FORMAT does not exist: ${VOELUND_CLANG_FORMAT}")
endif()
execute_process(COMMAND "${VOELUND_CLANG_FORMAT}" --version
    OUTPUT_VARIABLE version RESULT_VARIABLE version_result)
if(NOT version_result EQUAL 0 OR NOT version MATCHES "clang-format version 22\\.")
    message(FATAL_ERROR "The format check needs clang-format 22, found: ${version}")
endif()

set(directories apps include src tests tools third_party/planegcs/compat)
set(files "")
foreach(directory IN LISTS directories)
    file(GLOB_RECURSE found LIST_DIRECTORIES false
        "${VOELUND_SOURCE_ROOT}/${directory}/*.cpp" "${VOELUND_SOURCE_ROOT}/${directory}/*.hpp"
        "${VOELUND_SOURCE_ROOT}/${directory}/*.h" "${VOELUND_SOURCE_ROOT}/${directory}/*.cc")
    list(APPEND files ${found})
endforeach()
list(LENGTH files count)
if(count EQUAL 0)
    message(FATAL_ERROR "No C++ files found under ${VOELUND_SOURCE_ROOT}")
endif()

# One clang-format call per batch keeps the command line short on Windows.
set(unformatted "")
set(batch "")
list(APPEND files "<end>")
foreach(file IN LISTS files)
    if(NOT file STREQUAL "<end>")
        list(APPEND batch "${file}")
        list(LENGTH batch size)
        if(size LESS 40)
            continue()
        endif()
    endif()
    if(batch)
        execute_process(
            COMMAND "${VOELUND_CLANG_FORMAT}" --dry-run --Werror --style=file ${batch}
            WORKING_DIRECTORY "${VOELUND_SOURCE_ROOT}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE error)
        if(NOT result EQUAL 0)
            string(REGEX MATCHALL "[^\n]+: error: code should be clang-formatted" hits "${error}")
            if(NOT hits)
                message(FATAL_ERROR "clang-format failed (${result}):\n${error}")
            endif()
            foreach(hit IN LISTS hits)
                string(REGEX REPLACE ":[0-9]+:[0-9]+: error: code should be clang-formatted$" "" path "${hit}")
                list(APPEND unformatted "${path}")
            endforeach()
        endif()
        set(batch "")
    endif()
endforeach()

if(unformatted)
    list(REMOVE_DUPLICATES unformatted)
    list(LENGTH unformatted bad)
    list(JOIN unformatted "\n  " text)
    message(FATAL_ERROR "${bad} files are not clang-formatted (run clang-format -i on them):\n  ${text}")
endif()
message(STATUS "Format checked in ${count} files")
