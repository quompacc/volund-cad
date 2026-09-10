add_library(voelund_document STATIC DocumentSerializer.cpp ModelDocument.cpp)

target_include_directories(
    voelund_document
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_document
    PUBLIC
        voelund_project_options
        Voelund::Kernel
)

add_library(Voelund::Document ALIAS voelund_document)
