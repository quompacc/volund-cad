# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

add_executable(voelund_native_product_tests integration/NativeProductPersistenceTests.cpp)
add_executable(voelund_feature_dag_tests integration/FeatureDagTests.cpp)
target_link_libraries(voelund_feature_dag_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME features.dependency_dag COMMAND voelund_feature_dag_tests)
set_tests_properties(features.dependency_dag PROPERTIES LABELS "integration;features;application;regression" TIMEOUT 60)
add_executable(voelund_legacy_constraint_migration_tests integration/LegacyConstraintMigrationTests.cpp)
target_link_libraries(voelund_legacy_constraint_migration_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.legacy_constraint_migration COMMAND voelund_legacy_constraint_migration_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents")
set_tests_properties(document.legacy_constraint_migration PROPERTIES LABELS "integration;document;regression" TIMEOUT 60)
add_executable(voelund_migration_acceptance_tests integration/MigrationAcceptanceTests.cpp)
target_link_libraries(voelund_migration_acceptance_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.migration_acceptance COMMAND voelund_migration_acceptance_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents")
set_tests_properties(application.migration_acceptance PROPERTIES LABELS "integration;document;application;regression" TIMEOUT 120)
add_executable(voelund_product_exchange_tests integration/ProductFileExchangeTests.cpp)
target_link_libraries(voelund_product_exchange_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.product_file_exchange COMMAND voelund_product_exchange_tests)
set_tests_properties(application.product_file_exchange PROPERTIES LABELS "integration;document;application;native;regression" TIMEOUT 60)
target_link_libraries(voelund_native_product_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.native_persistence COMMAND voelund_native_product_tests)
set_tests_properties(application.native_persistence PROPERTIES LABELS "integration;document;application;native;regression" TIMEOUT 60)
add_executable(voelund_native_output_tests integration/NativeOutputLimitsTests.cpp)
target_link_libraries(voelund_native_output_tests PRIVATE voelund_project_options Voelund::Document TKBin)
add_test(NAME document.native_output_limits COMMAND voelund_native_output_tests)
set_tests_properties(document.native_output_limits PROPERTIES LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_attribute_tests integration/NativeAttributeValuesTests.cpp)
target_link_libraries(voelund_native_attribute_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.native_attribute_values COMMAND voelund_native_attribute_tests)
set_tests_properties(document.native_attribute_values PROPERTIES LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_structure_tests integration/NativeContainerStructureTests.cpp)
target_link_libraries(voelund_native_structure_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.native_container_structure COMMAND voelund_native_structure_tests)
set_tests_properties(document.native_container_structure PROPERTIES LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_header_tests integration/NativeContainerHeaderTests.cpp)
target_link_libraries(voelund_native_header_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.native_container_header COMMAND voelund_native_header_tests)
set_tests_properties(document.native_container_header PROPERTIES LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_document_file_input_tests integration/DocumentFileInputTests.cpp)
target_link_libraries(voelund_document_file_input_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.file_input COMMAND voelund_document_file_input_tests "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents")
set_tests_properties(document.file_input PROPERTIES LABELS "integration;document;regression" TIMEOUT 60)
add_executable(voelund_native_store_tests integration/NativeDocumentStoreTests.cpp)
target_link_libraries(voelund_native_store_tests PRIVATE voelund_project_options Voelund::Document TKBin)
add_test(NAME document.native_store COMMAND voelund_native_store_tests)
set_tests_properties(document.native_store PROPERTIES LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_model_backend_tests integration/ModelBackendParityTests.cpp)
target_link_libraries(voelund_model_backend_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME document.product_ocaf_backend COMMAND voelund_model_backend_tests)
set_tests_properties(document.product_ocaf_backend PROPERTIES LABELS "integration;document;application;regression" TIMEOUT 60)
add_executable(voelund_native_model_state_tests integration/NativeModelStateTests.cpp unit/AllocationFailure.cpp)
target_link_libraries(voelund_native_model_state_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.native_model_state COMMAND voelund_native_model_state_tests)
set_tests_properties(document.native_model_state PROPERTIES LABELS "integration;document;native;regression" TIMEOUT 60)
if(NOT MSVC OR NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_test(NAME document.native_model_allocations COMMAND voelund_native_model_state_tests --allocations)
    set_tests_properties(document.native_model_allocations PROPERTIES LABELS "document;native;allocation;regression" TIMEOUT 120)
endif()
add_executable(voelund_read_boundary_tests integration/DocumentReadBoundaryTests.cpp)
target_link_libraries(voelund_read_boundary_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.document_read_boundaries COMMAND voelund_read_boundary_tests)
set_tests_properties(application.document_read_boundaries PROPERTIES
    LABELS "integration;application;document;regression" TIMEOUT 60)
add_executable(voelund_angle_boundary_tests integration/AngleBoundaryTests.cpp)
target_link_libraries(voelund_angle_boundary_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME document.angle_boundaries COMMAND voelund_angle_boundary_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents")
set_tests_properties(document.angle_boundaries PROPERTIES LABELS "integration;document;regression" TIMEOUT 60)
add_executable(voelund_plane_migration_tests integration/PlaneMigrationTests.cpp)
target_link_libraries(voelund_plane_migration_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME document.plane_migration COMMAND voelund_plane_migration_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents" "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(document.plane_migration PROPERTIES LABELS "integration;document;regression" TIMEOUT 60)
add_executable(voelund_numeric_validation_tests integration/NumericValidationTests.cpp)
target_link_libraries(voelund_numeric_validation_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME document.numeric_validation COMMAND voelund_numeric_validation_tests)
set_tests_properties(document.numeric_validation PROPERTIES LABELS "integration;document;kernel;regression" TIMEOUT 60)
add_executable(voelund_legacy_import_tests integration/LegacyImportTests.cpp)
target_link_libraries(voelund_legacy_import_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME document.legacy_import COMMAND voelund_legacy_import_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents" "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(document.legacy_import PROPERTIES LABELS "integration;document;regression" TIMEOUT 60)
add_executable(voelund_scene_status_tests integration/SceneStatusTests.cpp)
add_executable(voelund_command_input_tests integration/CommandInputTests.cpp)
target_link_libraries(voelund_command_input_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.command_input COMMAND voelund_command_input_tests)
set_tests_properties(application.command_input PROPERTIES LABELS "integration;application;document;regression" TIMEOUT 60)
target_link_libraries(voelund_scene_status_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.scene_status COMMAND voelund_scene_status_tests "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(application.scene_status PROPERTIES LABELS "integration;application;document;regression" TIMEOUT 60)
add_executable(voelund_session_failure_tests integration/CadSessionFailureBoundaryTests.cpp)
target_link_libraries(voelund_session_failure_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.failure_boundaries COMMAND voelund_session_failure_tests)
set_tests_properties(application.failure_boundaries PROPERTIES LABELS "integration;application;document;regression" TIMEOUT 60)
add_executable(voelund_session_creation_tests integration/CadSessionCreationResultTests.cpp)
target_link_libraries(voelund_session_creation_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.creation_results COMMAND voelund_session_creation_tests)
set_tests_properties(application.creation_results PROPERTIES LABELS "integration;application;document;regression" TIMEOUT 60)
add_executable(voelund_session_transaction_tests integration/CadSessionTransactionTests.cpp)
target_link_libraries(voelund_session_transaction_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.command_transactions COMMAND voelund_session_transaction_tests)
set_tests_properties(application.command_transactions PROPERTIES LABELS "integration;application;document;regression" TIMEOUT 60)
add_executable(voelund_document_transaction_tests unit/DocumentTransactionTests.cpp unit/AllocationFailure.cpp)
target_link_libraries(voelund_document_transaction_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.transactions COMMAND voelund_document_transaction_tests)
set_tests_properties(document.transactions PROPERTIES LABELS "unit;document;regression" TIMEOUT 60)
# MSVC's debug STL allocates iterator proxies inside noexcept vector constructors.
# Run exhaustive bad_alloc injection in the separate Release build instead.
if(NOT MSVC OR NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_test(NAME document.transaction_allocation_failures COMMAND voelund_document_transaction_tests --allocations)
    set_tests_properties(document.transaction_allocation_failures PROPERTIES LABELS "unit;document;regression;allocation" TIMEOUT 60)
endif()
add_executable(voelund_command_contract_tests integration/CommandContractTests.cpp)
target_link_libraries(voelund_command_contract_tests PRIVATE voelund_project_options Voelund::Application)
foreach(fixture IN ITEMS compound errors)
    add_test(NAME application.command_contract_${fixture} COMMAND voelund_command_contract_tests ${fixture})
    set_tests_properties(application.command_contract_${fixture} PROPERTIES LABELS "integration;application;contract" TIMEOUT 15)
endforeach()
find_package(Threads REQUIRED)
add_executable(voelund_session_identity_tests integration/CadSessionIdentityTests.cpp)
target_link_libraries(voelund_session_identity_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.document_identity COMMAND voelund_session_identity_tests)
set_tests_properties(application.document_identity PROPERTIES
    LABELS "integration;application;document;regression" TIMEOUT 60)
add_executable(voelund_native_schema_tests integration/NativeSchemaTests.cpp)
target_link_libraries(voelund_native_schema_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.native_schema COMMAND voelund_native_schema_tests)
set_tests_properties(document.native_schema PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_extrusion_tests integration/NativeExtrusionTests.cpp)
target_link_libraries(voelund_native_extrusion_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.native_extrusions COMMAND voelund_native_extrusion_tests)
set_tests_properties(document.native_extrusions PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_sketch_pattern_tests integration/SketchPatternTests.cpp)
target_link_libraries(voelund_sketch_pattern_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.sketch_patterns COMMAND voelund_sketch_pattern_tests)
set_tests_properties(document.sketch_patterns PROPERTIES
    LABELS "integration;document;native;sketch;regression" TIMEOUT 60)
add_executable(voelund_native_parameter_tests integration/NativeParameterTests.cpp)
target_link_libraries(voelund_native_parameter_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.native_parameters COMMAND voelund_native_parameter_tests)
set_tests_properties(document.native_parameters PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_sketch_constraint_tests integration/NativeSketchConstraintTests.cpp)
target_link_libraries(voelund_native_sketch_constraint_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.native_sketch_constraints COMMAND voelund_native_sketch_constraint_tests)
set_tests_properties(document.native_sketch_constraints PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_sketch_tests integration/NativeSketchTests.cpp)
target_link_libraries(voelund_native_sketch_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.native_sketch_geometry COMMAND voelund_native_sketch_tests)
set_tests_properties(document.native_sketch_geometry PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_cylinder_tests integration/NativeCylinderTests.cpp)
target_link_libraries(voelund_native_cylinder_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.native_cylinders COMMAND voelund_native_cylinder_tests)
set_tests_properties(document.native_cylinders PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 60)
add_executable(voelund_native_box_tests integration/NativeBoxTests.cpp)
target_link_libraries(voelund_native_box_tests PRIVATE
    voelund_project_options Voelund::Features TKBin)
add_test(NAME document.native_boxes COMMAND voelund_native_box_tests)
set_tests_properties(document.native_boxes PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 30)
add_executable(voelund_native_metadata_tests integration/NativeMetadataTests.cpp)
target_link_libraries(voelund_native_metadata_tests PRIVATE
    voelund_project_options Voelund::Document TKBin)
add_test(NAME document.native_metadata COMMAND voelund_native_metadata_tests)
set_tests_properties(document.native_metadata PROPERTIES
    LABELS "integration;document;native;regression" TIMEOUT 30)
add_executable(voelund_document_identity_tests unit/DocumentIdentityTests.cpp)
target_link_libraries(voelund_document_identity_tests PRIVATE
    voelund_project_options Voelund::Document Threads::Threads)
foreach(identity_case IN ITEMS generator lifecycle)
    add_test(NAME document.identity_${identity_case}
        COMMAND voelund_document_identity_tests ${identity_case})
    set_tests_properties(document.identity_${identity_case} PROPERTIES
        LABELS "unit;document;identity;regression" TIMEOUT 30)
endforeach()
add_executable(voelund_safe_persistence_tests integration/CadSessionSafePersistenceTests.cpp)
target_link_libraries(voelund_safe_persistence_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.safe_persistence COMMAND voelund_safe_persistence_tests "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(application.safe_persistence PROPERTIES LABELS "integration;document;regression" TIMEOUT 60)
add_executable(voelund_document_parser_tests unit/DocumentParserTests.cpp)
target_link_libraries(voelund_document_parser_tests PRIVATE Voelund::Document)
add_test(NAME document.parser_limits_and_versions COMMAND voelund_document_parser_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents")
set_tests_properties(document.parser_limits_and_versions PROPERTIES LABELS "unit;document;regression" TIMEOUT 60)
# N01: expressions in parameters and measure fields.
add_executable(voelund_parameter_expression_tests unit/ParameterExpressionTests.cpp)
target_link_libraries(voelund_parameter_expression_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.parameter_expressions COMMAND voelund_parameter_expression_tests)
set_tests_properties(document.parameter_expressions PROPERTIES LABELS "unit;document" TIMEOUT 30)
add_executable(voelund_atomic_file_tests unit/AtomicFileStoreTests.cpp)
target_link_libraries(voelund_atomic_file_tests PRIVATE Voelund::Platform Threads::Threads)
add_test(NAME platform.atomic_file_store
    COMMAND voelund_atomic_file_tests "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(platform.atomic_file_store PROPERTIES LABELS "unit;platform;regression" TIMEOUT 30)

add_executable(voelund_kernel_tests unit/ShapeFactoryTests.cpp)
target_link_libraries(
    voelund_kernel_tests
    PRIVATE
        voelund_project_options
        Voelund::Kernel
)

add_test(NAME kernel.shape_factory COMMAND voelund_kernel_tests)

add_executable(voelund_support_provenance_tests integration/SupportProvenanceTests.cpp)
add_executable(voelund_edge_provenance_tests integration/EdgeProvenanceTests.cpp)
add_executable(voelund_projected_edge_tests integration/ProjectedEdgeReferenceTests.cpp)
target_link_libraries(voelund_projected_edge_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.projected_edge_reference COMMAND voelund_projected_edge_tests)
set_tests_properties(features.projected_edge_reference PROPERTIES LABELS "integration;features;native;regression" TIMEOUT 60)
add_executable(voelund_contour_provenance_tests integration/ContourProvenanceTests.cpp)
target_link_libraries(voelund_contour_provenance_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.contour_provenance COMMAND voelund_contour_provenance_tests)
set_tests_properties(features.contour_provenance PROPERTIES LABELS "integration;features;regression" TIMEOUT 60)
target_link_libraries(voelund_edge_provenance_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.edge_provenance COMMAND voelund_edge_provenance_tests)
set_tests_properties(features.edge_provenance PROPERTIES LABELS "integration;features;regression" TIMEOUT 60)
add_executable(voelund_support_binding_tests integration/SketchSupportBindingTests.cpp)
target_link_libraries(voelund_support_binding_tests PRIVATE voelund_project_options Voelund::Application TKBin)
add_test(NAME features.support_binding COMMAND voelund_support_binding_tests)
set_tests_properties(features.support_binding PROPERTIES LABELS "integration;features;native;regression" TIMEOUT 60)
target_link_libraries(voelund_support_provenance_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.support_provenance COMMAND voelund_support_provenance_tests)
set_tests_properties(features.support_provenance PROPERTIES LABELS "integration;features;regression" TIMEOUT 60)

add_executable(voelund_operation_history_tests integration/OperationHistoryTests.cpp)
target_link_libraries(voelund_operation_history_tests PRIVATE voelund_project_options Voelund::Kernel)
add_test(NAME kernel.operation_history COMMAND voelund_operation_history_tests)
set_tests_properties(kernel.operation_history PROPERTIES LABELS "integration;kernel;regression" TIMEOUT 60)

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

add_executable(
    voelund_application_tests
    integration/CadSessionTests.cpp
    integration/CadSessionHistoryTests.cpp
    integration/CadSessionEvaluationTests.cpp
    integration/CadSessionPersistenceTests.cpp
)
target_link_libraries(
    voelund_application_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.recompute_history COMMAND voelund_application_tests)

if(VOELUND_BUILD_DESKTOP)
    add_executable(voelund_sketch_command_failure_tests desktop/SketchCommandFailureTests.cpp)
    target_link_libraries(voelund_sketch_command_failure_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.sketch_command_failure COMMAND voelund_sketch_command_failure_tests)
    set_tests_properties(desktop.sketch_command_failure PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 30 RUN_SERIAL TRUE)
    if(WIN32)
        add_custom_command(TARGET voelund_sketch_command_failure_tests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE:Qt6::Widgets>"
                "$<TARGET_FILE:Qt6::OpenGL>" "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund_sketch_command_failure_tests>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:voelund_sketch_command_failure_tests>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:voelund_sketch_command_failure_tests>/platforms" VERBATIM)
    endif()
    add_executable(voelund_feature_status_tests desktop/FeatureStatusTests.cpp)
    add_executable(voelund_reference_repair_flow_tests desktop/ReferenceRepairFlowTests.cpp)
    add_executable(voelund_document_repair_flow_tests desktop/DocumentRepairFlowTests.cpp)
    target_link_libraries(voelund_document_repair_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.document_repair_flow COMMAND voelund_document_repair_flow_tests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(desktop.document_repair_flow PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 90 RUN_SERIAL TRUE)
    target_link_libraries(voelund_reference_repair_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.reference_repair_flow COMMAND voelund_reference_repair_flow_tests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(desktop.reference_repair_flow PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 90 RUN_SERIAL TRUE)
    add_executable(voelund_support_flow_tests desktop/SupportBindingFlowTests.cpp)
    add_executable(voelund_projected_edge_flow_tests desktop/ProjectedEdgeFlowTests.cpp)
    target_link_libraries(voelund_projected_edge_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.projected_edge_flow COMMAND voelund_projected_edge_flow_tests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(desktop.projected_edge_flow PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
    target_link_libraries(voelund_support_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.support_binding_flow COMMAND voelund_support_flow_tests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(desktop.support_binding_flow PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
    target_link_libraries(voelund_feature_status_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.feature_status COMMAND voelund_feature_status_tests "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(desktop.feature_status PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 30 RUN_SERIAL TRUE)
    if(WIN32)
        add_custom_command(TARGET voelund_feature_status_tests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE:Qt6::Widgets>"
                "$<TARGET_FILE:Qt6::OpenGL>" "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund_feature_status_tests>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:voelund_feature_status_tests>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:voelund_feature_status_tests>/platforms" VERBATIM)
    endif()
    add_executable(voelund_angle_input_tests desktop/AngleInputTests.cpp)
    target_link_libraries(voelund_angle_input_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.angle_inputs COMMAND voelund_angle_input_tests)
    set_tests_properties(desktop.angle_inputs PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 30 RUN_SERIAL TRUE)
    if(WIN32)
        add_custom_command(TARGET voelund_angle_input_tests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE:Qt6::Widgets>"
                "$<TARGET_FILE:Qt6::OpenGL>" "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund_angle_input_tests>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:voelund_angle_input_tests>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:voelund_angle_input_tests>/platforms" VERBATIM)
    endif()
    add_executable(voelund_parameter_failure_tests desktop/ParameterFailureTests.cpp)
    target_link_libraries(voelund_parameter_failure_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.parameter_failures COMMAND voelund_parameter_failure_tests)
    set_tests_properties(desktop.parameter_failures PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 30 RUN_SERIAL TRUE)
    if(WIN32)
        add_custom_command(TARGET voelund_parameter_failure_tests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE:Qt6::Widgets>"
                "$<TARGET_FILE:Qt6::OpenGL>" "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund_parameter_failure_tests>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:voelund_parameter_failure_tests>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:voelund_parameter_failure_tests>/platforms" VERBATIM)
    endif()
    add_executable(voelund_sketch_solver_gate_tests desktop/SketchSolverGateTests.cpp)
    target_link_libraries(voelund_sketch_solver_gate_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.sketch_solver_gate COMMAND voelund_sketch_solver_gate_tests)
    set_tests_properties(desktop.sketch_solver_gate PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 30 RUN_SERIAL TRUE)
    add_executable(voelund_sketch_drag_tests desktop/SketchDragTests.cpp)
    target_link_libraries(voelund_sketch_drag_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME desktop.sketch_drag COMMAND voelund_sketch_drag_tests)
    set_tests_properties(desktop.sketch_drag PROPERTIES LABELS "desktop;sketch;regression" TIMEOUT 30 RUN_SERIAL TRUE)
    if(WIN32)
        add_custom_command(TARGET voelund_sketch_solver_gate_tests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE:Qt6::Widgets>"
                "$<TARGET_FILE:Qt6::OpenGL>" "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund_sketch_solver_gate_tests>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:voelund_sketch_solver_gate_tests>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:voelund_sketch_solver_gate_tests>/platforms" VERBATIM)
    endif()
    add_executable(voelund_document_actions_tests desktop/DocumentActionsTests.cpp ../src/desktop/DocumentActions.cpp ../src/desktop/RecentFiles.cpp
        ../src/desktop/DocumentErrorText.cpp ../src/desktop/ShellState.cpp)
    target_link_libraries(voelund_document_actions_tests PRIVATE voelund_project_options Voelund::Application Qt6::Widgets)
    add_test(NAME desktop.document_actions COMMAND voelund_document_actions_tests)
    set_tests_properties(desktop.document_actions PROPERTIES LABELS "desktop;document;regression" TIMEOUT 30 RUN_SERIAL TRUE)
    if(WIN32)
        add_custom_command(TARGET voelund_document_actions_tests POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE:Qt6::Gui>" "$<TARGET_FILE:Qt6::Widgets>" "$<TARGET_FILE_DIR:voelund_document_actions_tests>"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:voelund_document_actions_tests>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>" "$<TARGET_FILE_DIR:voelund_document_actions_tests>/platforms" VERBATIM)
    endif()
    if(WIN32)
        # Explicit review tool: saving images alone must never count as visual approval.
        add_executable(voelund_viewport_visual_review EXCLUDE_FROM_ALL
            desktop/ViewportVisualReview.cpp)
        target_link_libraries(voelund_viewport_visual_review PRIVATE
            voelund_project_options Voelund::Application Voelund::Desktop opengl32)
    endif()
    add_executable(voelund_sketch_dynamic_input_tests viewport/SketchDynamicInputUnitTests.cpp)
    target_link_libraries(voelund_sketch_dynamic_input_tests PRIVATE
        voelund_project_options Voelund::Viewport)
    add_test(NAME viewport.sketch_dynamic_input COMMAND voelund_sketch_dynamic_input_tests)
    set_tests_properties(viewport.sketch_dynamic_input PROPERTIES
        LABELS "viewport;sketch;regression" TIMEOUT 30)
    if(VOELUND_VIEWPORT_BACKEND STREQUAL "qt-opengl")
        add_executable(voelund_viewport_lifecycle_tests
            desktop/ViewportLifecycleTests.cpp)
        target_link_libraries(voelund_viewport_lifecycle_tests PRIVATE
            voelund_project_options Voelund::Viewport Qt6::Widgets)
        if(WIN32)
            target_sources(voelund_viewport_lifecycle_tests PRIVATE desktop/ViewportDpiTests.cpp)
            target_link_libraries(voelund_viewport_lifecycle_tests PRIVATE psapi)
            target_compile_definitions(voelund_viewport_lifecycle_tests PRIVATE VOELUND_TEST_WINDOWS_DPI)
            add_custom_command(TARGET voelund_viewport_lifecycle_tests POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>" "$<TARGET_FILE:Qt6::Gui>"
                "$<TARGET_FILE:Qt6::Widgets>" "$<TARGET_FILE:Qt6::OpenGL>"
                "$<TARGET_FILE:Qt6::OpenGLWidgets>"
                "$<TARGET_FILE_DIR:voelund_viewport_lifecycle_tests>"
                COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:voelund_viewport_lifecycle_tests>/platforms"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:voelund_viewport_lifecycle_tests>/platforms"
                VERBATIM)
        endif()
        # UX01 inventory helper, built on demand only.
        add_executable(voelund_mcp_chip_capture EXCLUDE_FROM_ALL desktop/McpChipCapture.cpp)
        target_link_libraries(voelund_mcp_chip_capture PRIVATE voelund_project_options Voelund::Desktop)
        add_executable(voelund_mcp_client_acceptance EXCLUDE_FROM_ALL desktop/McpClientAcceptance.cpp)
        target_link_libraries(voelund_mcp_client_acceptance PRIVATE voelund_project_options Voelund::Desktop)
        add_executable(voelund_assistant_panel_acceptance EXCLUDE_FROM_ALL desktop/AssistantPanelAcceptance.cpp)
        target_link_libraries(voelund_assistant_panel_acceptance PRIVATE voelund_project_options Voelund::Desktop)
        add_executable(voelund_ui_inventory_capture EXCLUDE_FROM_ALL desktop/UiInventoryCapture.cpp)
        target_link_libraries(voelund_ui_inventory_capture PRIVATE
            voelund_project_options Voelund::Desktop)
        add_executable(voelund_base_plane_tests desktop/BasePlaneFlowTests.cpp)
        target_link_libraries(voelund_base_plane_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.base_planes COMMAND voelund_base_plane_tests)
        set_tests_properties(desktop.base_planes PROPERTIES
            LABELS "desktop;viewport;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.base_planes PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_start_tests desktop/SketchStartFlowTests.cpp)
        target_link_libraries(voelund_sketch_start_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_start COMMAND voelund_sketch_start_tests)
        set_tests_properties(desktop.sketch_start PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_start PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_face_pick_tests desktop/FacePickTests.cpp)
        target_link_libraries(voelund_face_pick_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.face_pick COMMAND voelund_face_pick_tests)
        set_tests_properties(desktop.face_pick PROPERTIES
            LABELS "desktop;viewport;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.face_pick PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_shell_state_tests desktop/ShellStateTests.cpp)
        target_link_libraries(voelund_shell_state_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.shell_state COMMAND voelund_shell_state_tests)
        set_tests_properties(desktop.shell_state PROPERTIES LABELS "desktop;regression" TIMEOUT 60)
        add_executable(voelund_command_surface_tests desktop/CommandSurfaceTests.cpp)
        target_link_libraries(voelund_command_surface_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.command_surfaces COMMAND voelund_command_surface_tests)
        set_tests_properties(desktop.command_surfaces PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.command_surfaces PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_tool_panel_tests desktop/ToolPanelLifecycleTests.cpp)
        target_link_libraries(voelund_tool_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.tool_panel_lifecycle COMMAND voelund_tool_panel_tests)
        set_tests_properties(desktop.tool_panel_lifecycle PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.tool_panel_lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_model_tree_tests desktop/ModelTreeTests.cpp)
        target_link_libraries(voelund_model_tree_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.model_tree COMMAND voelund_model_tree_tests)
        set_tests_properties(desktop.model_tree PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.model_tree PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_tool_rail_tests desktop/SketchToolRailTests.cpp)
        target_link_libraries(voelund_sketch_tool_rail_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_tool_rail COMMAND voelund_sketch_tool_rail_tests)
        set_tests_properties(desktop.sketch_tool_rail PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_tool_rail PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_v01_flow_tests desktop/V01FlowTests.cpp)
        target_link_libraries(voelund_v01_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.v01_flow COMMAND voelund_v01_flow_tests)
        set_tests_properties(desktop.v01_flow PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.v01_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_mcp_tool_tests integration/McpToolTests.cpp)
        target_link_libraries(voelund_mcp_tool_tests PRIVATE voelund_project_options Voelund::Mcp)
        add_test(NAME mcp.tools COMMAND voelund_mcp_tool_tests)
        set_tests_properties(mcp.tools PROPERTIES LABELS "integration;mcp;regression" TIMEOUT 90)
        add_executable(voelund_mcp_http_tests integration/McpHttpTests.cpp)
        target_link_libraries(voelund_mcp_http_tests PRIVATE voelund_project_options Voelund::Mcp)
        add_test(NAME mcp.http COMMAND voelund_mcp_http_tests)
        set_tests_properties(mcp.http PROPERTIES LABELS "integration;mcp;regression" TIMEOUT 90)
        if(WIN32)
            add_custom_command(TARGET voelund_mcp_http_tests POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:Qt6::Core>" "$<TARGET_FILE:Qt6::Network>"
                    "$<TARGET_FILE_DIR:voelund_mcp_http_tests>" VERBATIM)
        endif()
        add_executable(voelund_mcp_shell_tests desktop/McpShellTests.cpp)
        target_link_libraries(voelund_mcp_shell_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.mcp_shell COMMAND voelund_mcp_shell_tests)
        set_tests_properties(desktop.mcp_shell PROPERTIES
            LABELS "desktop;qt-opengl;mcp;regression" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.mcp_shell PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_fake_claude integration/FakeClaude.cpp)
        target_link_libraries(voelund_fake_claude PRIVATE voelund_project_options Qt6::Network)
        add_executable(voelund_assistant_backend_tests integration/AssistantBackendTests.cpp)
        target_link_libraries(voelund_assistant_backend_tests PRIVATE voelund_project_options
            Voelund::Assistant Voelund::Mcp)
        add_test(NAME assistant.backend
            COMMAND voelund_assistant_backend_tests "$<TARGET_FILE:voelund_fake_claude>")
        set_tests_properties(assistant.backend PROPERTIES LABELS "integration;assistant;regression" TIMEOUT 120)
        add_executable(voelund_assistant_panel_tests desktop/AssistantPanelTests.cpp)
        target_link_libraries(voelund_assistant_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.assistant_panel
            COMMAND voelund_assistant_panel_tests "$<TARGET_FILE:voelund_fake_claude>")
        set_tests_properties(desktop.assistant_panel PROPERTIES
            LABELS "desktop;qt-opengl;assistant;regression" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.assistant_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_view_cube_tests desktop/ViewCubeTests.cpp)
        target_link_libraries(voelund_view_cube_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.view_cube COMMAND voelund_view_cube_tests)
        set_tests_properties(desktop.view_cube PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.view_cube PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_feedback_tests desktop/SketchFeedbackTests.cpp)
        target_link_libraries(voelund_sketch_feedback_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_feedback COMMAND voelund_sketch_feedback_tests)
        set_tests_properties(desktop.sketch_feedback PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_feedback PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        foreach(flow panel_lifecycle_flow document_flow)
            if(flow STREQUAL "panel_lifecycle_flow")
                set(flow_source desktop/PanelLifecycleFlowTests.cpp)
            else()
                set(flow_source desktop/DocumentFlowTests.cpp)
            endif()
            add_executable(voelund_${flow}_tests ${flow_source})
            target_link_libraries(voelund_${flow}_tests PRIVATE voelund_project_options Voelund::Desktop)
            add_test(NAME desktop.${flow} COMMAND voelund_${flow}_tests)
            set_tests_properties(desktop.${flow} PROPERTIES
                LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
            if(VOELUND_NATIVE_WAYLAND)
                set_tests_properties(desktop.${flow} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
            endif()
        endforeach()
        add_executable(voelund_ui_flow_tests desktop/UiFlowTests.cpp)
        target_link_libraries(voelund_ui_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.ui_flow COMMAND voelund_ui_flow_tests)
        set_tests_properties(desktop.ui_flow PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.ui_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # S14: the reference sketch (face, regions, extrusion, Save/Open, A-REG-02) in the shell.
        add_executable(voelund_reference_sketch_flow_tests desktop/ReferenceSketchFlowTests.cpp)
        target_link_libraries(voelund_reference_sketch_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.reference_sketch_flow COMMAND voelund_reference_sketch_flow_tests)
        set_tests_properties(desktop.reference_sketch_flow PROPERTIES
            LABELS "desktop;qt-opengl;acceptance;sketch" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.reference_sketch_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # README pictures (not a test): VOELUND_SHOWCASE_DIR=docs/images voelund_showcase_capture
        add_executable(voelund_showcase_capture desktop/ShowcaseCapture.cpp)
        target_link_libraries(voelund_showcase_capture PRIVATE voelund_project_options Voelund::Desktop)
        # M18: the reference plate A-PART-01 through the shell, with history and STEP.
        add_executable(voelund_reference_part_flow_tests desktop/ReferencePartFlowTests.cpp)
        target_link_libraries(voelund_reference_part_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.reference_part COMMAND voelund_reference_part_flow_tests)
        set_tests_properties(desktop.reference_part PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 180 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.reference_part PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # L08: A-PART-02, the ring and an independent box through the shell.
        add_executable(voelund_reference_ring_flow_tests desktop/ReferenceRingFlowTests.cpp)
        target_link_libraries(voelund_reference_ring_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.reference_ring COMMAND voelund_reference_ring_flow_tests)
        set_tests_properties(desktop.reference_ring PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 180 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.reference_ring PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # N02b: groups of bodies and steps through the shell.
        add_executable(voelund_group_flow_tests desktop/GroupFlowTests.cpp)
        target_link_libraries(voelund_group_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.group_flow COMMAND voelund_group_flow_tests)
        set_tests_properties(desktop.group_flow PROPERTIES
            LABELS "desktop;qt-opengl" TIMEOUT 180 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.group_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # N02: copy and paste of bodies through the shell.
        add_executable(voelund_clipboard_flow_tests desktop/ClipboardFlowTests.cpp)
        target_link_libraries(voelund_clipboard_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.clipboard_flow COMMAND voelund_clipboard_flow_tests)
        set_tests_properties(desktop.clipboard_flow PROPERTIES
            LABELS "desktop;qt-opengl" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.clipboard_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # N02: "Zuletzt geöffnet" in the file menu.
        add_executable(voelund_recent_files_tests desktop/RecentFilesTests.cpp)
        target_link_libraries(voelund_recent_files_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.recent_files COMMAND voelund_recent_files_tests)
        set_tests_properties(desktop.recent_files PROPERTIES
            LABELS "desktop;qt-opengl;document" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.recent_files PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # N01: expressions in the fields and the parameter table.
        add_executable(voelund_expression_flow_tests desktop/ExpressionFlowTests.cpp)
        target_link_libraries(voelund_expression_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.expression_flow COMMAND voelund_expression_flow_tests)
        set_tests_properties(desktop.expression_flow PROPERTIES
            LABELS "desktop;qt-opengl" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.expression_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # N01b: measure fields of the tools and the properties bound to parameters.
        add_executable(voelund_field_binding_flow_tests desktop/FieldBindingFlowTests.cpp)
        target_link_libraries(voelund_field_binding_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.field_binding_flow COMMAND voelund_field_binding_flow_tests)
        set_tests_properties(desktop.field_binding_flow PROPERTIES
            LABELS "desktop;qt-opengl" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.field_binding_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # PAR02: the parameter table and "=Name" in the fields (user flow of the packet).
        add_executable(voelund_parameter_flow_tests desktop/ParameterFlowTests.cpp)
        target_link_libraries(voelund_parameter_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.parameter_flow COMMAND voelund_parameter_flow_tests)
        set_tests_properties(desktop.parameter_flow PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.parameter_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # M18: STEP import and export through the file menu (A-STEP-01, A-ERR-01).
        # N03: STL and 3MF through File > Export.
        add_executable(voelund_mesh_export_flow_tests desktop/MeshExportFlowTests.cpp)
        target_link_libraries(voelund_mesh_export_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.mesh_export_flow COMMAND voelund_mesh_export_flow_tests)
        set_tests_properties(desktop.mesh_export_flow PROPERTIES
            LABELS "desktop;qt-opengl" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.mesh_export_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_step_exchange_tests desktop/StepExchangeTests.cpp)
        target_link_libraries(voelund_step_exchange_tests PRIVATE voelund_project_options Voelund::Desktop TKDESTEP)
        add_test(NAME desktop.step_exchange COMMAND voelund_step_exchange_tests)
        set_tests_properties(desktop.step_exchange PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.step_exchange PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # M01: several bodies through the shell (A-BODY-01, A-BODY-03).
        add_executable(voelund_body_flow_tests desktop/BodyFlowTests.cpp)
        target_link_libraries(voelund_body_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.body_flow COMMAND voelund_body_flow_tests)
        set_tests_properties(desktop.body_flow PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.body_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # L03: "Über Völund…" in the main menu (version, revision, schema, copy).
        add_executable(voelund_about_dialog_tests desktop/AboutDialogTests.cpp)
        target_link_libraries(voelund_about_dialog_tests PRIVATE voelund_project_options Voelund::Desktop
            voelund_build_info_headers)
        add_test(NAME desktop.about_dialog COMMAND voelund_about_dialog_tests)
        set_tests_properties(desktop.about_dialog PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.about_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # L02: documents on the command line (first opens, umlaut/space path; further named).
        add_executable(voelund_product_arguments_tests desktop/ProductArgumentsTests.cpp)
        target_link_libraries(voelund_product_arguments_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.product_arguments COMMAND voelund_product_arguments_tests
            "${PROJECT_SOURCE_DIR}/tests/fixtures/documents/v09.volund")
        set_tests_properties(desktop.product_arguments PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.product_arguments PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        # S14: A-TXN-03 in the shell (sketch element and body manipulator drags).
        add_executable(voelund_drag_flow_tests desktop/DragFlowTests.cpp)
        target_link_libraries(voelund_drag_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.drag_flow COMMAND voelund_drag_flow_tests)
        set_tests_properties(desktop.drag_flow PROPERTIES
            LABELS "desktop;qt-opengl;acceptance" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.drag_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_view_switch_tests desktop/SketchViewSwitchTests.cpp)
        target_link_libraries(voelund_sketch_view_switch_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_view_switches COMMAND voelund_sketch_view_switch_tests)
        set_tests_properties(desktop.sketch_view_switches PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_view_switches PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_selection_tests desktop/SketchSelectionTests.cpp)
        target_link_libraries(voelund_sketch_selection_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_selection COMMAND voelund_sketch_selection_tests)
        set_tests_properties(desktop.sketch_selection PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_selection PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_keyboard_tests desktop/SketchKeyboardTests.cpp)
        target_link_libraries(voelund_sketch_keyboard_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_keyboard COMMAND voelund_sketch_keyboard_tests)
        set_tests_properties(desktop.sketch_keyboard PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_keyboard PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_dimension_edit_tests desktop/SketchDimensionEditTests.cpp)
        target_link_libraries(voelund_sketch_dimension_edit_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_dimension_edit COMMAND voelund_sketch_dimension_edit_tests)
        set_tests_properties(desktop.sketch_dimension_edit PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_dimension_edit PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_rectangle_options_flow_tests desktop/SketchRectangleOptionsFlowTests.cpp)
        target_link_libraries(voelund_sketch_rectangle_options_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_rectangle_options COMMAND voelund_sketch_rectangle_options_flow_tests)
        set_tests_properties(desktop.sketch_rectangle_options PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_rectangle_options PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_dimension_display_tests desktop/SketchDimensionDisplayTests.cpp)
        target_link_libraries(voelund_sketch_dimension_display_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_dimension_display COMMAND voelund_sketch_dimension_display_tests)
        set_tests_properties(desktop.sketch_dimension_display PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_dimension_display PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_curve_edit_flow_tests desktop/SketchCurveEditFlowTests.cpp)
        target_link_libraries(voelund_sketch_curve_edit_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_curve_edit_flow COMMAND voelund_sketch_curve_edit_flow_tests)
        set_tests_properties(desktop.sketch_curve_edit_flow PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_curve_edit_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_document_guard_tests desktop/DocumentGuardTests.cpp)
        target_link_libraries(voelund_document_guard_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.document_guards COMMAND voelund_document_guard_tests)
        set_tests_properties(desktop.document_guards PROPERTIES
            LABELS "desktop;document;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.document_guards PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_recovery_tests desktop/RecoveryTests.cpp)
        target_link_libraries(voelund_recovery_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.recovery COMMAND voelund_recovery_tests)
        set_tests_properties(desktop.recovery PROPERTIES
            LABELS "desktop;document;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.recovery PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_push_pull_panel_tests desktop/PushPullPanelTests.cpp)
        target_link_libraries(voelund_push_pull_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.push_pull_panel COMMAND voelund_push_pull_panel_tests)
        set_tests_properties(desktop.push_pull_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 90 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.push_pull_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_extrusion_panel_tests desktop/ExtrusionPanelTests.cpp)
        target_link_libraries(voelund_extrusion_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.extrusion_panel COMMAND voelund_extrusion_panel_tests)
        set_tests_properties(desktop.extrusion_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.extrusion_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_hole_panel_tests desktop/HolePanelTests.cpp)
        target_link_libraries(voelund_hole_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.hole_panel COMMAND voelund_hole_panel_tests)
        set_tests_properties(desktop.hole_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.hole_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_point_flow_tests desktop/SketchPointFlowTests.cpp)
        target_link_libraries(voelund_sketch_point_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_point_flow COMMAND voelund_sketch_point_flow_tests)
        set_tests_properties(desktop.sketch_point_flow PROPERTIES
            LABELS "desktop;qt-opengl;regression;sketch" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_point_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_project_flow_tests desktop/SketchProjectFlowTests.cpp)
        target_link_libraries(voelund_sketch_project_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_project_flow COMMAND voelund_sketch_project_flow_tests)
        set_tests_properties(desktop.sketch_project_flow PROPERTIES
            LABELS "desktop;qt-opengl;regression;sketch" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_project_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_reference_flow_tests desktop/SketchReferenceDimensionFlowTests.cpp)
        target_link_libraries(voelund_sketch_reference_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_reference_flow COMMAND voelund_sketch_reference_flow_tests)
        set_tests_properties(desktop.sketch_reference_flow PROPERTIES
            LABELS "desktop;qt-opengl;regression;sketch" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_reference_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_chamfer_flow_tests desktop/SketchChamferFlowTests.cpp)
        target_link_libraries(voelund_sketch_chamfer_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_chamfer_flow COMMAND voelund_sketch_chamfer_flow_tests)
        set_tests_properties(desktop.sketch_chamfer_flow PROPERTIES
            LABELS "desktop;qt-opengl;regression;sketch" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_chamfer_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_rotate_scale_flow_tests desktop/SketchRotateScaleFlowTests.cpp)
        target_link_libraries(voelund_sketch_rotate_scale_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_rotate_scale_flow COMMAND voelund_sketch_rotate_scale_flow_tests)
        set_tests_properties(desktop.sketch_rotate_scale_flow PROPERTIES
            LABELS "desktop;qt-opengl;regression;sketch" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_rotate_scale_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_primitive_panel_tests desktop/PrimitivePanelTests.cpp)
        target_link_libraries(voelund_primitive_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.primitive_panel COMMAND voelund_primitive_panel_tests)
        set_tests_properties(desktop.primitive_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.primitive_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sweep_panel_tests desktop/SweepPanelTests.cpp)
        target_link_libraries(voelund_sweep_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sweep_panel COMMAND voelund_sweep_panel_tests)
        set_tests_properties(desktop.sweep_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sweep_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_loft_panel_tests desktop/LoftPanelTests.cpp)
        target_link_libraries(voelund_loft_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.loft_panel COMMAND voelund_loft_panel_tests)
        set_tests_properties(desktop.loft_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.loft_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_helix_panel_tests desktop/HelixPanelTests.cpp)
        target_link_libraries(voelund_helix_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.helix_panel COMMAND voelund_helix_panel_tests)
        set_tests_properties(desktop.helix_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.helix_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_draft_panel_tests desktop/DraftPanelTests.cpp)
        target_link_libraries(voelund_draft_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.draft_panel COMMAND voelund_draft_panel_tests)
        set_tests_properties(desktop.draft_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.draft_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_bspline_flow_tests desktop/SketchBSplineFlowTests.cpp)
        target_link_libraries(voelund_sketch_bspline_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_bspline_flow COMMAND voelund_sketch_bspline_flow_tests)
        set_tests_properties(desktop.sketch_bspline_flow PROPERTIES
            LABELS "desktop;qt-opengl;regression;sketch" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_bspline_flow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_shell_panel_tests desktop/ShellPanelTests.cpp)
        target_link_libraries(voelund_shell_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.shell_panel COMMAND voelund_shell_panel_tests)
        set_tests_properties(desktop.shell_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.shell_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_mirror_panel_tests desktop/MirrorPanelTests.cpp)
        target_link_libraries(voelund_mirror_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.mirror_panel COMMAND voelund_mirror_panel_tests)
        set_tests_properties(desktop.mirror_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.mirror_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_pattern_panel_tests desktop/PatternPanelTests.cpp)
        target_link_libraries(voelund_pattern_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.pattern_panel COMMAND voelund_pattern_panel_tests)
        set_tests_properties(desktop.pattern_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.pattern_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_measure_panel_tests desktop/MeasurePanelTests.cpp)
        target_link_libraries(voelund_measure_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.measure_panel COMMAND voelund_measure_panel_tests)
        set_tests_properties(desktop.measure_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.measure_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_section_view_tests desktop/SectionViewTests.cpp)
        target_link_libraries(voelund_section_view_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.section_view COMMAND voelund_section_view_tests)
        set_tests_properties(desktop.section_view PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.section_view PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_body_appearance_tests desktop/BodyAppearanceTests.cpp)
        target_link_libraries(voelund_body_appearance_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.body_appearance COMMAND voelund_body_appearance_tests)
        set_tests_properties(desktop.body_appearance PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.body_appearance PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_dynamic_input_flow_tests desktop/SketchDynamicInputTests.cpp)
        target_link_libraries(voelund_sketch_dynamic_input_flow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_dynamic_input COMMAND voelund_sketch_dynamic_input_flow_tests)
        set_tests_properties(desktop.sketch_dynamic_input PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_dynamic_input PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_dimension_tool_tests desktop/SketchDimensionToolTests.cpp)
        target_link_libraries(voelund_sketch_dimension_tool_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_dimension_tool COMMAND voelund_sketch_dimension_tool_tests)
        set_tests_properties(desktop.sketch_dimension_tool PROPERTIES
            LABELS "desktop;qt-opengl;sketch;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_dimension_tool PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_extrude_arrow_tests desktop/ExtrudeArrowTests.cpp)
        target_link_libraries(voelund_extrude_arrow_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.extrude_arrow COMMAND voelund_extrude_arrow_tests)
        set_tests_properties(desktop.extrude_arrow PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.extrude_arrow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_tree_menu_tests desktop/TreeMenuTests.cpp)
        target_link_libraries(voelund_tree_menu_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.tree_menu COMMAND voelund_tree_menu_tests)
        set_tests_properties(desktop.tree_menu PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.tree_menu PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_fillet_twice_tests desktop/FilletTwiceTests.cpp)
        target_link_libraries(voelund_fillet_twice_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.fillet_twice COMMAND voelund_fillet_twice_tests)
        set_tests_properties(desktop.fillet_twice PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.fillet_twice PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_history_edit_tests desktop/HistoryEditTests.cpp)
        target_link_libraries(voelund_history_edit_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.history_edit COMMAND voelund_history_edit_tests)
        set_tests_properties(desktop.history_edit PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.history_edit PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_timeline_tests desktop/TimelineTests.cpp)
        target_link_libraries(voelund_timeline_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.timeline COMMAND voelund_timeline_tests)
        set_tests_properties(desktop.timeline PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.timeline PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_revolve_panel_tests desktop/RevolvePanelTests.cpp)
        target_link_libraries(voelund_revolve_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.revolve_panel COMMAND voelund_revolve_panel_tests)
        set_tests_properties(desktop.revolve_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.revolve_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_datum_panel_tests desktop/DatumPanelTests.cpp)
        target_link_libraries(voelund_datum_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.datum_panel COMMAND voelund_datum_panel_tests)
        set_tests_properties(desktop.datum_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.datum_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_combine_panel_tests desktop/CombinePanelTests.cpp)
        target_link_libraries(voelund_combine_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.combine_panel COMMAND voelund_combine_panel_tests)
        set_tests_properties(desktop.combine_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.combine_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_fillet_panel_tests desktop/FilletPanelTests.cpp)
        target_link_libraries(voelund_fillet_panel_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.fillet_panel COMMAND voelund_fillet_panel_tests)
        set_tests_properties(desktop.fillet_panel PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.fillet_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_action_shell_tests desktop/ActionShellTests.cpp)
        target_link_libraries(voelund_action_shell_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.action_shell COMMAND voelund_action_shell_tests)
        set_tests_properties(desktop.action_shell PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 120 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.action_shell PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_sketch_snap_tests desktop/SketchSnapTests.cpp)
        target_link_libraries(voelund_sketch_snap_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.sketch_snap COMMAND voelund_sketch_snap_tests)
        set_tests_properties(desktop.sketch_snap PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.sketch_snap PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_executable(voelund_command_inventory_tests desktop/CommandInventoryTests.cpp)
        target_link_libraries(voelund_command_inventory_tests PRIVATE voelund_project_options Voelund::Desktop)
        add_test(NAME desktop.command_inventory COMMAND voelund_command_inventory_tests)
        set_tests_properties(desktop.command_inventory PROPERTIES
            LABELS "desktop;qt-opengl;regression" TIMEOUT 60 RUN_SERIAL TRUE)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.command_inventory PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=wayland")
        endif()
        add_test(NAME desktop.viewport_lifecycle_qt COMMAND voelund_viewport_lifecycle_tests)
        set_tests_properties(desktop.viewport_lifecycle_qt
            PROPERTIES LABELS "desktop;viewport;lifecycle;qt-opengl" TIMEOUT 45)
        if(VOELUND_NATIVE_WAYLAND)
            set_tests_properties(desktop.viewport_lifecycle_qt PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=wayland"
                LABELS "desktop;viewport;lifecycle;qt-opengl;${VOELUND_PLATFORM_LABEL};wayland")
        endif()
        if(WIN32)
            foreach(scale IN ITEMS 1 1.5 2)
                add_test(NAME desktop.viewport_dpi_${scale}
                    COMMAND voelund_viewport_lifecycle_tests --dpi)
                set_tests_properties(desktop.viewport_dpi_${scale} PROPERTIES
                    LABELS "desktop;viewport;dpi;qt-opengl" TIMEOUT 120
                    RUN_SERIAL TRUE ENVIRONMENT "QT_SCALE_FACTOR=${scale}")
            endforeach()
        endif()
    endif()
endif()

add_executable(voelund_sketch_editor_tests integration/SketchEditorSessionTests.cpp integration/SketchEditorRelationTests.cpp)
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

# S15: single sketch points.
add_executable(voelund_sketch_point_tests integration/SketchPointTests.cpp)
target_link_libraries(voelund_sketch_point_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_points COMMAND voelund_sketch_point_tests)
set_tests_properties(application.sketch_points PROPERTIES LABELS "integration;application;sketch" TIMEOUT 120)

# S17: body edges projected into a sketch.
add_executable(voelund_sketch_projection_tests integration/SketchProjectionTests.cpp)
target_link_libraries(voelund_sketch_projection_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_projections COMMAND voelund_sketch_projection_tests)
set_tests_properties(application.sketch_projections PROPERTIES LABELS "integration;application;sketch" TIMEOUT 120)

add_executable(voelund_sketch_alignment_tests integration/SketchAlignmentTests.cpp)
target_link_libraries(
    voelund_sketch_alignment_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.sketch_alignments COMMAND voelund_sketch_alignment_tests)

add_executable(voelund_sketch_fix_distance_tests integration/SketchFixDistanceTests.cpp)
target_link_libraries(
    voelund_sketch_fix_distance_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.sketch_fix_distance COMMAND voelund_sketch_fix_distance_tests)

add_executable(voelund_sketch_basic_tools_tests integration/SketchBasicToolsTests.cpp)
target_link_libraries(
    voelund_sketch_basic_tools_tests
    PRIVATE
        voelund_project_options
        Voelund::Application
)

add_test(NAME application.sketch_basic_tools COMMAND voelund_sketch_basic_tools_tests)

add_executable(voelund_sketch_rectangle_options_tests integration/SketchRectangleOptionsTests.cpp)
target_link_libraries(voelund_sketch_rectangle_options_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_rectangle_options COMMAND voelund_sketch_rectangle_options_tests)

add_executable(voelund_sketch_creation_tests integration/SketchCreationTests.cpp)
target_link_libraries(voelund_sketch_creation_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_creation COMMAND voelund_sketch_creation_tests)
set_tests_properties(application.sketch_creation PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 60)

add_executable(voelund_sketch_editor_state_tests integration/SketchEditorStateTests.cpp)
target_link_libraries(voelund_sketch_editor_state_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_editor_state COMMAND voelund_sketch_editor_state_tests)
set_tests_properties(application.sketch_editor_state PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 60)

add_executable(voelund_sketch_slot_tests integration/SketchSlotTests.cpp)
target_link_libraries(voelund_sketch_slot_tests PRIVATE
    voelund_project_options Voelund::Application)
add_test(NAME application.sketch_slot_dimensions COMMAND voelund_sketch_slot_tests)
set_tests_properties(application.sketch_slot_dimensions
    PROPERTIES LABELS "integration;application;regression")

add_executable(voelund_workspace_tests unit/CommandRegistryTests.cpp)
target_link_libraries(
    voelund_workspace_tests
    PRIVATE
        voelund_project_options
        Voelund::ModuleSolid
)

add_test(NAME workspaces.module_registry COMMAND voelund_workspace_tests)

set_tests_properties(
    kernel.shape_factory
    PROPERTIES LABELS "unit;kernel"
)
set_tests_properties(
    document.history
    PROPERTIES LABELS "unit;document"
)
set_tests_properties(
    features.sketch_profiles
    features.planegcs_solver
    features.sketch_constraint_adapter
    PROPERTIES LABELS "unit;features"
)
set_tests_properties(
    application.recompute_history
    application.sketch_editor_flow
    application.sketch_construction
    application.sketch_circle_constraints
    application.sketch_alignments
    application.sketch_fix_distance
    application.sketch_basic_tools
    PROPERTIES LABELS "integration;application"
)
set_tests_properties(
    workspaces.module_registry
    PROPERTIES LABELS "unit;workspaces"
)

add_test(
    NAME architecture.source_limits
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/tools/cmake/CheckSourceLimits.cmake"
)

add_test(
    NAME architecture.source_limits_regression
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/source-limit-fixtures"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/CheckSourceLimitsTests.cmake"
)

# LIZENZ: every own source file names its licence (SPDX), with failing fixtures.
add_test(
    NAME architecture.source_licenses
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/tools/cmake/CheckLicenseHeaders.cmake"
)
add_test(
    NAME architecture.source_licenses_regression
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/license-header-fixtures"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/CheckLicenseHeadersTests.cmake"
)

set_tests_properties(
    architecture.source_limits
    architecture.source_limits_regression
    architecture.source_licenses
    architecture.source_licenses_regression
    PROPERTIES LABELS "architecture;regression"
)

# A04: format and K01 layer includes over the tree, plus their failing fixtures. A missing
# clang-format fails these tests instead of skipping them.
add_test(NAME architecture.format
    COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DVOELUND_CLANG_FORMAT=${VOELUND_CLANG_FORMAT}"
        -P "${PROJECT_SOURCE_DIR}/tools/cmake/CheckFormat.cmake")
add_test(NAME architecture.layer_includes
    COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/tools/cmake/CheckLayerIncludes.cmake")
add_test(NAME architecture.layer_and_format_regression
    COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DVOELUND_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/layer-format-fixtures"
        "-DVOELUND_CLANG_FORMAT=${VOELUND_CLANG_FORMAT}"
        "-DVOELUND_GENERATOR=${CMAKE_GENERATOR}" "-DVOELUND_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/LayerAndFormatTests.cmake")
add_test(NAME architecture.function_limits_regression
    COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        "-DVOELUND_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/function-limit-fixtures"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/FunctionLimitsTests.cmake")
set_tests_properties(
    architecture.format
    architecture.layer_includes
    architecture.layer_and_format_regression
    architecture.function_limits_regression
    PROPERTIES LABELS "architecture;regression"
)

add_test(
    NAME architecture.ctest_label_runner
    COMMAND
        "${CMAKE_COMMAND}"
        "-DVOELUND_CTEST_COMMAND=${CMAKE_CTEST_COMMAND}"
        "-DVOELUND_BINARY_DIR=${CMAKE_BINARY_DIR}"
        "-DVOELUND_TEST_LABEL=unit"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/RunLabeledTests.cmake"
)
set_tests_properties(architecture.ctest_label_runner PROPERTIES LABELS "architecture;regression")

add_test(NAME architecture.desktop_smoke_runner
    COMMAND "${CMAKE_COMMAND}"
        "-DVOELUND_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/expected-smoke-fixtures"
        -P "${PROJECT_SOURCE_DIR}/tests/cmake/ExpectedSmokeRunnerTests.cmake")
set_tests_properties(architecture.desktop_smoke_runner
    PROPERTIES LABELS "architecture;regression")

if(WIN32)
    find_program(VOELUND_POWERSHELL7_EXECUTABLE NAMES pwsh.exe REQUIRED)
    add_test(NAME architecture.platform_report
        COMMAND "${VOELUND_POWERSHELL7_EXECUTABLE}" -NoProfile -File
            "${PROJECT_SOURCE_DIR}/tests/powershell/PlatformReport.Tests.ps1")
    set_tests_properties(architecture.platform_report PROPERTIES LABELS "architecture;regression" TIMEOUT 60)
    find_program(VOELUND_POWERSHELL_EXECUTABLE NAMES pwsh.exe powershell.exe REQUIRED)
    add_test(
        NAME architecture.setup_windows_mapping
        COMMAND
            "${VOELUND_POWERSHELL_EXECUTABLE}"
            -NoProfile
            -ExecutionPolicy Bypass
            -File "${PROJECT_SOURCE_DIR}/tests/powershell/SetupWindowsMapping.Tests.ps1"
    )
    set_tests_properties(architecture.setup_windows_mapping PROPERTIES LABELS "architecture;regression")
endif()
add_executable(voelund_numeric_policy_tests integration/NumericPolicyTests.cpp)
target_link_libraries(voelund_numeric_policy_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME document.numeric_policy COMMAND voelund_numeric_policy_tests)
set_tests_properties(document.numeric_policy PROPERTIES LABELS "integration;document;kernel;regression" TIMEOUT 60)
add_executable(voelund_boolean_connection_tests integration/BooleanConnectionTests.cpp)
target_link_libraries(voelund_boolean_connection_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME features.boolean_connection COMMAND voelund_boolean_connection_tests)
set_tests_properties(features.boolean_connection PROPERTIES LABELS "integration;features;kernel;regression" TIMEOUT 60)
add_executable(voelund_sketch_id_capacity_tests integration/SketchIdCapacityTests.cpp)
target_link_libraries(voelund_sketch_id_capacity_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_id_capacity COMMAND voelund_sketch_id_capacity_tests)
set_tests_properties(application.sketch_id_capacity PROPERTIES LABELS "integration;document;application;regression" TIMEOUT 60)
add_executable(voelund_legacy_body_identity_tests integration/LegacyBodyIdentityTests.cpp)
target_link_libraries(voelund_legacy_body_identity_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.legacy_body_identity COMMAND voelund_legacy_body_identity_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents")
set_tests_properties(document.legacy_body_identity PROPERTIES LABELS "integration;document;regression" TIMEOUT 60)
add_executable(voelund_feature_id_lifetime_tests integration/FeatureIdLifetimeTests.cpp)
target_link_libraries(voelund_feature_id_lifetime_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME document.feature_id_lifetime COMMAND voelund_feature_id_lifetime_tests)
set_tests_properties(document.feature_id_lifetime PROPERTIES LABELS "integration;document;application;regression" TIMEOUT 60)
add_executable(voelund_body_identity_evaluation_tests integration/BodyIdentityEvaluationTests.cpp)
add_executable(voelund_sketch_reference_review_tests integration/SketchReferenceReviewTests.cpp)
add_executable(voelund_sketch_reference_repair_tests integration/SketchReferenceRepairTests.cpp)
target_link_libraries(voelund_sketch_reference_repair_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_reference_repair COMMAND voelund_sketch_reference_repair_tests "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(application.sketch_reference_repair PROPERTIES LABELS "integration;application;regression" TIMEOUT 60)
target_link_libraries(voelund_sketch_reference_review_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_reference_review COMMAND voelund_sketch_reference_review_tests "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/documents")
set_tests_properties(application.sketch_reference_review PROPERTIES LABELS "integration;application;regression" TIMEOUT 60)
target_link_libraries(voelund_body_identity_evaluation_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME features.body_identity COMMAND voelund_body_identity_evaluation_tests)
set_tests_properties(features.body_identity PROPERTIES LABELS "integration;document;features;regression" TIMEOUT 60)

add_executable(voelund_sketch_contour_graph_tests unit/SketchContourGraphTests.cpp)
target_link_libraries(voelund_sketch_contour_graph_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.sketch_contour_graph COMMAND voelund_sketch_contour_graph_tests)
set_tests_properties(features.sketch_contour_graph PROPERTIES LABELS "features;sketch;regression" TIMEOUT 90)

add_executable(voelund_sketch_intersection_tests unit/SketchIntersectionTests.cpp)
target_link_libraries(voelund_sketch_intersection_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.sketch_intersections COMMAND voelund_sketch_intersection_tests)
set_tests_properties(features.sketch_intersections PROPERTIES LABELS "features;sketch;regression" TIMEOUT 90)

add_executable(voelund_sketch_region_tests unit/SketchRegionTests.cpp)
target_link_libraries(voelund_sketch_region_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.sketch_regions COMMAND voelund_sketch_region_tests)
set_tests_properties(features.sketch_regions PROPERTIES LABELS "features;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_region_identity_tests unit/SketchRegionIdentityTests.cpp)
target_link_libraries(voelund_sketch_region_identity_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.sketch_region_identity COMMAND voelund_sketch_region_identity_tests)
set_tests_properties(features.sketch_region_identity PROPERTIES LABELS "unit;features;sketch;regression" TIMEOUT 60)
add_executable(voelund_sketch_region_face_tests unit/SketchRegionFaceTests.cpp)
target_link_libraries(voelund_sketch_region_face_tests PRIVATE voelund_project_options Voelund::Features)
add_test(NAME features.sketch_region_faces COMMAND voelund_sketch_region_face_tests)
set_tests_properties(features.sketch_region_faces PROPERTIES LABELS "unit;features;sketch;regression" TIMEOUT 90)
add_executable(voelund_region_migration_tests integration/RegionMigrationTests.cpp)
target_link_libraries(voelund_region_migration_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.region_migration COMMAND voelund_region_migration_tests "${CMAKE_CURRENT_BINARY_DIR}")
set_tests_properties(application.region_migration PROPERTIES LABELS "integration;document;application;regression" TIMEOUT 60)
add_executable(voelund_region_selection_tests integration/RegionSelectionAcceptanceTests.cpp)
target_link_libraries(voelund_region_selection_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.region_selection_acceptance COMMAND voelund_region_selection_tests)
set_tests_properties(application.region_selection_acceptance PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_datum_sketch_tests integration/DatumSketchTests.cpp)
target_link_libraries(voelund_datum_sketch_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.datum_sketch COMMAND voelund_datum_sketch_tests)
set_tests_properties(application.datum_sketch PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_offset_tests integration/SketchOffsetTests.cpp)
target_link_libraries(voelund_sketch_offset_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_offset COMMAND voelund_sketch_offset_tests)
set_tests_properties(application.sketch_offset PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
# S14: the tool x aspect matrix of A-SKETCH-01 (preview, create, edit, undo/redo, save/open).
add_executable(voelund_sketch_tool_matrix_tests integration/SketchToolMatrixTests.cpp)
target_link_libraries(voelund_sketch_tool_matrix_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_tool_matrix COMMAND voelund_sketch_tool_matrix_tests)
set_tests_properties(application.sketch_tool_matrix PROPERTIES LABELS "integration;application;sketch;acceptance" TIMEOUT 120)
add_executable(voelund_sketch_curve_edit_tests integration/SketchCurveEditTests.cpp)
target_link_libraries(voelund_sketch_curve_edit_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_curve_edit COMMAND voelund_sketch_curve_edit_tests)
add_executable(voelund_sketch_annotation_tests integration/SketchAnnotationTests.cpp)
target_link_libraries(voelund_sketch_annotation_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_annotations COMMAND voelund_sketch_annotation_tests)
set_tests_properties(application.sketch_annotations PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_editor_pattern_tests integration/SketchEditorPatternTests.cpp)
target_link_libraries(voelund_sketch_editor_pattern_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_editor_patterns COMMAND voelund_sketch_editor_pattern_tests)
set_tests_properties(application.sketch_editor_patterns PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_fillet_tests integration/SketchFilletTests.cpp)
target_link_libraries(voelund_sketch_fillet_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_fillet COMMAND voelund_sketch_fillet_tests)
set_tests_properties(application.sketch_fillet PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_polygon_slot_tests integration/SketchPolygonSlotTests.cpp)
target_link_libraries(voelund_sketch_polygon_slot_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_polygon_slot COMMAND voelund_sketch_polygon_slot_tests)
set_tests_properties(application.sketch_polygon_slot PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
set_tests_properties(application.sketch_curve_edit PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_curve_creation_tests integration/SketchCurveCreationTests.cpp)
target_link_libraries(voelund_sketch_curve_creation_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_curve_creation COMMAND voelund_sketch_curve_creation_tests)
set_tests_properties(application.sketch_curve_creation PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_snap_matrix_tests integration/SketchSnapMatrixTests.cpp)
target_link_libraries(voelund_sketch_snap_matrix_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_snap_matrix COMMAND voelund_sketch_snap_matrix_tests)
set_tests_properties(application.sketch_snap_matrix PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_transform_tests integration/SketchTransformTests.cpp)
target_link_libraries(voelund_sketch_transform_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_transforms COMMAND voelund_sketch_transform_tests)
set_tests_properties(application.sketch_transforms PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_sketch_support_frame_tests integration/SketchSupportFrameTests.cpp)
target_link_libraries(voelund_sketch_support_frame_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_support_frames COMMAND voelund_sketch_support_frame_tests)
set_tests_properties(application.sketch_support_frames PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_extrusion_operation_tests integration/ExtrusionOperationTests.cpp)
target_link_libraries(voelund_extrusion_operation_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.extrusion_operations COMMAND voelund_extrusion_operation_tests)
set_tests_properties(application.extrusion_operations PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)
add_executable(voelund_push_pull_tests integration/PushPullTests.cpp)
target_link_libraries(voelund_push_pull_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.push_pull COMMAND voelund_push_pull_tests)
set_tests_properties(application.push_pull PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)
add_executable(voelund_document_parameter_tests integration/DocumentParameterTests.cpp)
target_link_libraries(voelund_document_parameter_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.document_parameters COMMAND voelund_document_parameter_tests)
set_tests_properties(application.document_parameters PROPERTIES LABELS "integration;application;sketch;regression" TIMEOUT 90)
add_executable(voelund_action_request_tests integration/ActionRequestTests.cpp)
target_link_libraries(voelund_action_request_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.action_requests COMMAND voelund_action_request_tests)
set_tests_properties(application.action_requests PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)
# M03: A-EXT-02 join, cut/pocket and intersect with explained refusals.
add_executable(voelund_boolean_extrusion_tests integration/BooleanExtrusionTests.cpp)
target_link_libraries(voelund_boolean_extrusion_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.boolean_extrusions COMMAND voelund_boolean_extrusion_tests)
set_tests_properties(application.boolean_extrusions PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M08: A-HOLE-01 holes into a box face.
add_executable(voelund_hole_tests integration/HoleTests.cpp)
target_link_libraries(voelund_hole_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.hole COMMAND voelund_hole_tests)
set_tests_properties(application.hole PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M22: shells.
add_executable(voelund_shell_tests integration/ShellTests.cpp)
target_link_libraries(voelund_shell_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.shell COMMAND voelund_shell_tests)
set_tests_properties(application.shell PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M09: mirrored bodies.
add_executable(voelund_mirror_tests integration/MirrorTests.cpp)
target_link_libraries(voelund_mirror_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.mirror COMMAND voelund_mirror_tests)
set_tests_properties(application.mirror PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M10: linear and circular body patterns.
add_executable(voelund_pattern_tests integration/PatternTests.cpp)
target_link_libraries(voelund_pattern_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.pattern COMMAND voelund_pattern_tests)
set_tests_properties(application.pattern PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M11: moving any body.
add_executable(voelund_move_tests integration/MoveTests.cpp)
target_link_libraries(voelund_move_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.move COMMAND voelund_move_tests)
set_tests_properties(application.move PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# PAR02: binding parameters in the sketch editor and in a new extrusion.
# N01b: measure fields of every feature bound to parameters.
add_executable(voelund_field_binding_tests integration/FieldBindingSessionTests.cpp)
target_link_libraries(voelund_field_binding_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.field_bindings COMMAND voelund_field_binding_tests)
set_tests_properties(application.field_bindings PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# N01: parameters as expressions in the session.
add_executable(voelund_parameter_expression_session_tests integration/ParameterExpressionSessionTests.cpp)
target_link_libraries(voelund_parameter_expression_session_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.parameter_expressions COMMAND voelund_parameter_expression_session_tests)
set_tests_properties(application.parameter_expressions PROPERTIES LABELS "integration;application" TIMEOUT 120)
add_executable(voelund_parameter_binding_tests integration/ParameterBindingTests.cpp)
target_link_libraries(voelund_parameter_binding_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.parameter_binding COMMAND voelund_parameter_binding_tests)
set_tests_properties(application.parameter_binding PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M18: A-PART-02, the ring and an independent box.
add_executable(voelund_reference_ring_tests integration/ReferenceRingTests.cpp)
target_link_libraries(voelund_reference_ring_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.reference_ring COMMAND voelund_reference_ring_tests)
set_tests_properties(application.reference_ring PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# N02b: several bodies moved or steps suppressed as one undo step.
add_executable(voelund_group_edit_tests integration/GroupEditTests.cpp)
target_link_libraries(voelund_group_edit_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.group_edits COMMAND voelund_group_edit_tests)
set_tests_properties(application.group_edits PROPERTIES LABELS "integration;application" TIMEOUT 120)
# N02: copied bodies pasted as fixed copies.
add_executable(voelund_clipboard_tests integration/ClipboardTests.cpp)
target_link_libraries(voelund_clipboard_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.clipboard COMMAND voelund_clipboard_tests)
set_tests_properties(application.clipboard PROPERTIES LABELS "integration;application" TIMEOUT 120)
# M17: STEP import and export.
add_executable(voelund_step_tests integration/StepTests.cpp)
target_link_libraries(voelund_step_tests PRIVATE voelund_project_options Voelund::Application TKDESTEP)
add_test(NAME application.step COMMAND voelund_step_tests)
set_tests_properties(application.step PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# N03: meshes for 3D printing (STL, 3MF).
add_executable(voelund_mesh_export_tests unit/MeshExportTests.cpp)
target_link_libraries(voelund_mesh_export_tests PRIVATE voelund_project_options Voelund::Kernel)
add_test(NAME kernel.mesh_export COMMAND voelund_mesh_export_tests)
set_tests_properties(kernel.mesh_export PROPERTIES LABELS "unit;kernel;acceptance" TIMEOUT 120)
add_executable(voelund_mesh_exchange_tests integration/MeshExchangeTests.cpp)
target_link_libraries(voelund_mesh_exchange_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.mesh_export COMMAND voelund_mesh_exchange_tests)
set_tests_properties(application.mesh_export PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# N04: IGES export of bodies, DXF export of a sketch.
add_executable(voelund_iges_export_tests unit/IgesExportTests.cpp)
target_link_libraries(voelund_iges_export_tests PRIVATE voelund_project_options Voelund::Kernel TKDEIGES)
add_test(NAME kernel.iges_export COMMAND voelund_iges_export_tests)
set_tests_properties(kernel.iges_export PROPERTIES LABELS "unit;kernel;acceptance" TIMEOUT 120)
add_executable(voelund_sketch_dxf_tests unit/SketchDxfTests.cpp)
target_link_libraries(voelund_sketch_dxf_tests PRIVATE voelund_project_options Voelund::Document)
add_test(NAME document.sketch_dxf COMMAND voelund_sketch_dxf_tests)
set_tests_properties(document.sketch_dxf PROPERTIES LABELS "unit;document;acceptance" TIMEOUT 60)
# L03: the program reports its version without a window.
if(VOELUND_BUILD_DESKTOP)
    add_test(NAME platform.version COMMAND voelund-cad --version)
    set_tests_properties(platform.version PROPERTIES LABELS "platform;package"
        PASS_REGULAR_EXPRESSION "^Völund CAD ${PROJECT_VERSION} \\(" TIMEOUT 30)
endif()
# L02: the installed runtime is complete and self-contained; a missing library is named.
if(VOELUND_BUILD_DESKTOP AND (WIN32 OR VOELUND_NATIVE_WAYLAND))
    add_test(NAME platform.package_runtime
        COMMAND "${CMAKE_COMMAND}" "-DVOELUND_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
            "-DVOELUND_BUILD_DIR=${PROJECT_BINARY_DIR}" "-DVOELUND_CONFIG=$<CONFIG>"
            "-DVOELUND_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/package-runtime"
            "-DVOELUND_VERSION=${PROJECT_VERSION}"
            "-DVOELUND_COMPONENTS=${VOELUND_COMPONENTS}"
            "-DVOELUND_COMPONENT_VERSIONS=${VOELUND_COMPONENT_VERSIONS}"
            -P "${PROJECT_SOURCE_DIR}/tests/cmake/PackageRuntimeTests.cmake")
    set_tests_properties(platform.package_runtime PROPERTIES
        LABELS "platform;package;acceptance" TIMEOUT 300 RUN_SERIAL TRUE)
endif()
# U15: German text for every evaluation error code.
if(VOELUND_BUILD_DESKTOP)
    add_executable(voelund_feature_issue_text_tests unit/FeatureIssueTextTests.cpp)
    target_link_libraries(voelund_feature_issue_text_tests PRIVATE voelund_project_options Voelund::Desktop)
    add_test(NAME unit.feature_issue_text COMMAND voelund_feature_issue_text_tests)
    set_tests_properties(unit.feature_issue_text PROPERTIES LABELS "unit;regression" TIMEOUT 30)
endif()
# L06 (A-PERF-01): load stages; the full series runs by hand (tools/run-perf.sh), CTest only
# keeps the smallest stages working.
add_executable(voelund_perf_bench perf/PerfBench.cpp)
target_link_libraries(voelund_perf_bench PRIVATE voelund_project_options Voelund::Application)
if(WIN32)
    target_link_libraries(voelund_perf_bench PRIVATE psapi)
endif()
foreach(stage sketch-100 features-10 bodies-10)
    add_test(NAME perf.smoke.${stage} COMMAND voelund_perf_bench ${stage} "${CMAKE_CURRENT_BINARY_DIR}")
    set_tests_properties(perf.smoke.${stage} PROPERTIES LABELS "perf;smoke" TIMEOUT 300)
endforeach()
# UNDO-MESSUNG: memory and time of the undo history (full OCAF states); the series runs by
# hand (tools/run-undo-bench.sh), CTest only keeps the smallest run working.
add_executable(voelund_undo_bench perf/UndoBench.cpp)
target_link_libraries(voelund_undo_bench PRIVATE voelund_project_options Voelund::Application)
if(WIN32)
    target_link_libraries(voelund_undo_bench PRIVATE psapi)
endif()
add_test(NAME perf.smoke.undo-small-10 COMMAND voelund_undo_bench small 10)
set_tests_properties(perf.smoke.undo-small-10 PROPERTIES LABELS "perf;smoke" TIMEOUT 300)
# Q01: NIST test parts as an outside reference for STEP import.
add_executable(voelund_nist_step_tests differential/NistStepTests.cpp)
target_link_libraries(voelund_nist_step_tests PRIVATE voelund_project_options Voelund::Kernel TKDESTEP)
add_test(NAME differential.nist_step
    COMMAND voelund_nist_step_tests "${CMAKE_CURRENT_SOURCE_DIR}/differential/nist"
        "${CMAKE_CURRENT_SOURCE_DIR}/differential/nist/reference.txt")
set_tests_properties(differential.nist_step PROPERTIES
    LABELS "differential;kernel;acceptance" TIMEOUT 600)
add_executable(voelund_freecad_reference_tests differential/FreecadReferenceTests.cpp)
target_link_libraries(voelund_freecad_reference_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME differential.freecad_parts
    COMMAND voelund_freecad_reference_tests "${CMAKE_CURRENT_SOURCE_DIR}/differential/freecad/reference.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/differential/freecad/known-differences.txt")
set_tests_properties(differential.freecad_parts PROPERTIES
    LABELS "differential;application;acceptance" TIMEOUT 120)
# The comparison must be able to fail: a table with CTC-03 1e-4 off.
add_test(NAME differential.nist_step_detects
    COMMAND voelund_nist_step_tests "${CMAKE_CURRENT_SOURCE_DIR}/differential/nist"
        "${CMAKE_CURRENT_SOURCE_DIR}/differential/nist/reference-wrong.txt")
set_tests_properties(differential.nist_step_detects PROPERTIES
    LABELS "differential;kernel;regression" WILL_FAIL TRUE TIMEOUT 300)
# M12: measuring.
add_executable(voelund_measure_tests integration/MeasureTests.cpp)
target_link_libraries(voelund_measure_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.measure COMMAND voelund_measure_tests)
set_tests_properties(application.measure PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M13: early edits through the whole history.
add_executable(voelund_history_tests integration/HistoryTests.cpp)
target_link_libraries(voelund_history_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.history COMMAND voelund_history_tests)
set_tests_properties(application.history PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M20: revolves and grooves.
add_executable(voelund_revolve_tests integration/RevolveTests.cpp)
target_link_libraries(voelund_revolve_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.revolve COMMAND voelund_revolve_tests)
set_tests_properties(application.revolve PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M19: construction planes and axes.
add_executable(voelund_datum_tests integration/DatumTests.cpp)
target_link_libraries(voelund_datum_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.datum COMMAND voelund_datum_tests)
set_tests_properties(application.datum PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M21: Booleans of whole bodies.
add_executable(voelund_combine_tests integration/CombineTests.cpp)
target_link_libraries(voelund_combine_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.combine COMMAND voelund_combine_tests)
set_tests_properties(application.combine PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M05: A-FILLET-01 fillet of a box edge and an extrusion side edge.
add_executable(voelund_fillet_tests integration/FilletTests.cpp)
target_link_libraries(voelund_fillet_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.fillet COMMAND voelund_fillet_tests)
set_tests_properties(application.fillet PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M02: A-EXT-01 directions, symmetric and intersect extrusions.
add_executable(voelund_extrusion_direction_tests integration/ExtrusionDirectionTests.cpp)
target_link_libraries(voelund_extrusion_direction_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.extrusion_directions COMMAND voelund_extrusion_direction_tests)
set_tests_properties(application.extrusion_directions PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)
# M01: A-DAG-01 with two sketch-and-extrusion bodies.
add_executable(voelund_body_dag_tests integration/BodyDagTests.cpp)
target_link_libraries(voelund_body_dag_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.body_dag COMMAND voelund_body_dag_tests)
set_tests_properties(application.body_dag PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)
# M01: body names and visibility (stored, rename undoable, hide without undo step).
add_executable(voelund_body_attribute_tests integration/BodyAttributeTests.cpp)
target_link_libraries(voelund_body_attribute_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.body_attributes COMMAND voelund_body_attribute_tests)
set_tests_properties(application.body_attributes PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)
# M01: a cylinder as a new body or on the selected body.
add_executable(voelund_new_body_cylinder_tests integration/NewBodyCylinderTests.cpp)
target_link_libraries(voelund_new_body_cylinder_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.new_body_cylinder COMMAND voelund_new_body_cylinder_tests)
set_tests_properties(application.new_body_cylinder PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)
add_executable(voelund_action_target_tests integration/ActionTargetTests.cpp)
target_link_libraries(voelund_action_target_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.action_targets COMMAND voelund_action_target_tests)
set_tests_properties(application.action_targets PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)
add_executable(voelund_action_candidate_tests integration/ActionCandidateTests.cpp)
target_link_libraries(voelund_action_candidate_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.action_candidates COMMAND voelund_action_candidate_tests)
set_tests_properties(application.action_candidates PROPERTIES LABELS "integration;application;regression" TIMEOUT 90)

# S18: reference dimensions.
add_executable(voelund_sketch_reference_dimension_tests integration/SketchReferenceDimensionTests.cpp)
target_link_libraries(voelund_sketch_reference_dimension_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_reference_dimensions COMMAND voelund_sketch_reference_dimension_tests)
set_tests_properties(application.sketch_reference_dimensions PROPERTIES LABELS "integration;application;sketch" TIMEOUT 120)

# S19: sketch chamfer.
add_executable(voelund_sketch_chamfer_tests integration/SketchChamferTests.cpp)
target_link_libraries(voelund_sketch_chamfer_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_chamfer COMMAND voelund_sketch_chamfer_tests)
set_tests_properties(application.sketch_chamfer PROPERTIES LABELS "integration;application;sketch" TIMEOUT 120)

# S20: turning and scaling in the sketch.
add_executable(voelund_sketch_rotate_scale_tests integration/SketchRotateScaleTests.cpp)
target_link_libraries(voelund_sketch_rotate_scale_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_rotate_scale COMMAND voelund_sketch_rotate_scale_tests)
set_tests_properties(application.sketch_rotate_scale PROPERTIES LABELS "integration;application;sketch" TIMEOUT 120)

# M27: further primitive solids.
add_executable(voelund_primitive_solid_tests integration/PrimitiveSolidTests.cpp)
target_link_libraries(voelund_primitive_solid_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.primitive_solids COMMAND voelund_primitive_solid_tests)
set_tests_properties(application.primitive_solids PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)

# M23: sweeps along a path.
add_executable(voelund_sweep_tests integration/SweepTests.cpp)
target_link_libraries(voelund_sweep_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sweep COMMAND voelund_sweep_tests)
set_tests_properties(application.sweep PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)

# M24: lofts through sections.
add_executable(voelund_loft_tests integration/LoftTests.cpp)
target_link_libraries(voelund_loft_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.loft COMMAND voelund_loft_tests)
set_tests_properties(application.loft PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)

# M26: helices.
add_executable(voelund_helix_tests integration/HelixTests.cpp)
target_link_libraries(voelund_helix_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.helix COMMAND voelund_helix_tests)
set_tests_properties(application.helix PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 180)

# M25: drafts.
add_executable(voelund_draft_tests integration/DraftTests.cpp)
target_link_libraries(voelund_draft_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.draft COMMAND voelund_draft_tests)
set_tests_properties(application.draft PROPERTIES LABELS "integration;application;acceptance" TIMEOUT 120)

# S21: B-splines in sketches.
add_executable(voelund_sketch_bspline_tests integration/SketchBSplineTests.cpp)
target_link_libraries(voelund_sketch_bspline_tests PRIVATE voelund_project_options Voelund::Application)
add_test(NAME application.sketch_bspline COMMAND voelund_sketch_bspline_tests)
set_tests_properties(application.sketch_bspline PROPERTIES LABELS "integration;application;sketch" TIMEOUT 120)
