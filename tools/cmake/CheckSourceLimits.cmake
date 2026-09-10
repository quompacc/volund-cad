cmake_policy(SET CMP0007 NEW)

if(NOT DEFINED VOELUND_SOURCE_ROOT)
    message(FATAL_ERROR "VOELUND_SOURCE_ROOT is required")
endif()

set(source_directories apps include src tests tools)
set(source_extensions c cc cpp cxx h hh hpp hxx cmake glsl vert frag ui qrc)
set(violations "")

foreach(source_directory IN LISTS source_directories)
    foreach(source_extension IN LISTS source_extensions)
        file(
            GLOB_RECURSE candidate_files
            LIST_DIRECTORIES false
            "${VOELUND_SOURCE_ROOT}/${source_directory}/*.${source_extension}"
        )
        foreach(candidate_file IN LISTS candidate_files)
            file(READ "${candidate_file}" source_content)
            string(REPLACE "\r\n" "\n" source_content "${source_content}")
            string(REPLACE "\r" "\n" source_content "${source_content}")
            if(source_content STREQUAL "")
                set(line_count 0)
            else()
                string(REPLACE ";" "\\;" source_content "${source_content}")
                string(REPLACE "\n" ";" source_lines "${source_content}")
                list(LENGTH source_lines line_count)
                if(source_content MATCHES "\n$")
                    math(EXPR line_count "${line_count} - 1")
                endif()
            endif()
            if(line_count GREATER 600)
                list(APPEND violations "${candidate_file}: ${line_count} lines")
            endif()
        endforeach()
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n  " violation_text)
    message(FATAL_ERROR "Source files exceed 600 lines:\n  ${violation_text}")
endif()

message(STATUS "Source file limit check passed")
