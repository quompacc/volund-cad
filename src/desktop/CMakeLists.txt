add_library(
    voelund_desktop STATIC
    FeaturePropertiesPanel.cpp
    DocumentActions.cpp
    MainWindow.cpp
    MainWindowParameters.cpp
    MainWindowReferenceRepair.cpp
    ReferenceRepairDialog.cpp
    DocumentRepairDialog.cpp
    ReferenceRepairPreview.cpp
    MainWindowSupport.cpp
    MeasurementInput.cpp
    ModelTreePanel.cpp
    SketchConstraintLabel.cpp
    SketchEditorConstraintState.cpp
    SketchEditorCommand.cpp
    SketchEditorCreation.cpp
    SketchEditorPresentation.cpp
    SketchEditorToolController.cpp
    SketchEditorStart.cpp
    SketchExtrusionToolController.cpp
    SketchViewAdapter.cpp
    ToolPanelHost.cpp
    WorkspaceToolbar.cpp
)

target_include_directories(
    voelund_desktop
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
)
target_link_libraries(
    voelund_desktop
    PUBLIC
        voelund_project_options
        Voelund::Application
        Voelund::Viewport
        Voelund::ModuleSolid
        Qt6::Widgets
)

add_library(Voelund::Desktop ALIAS voelund_desktop)
