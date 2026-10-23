# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Eduard Hart

add_library(
    voelund_desktop STATIC
    AboutDialog.cpp
    DocumentErrorText.cpp
    FeatureErrorCard.cpp
    FeatureIssueText.cpp
    FeaturePropertiesPanel.cpp
    FloatingCard.cpp
    OverlayCard.cpp
    PillButton.cpp
    DocumentActions.cpp
    RecentFiles.cpp
    CommandActions.cpp
    CommandPalette.cpp
    ContextBar.cpp
    ContextCommands.cpp
    MainWindow.cpp
    MainWindowBodies.cpp
    MainWindowClipboard.cpp
    MainWindowGroups.cpp
    ProductShell.cpp
    AssistantPanel.cpp
    MainWindowAssistant.cpp
    MainWindowCommands.cpp
    MainWindowParameters.cpp
    MainWindowRecovery.cpp
    MainWindowReferenceRepair.cpp
    ReferenceRepairDialog.cpp
    RecoveryDialog.cpp
    RecoveryStore.cpp
    AutosaveController.cpp
    DocumentRepairDialog.cpp
    ReferenceRepairPreview.cpp
    MainWindowSupport.cpp
    MainWindowView.cpp
    McpAccess.cpp
    MeasurementInput.cpp
    ParameterInput.cpp
    ParameterPanel.cpp
    MainWindowDocumentParameters.cpp
    ModelTreePanel.cpp
    SketchEditorConstraintState.cpp
    SketchEditorCommand.cpp
    SketchEditorCreation.cpp
    SketchTypedDimensions.cpp
    SketchEditorDrag.cpp
    SketchEditorPresentation.cpp
    SketchEditorDimensionEdit.cpp
    SketchEditorParameters.cpp
    SketchExtrusionModes.cpp
    DraftToolController.cpp
    SketchEditorClosing.cpp
    SketchEditorRecovery.cpp
    SketchEditorToolController.cpp
    SketchIcons.cpp
    SketchEditorStart.cpp
    SketchExtrusionToolController.cpp
    PushPullToolController.cpp
    FilletToolController.cpp
    HoleToolController.cpp
    CombineToolController.cpp
    DatumToolController.cpp
    ShellToolController.cpp
    MirrorToolController.cpp
    PatternToolController.cpp
    MoveToolController.cpp
    MeasureToolController.cpp
    MainWindowTools.cpp
    MainWindowInteractions.cpp
    ShellHeader.cpp
    ShellIcons.cpp
    SectionController.cpp
    BodyColorCard.cpp
    DisplayStyleMenu.cpp
    ShellState.cpp
    SketchToolActions.cpp
    SketchViewAdapter.cpp
    StaleBanner.cpp
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
        Voelund::Platform
        Voelund::Viewport
        Voelund::ModuleSolid
        Voelund::Mcp
        Voelund::Assistant
        Qt6::Widgets
    PRIVATE
        voelund_build_info_headers
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

# N16: the programme symbol for the window and task bar (packaging/icons).
qt_add_resources(
    voelund_desktop
    "voelund_icons"
    PREFIX "/voelund/icons"
    BASE "${PROJECT_SOURCE_DIR}/packaging/icons"
    FILES "${PROJECT_SOURCE_DIR}/packaging/icons/voelund-cad-256.png"
)

add_library(Voelund::Desktop ALIAS voelund_desktop)
