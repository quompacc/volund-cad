add_library(voelund_kernel STATIC ShapeFactory.cpp ShapeOperations.cpp Prism.cpp)

target_include_directories(
    voelund_kernel
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_include_directories(
    voelund_kernel
    SYSTEM PUBLIC
        "${OpenCASCADE_INCLUDE_DIR}"
)
target_link_libraries(
    voelund_kernel
    PUBLIC
        voelund_project_options
        TKPrim
        TKBO
)

add_library(Voelund::Kernel ALIAS voelund_kernel)
