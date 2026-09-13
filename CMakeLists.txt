cmake_minimum_required(VERSION 3.28)

project(
    VoelundCAD
    VERSION 0.1.0
    DESCRIPTION "Native parametric CAD application"
    LANGUAGES CXX
)

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

find_package(OpenCASCADE CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
if(Eigen3_VERSION VERSION_LESS 3.4)
    message(FATAL_ERROR "Völund requires Eigen 3.4 or newer")
endif()
find_package(Boost 1.75 REQUIRED COMPONENTS graph)

if(VOELUND_BUILD_DESKTOP)
    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
    if(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
        find_package(Qt6 CONFIG REQUIRED COMPONENTS OpenGLWidgets)
        if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
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
    add_subdirectory(src/desktop)
    add_subdirectory(apps/voelund-cad)
    add_subdirectory(apps/voelund-ui-preview)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_custom_target(
    check-source-limits
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/CheckSourceLimits.cmake"
    VERBATIM
)
