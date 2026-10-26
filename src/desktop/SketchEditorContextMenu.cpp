// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"

#include "ToolPanelHost.hpp"

#include <QMenu>
#include <QObject>

#include <utility>

namespace voelund::desktop {
namespace {

// The relations the menu offers, as SolidCraft's "Constraints" heading: only those the
// selection allows show (the panel enables them by the selection).
constexpr std::pair<const char*, const char*> relations[]{
    {"horizontal", QT_TR_NOOP("Horizontal")},
    {"vertical", QT_TR_NOOP("Vertikal")},
    {"coincident", QT_TR_NOOP("Koinzident")},
    {"parallel", QT_TR_NOOP("Parallel")},
    {"perpendicular", QT_TR_NOOP("Senkrecht")},
    {"collinear", QT_TR_NOOP("Kollinear")},
    {"equal", QT_TR_NOOP("Gleich")},
    {"tangent", QT_TR_NOOP("Tangential")},
    {"concentric", QT_TR_NOOP("Konzentrisch")},
    {"midpoint", QT_TR_NOOP("Mittelpunkt")},
    {"symmetric", QT_TR_NOOP("Symmetrisch")},
    {"point_on_object", QT_TR_NOOP("Punkt auf Objekt")},
    {"horizontal_points", QT_TR_NOOP("Horizontal (Punkte)")},
    {"vertical_points", QT_TR_NOOP("Vertikal (Punkte)")},
    {"fixed", QT_TR_NOOP("Fixieren")}};

// The drawing tools at hand, by their index in the panel's tool choice.
constexpr std::pair<int, const char*> tools[]{{1, QT_TR_NOOP("Linie\tL")},
                                              {3, QT_TR_NOOP("Rechteck\tR")},
                                              {4, QT_TR_NOOP("Kreis\tC")},
                                              {13, QT_TR_NOOP("Bemaßung\tD")}};

void addTools(QMenu& menu, ToolPanelHost& panel) {
    for (const auto& [index, label] : tools)
        QObject::connect(menu.addAction(QObject::tr(label)), &QAction::triggered, &menu,
                         [&panel, tool = index] { panel.chooseOption("tool", tool); });
}

// Only with a selection, and only those the panel allows for it.
void addRelations(QMenu& menu, ToolPanelHost& panel) {
    bool heading = false;
    for (const auto& [id, label] : relations) {
        if (!panel.isActionEnabled(id))
            continue;
        if (!heading)
            menu.addSection(QObject::tr("Beziehungen"));
        heading = true;
        QObject::connect(menu.addAction(QObject::tr(label)), &QAction::triggered, &menu,
                         [&panel, action = std::string(id)] { panel.triggerAction(action); });
    }
}

void addEdits(QMenu& menu, ToolPanelHost& panel) {
    menu.addSeparator();
    for (const auto& [id, label] : {std::pair{"construction", QT_TR_NOOP("Hilfsgeometrie\tX")},
                                    std::pair{"delete", QT_TR_NOOP("Löschen\tEntf")}}) {
        QAction* action = menu.addAction(QObject::tr(label));
        action->setEnabled(panel.isActionEnabled(id));
        QObject::connect(action, &QAction::triggered, &menu,
                         [&panel, actionId = std::string(id)] { panel.triggerAction(actionId); });
    }
    menu.addSeparator();
    QObject::connect(menu.addAction(QObject::tr("Skizze beenden")), &QAction::triggered, &menu,
                     [&panel] { panel.requestAccept(); });
}

} // namespace

void SketchEditorToolController::dragDimension(std::uint64_t id, double x, double y,
                                               bool finished) {
    if (!editor_)
        return;
    if (editor_->moveDimension(document::SketchConstraintId{id}, {x, y}))
        refreshPresentation();
    if (!finished)
        return;
    editor_->finishDimensionMove();
    syncConstraintState();
    statusChanged_(QObject::tr("Maß verschoben"));
}

void SketchEditorToolController::contextRequested(QPoint globalPosition) {
    if (!editor_ || !panel_.isActive())
        return;
    // As SolidCraft and Fusion: a right click ends the drawing tool.
    if (panel_.choiceIndex("tool") > 0) {
        panel_.chooseOption("tool", 0);
        return;
    }
    QMenu menu(&viewport_); // a popup needs its window under Wayland
    menu.setObjectName(QStringLiteral("sketchContextMenu"));
    addTools(menu, panel_);
    if (editor_->selectedElement() || !editor_->selectedReferences().empty())
        addRelations(menu, panel_);
    addEdits(menu, panel_);
    // Test hook: the entries as offered (a virtual session shows no popups).
    QStringList entries;
    for (const QAction* action : menu.actions())
        if (!action->isSeparator())
            entries << action->text();
    viewport_.setProperty("sketchMenuEntries", entries);
    menu.exec(globalPosition);
}

} // namespace voelund::desktop
