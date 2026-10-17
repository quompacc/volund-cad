# L02 (Linux): the installed package, registered for a private user by install-user.sh, opens a
# .volund document with umlauts and spaces in its path the way the desktop does: the type comes
# from the user's MIME database, the program from the registered desktop entry's Exec line.
cmake_policy(SET CMP0007 NEW)
foreach(required VOELUND_BUILD_DIR VOELUND_TEST_ROOT VOELUND_FIXTURE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef run_id)
set(root "${VOELUND_TEST_ROOT}/association-${run_id}")
set(prefix "${root}/Völund Paket")
set(xdg "${root}/xdg")

function(run what)
    execute_process(COMMAND ${ARGN} RESULT_VARIABLE result OUTPUT_VARIABLE output
        ERROR_VARIABLE error TIMEOUT 60)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${what} failed (${result}):\n${output}${error}")
    endif()
    string(STRIP "${output}" output)
    set(run_output "${output}" PARENT_SCOPE)
endfunction()

run("Installation" "${CMAKE_COMMAND}" --install "${VOELUND_BUILD_DIR}" --prefix "${prefix}")
set(user_env "${CMAKE_COMMAND}" -E env "XDG_DATA_HOME=${xdg}")
run("User installation" ${user_env} sh "${prefix}/install-user.sh")

set(document "${root}/Prüf Ordner/Teil mit Ä.volund")
file(MAKE_DIRECTORY "${root}/Prüf Ordner")
file(COPY_FILE "${VOELUND_FIXTURE}" "${document}")

# The MIME database update-mime-database built for this user. (xdg-mime query filetype falls
# back to file(1) outside a desktop session and reports text/plain there.)
file(STRINGS "${xdg}/mime/globs2" globs REGEX "application/x-voelund:\\*\\.volund")
if(NOT globs)
    message(FATAL_ERROR "The user's MIME database does not map *.volund to application/x-voelund")
endif()
run("Default application" ${user_env} xdg-mime query default application/x-voelund)
if(NOT run_output STREQUAL "io.github.quompacc.Voelund.desktop")
    message(FATAL_ERROR "The default application is '${run_output}'")
endif()

# UTF-8: without it file(STRINGS) stops at the umlaut of the package folder.
file(STRINGS "${xdg}/applications/io.github.quompacc.Voelund.desktop" exec ENCODING UTF-8
    REGEX "^Exec=")
if(NOT exec MATCHES "^Exec=\"([^\"]+)\" %F$")
    message(FATAL_ERROR "Unexpected Exec line: ${exec}")
endif()
set(program "${CMAKE_MATCH_1}")
run("Opening through the association" "${CMAKE_COMMAND}" -E env --unset=DISPLAY
    QT_QPA_PLATFORM=wayland "VOELUND_RECOVERY_DIR=${root}/recovery"
    "${program}" --smoke-test-open "${document}")
run("User uninstallation" ${user_env} sh "${prefix}/uninstall-user.sh")
file(REMOVE_RECURSE "${root}")
message(STATUS "The registered package opened a document under a path with umlauts and spaces")
