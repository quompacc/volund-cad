# L02: installs the product into a fresh prefix and checks its runtime
# (CheckPackageRuntime.cmake); then removes one OpenCASCADE library from that prefix, which
# the check must report by name.
cmake_policy(SET CMP0007 NEW)
foreach(required VOELUND_SOURCE_ROOT VOELUND_BUILD_DIR VOELUND_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef run_id)
set(prefix "${VOELUND_TEST_ROOT}/package-${run_id}")
if(EXISTS "${prefix}")
    message(FATAL_ERROR "Prefix already exists: ${prefix}")
endif()

set(install_command "${CMAKE_COMMAND}" --install "${VOELUND_BUILD_DIR}" --prefix "${prefix}")
if(VOELUND_CONFIG)
    list(APPEND install_command --config "${VOELUND_CONFIG}")
endif()
execute_process(COMMAND ${install_command} RESULT_VARIABLE result OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Installation failed (${result}):\n${output}${error}")
endif()

function(check out_result out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DVOELUND_PREFIX=${prefix}"
            -P "${VOELUND_SOURCE_ROOT}/tools/cmake/CheckPackageRuntime.cmake"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_text} "${output}\n${error}" PARENT_SCOPE)
endfunction()

check(complete_result complete_text)
if(NOT complete_result EQUAL 0)
    message(FATAL_ERROR "The installed runtime must be complete:\n${complete_text}")
endif()

# The per-user registration scripts at the top of the package register this copy and remove
# it again. Windows uses a registry key of the test's own instead of the user's classes; Linux
# a private XDG_DATA_HOME.
function(run what)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output
        ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${what} failed (${result}):\n${output}${error}")
    endif()
    set(run_output "${output}" PARENT_SCOPE)
endfunction()
if(WIN32)
    # The registry path as a bracket argument: CMake strings would read its backslashes as
    # escapes. FileTypeRegistryProbe.ps1 reads and removes the test copy.
    set(software [=[HKCU:\Software\]=])
    set(test_key "${software}VoelundTest-${run_id}")
    set(classes "${test_key}\\Classes")
    set(powershell powershell -NoProfile -ExecutionPolicy Bypass -File)
    set(probe "${VOELUND_SOURCE_ROOT}/tests/cmake/FileTypeRegistryProbe.ps1")
    file(TO_NATIVE_PATH "${prefix}/bin/voelund-cad.exe" native_product)
    run("Registration" ${powershell} "${prefix}/register-file-type.ps1" -ClassesRoot "${classes}")
    run("Reading the registration" ${powershell} "${probe}" -Action read -Root "${test_key}")
    string(REPLACE "\r" "" registered "${run_output}")
    string(STRIP "${registered}" registered)
    set(expected "\"${native_product}\" \"%1\"\nVoelund.Document")
    if(NOT registered STREQUAL expected)
        run("Removing the test key" ${powershell} "${probe}" -Action clean -Root "${test_key}")
        message(FATAL_ERROR "Unexpected registration:\n${registered}\nexpected:\n${expected}")
    endif()
    run("Unregistration" ${powershell} "${prefix}/unregister-file-type.ps1" -ClassesRoot "${classes}")
    run("Checking the removal" ${powershell} "${probe}" -Action left -Root "${test_key}")
    string(REPLACE "\r" "" remaining "${run_output}")
    string(STRIP "${remaining}" remaining)
    run("Removing the test key" ${powershell} "${probe}" -Action clean -Root "${test_key}")
    if(NOT remaining STREQUAL "False\nFalse")
        message(FATAL_ERROR "Unregistration left keys behind:\n${remaining}")
    endif()
else()
    set(xdg "${prefix}-xdg")
    run("User installation" "${CMAKE_COMMAND}" -E env "XDG_DATA_HOME=${xdg}"
        sh "${prefix}/install-user.sh")
    set(entry "${xdg}/applications/io.github.quompacc.Voelund.desktop")
    file(READ "${entry}" desktop_entry)
    if(NOT desktop_entry MATCHES "\nExec=\"${prefix}/bin/voelund-cad\" %F\n" OR
       NOT EXISTS "${xdg}/mime/packages/io.github.quompacc.Voelund.xml")
        message(FATAL_ERROR "Unexpected user installation:\n${desktop_entry}")
    endif()
    run("User uninstallation" "${CMAKE_COMMAND}" -E env "XDG_DATA_HOME=${xdg}"
        sh "${prefix}/uninstall-user.sh")
    if(EXISTS "${entry}" OR EXISTS "${xdg}/mime/packages/io.github.quompacc.Voelund.xml")
        message(FATAL_ERROR "Uninstallation left files behind")
    endif()
    file(REMOVE_RECURSE "${xdg}")
endif()

file(GLOB_RECURSE removed "${prefix}/*TKMath.dll" "${prefix}/*libTKMath.so*")
if(NOT removed)
    message(FATAL_ERROR "No TKMath library in the installed prefix")
endif()
file(REMOVE ${removed})
check(missing_result missing_text)
if(missing_result EQUAL 0 OR NOT missing_text MATCHES "missing:.*TKMath")
    message(FATAL_ERROR "A removed library must be reported missing:\n${missing_text}")
endif()
file(REMOVE_RECURSE "${prefix}")
message(STATUS "Package runtime complete; a missing library is reported by name")
