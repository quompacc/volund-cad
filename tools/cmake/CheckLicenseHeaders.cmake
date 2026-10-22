# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# LIZENZ (decision of 2026-10-22): every own source, header, CMake and script file names its
# licence in an SPDX line among its first five lines. third_party/ and spikes/ are not ours and
# are not looked at.
cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED VOELUND_SOURCE_ROOT OR VOELUND_SOURCE_ROOT STREQUAL "")
    message(FATAL_ERROR "VOELUND_SOURCE_ROOT is required")
endif()
if(NOT IS_DIRECTORY "${VOELUND_SOURCE_ROOT}")
    message(FATAL_ERROR "VOELUND_SOURCE_ROOT must be an existing directory: ${VOELUND_SOURCE_ROOT}")
endif()

set(license_directories apps cmake include src tests tools)
set(license_patterns *.c *.cpp *.h *.hpp *.rc *.cmake *.sh *.ps1 *.psm1 *.py CMakeLists.txt)
set(missing "")
set(checked_file_count 0)

foreach(license_directory IN LISTS license_directories)
    foreach(license_pattern IN LISTS license_patterns)
        file(
            GLOB_RECURSE candidate_files
            LIST_DIRECTORIES false
            "${VOELUND_SOURCE_ROOT}/${license_directory}/${license_pattern}"
        )
        foreach(candidate_file IN LISTS candidate_files)
            math(EXPR checked_file_count "${checked_file_count} + 1")
            file(STRINGS "${candidate_file}" first_lines LIMIT_COUNT 5)
            set(found FALSE)
            foreach(line IN LISTS first_lines)
                if(line MATCHES "SPDX-License-Identifier: [A-Za-z0-9.+-]+")
                    set(found TRUE)
                endif()
            endforeach()
            if(NOT found)
                list(APPEND missing "${candidate_file}")
            endif()
        endforeach()
    endforeach()
endforeach()

if(checked_file_count EQUAL 0)
    message(FATAL_ERROR "No source files found under VOELUND_SOURCE_ROOT: ${VOELUND_SOURCE_ROOT}")
endif()
message(STATUS "Checked ${checked_file_count} files for their licence line")

if(missing)
    list(JOIN missing "\n  " missing_text)
    message(FATAL_ERROR "Files without an SPDX-License-Identifier line:\n  ${missing_text}")
endif()

message(STATUS "Licence header check passed")
