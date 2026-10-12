#include "ContextCommands.hpp"

#include <QObject>

namespace voelund::desktop {
namespace {

QString planeName(document::DatumPlane plane) {
    switch (plane) {
    case document::DatumPlane::xy:
        return QObject::tr("XY-Ebene");
    case document::DatumPlane::xz:
        return QObject::tr("XZ-Ebene");
    case document::DatumPlane::yz:
        return QObject::tr("YZ-Ebene");
    }
    return QObject::tr("Ebene");
}

QString featureName(const ShellState& state) {
    if (!state.featureKind.has_value())
        return QObject::tr("Feature");
    switch (*state.featureKind) {
    case document::FeatureKind::box:
        return QObject::tr("Quader");
    case document::FeatureKind::cylinder:
        return QObject::tr("Zylinder");
    case document::FeatureKind::extrusion:
        return QObject::tr("Extrusion");
    case document::FeatureKind::sketch:
        return QObject::tr("Skizze");
    }
    return QObject::tr("Feature");
}

} // namespace

ContextCommands contextCommands(const ShellState& state) {
    ContextCommands result;
    switch (state.tool) {
    case ShellTool::sketchEditor:
    case ShellTool::extrusion:
        // The active tool's panel carries its own controls and OK/Cancel.
        result.visible = false;
        return result;
    case ShellTool::transform:
        result.title = QObject::tr("Verschieben/Drehen");
        result.commands = {"transform.snap", "solid.modify.transform"};
        result.transformSettings = true;
        return result;
    case ShellTool::none:
        break;
    }
    switch (state.selection) {
    case ShellSelection::nothing:
        result.title = QObject::tr("Ebene oder Fläche anklicken");
        result.commands = {"solid.create.sketch", "solid.create.box", "solid.create.cylinder"};
        break;
    case ShellSelection::basePlane:
        result.title = planeName(*state.basePlane);
        result.commands = {"solid.create.sketch"};
        result.primary = "solid.create.sketch";
        break;
    case ShellSelection::planarFace:
        result.title = QObject::tr("Ebene Fläche");
        result.commands = {"solid.create.sketch", "solid.modify.push_pull"};
        result.primary = "solid.create.sketch";
        break;
    case ShellSelection::body:
        // M01 (user decision 2026-10-10): rename, show/hide, move where possible, and a
        // cylinder on this body.
        result.title = state.bodyName;
        result.commands = {"body.rename", "body.visibility", "solid.modify.transform",
                           "solid.create.cylinder"};
        break;
    case ShellSelection::sketchRegion:
        result.title = QObject::tr("Skizzenbereich");
        result.commands = {"solid.create.extrude_profiles", "solid.modify.edit_sketch"};
        result.primary = "solid.create.extrude_profiles";
        break;
    case ShellSelection::sketchElement:
    case ShellSelection::sketch:
        result.title = QObject::tr("Skizze");
        result.commands = {"solid.modify.edit_sketch", "repair.references"};
        result.primary = "solid.modify.edit_sketch";
        break;
    case ShellSelection::feature:
        result.title = featureName(state);
        result.commands = {"solid.modify.transform", "solid.create.cylinder"};
        if (state.extrusionEditable) {
            result.commands.insert(result.commands.begin(), "solid.modify.edit_sketch");
            result.primary = "solid.modify.edit_sketch";
        }
        break;
    }
    return result;
}

} // namespace voelund::desktop
