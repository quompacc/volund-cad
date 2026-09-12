add_executable(voelund_kernel_tests unit/ShapeFactoryTests.cpp)
target_link_libraries(
    voelund_kernel_tests
    PRIVATE
        voelund_project_options
        Voelund::Kernel
)

add_test(NAME kernel.shape_factory COMMAND voelund_kernel_tests)

add_executable(voelund_document_tests unit/ModelDocumentTests.cpp)
target_link_libraries(
    voelund_document_tests
    PRIVATE
        voelund_project_options
        Voelund::Document
)

add_test(NAME document.history COMMAND voelund_document_tests)

add_executable(voelund_sketch_profile_tests unit/SketchProfileTests.cpp)
target_link_libraries(
    voelund_sketch_profile_tests
    PRIVATE
        voelund_project_options
        Voelund::Features
)

add_test(NAME features.sketch_profiles COMMAND voelund_sketch_profile_tests)

add_executable(voelund_planegcs_tests unit/PlaneGcsTests.cpp)
target_link_libraries(
    voelund_planegcs_tests
    PRIVATE
        voelund_project_options
        Voelund::PlaneGCS
)

add_test(NAME features.planegcs_solver COMMAND voelund_planegcs_tests)

add_executable(voelund_sketch_constraint_tests unit/SketchConstraintSolverTests.cpp)
target_link_libraries(
    voelund_sketch_constraint_tests
    PRIVATE
        voelund_project_options
        Voelund::Features
)

add_test(NAME features.sketch_constraint_adapter COMMAND voelund_sketch_constraint_tests)

add_executable(voelund_application_tests integration/CadSessionTests.cpp)
target_link_libraries(
    voelund_application_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.recompute_history COMMAND voelund_application_tests)

add_executable(voelund_sketch_editor_tests integration/SketchEditorSessionTests.cpp)
target_link_libraries(
    voelund_sketch_editor_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.sketch_editor_flow COMMAND voelund_sketch_editor_tests)

add_executable(voelund_sketch_construction_tests integration/SketchConstructionTests.cpp)
target_link_libraries(
    voelund_sketch_construction_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.sketch_construction COMMAND voelund_sketch_construction_tests)

add_executable(voelund_sketch_circle_constraint_tests integration/SketchCircleConstraintTests.cpp)
target_link_libraries(
    voelund_sketch_circle_constraint_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.sketch_circle_constraints COMMAND voelund_sketch_circle_constraint_tests)

add_executable(voelund_sketch_alignment_tests integration/SketchAlignmentTests.cpp)
target_link_libraries(
    voelund_sketch_alignment_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.sketch_alignments COMMAND voelund_sketch_alignment_tests)

add_executable(voelund_workspace_tests unit/CommandRegistryTests.cpp)
target_link_libraries(
    voelund_workspace_tests
    PRIVATE
        voelund_project_options
        Voelund::ModuleSolid
)

add_test(NAME workspaces.module_registry COMMAND voelund_workspace_tests)

add_test(
    NAME architecture.source_limits
    COMMAND
        "${CMAKE_COMMAND}"
        -DVOELUND_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/tools/cmake/CheckSourceLimits.cmake"
)
