#include "ShellState.hpp"

#include <voelund/application/CadSession.hpp>

#include <QFileInfo>

namespace voelund::desktop {

QString nativeDisplayPath(const std::filesystem::path& path) {
#ifdef _WIN32
    return QString::fromStdWString(path.native());
#else
    return QString::fromUtf8(path.native().c_str());
#endif
}

bool ShellState::toolBlocksCommands() const noexcept {
    return tool == ShellTool::sketchEditor || tool == ShellTool::extrusion;
}

bool ShellState::canSketch() const noexcept {
    return modelCurrent && !toolBlocksCommands() &&
           (selection == ShellSelection::basePlane || selection == ShellSelection::planarFace);
}

bool ShellState::canExtrude() const noexcept {
    return selection == ShellSelection::sketchRegion && regionExtrudable && !toolBlocksCommands();
}

bool ShellState::canEditSketch() const noexcept {
    return feature.has_value() && featureKind == document::FeatureKind::sketch &&
           !toolBlocksCommands();
}

bool ShellState::canEditFeature() const noexcept {
    return canEditSketch() ||
           ((extrusionEditable || filletEditable) && !toolBlocksCommands());
}

QString ShellState::windowTitle() const {
    return QStringLiteral("%1%2 — Völund CAD")
        .arg(documentName, modified ? QStringLiteral(" *") : QString{});
}

QString documentDisplayName(const application::CadSession& session) {
    const QString currentFile = nativeDisplayPath(session.currentFile());
    return currentFile.isEmpty() ? QObject::tr("Unbenannt") : QFileInfo(currentFile).fileName();
}

ShellState projectShell(const application::CadSession& session, const ShellInputs& inputs) {
    ShellState state;
    state.documentName = documentDisplayName(session);
    state.modified = session.isModified();
    state.canUndo = session.canUndo();
    state.canRedo = session.canRedo();
    state.modelCurrent = session.sceneStatus().isCurrent();
    state.tool = inputs.tool;
    if (state.toolBlocksCommands())
        state.canUndo = state.canRedo = false; // The tool panel owns Ctrl+Z meanwhile.
    state.feature = session.selectedFeature();
    if (state.feature.has_value())
        state.featureKind = session.selectedFeatureDescriptor().kind;
    state.regionExtrudable = session.canExtrudeSelectedSketchProfiles();
    state.filletEditable = state.featureKind == document::FeatureKind::fillet ||
                           state.featureKind == document::FeatureKind::chamfer;
    if (state.featureKind == document::FeatureKind::extrusion)
        state.extrusionEditable =
            !session.identifiedSnapshot().snapshot().extrusionParameters(*state.feature).faceProfile;
    // View picks are more specific than the document selection they sit on.
    if (inputs.basePlane.has_value()) {
        state.selection = ShellSelection::basePlane;
        state.basePlane = inputs.basePlane;
    } else if (inputs.planarFace) {
        state.selection = ShellSelection::planarFace;
    } else if (inputs.body.has_value()) {
        state.selection = ShellSelection::body;
        state.body = inputs.body;
        state.bodyName = inputs.bodyName;
        state.bodyHidden = inputs.bodyHidden;
    } else if (!session.selectedSketchProfiles().empty()) {
        state.selection = ShellSelection::sketchRegion;
    } else if (session.selectedSketchElement().has_value()) {
        state.selection = ShellSelection::sketchElement;
    } else if (state.featureKind == document::FeatureKind::sketch) {
        state.selection = ShellSelection::sketch;
    } else if (state.feature.has_value()) {
        state.selection = ShellSelection::feature;
    }
    return state;
}

} // namespace voelund::desktop
