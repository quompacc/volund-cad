# U12: installs the product into an empty prefix and starts the installed executable, not the
# one in the build tree: the startup smoke test (shell, viewport, commands) and opening a
# document given on the command line.
foreach(variable VOELUND_BUILD_DIR VOELUND_INSTALL_DIR VOELUND_FIXTURE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "${variable} missing")
    endif()
endforeach()

file(REMOVE_RECURSE "${VOELUND_INSTALL_DIR}")
set(install_command "${CMAKE_COMMAND}" --install "${VOELUND_BUILD_DIR}"
    --prefix "${VOELUND_INSTALL_DIR}")
if(VOELUND_CONFIG)
    list(APPEND install_command --config "${VOELUND_CONFIG}")
endif()
execute_process(COMMAND ${install_command}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installation failed (${result}):\n${output}${error}")
endif()

if(WIN32)
    set(product "${VOELUND_INSTALL_DIR}/bin/voelund-cad.exe")
else()
    set(product "${VOELUND_INSTALL_DIR}/bin/voelund-cad")
endif()
if(NOT EXISTS "${product}")
    message(FATAL_ERROR "Installed product missing: ${product}")
endif()

# Autosave and recovery of the smoke run stay inside the test prefix.
set(environment "${CMAKE_COMMAND}" -E env)
if(VOELUND_REQUIRE_WAYLAND)
    list(APPEND environment --unset=DISPLAY QT_QPA_PLATFORM=wayland)
endif()
list(APPEND environment "VOELUND_RECOVERY_DIR=${VOELUND_INSTALL_DIR}/recovery")

function(run_installed expected)
    execute_process(COMMAND ${environment} "${product}" ${ARGN}
        WORKING_DIRECTORY "${VOELUND_INSTALL_DIR}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 60)
    if(NOT "${result}" STREQUAL "${expected}")
        message(FATAL_ERROR
            "Installed product ${ARGN}: exit '${result}', expected '${expected}':\n${output}${error}")
    endif()
endfunction()

run_installed(0 --smoke-test)
run_installed(0 --smoke-test-open "${VOELUND_FIXTURE}")
message(STATUS "Installed product started and opened ${VOELUND_FIXTURE}")
