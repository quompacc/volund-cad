# Fixtures for CheckFunctionLimits.cmake on a synthetic clang-tidy report: the listed state
# passes; a new violation, a rise, a stale entry, an analysis error, a missing report and an
# empty analysis each fail.
cmake_policy(SET CMP0007 NEW)
foreach(required VOELUND_SOURCE_ROOT VOELUND_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef fixture_run_id)
set(root "${VOELUND_TEST_ROOT}/function-limits-${fixture_run_id}")
if(EXISTS "${root}")
    message(FATAL_ERROR "Fixture directory already exists: ${root}")
endif()
set(tree "${root}/tree")

set(report_base "${tree}/src/a.cpp:10:6: warning: function 'big' exceeds recommended size/complexity thresholds [readability-function-size]
   10 | void big() {
      |      ^
${tree}/src/a.cpp:10:6: note: 95 lines including whitespace and comments (threshold 80)
${tree}/src/a.cpp:40:13: note: nesting level 5 starts here (threshold 4)
${tree}/src/a.cpp:70:13: note: nesting level 5 starts here (threshold 4)
${tree}/include/b.hpp:3:5: warning: cyclomatic complexity 14 of 'ns::Shape::branchy' exceeds limit 10 [voelund-cyclomatic-complexity]
${tree}/include/b.hpp:3:5: warning: cyclomatic complexity 14 of 'ns::Shape::branchy' exceeds limit 10 [voelund-cyclomatic-complexity]
")
set(list_base "# rule | path | function | value | reason
lines | src/a.cpp | big | 95 | Altbestand
nesting | src/a.cpp | big | 2 | Altbestand
cyclomatic | include/b.hpp | ns::Shape::branchy | 14 | Altbestand
")

function(check name report list files pattern)
    file(WRITE "${root}/${name}/report.txt" "${report}")
    file(WRITE "${root}/${name}/limits.txt" "${list}")
    set(report_file "${root}/${name}/report.txt")
    if(name STREQUAL "missing-report")
        set(report_file "${root}/${name}/absent.txt")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${tree}" "-DVOELUND_TIDY_REPORT=${report_file}"
            "-DVOELUND_ANALYSED_FILES=${files}" "-DVOELUND_FUNCTION_LIMITS=${root}/${name}/limits.txt"
            "-DVOELUND_CURRENT_LIMITS=${root}/${name}/current.txt"
            -P "${VOELUND_SOURCE_ROOT}/tools/cmake/CheckFunctionLimits.cmake"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(text "${output}\n${error}")
    if(pattern STREQUAL "")
        if(NOT result EQUAL 0)
            message(FATAL_ERROR "${name} must pass:\n${text}")
        endif()
    elseif(result EQUAL 0 OR NOT text MATCHES "${pattern}")
        message(FATAL_ERROR "${name} must fail with '${pattern}':\n${text}")
    endif()
endfunction()

check(listed "${report_base}" "${list_base}" 2 "")
file(READ "${root}/listed/current.txt" current)
if(NOT current MATCHES "cyclomatic \\| include/b.hpp \\| ns::Shape::branchy \\| 14 \\| Altbestand")
    message(FATAL_ERROR "The measured state is written in list format:\n${current}")
endif()

check(new "${report_base}${tree}/src/c.cpp:1:5: warning: cyclomatic complexity 11 of 'fresh' exceeds limit 10 [voelund-cyclomatic-complexity]\n"
    "${list_base}" 2 "new: cyclomatic \\| src/c.cpp \\| fresh = 11")
string(REPLACE "complexity 14 of" "complexity 15 of" risen "${report_base}")
check(rose "${risen}" "${list_base}" 2 "rose: cyclomatic \\| include/b.hpp \\| ns::Shape::branchy from 14 to 15")
check(stale "${report_base}" "${list_base}parameters | src/a.cpp | big | 7 | Altbestand\n" 2
    "stale: parameters \\| src/a.cpp \\| big")
check(error "${report_base}${tree}/src/d.cpp:2:1: error: unknown type name 'Foo' [clang-diagnostic-error]\n"
    "${list_base}" 2 "Analysis errors")
check(missing-report "${report_base}" "${list_base}" 2 "No analysis report")
check(nothing "${report_base}" "${list_base}" 0 "No translation unit was analysed")
check(malformed "${report_base}" "${list_base}cyclomatic | src/a.cpp | big\n" 2 "Malformed exception")

message(STATUS "Function-limit fixtures passed")
