# Mandatory McCabe cyclomatic-complexity check (limit 10) through the build-only
# voelund-cyclomatic tool (tools/cyclomatic), which measures on Clang's CFG.
#
#   -DVOELUND_CYCLOMATIC_TOOL=<path to voelund-cyclomatic>
#   -DVOELUND_SOURCE_FILES=<file;file...>   (or VOELUND_SOURCE_FILE for one file)
#   -DVOELUND_BUILD_DIR=<dir with compile_commands.json>   optional; without it the
#       files are parsed as plain C++20
#   -DVOELUND_CYCLOMATIC_ROOTS=<dir;dir...> / -DVOELUND_CYCLOMATIC_EXCLUDES=<...> optional
#
# A missing tool fails; there is no textual fallback, which could not tell control flow
# from comments, strings, macros or templates.
if(NOT DEFINED VOELUND_SOURCE_FILES AND DEFINED VOELUND_SOURCE_FILE)
    set(VOELUND_SOURCE_FILES "${VOELUND_SOURCE_FILE}")
endif()
if(NOT DEFINED VOELUND_SOURCE_FILES OR VOELUND_SOURCE_FILES STREQUAL "")
    message(FATAL_ERROR "VOELUND_SOURCE_FILES is required for cyclomatic-complexity checking")
endif()
foreach(source IN LISTS VOELUND_SOURCE_FILES)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "Source file does not exist: ${source}")
    endif()
endforeach()
if(NOT DEFINED VOELUND_CYCLOMATIC_TOOL OR VOELUND_CYCLOMATIC_TOOL STREQUAL "")
    message(FATAL_ERROR
        "Cyclomatic-complexity checking requires the voelund-cyclomatic Clang CFG tool "
        "(tools/cyclomatic, LLVM 22). clang-tidy's cognitive complexity is not a substitute."
    )
endif()
if(NOT EXISTS "${VOELUND_CYCLOMATIC_TOOL}")
    message(FATAL_ERROR "VOELUND_CYCLOMATIC_TOOL does not exist: ${VOELUND_CYCLOMATIC_TOOL}")
endif()

set(tool_arguments --limit=10)
foreach(root IN LISTS VOELUND_CYCLOMATIC_ROOTS)
    list(APPEND tool_arguments "--root=${root}")
endforeach()
foreach(exclude IN LISTS VOELUND_CYCLOMATIC_EXCLUDES)
    list(APPEND tool_arguments "--exclude=${exclude}")
endforeach()
if(DEFINED VOELUND_BUILD_DIR AND NOT VOELUND_BUILD_DIR STREQUAL "")
    list(APPEND tool_arguments -p "${VOELUND_BUILD_DIR}" ${VOELUND_SOURCE_FILES})
else()
    list(APPEND tool_arguments ${VOELUND_SOURCE_FILES} -- -std=c++20)
endif()

execute_process(
    COMMAND "${VOELUND_CYCLOMATIC_TOOL}" ${tool_arguments}
    RESULT_VARIABLE check_result
    OUTPUT_VARIABLE check_output
    ERROR_VARIABLE check_error
)
if(check_result EQUAL 1)
    message(FATAL_ERROR "Cyclomatic complexity over 10:\n${check_error}")
elseif(NOT check_result EQUAL 0)
    message(FATAL_ERROR "Cyclomatic-complexity analysis failed (${check_result}):\n${check_output}\n${check_error}")
endif()
message(STATUS "${check_error}")
