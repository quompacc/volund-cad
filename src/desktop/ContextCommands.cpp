// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ContextCommands.hpp"

#include <QObject>

#include <iterator>

namespace voelund::desktop {
namespace {

// M01 (user decision 2026-10-10): rename, show/hide, move where possible, and a cylinder on this
// body; N02: copy.
std::vector<std::string> bodyCommands() {
    return {"body.rename",
            "body.visibility",
            "edit.copy",
            "solid.modify.transform",
            "solid.modify.fillet",
            "solid.modify.chamfer",
            "solid.modify.combine",
            "solid.modify.mirror",
            "solid.create.cylinder"};
}

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

// The name of each feature kind, in FeatureKind order (N12a: a table instead of a switch).
constexpr const char* featureNames[]{
    QT_TRANSLATE_NOOP("QObject", "Quader"),         QT_TRANSLATE_NOOP("QObject", "Zylinder"),
    QT_TRANSLATE_NOOP("QObject", "Skizze"),         QT_TRANSLATE_NOOP("QObject", "Extrusion"),
    QT_TRANSLATE_NOOP("QObject", "Verrundung"),     QT_TRANSLATE_NOOP("QObject", "Fase"),
    QT_TRANSLATE_NOOP("QObject", "Bohrung"),        QT_TRANSLATE_NOOP("QObject", "Kombinieren"),
    QT_TRANSLATE_NOOP("QObject", "Bezugsebene"),    QT_TRANSLATE_NOOP("QObject", "Bezugsachse"),
    QT_TRANSLATE_NOOP("QObject", "Wandstärke"),     QT_TRANSLATE_NOOP("QObject", "Grundkörper"),
    QT_TRANSLATE_NOOP("QObject", "Formschräge"),    QT_TRANSLATE_NOOP("QObject", "Spiegeln"),
    QT_TRANSLATE_NOOP("QObject", "Muster"),         QT_TRANSLATE_NOOP("QObject", "Musterkopie"),
    QT_TRANSLATE_NOOP("QObject", "Bewegen"),        QT_TRANSLATE_NOOP("QObject", "Import"),
    QT_TRANSLATE_NOOP("QObject", "Importteil"),     QT_TRANSLATE_NOOP("QObject", "Teilen"),
    QT_TRANSLATE_NOOP("QObject", "Geteilter Teil"), QT_TRANSLATE_NOOP("QObject", "Flächen"),
    QT_TRANSLATE_NOOP("QObject", "Bezugspunkt")};
static_assert(std::size(featureNames) ==
              static_cast<std::size_t>(document::FeatureKind::datumPoint) + 1);

QString featureName(const ShellState& state) {
    if (!state.featureKind.has_value())
        return QObject::tr("Feature");
    return QObject::tr(featureNames[static_cast<std::size_t>(*state.featureKind)]);
}

} // namespace

// A picked plane offers what starts from a plane; I04: any other face only the face tools. False
// when no face is picked.
static bool faceCommands(ShellSelection selection, ContextCommands& result) {
    if (selection == ShellSelection::planarFace) {
        result.title = QObject::tr("Ebene Fläche");
        result.commands = {"solid.create.sketch", "solid.modify.push_pull", "solid.modify.hole",
                           "solid.modify.shell", "solid.create.datum_plane"};
        result.primary = "solid.create.sketch";
        return true;
    }
    if (selection != ShellSelection::curvedFace)
        return false;
    result.title = QObject::tr("Gekrümmte Fläche");
    result.commands = {"solid.modify.offset_face", "solid.modify.delete_face"};
    result.primary = "solid.modify.offset_face";
    return true;
}

// What a running tool shows instead of the selection's commands; false when none runs.
static bool toolCommands(ShellTool tool, ContextCommands& result) {
    switch (tool) {
    case ShellTool::sketchEditor:
    case ShellTool::extrusion:
        // The active tool's panel carries its own controls and OK/Cancel.
        result.visible = false;
        return true;
    case ShellTool::transform:
        result.title = QObject::tr("Verschieben/Drehen");
        result.commands = {"transform.snap", "solid.modify.transform"};
        result.transformSettings = true;
        return true;
    case ShellTool::none:
        break;
    }
    return false;
}

ContextCommands contextCommands(const ShellState& state) {
    ContextCommands result;
    if (toolCommands(state.tool, result))
        return result;
    if (faceCommands(state.selection, result))
        return result;
    switch (state.selection) {
    case ShellSelection::planarFace:
    case ShellSelection::curvedFace:
        break; // above
    case ShellSelection::nothing:
        result.title = QObject::tr("Ebene oder Fläche anklicken");
        result.commands = {"solid.create.sketch", "solid.create.box", "solid.create.cylinder"};
        break;
    case ShellSelection::basePlane:
        result.title = planeName(*state.basePlane);
        result.commands = {"solid.create.sketch", "solid.create.datum_plane"};
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
        result.title = state.bodyName;
        result.commands = bodyCommands();
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
        result.commands = {"solid.modify.edit_sketch", "repair.references", "history.suppress",
                           "history.rollback_here", "history.delete"};
        result.primary = "solid.modify.edit_sketch";
        break;
    case ShellSelection::feature:
        result.title = featureName(state);
        // M19: a construction plane carries sketches; both kinds reopen their panel.
        if (state.featureKind && document::isConstruction(*state.featureKind)) {
            result.commands = {"solid.modify.edit_sketch", "history.suppress",
                               "history.rollback_here", "history.delete"};
            if (state.featureKind == document::FeatureKind::datumPlane) {
                result.commands.insert(result.commands.begin(), "solid.create.sketch");
                result.primary = "solid.create.sketch";
            }
            break;
        }
        result.commands = {"solid.modify.transform", "solid.modify.fillet", "solid.modify.chamfer",
                           "solid.create.cylinder",  "history.suppress",    "history.rollback_here",
                           "history.delete"};
        if (state.extrusionEditable || state.filletEditable) {
            result.commands.insert(result.commands.begin(), "solid.modify.edit_sketch");
            result.primary = "solid.modify.edit_sketch";
        }
        break;
    }
    return result;
}

} // namespace voelund::desktop
