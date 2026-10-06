add_library(
    voelund_desktop STATIC
    FeaturePropertiesPanel.cpp
    FloatingCard.cpp
    DocumentActions.cpp
    CommandActions.cpp
    CommandPalette.cpp
    ContextBar.cpp
    ContextCommands.cpp
    MainWindow.cpp
    AssistantPanel.cpp
    MainWindowAssistant.cpp
    MainWindowCommands.cpp
    MainWindowParameters.cpp
    MainWindowReferenceRepair.cpp
    ReferenceRepairDialog.cpp
    DocumentRepairDialog.cpp
    ReferenceRepairPreview.cpp
    MainWindowSupport.cpp
    MainWindowView.cpp
    McpAccess.cpp
    MeasurementInput.cpp
    ModelTreePanel.cpp
    SketchConstraintLabel.cpp
    SketchEditorConstraintState.cpp
    SketchEditorCommand.cpp
    SketchEditorCreation.cpp
    SketchEditorDrag.cpp
    SketchEditorPresentation.cpp
    SketchEditorToolController.cpp
    SketchIcons.cpp
    SketchEditorStart.cpp
    SketchExtrusionToolController.cpp
    ShellHeader.cpp
    ShellIcons.cpp
    ShellState.cpp
    SketchToolActions.cpp
    SketchViewAdapter.cpp
    Theme.cpp
    ToolPanelHost.cpp
    TimelineBar.cpp
    ToolRail.cpp
    ViewCube.cpp
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
        Voelund::Mcp
        Voelund::Assistant
        Qt6::Widgets
)

# Fonts of ADR 0012 (OFL 1.1, docs/provenance/fonts.md).
qt_add_resources(
    voelund_desktop
    "voelund_fonts"
    PREFIX "/voelund/fonts"
    BASE "${PROJECT_SOURCE_DIR}/resources/fonts"
    FILES
        "${PROJECT_SOURCE_DIR}/resources/fonts/AlbertSans-Variable.ttf"
        "${PROJECT_SOURCE_DIR}/resources/fonts/JetBrainsMono-Variable.ttf"
)

add_library(Voelund::Desktop ALIAS voelund_desktop)
