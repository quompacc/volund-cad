# AI03: the assistant behind the Claude panel (docs/implementation-plan/AI03-plan.md).
add_library(voelund_assistant STATIC
    ClaudeCodeBackend.cpp
    StreamJson.cpp
)

target_include_directories(
    voelund_assistant
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_assistant
    PUBLIC
        voelund_project_options
        Qt6::Core
)

add_library(Voelund::Assistant ALIAS voelund_assistant)
