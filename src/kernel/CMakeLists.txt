# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

add_library(voelund_kernel STATIC ShapeFactory.cpp ShapeOperations.cpp Prism.cpp TaperedPrism.cpp Revolution.cpp ShellOperation.cpp SolidPrimitives.cpp Sweep.cpp Loft.cpp Helix.cpp DraftOperation.cpp Measurement.cpp MassProperties.cpp Interference.cpp HoleTool.cpp ChamferVariants.cpp PathFrames.cpp StepExchange.cpp MergingFuse.cpp MeshExport.cpp ZipStore.cpp ShapeQueries.cpp EdgeQueries.cpp TangentChain.cpp PlanarCurve.cpp SketchShapes.cpp)

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
        TKMesh # N03: meshes for STL and 3MF
        # M17: STEP with names and units through an XCAF document.
        TKDESTEP
        TKDEIGES # N04
        TKXCAF
        TKDE
        TKXSBase
        TKLCAF
        TKCAF
)

add_library(Voelund::Kernel ALIAS voelund_kernel)
