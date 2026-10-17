add_library(voelund_kernel STATIC ShapeFactory.cpp ShapeOperations.cpp Prism.cpp Revolution.cpp ShellOperation.cpp SolidPrimitives.cpp Sweep.cpp Loft.cpp Helix.cpp DraftOperation.cpp Measurement.cpp StepExchange.cpp MergingFuse.cpp)

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
        TKFillet
        TKOffset
    PRIVATE
        TKShHealing # T05: ShapeUpgrade_UnifySameDomain
        # M17: STEP with names and units through an XCAF document.
        TKDESTEP
        TKXCAF
        TKDE
        TKXSBase
        TKLCAF
        TKCAF
)

add_library(Voelund::Kernel ALIAS voelund_kernel)
