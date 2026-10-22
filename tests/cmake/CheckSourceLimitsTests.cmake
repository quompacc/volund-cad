# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED VOELUND_TEST_ROOT OR VOELUND_TEST_ROOT STREQUAL "")
    message(FATAL_ERROR "VOELUND_TEST_ROOT is required for isolated fixtures")
endif()

get_filename_component(project_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(check_script "${project_root}/tools/cmake/CheckSourceLimits.cmake")

function(check_result case_name expect_success expected_output)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${ARGN} -P "${check_script}"
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

file(MAKE_DIRECTORY "${VOELUND_TEST_ROOT}/valid path ü/src")
set(valid_root "${VOELUND_TEST_ROOT}/valid path ü")
file(WRITE "${valid_root}/src/fixture.cpp" "int value = 1;\n")

check_result(missing_variable FALSE "VOELUND_SOURCE_ROOT is required")
check_result(empty_value FALSE "VOELUND_SOURCE_ROOT must not be empty" "-DVOELUND_SOURCE_ROOT=")
check_result(
    nonexistent_directory FALSE "VOELUND_SOURCE_ROOT must be an existing directory"
    "-DVOELUND_SOURCE_ROOT=${VOELUND_TEST_ROOT}/nonexistent"
)
check_result(
    regular_file FALSE "VOELUND_SOURCE_ROOT must be an existing directory"
    "-DVOELUND_SOURCE_ROOT=${valid_root}/src/fixture.cpp"
)
check_result(
    literal_quotes FALSE "VOELUND_SOURCE_ROOT must be an existing directory"
    "-DVOELUND_SOURCE_ROOT=\"${valid_root}\""
)

file(MAKE_DIRECTORY "${VOELUND_TEST_ROOT}/empty")
check_result(
    empty_directory FALSE "No source files found"
    "-DVOELUND_SOURCE_ROOT=${VOELUND_TEST_ROOT}/empty"
)
file(MAKE_DIRECTORY "${VOELUND_TEST_ROOT}/unsupported-only")
file(WRITE "${VOELUND_TEST_ROOT}/unsupported-only/README.md" "Not a source fixture.\n")
check_result(
    unsupported_files_only FALSE "No source files found"
    "-DVOELUND_SOURCE_ROOT=${VOELUND_TEST_ROOT}/unsupported-only"
)
check_result(
    spaces_and_unicode TRUE "Checked 1 source files"
    "-DVOELUND_SOURCE_ROOT=${valid_root}"
)

foreach(line_ending IN ITEMS lf crlf)
    if(line_ending STREQUAL "crlf")
        set(newline "\r\n")
    else()
        set(newline "\n")
    endif()
    foreach(line_count IN ITEMS 600 601)
        foreach(final_newline IN ITEMS yes no)
            set(case_name "${line_count}_${line_ending}_final_newline_${final_newline}")
            set(fixture_root "${VOELUND_TEST_ROOT}/${case_name}")
            file(MAKE_DIRECTORY "${fixture_root}/src")
            math(EXPR preceding_lines "${line_count} - 1")
            string(REPEAT "int value;${newline}" ${preceding_lines} fixture_content)
            string(APPEND fixture_content "int value;")
            if(final_newline STREQUAL "yes")
                string(APPEND fixture_content "${newline}")
            endif()
            file(WRITE "${fixture_root}/src/fixture.cpp" "${fixture_content}")
            if(line_count EQUAL 600)
                check_result(
                    "${case_name}" TRUE "Source file limit check passed"
                    "-DVOELUND_SOURCE_ROOT=${fixture_root}"
                )
            else()
                check_result(
                    "${case_name}" FALSE "fixture.cpp: 601 lines"
                    "-DVOELUND_SOURCE_ROOT=${fixture_root}"
                )
            endif()
        endforeach()
    endforeach()
endforeach()
