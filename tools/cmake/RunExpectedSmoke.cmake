# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

if(NOT DEFINED VOELUND_SMOKE_EXECUTABLE OR NOT EXISTS "${VOELUND_SMOKE_EXECUTABLE}")
    message(FATAL_ERROR "Smoke executable missing")
endif()
if(NOT DEFINED VOELUND_SMOKE_ARGUMENTS)
    set(VOELUND_SMOKE_ARGUMENTS --smoke-test-uninitialized)
endif()
if(NOT DEFINED VOELUND_EXPECTED_EXIT)
    set(VOELUND_EXPECTED_EXIT 10)
endif()
if(NOT DEFINED VOELUND_SMOKE_TIMEOUT)
    set(VOELUND_SMOKE_TIMEOUT 12)
endif()
set(smoke_command "${VOELUND_SMOKE_EXECUTABLE}" ${VOELUND_SMOKE_ARGUMENTS})
if(VOELUND_REQUIRE_WAYLAND)
    set(smoke_command "${CMAKE_COMMAND}" -E env --unset=DISPLAY
        QT_QPA_PLATFORM=wayland ${smoke_command})
endif()
execute_process(COMMAND ${smoke_command}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT "${VOELUND_SMOKE_TIMEOUT}")
if(NOT "${result}" STREQUAL "${VOELUND_EXPECTED_EXIT}")
    message(FATAL_ERROR "Unexpected smoke exit '${result}', expected '${VOELUND_EXPECTED_EXIT}':\n${output}${error}")
endif()
if(NOT "${output}${error}" MATCHES "SMOKE_REJECTED uninitialized")
    message(FATAL_ERROR "Expected readiness rejection diagnostic missing:\n${output}${error}")
endif()
message(STATUS "Uninitialized viewport rejected with exact exit ${result}")
