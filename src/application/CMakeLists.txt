add_library(voelund_application STATIC CadSession.cpp)

target_include_directories(
    voelund_application
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_application
    PUBLIC
        voelund_project_options
        Voelund::Document
        Voelund::Features
)

add_library(Voelund::Application ALIAS voelund_application)
