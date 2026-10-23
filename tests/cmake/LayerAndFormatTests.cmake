# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# Fixtures for the A04 checks: a forbidden layer include (a UI header in document), a
# forbidden upward include, an OCCT algorithm or exception outside the kernel (K-GRENZE), a
# misformatted file and a missing clang-format each fail; the clean trees pass.
cmake_policy(SET CMP0007 NEW)
foreach(required VOELUND_SOURCE_ROOT VOELUND_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef fixture_run_id)
set(root "${VOELUND_TEST_ROOT}/layer-format-${fixture_run_id}")
if(EXISTS "${root}")
    message(FATAL_ERROR "Fixture directory already exists: ${root}")
endif()

function(run_script script tree out_result out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${tree}"
            "-DVOELUND_CLANG_FORMAT=${VOELUND_CLANG_FORMAT}"
            -P "${VOELUND_SOURCE_ROOT}/tools/cmake/${script}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_text} "${output}\n${error}" PARENT_SCOPE)
endfunction()

function(expect_failure script tree pattern what)
    run_script("${script}" "${tree}" result text)
    if(result EQUAL 0 OR NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${what} must fail with '${pattern}':\n${text}")
    endif()
endfunction()

function(expect_success script tree what)
    run_script("${script}" "${tree}" result text)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${what} must pass:\n${text}")
    endif()
endfunction()

# Layers.
file(WRITE "${root}/clean/src/document/Model.cpp" "#include <voelund/kernel/Shape.hpp>\n#include <string>\n")
file(WRITE "${root}/clean/src/desktop/Window.cpp" "#include <QWidget>\n#include <voelund/document/Model.hpp>\n")
expect_success(CheckLayerIncludes.cmake "${root}/clean" "Allowed includes")
file(WRITE "${root}/ui/src/document/Model.cpp" "#include <string>\n#include <QWidget>\n")
expect_failure(CheckLayerIncludes.cmake "${root}/ui"
    "src/document/Model.cpp \\(document\\) includes Qt header QWidget" "A UI include in document")
file(WRITE "${root}/upward/include/voelund/kernel/Shape.hpp" "#include <voelund/desktop/MainWindow.hpp>\n")
file(WRITE "${root}/upward/src/kernel/Shape.cpp" "\n")
expect_failure(CheckLayerIncludes.cmake "${root}/upward"
    "kernel/Shape.hpp \\(kernel\\) includes voelund/desktop/MainWindow.hpp" "An upward include")
file(WRITE "${root}/relative/src/features/Graph.cpp" "#include \"../desktop/Window.hpp\"\n")
# K-GRENZE: TopoDS and gp are data everywhere; algorithms only in kernel and viewport.
file(WRITE "${root}/occt-clean/src/features/Graph.cpp" "#include <TopoDS_Shape.hxx>\n#include <gp_Pnt.hxx>\n")
file(WRITE "${root}/occt-clean/src/kernel/Prism.cpp" "#include <BRepPrimAPI_MakePrism.hxx>\n")
file(WRITE "${root}/occt-clean/src/viewport/View.cpp" "#include <BRep_Tool.hxx>\n")
file(WRITE "${root}/occt-clean/src/features/Solver.cpp" "#include <GCS.h>\n")
expect_success(CheckLayerIncludes.cmake "${root}/occt-clean" "OCCT data types outside the kernel")
file(WRITE "${root}/occt-algorithm/src/features/Graph.cpp" "#include <BRepBuilderAPI_MakeEdge.hxx>\n")
expect_failure(CheckLayerIncludes.cmake "${root}/occt-algorithm"
    "src/features/Graph.cpp \\(features\\) includes OCCT algorithm header BRepBuilderAPI_MakeEdge.hxx"
    "An OCCT algorithm in features")
file(WRITE "${root}/occt-geometry/src/application/Session.cpp" "#include <Geom2d_Circle.hxx>\n")
expect_failure(CheckLayerIncludes.cmake "${root}/occt-geometry" "includes OCCT algorithm header Geom2d_Circle.hxx"
    "An OCCT curve in application")
file(WRITE "${root}/occt-catch/src/desktop/Feedback.hpp" "void f() {\n    try {\n    } catch (const Standard_Failure& error) {\n    }\n}\n")
expect_failure(CheckLayerIncludes.cmake "${root}/occt-catch"
    "src/desktop/Feedback.hpp \\(desktop\\) catches an OCCT exception" "An OCCT catch in desktop")
expect_failure(CheckLayerIncludes.cmake "${root}/relative" "by a relative path" "A relative include out of the layer")

# Link edges: a document target linking the desktop fails at configure time.
function(configure_links name document_link out_result out_text)
    set(project "${root}/links-${name}")
    file(WRITE "${project}/CMakeLists.txt" "cmake_minimum_required(VERSION 3.28)
project(layer_links LANGUAGES NONE)
include(\"${VOELUND_SOURCE_ROOT}/tools/cmake/VoelundLayers.cmake\")
foreach(name kernel desktop)
    add_library(voelund_\${name} INTERFACE)
endforeach()
add_library(Voelund::Kernel ALIAS voelund_kernel)
add_library(Voelund::Desktop ALIAS voelund_desktop)
add_library(voelund_document INTERFACE)
target_link_libraries(voelund_document INTERFACE ${document_link})
voelund_check_layer_links()
")
    set(generator "")
    if(DEFINED VOELUND_GENERATOR AND NOT VOELUND_GENERATOR STREQUAL "")
        set(generator -G "${VOELUND_GENERATOR}")
        if(DEFINED VOELUND_MAKE_PROGRAM AND NOT VOELUND_MAKE_PROGRAM STREQUAL "")
            list(APPEND generator "-DCMAKE_MAKE_PROGRAM=${VOELUND_MAKE_PROGRAM}")
        endif()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" ${generator} -S "${project}" -B "${project}/build"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_text} "${output}\n${error}" PARENT_SCOPE)
endfunction()
configure_links(allowed Voelund::Kernel allowed_result allowed_text)
if(NOT allowed_result EQUAL 0)
    message(FATAL_ERROR "document linking kernel must configure:\n${allowed_text}")
endif()
configure_links(upward Voelund::Desktop upward_result upward_text)
if(upward_result EQUAL 0 OR NOT upward_text MATCHES "voelund_document \\(document\\) links Voelund::Desktop \\(desktop\\)")
    message(FATAL_ERROR "document linking desktop must fail the configure:\n${upward_text}")
endif()

# Format.
file(WRITE "${root}/format/.clang-format" "BasedOnStyle: LLVM\nIndentWidth: 4\nColumnLimit: 100\n")
file(WRITE "${root}/format/src/kernel/Good.cpp" "int good(int value) { return value + 1; }\n")
if(NOT DEFINED VOELUND_CLANG_FORMAT OR VOELUND_CLANG_FORMAT STREQUAL ""
   OR VOELUND_CLANG_FORMAT MATCHES "-NOTFOUND$")
    message(FATAL_ERROR "VOELUND_CLANG_FORMAT is required; the format fixtures must not be skipped")
endif()
expect_success(CheckFormat.cmake "${root}/format" "A formatted tree")
file(WRITE "${root}/format/src/kernel/Bad.cpp" "int   bad( int value ){return value;}\n")
expect_failure(CheckFormat.cmake "${root}/format" "1 files are not clang-formatted.*kernel/Bad\\.cpp"
    "A misformatted file")
set(VOELUND_CLANG_FORMAT "")
expect_failure(CheckFormat.cmake "${root}/format" "requires VOELUND_CLANG_FORMAT" "A missing clang-format")

message(STATUS "Layer and format fixtures passed")
