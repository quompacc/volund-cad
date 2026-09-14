if(NOT DEFINED PROBE_EXE OR NOT DEFINED OCCT_BIN)
    message(FATAL_ERROR "PROBE_EXE and OCCT_BIN are required")
endif()
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${PROBE_EXE}"
    DIRECTORIES "${OCCT_BIN}"
    RESOLVED_DEPENDENCIES_VAR dependencies
    UNRESOLVED_DEPENDENCIES_VAR missing
    PRE_EXCLUDE_REGEXES "[Aa][Pp][Ii]-[Mm][Ss]-" "[Ee][Xx][Tt]-[Mm][Ss]-"
    POST_EXCLUDE_REGEXES ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\].*")
if(missing)
    message(FATAL_ERROR "Unresolved OCAF runtime dependencies: ${missing}")
endif()
get_filename_component(destination "${PROBE_EXE}" DIRECTORY)
foreach(dependency IN LISTS dependencies)
    file(COPY "${dependency}" DESTINATION "${destination}")
endforeach()
file(WRITE "${destination}/runtime-dependencies.txt" "${dependencies}\n")
