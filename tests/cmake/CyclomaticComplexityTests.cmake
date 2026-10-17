# Fixtures for the McCabe check (A02.3): exact values fixed beforehand for representative
# control flow, the 10/11 limit through the mandatory check script, an analysis error kept
# apart from a limit violation, and the missing tool as its own failure.
cmake_policy(SET CMP0007 NEW)

foreach(required VOELUND_SOURCE_ROOT VOELUND_TEST_ROOT VOELUND_CYCLOMATIC_TOOL)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

# Each run owns a fresh child directory (see QualityRulesTests.cmake).
string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef fixture_run_id)
set(VOELUND_TEST_ROOT "${VOELUND_TEST_ROOT}/cyclomatic-${fixture_run_id}")
if(EXISTS "${VOELUND_TEST_ROOT}")
    message(FATAL_ERROR "Fixture directory already exists: ${VOELUND_TEST_ROOT}")
endif()
file(MAKE_DIRECTORY "${VOELUND_TEST_ROOT}")
set(check_script "${VOELUND_SOURCE_ROOT}/tools/cmake/CheckCyclomaticComplexity.cmake")

function(branches count out)
    set(body "")
    foreach(index RANGE 1 ${count})
        string(APPEND body "    if (value == ${index}) return ${index};\n")
    endforeach()
    set(${out} "${body}" PARENT_SCOPE)
endfunction()

branches(9 nine)
branches(10 ten)
file(WRITE "${VOELUND_TEST_ROOT}/ten.cpp" "int exactlyTen(int value) {\n${nine}    return 0;\n}\n")
file(WRITE "${VOELUND_TEST_ROOT}/eleven.cpp" "int tooComplex(int value) {\n${ten}    return 0;\n}\n")
file(WRITE "${VOELUND_TEST_ROOT}/broken.cpp" "int broken(int value) {\n    return value +;\n}\n")
file(WRITE "${VOELUND_TEST_ROOT}/flow.cpp" [=[
#define PICK(v) ((v) > 0 ? 1 : 0)
void mayThrow();
bool stop();

int straight() { return 1; }
int clean(int value) { if (value > 0) { return value; } return 0; }
int shortCircuit(bool a, bool b, bool c) { if ((a && b) || c) return 1; return 0; }
int ternary(int value) { return value > 0 ? 1 : 0; }
int switchCases(int value) {
    switch (value) {
    case 1: return 10;
    case 2: return 20;
    case 3: return 30;
    default: return 0;
    }
}
int switchNoDefault(int value) {
    switch (value) {
    case 1: return 10;
    case 2: return 20;
    }
    return 0;
}
int loops(int count) {
    int sum = 0;
    for (int i = 0; i < count; ++i) sum += i;
    while (sum > 100) sum -= 7;
    do { --sum; } while (sum > 50);
    int values[3] = {1, 2, 3};
    for (int value : values) sum += value;
    return sum;
}
int withLambda() {
    auto pick = [](int value) { if (value > 0) return 1; return 0; };
    return pick(2);
}
template <typename T> int templated(T value) { if (value > T{}) return 1; return 0; }
int useTemplate() { return templated(3) + templated(2.0); }
template <typename C> int sumOf(const C& values) { int sum = 0; for (int value : values) sum += value; return sum; }
int useSum() { int values[2] = {1, 2}; return sumOf(values); }
int tryCatch() {
    try { mayThrow(); }
    catch (int) { return 1; }
    catch (...) { return 2; }
    return 0;
}
int unreachableAfterReturn(int value) { return 1; if (value) return 2; return 0; }
int constantCondition() { if (true) return 1; return 0; }
int macroBranch(int value) { return PICK(value); }
void endless() { for (;;) { if (stop()) return; } }
]=])

# Sollwerte: 1 + one per decision (if, loop, case label, catch, &&, ||, ?:).
set(expected
    "straight: 1" "clean: 2" "shortCircuit: 4" "ternary: 2" "switchCases: 4"
    "switchNoDefault: 3" "loops: 5" "withLambda: 1" "withLambda::\\(lambda\\): 2"
    "templated: 2" "useTemplate: 1" "sumOf: 2" "useSum: 1" "tryCatch: 3" "unreachableAfterReturn: 1"
    "constantCondition: 2" "macroBranch: 2" "endless: 2"
)
execute_process(
    COMMAND "${VOELUND_CYCLOMATIC_TOOL}" --report-all "${VOELUND_TEST_ROOT}/flow.cpp" -- -std=c++20
    RESULT_VARIABLE flow_result
    OUTPUT_VARIABLE flow_output
    ERROR_VARIABLE flow_error
)
if(NOT flow_result EQUAL 0)
    message(FATAL_ERROR "Control-flow fixture failed (${flow_result}):\n${flow_output}\n${flow_error}")
endif()
foreach(value IN LISTS expected)
    if(NOT flow_output MATCHES ": ${value}\n")
        message(FATAL_ERROR "Expected '${value}' in:\n${flow_output}")
    endif()
endforeach()
list(LENGTH expected expected_count)
string(REGEX MATCHALL "\n" lines "${flow_output}")
list(LENGTH lines measured_count)
if(NOT measured_count EQUAL expected_count)
    message(FATAL_ERROR "Expected ${expected_count} functions (instantiations not repeated), got:\n${flow_output}")
endif()

# The clang-tidy plugin measures the same: with threshold 1 it reports every value above 1.
if(DEFINED VOELUND_TIDY_PLUGIN AND NOT VOELUND_TIDY_PLUGIN STREQUAL "")
    find_program(clang_tidy clang-tidy REQUIRED)
    execute_process(
        COMMAND "${clang_tidy}" "--load=${VOELUND_TIDY_PLUGIN}" "--checks=-*,voelund-*"
            "--config={CheckOptions: {voelund-cyclomatic-complexity.Threshold: 1}}"
            "${VOELUND_TEST_ROOT}/flow.cpp" -- -std=c++20
        RESULT_VARIABLE plugin_result OUTPUT_VARIABLE plugin_output ERROR_VARIABLE plugin_error)
    foreach(value IN LISTS expected)
        if(value MATCHES "^(.+): ([0-9]+)$" AND NOT CMAKE_MATCH_2 EQUAL 1)
            set(name "${CMAKE_MATCH_1}")
            set(number "${CMAKE_MATCH_2}")
            if(NOT plugin_output MATCHES "cyclomatic complexity ${number} of '${name}' exceeds limit 1")
                message(FATAL_ERROR "The plugin disagrees on ${value}:\n${plugin_output}\n${plugin_error}")
            endif()
        endif()
    endforeach()
endif()

function(run_check file out_result out_text)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_FILE=${file}"
            "-DVOELUND_CYCLOMATIC_TOOL=${VOELUND_CYCLOMATIC_TOOL}" -P "${check_script}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(${out_result} "${result}" PARENT_SCOPE)
    set(${out_text} "${output}\n${error}" PARENT_SCOPE)
endfunction()

run_check("${VOELUND_TEST_ROOT}/ten.cpp" ten_result ten_text)
if(NOT ten_result EQUAL 0)
    message(FATAL_ERROR "Complexity exactly 10 must pass:\n${ten_text}")
endif()
run_check("${VOELUND_TEST_ROOT}/eleven.cpp" eleven_result eleven_text)
if(eleven_result EQUAL 0 OR NOT eleven_text MATCHES "cyclomatic complexity 11 of 'tooComplex' exceeds limit 10")
    message(FATAL_ERROR "Complexity 11 must fail with its value:\n${eleven_text}")
endif()
run_check("${VOELUND_TEST_ROOT}/broken.cpp" broken_result broken_text)
if(broken_result EQUAL 0 OR NOT broken_text MATCHES "analysis failed \\(2\\)" OR broken_text MATCHES "over 10")
    message(FATAL_ERROR "A parse error must fail as an analysis error, not a violation:\n${broken_text}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_FILE=${VOELUND_TEST_ROOT}/ten.cpp" -P "${check_script}"
    RESULT_VARIABLE missing_result
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_error
)
if(missing_result EQUAL 0 OR NOT "${missing_output}${missing_error}" MATCHES "requires the voelund-cyclomatic")
    message(FATAL_ERROR "A missing tool must fail the mandatory check:\n${missing_output}\n${missing_error}")
endif()

message(STATUS "Cyclomatic fixtures passed: control-flow values, 10 passes, 11 fails, analysis error and missing tool reported apart")
