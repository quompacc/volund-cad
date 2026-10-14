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
    case document::FeatureKind::fillet:
        return QObject::tr("Verrundung");
    case document::FeatureKind::chamfer:
        return QObject::tr("Fase");
    case document::FeatureKind::hole:
        return QObject::tr("Bohrung");
    case document::FeatureKind::combine:
        return QObject::tr("Kombinieren");
    case document::FeatureKind::datumPlane:
        return QObject::tr("Bezugsebene");
    case document::FeatureKind::datumAxis:
        return QObject::tr("Bezugsachse");
    case document::FeatureKind::shell:
        return QObject::tr("Wandstärke");
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
        result.commands = {"solid.create.sketch", "solid.create.datum_plane"};
        result.primary = "solid.create.sketch";
        break;
    case ShellSelection::planarFace:
        result.title = QObject::tr("Ebene Fläche");
        result.commands = {"solid.create.sketch", "solid.modify.push_pull", "solid.modify.hole",
                           "solid.modify.shell", "solid.create.datum_plane"};
        result.primary = "solid.create.sketch";
        break;
    case ShellSelection::edges:
        result.title = QObject::tr("%1 Kante(n)").arg(state.edgeCount);
        result.commands = {"solid.modify.fillet", "solid.modify.chamfer"};
        if (state.edgeCount == 1)
            result.commands.push_back("solid.create.datum_axis"); // M19
        result.primary = "solid.modify.fillet";
        break;
    case ShellSelection::body:
        // M01 (user decision 2026-10-10): rename, show/hide, move where possible, and a
        // cylinder on this body.
        result.title = state.bodyName;
        result.commands = {"body.rename", "body.visibility", "solid.modify.transform",
                           "solid.modify.fillet", "solid.modify.chamfer", "solid.modify.combine",
                           "solid.create.cylinder"};
        break;
    case ShellSelection::sketchRegion:
        result.title = QObject::tr("Skizzenbereich");
        result.commands = {"solid.create.extrude_profiles", "solid.create.revolve",
                           "solid.modify.edit_sketch"};
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
        // M19: a construction plane carries sketches; both kinds reopen their panel.
        if (state.featureKind == document::FeatureKind::datumPlane ||
            state.featureKind == document::FeatureKind::datumAxis) {
            result.commands = {"solid.modify.edit_sketch"};
            if (state.featureKind == document::FeatureKind::datumPlane) {
                result.commands.insert(result.commands.begin(), "solid.create.sketch");
                result.primary = "solid.create.sketch";
            }
            break;
        }
        result.commands = {"solid.modify.transform", "solid.modify.fillet",
                           "solid.modify.chamfer", "solid.create.cylinder"};
        if (state.extrusionEditable || state.filletEditable) {
            result.commands.insert(result.commands.begin(), "solid.modify.edit_sketch");
            result.primary = "solid.modify.edit_sketch";
        }
        break;
    }
    return result;
}

} // namespace voelund::desktop
