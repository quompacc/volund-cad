// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "MeasureToolController.hpp"

#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/kernel/Measurement.hpp>

#include <QApplication>
#include <QClipboard>
#include <QLocale>
#include <QObject>
#include <QStringList>

#include <cmath>
#include <numbers>
#include <utility>

namespace voelund::desktop {
namespace {

// Full precision: the 15 significant digits a double carries reliably (17 show its noise).
QString number(double value, bool fullPrecision) {
    return fullPrecision ? QLocale().toString(value, 'g', 15) : QLocale().toString(value, 'f', 3);
}

QString kindOf(const TopoDS_Shape& shape) {
    switch (shape.ShapeType()) {
    case TopAbs_EDGE:
        return QObject::tr("Kante");
    case TopAbs_VERTEX:
        return QObject::tr("Punkt");
    default:
        return QObject::tr("Fläche");
    }
}

QString triple(const std::array<double, 3>& values, bool fullPrecision) {
    return QObject::tr("X %1; Y %2; Z %3 mm")
        .arg(number(values[0], fullPrecision), number(values[1], fullPrecision),
             number(values[2], fullPrecision));
}

} // namespace

MeasureToolController::MeasureToolController(application::CadSession& session,
                                             viewport::CadViewport& viewport, ToolPanelHost& panel,
                                             std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel),
      statusChanged_(std::move(statusChanged)) {}

void MeasureToolController::begin() {
    active_ = true;
    picks_.clear();
    edgeAs_ = 0;
    panel_.begin(ToolPanelDefinition{
        QObject::tr("Messen"),
        {},
        {},
        // N08: an edge click measures the edge, its middle or the centre of its circle.
        {{"edgeAs",
          QObject::tr("Kanten messen als"),
          {QObject::tr("Kante"), QObject::tr("Mittelpunkt"), QObject::tr("Kreiszentrum")},
          0}},
        {{"copy", QObject::tr("Werte kopieren"), QObject::tr("Messen")},
         {"reset", QObject::tr("Neu beginnen"), QObject::tr("Messen")}},
        [](const std::string&, double) {},
        [this](const std::string&, int index) { edgeAs_ = index; },
        [this](const std::string& id) {
            if (id == "copy")
                QApplication::clipboard()->setText(report(true));
            if (id == "reset") {
                picks_.clear();
                show();
            }
        },
        [this]() { finish(); },
        [this]() { cancel(); },
        {}});
    panel_.setAcceptEnabled(true);
    viewport_.setVertexPicking(true); // N08: corners
    show();
    statusChanged_(QObject::tr("Messen: Kante, Eckpunkt oder ebene Fläche anklicken"));
}

void MeasureToolController::pickedEdge(std::uint64_t body, const TopoDS_Shape& edge) {
    if (edge.IsNull() || edge.ShapeType() != TopAbs_EDGE || edgeAs_ == 0)
        return picked({body, edge});
    const auto point = kernel::edgePoint(edge, edgeAs_ == 1 ? kernel::EdgePoint::middle
                                                            : kernel::EdgePoint::center);
    if (point.IsNull()) {
        statusChanged_(QObject::tr("Diese Kante ist kein Kreis; sie hat kein Zentrum."));
        return;
    }
    picked({body, point});
}

void MeasureToolController::pickedFace(const viewport::PlanarFaceSelection& face) {
    picked({face.bodyId, face.selectedFace});
}

// A third pick starts a new measurement.
void MeasureToolController::picked(Pick pick) {
    if (!active_ || pick.shape.IsNull())
        return;
    if (picks_.size() == 2)
        picks_.clear();
    picks_.push_back(std::move(pick));
    show();
}

// The values of one pick: length, area, radius or position, and the volume of its body.
QStringList MeasureToolController::describe(std::size_t index, bool fullPrecision) const {
    const auto& pick = picks_[index];
    const auto measure = kernel::measureShape(pick.shape);
    const QString which = QObject::tr("%1. %2").arg(index + 1).arg(kindOf(pick.shape));
    QStringList lines;
    if (measure.length)
        lines << QObject::tr("%1: Länge %2 mm").arg(which, number(*measure.length, fullPrecision));
    if (measure.area)
        lines << QObject::tr("%1: Fläche %2 mm²").arg(which, number(*measure.area, fullPrecision));
    if (measure.radius)
        lines << QObject::tr("%1: Radius %2 mm").arg(which, number(*measure.radius, fullPrecision));
    if (measure.point)
        lines << QObject::tr("%1: %2").arg(which, triple(*measure.point, fullPrecision));
    if (const auto shape = session_.featureShape(document::FeatureId{pick.body}))
        if (const auto volume = kernel::measureShape(*shape).volume)
            lines << QObject::tr("%1: Körpervolumen %2 mm³")
                         .arg(which, number(*volume, fullPrecision));
    return lines;
}

// Distance and angle between the two picks; for two points also the difference per axis.
QStringList MeasureToolController::between(bool fullPrecision) const {
    QStringList lines;
    try {
        const auto pair = kernel::measureBetween(picks_[0].shape, picks_[1].shape);
        lines << QObject::tr("Abstand %1 mm").arg(number(pair.distance, fullPrecision));
        if (pair.delta)
            lines << QObject::tr("Δ %1").arg(triple(*pair.delta, fullPrecision));
        if (pair.angle) {
            const double degrees = *pair.angle * 180 / std::numbers::pi;
            lines << QObject::tr("Winkel %1°")
                         .arg(fullPrecision ? QLocale().toString(degrees, 'g', 15)
                                            : QLocale().toString(degrees, 'f', 2));
        }
    } catch (const std::exception&) {
        lines << QObject::tr("Abstand und Winkel lassen sich hier nicht messen.");
    }
    return lines;
}

QString MeasureToolController::report(bool fullPrecision) const {
    if (!session_.sceneStatus().isCurrent())
        return QObject::tr("Das Modell ist nicht aktuell; Messen ist gesperrt, bis es neu "
                           "berechnet ist.");
    if (picks_.empty())
        return QObject::tr("Klicke eine Kante, einen Eckpunkt oder eine ebene Fläche an; ein "
                           "zweiter Klick misst Abstand und Winkel.");
    QStringList lines;
    for (std::size_t index = 0; index < picks_.size(); ++index)
        lines << describe(index, fullPrecision);
    if (picks_.size() == 2)
        lines << between(fullPrecision);
    return lines.join(QLatin1Char('\n'));
}

void MeasureToolController::show() {
    if (!active_)
        return;
    // Stale geometry is never shown as a current measure.
    if (!session_.sceneStatus().isCurrent())
        picks_.clear();
    std::vector<TopoDS_Shape> edges;
    for (const auto& pick : picks_)
        if (pick.shape.ShapeType() == TopAbs_EDGE)
            edges.push_back(pick.shape);
    viewport_.setHighlightedEdges(edges);
    panel_.setGuidance(report());
    panel_.setActionEnabled("copy", !picks_.empty());
}

void MeasureToolController::cancel() {
    if (!active_)
        return;
    finish();
}

void MeasureToolController::finish() {
    active_ = false;
    picks_.clear();
    viewport_.setHighlightedEdges({});
    viewport_.setVertexPicking(false);
    panel_.end();
    statusChanged_(QObject::tr("Messen beendet"));
}

} // namespace voelund::desktop
