# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

# LIZENZ: the licence header check passes a tree whose files carry SPDX lines (after a shebang,
# too) and fails, naming the file, when one lacks it; third_party/ is not looked at.
cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED VOELUND_TEST_ROOT OR VOELUND_TEST_ROOT STREQUAL "")
    message(FATAL_ERROR "VOELUND_TEST_ROOT is required for isolated fixtures")
endif()

get_filename_component(project_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(check_script "${project_root}/tools/cmake/CheckLicenseHeaders.cmake")

function(check_result case_name root expect_success expected_output)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${root}" -P "${check_script}"
        RESULT_VARIABLE check_exit
        OUTPUT_VARIABLE check_output
        ERROR_VARIABLE check_error
    )
    if(expect_success AND NOT check_exit STREQUAL "0")
        message(FATAL_ERROR "${case_name}: expected success, got ${check_exit}\n${check_output}${check_error}")
    elseif(NOT expect_success AND check_exit STREQUAL "0")
        message(FATAL_ERROR "${case_name}: expected failure\n${check_output}${check_error}")
    endif()
    if(NOT "${check_output}${check_error}" MATCHES "${expected_output}")
        message(FATAL_ERROR "${case_name}: missing '${expected_output}'\n${check_output}${check_error}")
    endif()
    message(STATUS "${case_name}: passed")
endfunction()

set(header "SPDX-License-Identifier: GPL-3.0-or-later")
set(good "${VOELUND_TEST_ROOT}/good tree ü")
file(REMOVE_RECURSE "${VOELUND_TEST_ROOT}")
file(WRITE "${good}/src/fixture.cpp" "// ${header}\nint value = 1;\n")
file(WRITE "${good}/tools/run.sh" "#!/bin/sh\n# ${header}\necho ok\n")
file(WRITE "${good}/CMakeLists.txt" "not looked at outside the source directories\n")
file(WRITE "${good}/third_party/lib.cpp" "int foreign = 1;\n")
check_result(licensed_tree "${good}" TRUE "Checked 2 files")

set(bad "${VOELUND_TEST_ROOT}/bad tree")
file(WRITE "${bad}/src/fixture.cpp" "// ${header}\nint value = 1;\n")
file(WRITE "${bad}/src/CMakeLists.txt" "add_library(x STATIC fixture.cpp)\n")
check_result(missing_header "${bad}" FALSE "src/CMakeLists.txt")

set(empty "${VOELUND_TEST_ROOT}/empty")
file(MAKE_DIRECTORY "${empty}")
check_result(no_files "${empty}" FALSE "No source files found")
