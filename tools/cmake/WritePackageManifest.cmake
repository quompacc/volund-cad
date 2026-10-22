# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# L04: writes MANIFEST.txt into an installed package from what was actually staged: every
# component with version and licence, then every file with its SHA-256 and component. Runs as
# the last install step; CheckPackageNotices.cmake compares it with the package.
#
#   cmake -DVOELUND_PREFIX=<prefix> -DVOELUND_COMPONENTS=<Components.cmake>
#         -DVOELUND_COMPONENT_VERSIONS=<component-versions.cmake> -P WritePackageManifest.cmake
cmake_policy(SET CMP0007 NEW)
include("${CMAKE_CURRENT_LIST_DIR}/PackageComponents.cmake")
if(NOT IS_DIRECTORY "${VOELUND_PREFIX}")
    message(FATAL_ERROR "VOELUND_PREFIX must be the installed package root")
endif()
# A relative prefix (cmake --install --prefix <dir>) is relative to the working directory.
file(REAL_PATH "${VOELUND_PREFIX}" VOELUND_PREFIX)

voelund_package_files("${VOELUND_PREFIX}" files)
set(file_lines "")
set(present "")
foreach(path IN LISTS files)
    voelund_component_of("${path}" component)
    if(component STREQUAL "")
        set(component "UNASSIGNED")
    else()
        list(APPEND present "${component}")
    endif()
    file(SHA256 "${VOELUND_PREFIX}/${path}" hash)
    string(APPEND file_lines "file\t${hash}\t${component}\t${path}\n")
endforeach()

set(component_lines "")
get_property(components GLOBAL PROPERTY voelund_components)
foreach(name IN LISTS components)
    get_property(embedded GLOBAL PROPERTY "voelund_component_${name}_embedded")
    if(NOT embedded AND NOT name IN_LIST present)
        continue()
    endif()
    get_property(version GLOBAL PROPERTY "voelund_component_${name}_version")
    get_property(license GLOBAL PROPERTY "voelund_component_${name}_license")
    string(APPEND component_lines "component\t${name}\t${version}\t${license}\n")
endforeach()

file(WRITE "${VOELUND_PREFIX}/MANIFEST.txt"
    "# Völund CAD package manifest (L04): components, then files (SHA-256, component, path).\n"
    "${component_lines}${file_lines}")
list(LENGTH files count)
message(STATUS "Package manifest: ${count} files")
