add_library(voelund_application STATIC
    CommandTransaction.cpp
    CadSession.cpp
    CadSessionHistory.cpp
    CadSessionCreation.cpp
    CadSessionTransactions.cpp
    CadSessionPersistence.cpp
    CadSessionSketchProfiles.cpp
    SketchEditorConstruction.cpp
    SketchEditorAcceptance.cpp
    SketchEditorDimensions.cpp
    SketchEditorGeometry.cpp
    SketchEditorHistory.cpp
    SketchEditorOperations.cpp
    SketchEditorPointConstraints.cpp
    SketchEditorRelations.cpp
    SketchEditorRectangle.cpp
    SketchEditorSession.cpp
)

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
        Voelund::Platform
)

add_library(Voelund::Application ALIAS voelund_application)
