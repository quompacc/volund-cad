# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# L02: checks an installed Völund prefix for a complete, self-contained runtime.
#
#   cmake -DVOELUND_PREFIX=<installed prefix> -P tools/cmake/CheckPackageRuntime.cmake
#
# Every library the executable and its plugins need must resolve, and only to the prefix or to
# the operating system: never to a toolchain copy (.tools), a build tree or a developer
# directory. A missing library fails with its name.
cmake_policy(SET CMP0007 NEW)
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW) # paths normalised before matching
endif()
if(NOT DEFINED VOELUND_PREFIX OR NOT IS_DIRECTORY "${VOELUND_PREFIX}")
    message(FATAL_ERROR "VOELUND_PREFIX must be an installed prefix")
endif()
file(REAL_PATH "${VOELUND_PREFIX}" prefix)

if(WIN32)
    set(executable "${prefix}/bin/voelund-cad.exe")
    file(GLOB plugins "${prefix}/bin/platforms/*.dll")
    if(NOT plugins)
        message(FATAL_ERROR "No Qt platform plugin under ${prefix}/bin/platforms")
    endif()
    set(search "${prefix}/bin")
    # Any drive's Windows directory, in any letter case; its DLLs are not scanned further.
    set(system_patterns "^[A-Za-z]:/[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/")
    set(pre_exclude "^api-ms-" "^ext-ms-")
else()
    set(executable "${prefix}/bin/voelund-cad")
    set(plugins "")
    set(search "${prefix}/lib64" "${prefix}/lib")
    set(system_patterns "^/usr/lib" "^/lib")
    set(pre_exclude "^ld-linux")
endif()
if(NOT EXISTS "${executable}")
    message(FATAL_ERROR "Installed executable missing: ${executable}")
endif()

file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${executable}"
    MODULES ${plugins}
    DIRECTORIES ${search}
    PRE_EXCLUDE_REGEXES ${pre_exclude}
    POST_EXCLUDE_REGEXES ${system_patterns}
    RESOLVED_DEPENDENCIES_VAR resolved
    UNRESOLVED_DEPENDENCIES_VAR unresolved)

set(outside "")
foreach(library IN LISTS resolved)
    file(TO_CMAKE_PATH "${library}" library)
    string(FIND "${library}" "${prefix}/" in_prefix)
    if(in_prefix EQUAL 0)
        continue()
    endif()
    set(system FALSE)
    foreach(pattern IN LISTS system_patterns)
        if(library MATCHES "${pattern}")
            set(system TRUE)
        endif()
    endforeach()
    if(NOT system)
        list(APPEND outside "${library}")
    endif()
endforeach()

if(unresolved)
    list(JOIN unresolved "\n  " text)
    message(FATAL_ERROR "The installed runtime is incomplete; missing:\n  ${text}")
endif()
if(outside)
    list(JOIN outside "\n  " text)
    message(FATAL_ERROR "The installed product depends on files outside the prefix and the system:\n  ${text}")
endif()
list(LENGTH resolved count)
message(STATUS "Installed runtime complete: ${count} libraries, all in the prefix or the system")
