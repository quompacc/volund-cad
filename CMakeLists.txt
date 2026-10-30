cmake_minimum_required(VERSION 3.28)

project(
    VoelundCAD
    VERSION 1.0.0
    DESCRIPTION "Native parametric CAD application"
    LANGUAGES CXX
)

# CMake 4.2+: install(RUNTIME_DEPENDENCY_SET) matches normalised paths (L02).
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(VOELUND_BUILD_DESKTOP "Build the Qt desktop application" ON)
option(VOELUND_WARNINGS_AS_ERRORS "Treat project warnings as errors" ON)
set(VOELUND_VIEWPORT_BACKEND "native" CACHE STRING "Viewport backend: native or qt-opengl")
set_property(CACHE VOELUND_VIEWPORT_BACKEND PROPERTY STRINGS native qt-opengl)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")
include(VoelundWarnings)
include(VoelundLayers)

# A04: clang-format of LLVM 22 for the format check (Visual Studio ships it with its LLVM tools).
find_program(VOELUND_CLANG_FORMAT clang-format HINTS "$ENV{VCINSTALLDIR}Tools/Llvm/x64/bin")

find_package(OpenCASCADE CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
if(Eigen3_VERSION VERSION_LESS 3.4)
    message(FATAL_ERROR "Völund requires Eigen 3.4 or newer")
endif()
# Boost ships BoostConfig.cmake on every target; CMake 3.30+ removed FindBoost.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()
find_package(Boost 1.75 REQUIRED COMPONENTS graph)

# FreeBSD and Linux desktops share the native Wayland/EGL path (ADR 0004, ADR 0011).
set(VOELUND_NATIVE_WAYLAND OFF)
set(VOELUND_PLATFORM_LABEL "")
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    set(VOELUND_NATIVE_WAYLAND ON)
    set(VOELUND_PLATFORM_LABEL freebsd)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(VOELUND_NATIVE_WAYLAND ON)
    set(VOELUND_PLATFORM_LABEL linux)
endif()

if(VOELUND_BUILD_DESKTOP)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Network Widgets)
    if(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
        find_package(Qt6 CONFIG REQUIRED COMPONENTS OpenGLWidgets)
        if(VOELUND_NATIVE_WAYLAND)
            find_package(OpenGL REQUIRED COMPONENTS EGL GLES2)
        elseif(WIN32)
            find_package(OpenGL REQUIRED)
        endif()
    elseif(NOT VOELUND_VIEWPORT_BACKEND STREQUAL "native")
        message(FATAL_ERROR "Unknown VOELUND_VIEWPORT_BACKEND: ${VOELUND_VIEWPORT_BACKEND}")
    endif()
endif()

add_library(voelund_project_options INTERFACE)
target_compile_features(voelund_project_options INTERFACE cxx_std_20)
voelund_enable_warnings(voelund_project_options)
if(MSVC)
    # Ninja compiles translation units concurrently; synchronize writes to MSVC debug PDBs.
    target_compile_options(voelund_project_options INTERFACE /FS)
    # MSVC 14.51 can emit LNK1163 for valid COMDAT data during incremental debug links.
    target_link_options(voelund_project_options INTERFACE /INCREMENTAL:NO)
endif()

# L10: the product version with its pre-release label (SemVer: 1.0.0-rc.1); empty for a release.
set(VOELUND_PRERELEASE "rc.1" CACHE STRING "Pre-release label of the product version, empty for a release")
if(VOELUND_PRERELEASE STREQUAL "")
    set(VOELUND_FULL_VERSION "${PROJECT_VERSION}")
else()
    set(VOELUND_FULL_VERSION "${PROJECT_VERSION}-${VOELUND_PRERELEASE}")
endif()

# L03: version, source revision and build type for the product and its diagnostics, written
# on every build and changed only when they change (tools/cmake/WriteBuildInfo.cmake).
set(VOELUND_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated")
add_custom_target(voelund_build_info ALL
    COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DVOELUND_OUTPUT=${VOELUND_GENERATED_DIR}/voelund/BuildInfo.hpp"
        "-DVOELUND_VERSION=${VOELUND_FULL_VERSION}" "-DVOELUND_BUILD_TYPE=$<CONFIG>"
        -P "${PROJECT_SOURCE_DIR}/tools/cmake/WriteBuildInfo.cmake"
    BYPRODUCTS "${VOELUND_GENERATED_DIR}/voelund/BuildInfo.hpp"
    VERBATIM)
add_library(voelund_build_info_headers INTERFACE)
target_include_directories(voelund_build_info_headers INTERFACE "${VOELUND_GENERATED_DIR}")
add_dependencies(voelund_build_info_headers voelund_build_info)

add_subdirectory(third_party/planegcs)
add_subdirectory(src/kernel)
add_subdirectory(src/document)
add_subdirectory(src/features)
add_subdirectory(src/platform)
add_subdirectory(src/application)
add_subdirectory(src/workspaces)
add_subdirectory(src/modules/solid)

if(VOELUND_BUILD_DESKTOP)
    add_subdirectory(src/viewport)
    add_subdirectory(src/mcp)
    add_subdirectory(src/assistant)
    add_subdirectory(src/desktop)
    add_subdirectory(apps/voelund-cad)
    # L04: notices, licence texts, PlaneGCS source and the manifest; installed last.
    add_subdirectory(packaging)
endif()

# K01: link edges between the layer targets (VoelundLayers.cmake).
voelund_check_layer_links()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_custom_target(
    format-check
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DVOELUND_CLANG_FORMAT=${VOELUND_CLANG_FORMAT}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/CheckFormat.cmake"
    VERBATIM
)

add_custom_target(
    layer-check
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/CheckLayerIncludes.cmake"
    VERBATIM
)

add_custom_target(
    check-source-limits
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/CheckSourceLimits.cmake"
    VERBATIM
)

# L02 (user decision 2026-10-17): a portable ZIP on Windows and a tarball on Linux, each
# registered per user by the scripts at its top level (docs/packaging.md).
if(VOELUND_BUILD_DESKTOP)
    set(CPACK_PACKAGE_NAME "voelund-cad")
    set(CPACK_PACKAGE_VENDOR "QuompaCC")
    set(CPACK_PACKAGE_VERSION "${VOELUND_FULL_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
    if(WIN32)
        set(CPACK_GENERATOR ZIP)
        set(CPACK_PACKAGE_FILE_NAME "voelund-cad-${VOELUND_FULL_VERSION}-windows-x64")
    else()
        set(CPACK_GENERATOR TGZ)
        set(CPACK_PACKAGE_FILE_NAME
            "voelund-cad-${VOELUND_FULL_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    endif()
    include(CPack)
endif()
