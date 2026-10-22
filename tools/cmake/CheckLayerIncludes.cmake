# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# K01 include check: every #include of src/<layer> and include/voelund/<layer> stays within
# the layers allowed by VoelundLayers.cmake, and layers below the UI include no Qt header.
#
#   cmake -DVOELUND_SOURCE_ROOT=<repository> -P tools/cmake/CheckLayerIncludes.cmake
#
# Include directives are preprocessor lines, so reading them line by line is exact apart from
# includes hidden in block comments, which then count as edges too.
cmake_policy(SET CMP0007 NEW)
if(NOT DEFINED VOELUND_SOURCE_ROOT OR VOELUND_SOURCE_ROOT STREQUAL "")
    message(FATAL_ERROR "VOELUND_SOURCE_ROOT is required")
endif()
if(NOT IS_DIRECTORY "${VOELUND_SOURCE_ROOT}/src")
    message(FATAL_ERROR "No src directory under VOELUND_SOURCE_ROOT: ${VOELUND_SOURCE_ROOT}")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/VoelundLayers.cmake")

set(violations "")
set(checked 0)
foreach(layer IN LISTS VOELUND_LAYERS)
    file(GLOB_RECURSE files LIST_DIRECTORIES false
        "${VOELUND_SOURCE_ROOT}/src/${layer}/*.cpp" "${VOELUND_SOURCE_ROOT}/src/${layer}/*.hpp"
        "${VOELUND_SOURCE_ROOT}/src/${layer}/*.h"
        "${VOELUND_SOURCE_ROOT}/include/voelund/${layer}/*.hpp")
    foreach(file IN LISTS files)
        math(EXPR checked "${checked} + 1")
        file(STRINGS "${file}" lines REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"]")
        file(RELATIVE_PATH shown "${VOELUND_SOURCE_ROOT}" "${file}")
        foreach(line IN LISTS lines)
            if(NOT line MATCHES "#[ \t]*include[ \t]*[<\"]([^>\"]+)[>\"]")
                continue()
            endif()
            set(header "${CMAKE_MATCH_1}")
            if(header MATCHES "^voelund/([a-z]+)/" AND NOT CMAKE_MATCH_1 IN_LIST VOELUND_LAYER_ALLOWED_${layer})
                list(APPEND violations "${shown} (${layer}) includes ${header}")
            elseif(header MATCHES "^(Q[A-Za-z]*|Qt[A-Za-z]*/.*)$" AND layer IN_LIST VOELUND_LAYERS_WITHOUT_QT)
                list(APPEND violations "${shown} (${layer}) includes Qt header ${header}")
            elseif(header MATCHES "(^|/)\\.\\./")
                list(APPEND violations "${shown} (${layer}) includes ${header} by a relative path out of its directory")
            endif()
        endforeach()
    endforeach()
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR "No layer sources found under ${VOELUND_SOURCE_ROOT}")
endif()
if(violations)
    list(JOIN violations "\n  " text)
    message(FATAL_ERROR "Forbidden layer include (K01):\n  ${text}")
endif()
message(STATUS "Layer includes checked in ${checked} files")
