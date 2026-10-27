// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "InterferenceToolController.hpp"

#include "ToolPanelHost.hpp"

#include <voelund/kernel/Interference.hpp>

#include <QApplication>
#include <QClipboard>
#include <QLocale>
#include <QObject>
#include <QStringList>

#include <TopExp_Explorer.hxx>

#include <utility>

namespace voelund::desktop {
namespace {

QString number(double value, bool fullPrecision) {
    return fullPrecision ? QLocale().toString(value, 'g', 15) : QLocale().toString(value, 'f', 3);
}

} // namespace

InterferenceToolController::InterferenceToolController(
    viewport::CadViewport& viewport, ToolPanelHost& panel,
    std::function<void(const QString&)> statusChanged)
    : viewport_(viewport), panel_(panel), statusChanged_(std::move(statusChanged)) {}

void InterferenceToolController::begin(std::vector<Body> bodies, bool selected) {
    active_ = true;
    selected_ = selected;
    checked_ = bodies.size();
    found_.clear();
    std::vector<TopoDS_Shape> shapes;
    for (const auto& body : bodies)
        shapes.push_back(body.shape);
    std::vector<TopoDS_Shape> outlines;
    try {
        for (const auto& overlap : kernel::findInterferences(shapes)) {
            found_.push_back(
                {bodies[overlap.first].name, bodies[overlap.second].name, overlap.volume});
            for (TopExp_Explorer edge(overlap.common, TopAbs_EDGE); edge.More(); edge.Next())
                outlines.push_back(edge.Current());
        }
    } catch (const std::exception&) {
        found_.clear();
        outlines.clear();
    }
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Überschneidungen"),
        {},
        {},
        {},
        {{"copy", QObject::tr("Werte kopieren"), QObject::tr("Überschneidungen")}},
        [](const std::string&, double) {},
        [](const std::string&, int) {},
        [this](const std::string& id) {
            if (id == "copy")
                QApplication::clipboard()->setText(report(true));
        },
        [this]() { finish(); },
        [this]() { cancel(); },
        {}});
    panel_.setAcceptEnabled(true);
    panel_.setActionEnabled("copy", !found_.empty());
    panel_.setGuidance(report());
    viewport_.setHighlightedEdges({}, outlines);
    statusChanged_(found_.empty() ? QObject::tr("Keine Überschneidungen")
                                  : QObject::tr("%1 Überschneidung(en)").arg(found_.size()));
}

QString InterferenceToolController::report(bool fullPrecision) const {
    const QString scope = selected_
                              ? QObject::tr("%1 gewählte Körper geprüft.").arg(checked_)
                              : QObject::tr("Alle %1 sichtbaren Körper geprüft.").arg(checked_);
    if (checked_ < 2)
        return QObject::tr("Zum Prüfen braucht es mindestens zwei Körper.");
    if (found_.empty())
        return scope + QLatin1Char('\n') + QObject::tr("Keine Überschneidungen.");
    QStringList lines{scope};
    for (const auto& overlap : found_)
        lines << QObject::tr("%1 ∩ %2: %3 mm³")
                     .arg(overlap.first, overlap.second, number(overlap.volume, fullPrecision));
    return lines.join(QLatin1Char('\n'));
}

void InterferenceToolController::cancel() {
    if (active_)
        finish();
}

void InterferenceToolController::finish() {
    active_ = false;
    found_.clear();
    viewport_.setHighlightedEdges({});
    panel_.end();
}

} // namespace voelund::desktop
