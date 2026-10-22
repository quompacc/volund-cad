# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

if(NOT DEFINED VOELUND_TEST_ROOT OR VOELUND_TEST_ROOT STREQUAL "")
    message(FATAL_ERROR "Fixture root required")
endif()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef fixture_id)
set(fixture_root "${VOELUND_TEST_ROOT}/${fixture_id}")
file(MAKE_DIRECTORY "${fixture_root}")
set(runner "${CMAKE_CURRENT_LIST_DIR}/../../tools/cmake/RunExpectedSmoke.cmake")

function(check_fixture name source should_pass diagnostic)
    set(script "${fixture_root}/${name}.cmake")
    file(WRITE "${script}" "${source}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DVOELUND_SMOKE_EXECUTABLE=${CMAKE_COMMAND}"
        "-DVOELUND_SMOKE_ARGUMENTS=-P;${script}"
        -DVOELUND_EXPECTED_EXIT=1 -DVOELUND_SMOKE_TIMEOUT=1
        -P "${runner}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(should_pass AND NOT result EQUAL 0)
        message(FATAL_ERROR "${name} unexpectedly rejected: ${output}${error}")
    elseif(NOT should_pass AND result EQUAL 0)
        message(FATAL_ERROR "${name} falsely accepted")
    endif()
    if(NOT "${output}${error}" MATCHES "${diagnostic}")
        message(FATAL_ERROR "${name}: wrong diagnostic: ${output}${error}")
    endif()
endfunction()

check_fixture(expected "message(FATAL_ERROR \"SMOKE_REJECTED uninitialized\")"
    TRUE "exact exit 1")
check_fixture(unexpected_success "message(STATUS \"SMOKE_REJECTED uninitialized\")"
    FALSE "Unexpected smoke exit")
check_fixture(wrong_reason "message(FATAL_ERROR \"Different failure\")"
    FALSE "diagnostic missing")
check_fixture(timeout "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E sleep 3)"
    FALSE "Unexpected smoke exit")
message(STATUS "Expected-smoke runner: 4 cases passed")
