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

} // namespace

MeasureToolController::MeasureToolController(application::CadSession& session,
                                             viewport::CadViewport& viewport, ToolPanelHost& panel,
                                             std::function<void(const QString&)> statusChanged)
    : session_(session), viewport_(viewport), panel_(panel),
      statusChanged_(std::move(statusChanged)) {}

void MeasureToolController::begin() {
    active_ = true;
    picks_.clear();
    panel_.begin(
        ToolPanelDefinition{QObject::tr("Messen"),
                            {},
                            {},
                            {},
                            {{"copy", QObject::tr("Werte kopieren"), QObject::tr("Messen")},
                             {"reset", QObject::tr("Neu beginnen"), QObject::tr("Messen")}},
                            [](const std::string&, double) {},
                            [](const std::string&, int) {},
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
    show();
    statusChanged_(QObject::tr("Messen: Kante oder ebene Fläche anklicken"));
}

void MeasureToolController::pickedEdge(std::uint64_t body, const TopoDS_Shape& edge) {
    picked({body, edge});
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

QString MeasureToolController::report(bool fullPrecision) const {
    if (!session_.sceneStatus().isCurrent())
        return QObject::tr("Das Modell ist nicht aktuell; Messen ist gesperrt, bis es neu "
                           "berechnet ist.");
    if (picks_.empty())
        return QObject::tr("Klicke eine Kante oder ebene Fläche an; ein zweiter Klick misst "
                           "Abstand und Winkel.");
    const auto mm = [&](double value) { return number(value, fullPrecision) + " mm"; };
    QStringList lines;
    for (std::size_t index = 0; index < picks_.size(); ++index) {
        const auto& pick = picks_[index];
        const auto measure = kernel::measureShape(pick.shape);
        const QString which = QObject::tr("%1. %2").arg(index + 1).arg(
            pick.shape.ShapeType() == TopAbs_EDGE ? QObject::tr("Kante") : QObject::tr("Fläche"));
        if (measure.length)
            lines << QObject::tr("%1: Länge %2").arg(which, mm(*measure.length));
        if (measure.area)
            lines << QObject::tr("%1: Fläche %2 mm²")
                         .arg(which, number(*measure.area, fullPrecision));
        if (measure.radius)
            lines << QObject::tr("%1: Radius %2").arg(which, mm(*measure.radius));
        if (const auto shape = session_.featureShape(document::FeatureId{pick.body}))
            if (const auto volume = kernel::measureShape(*shape).volume)
                lines << QObject::tr("%1: Körpervolumen %2 mm³")
                             .arg(which, number(*volume, fullPrecision));
    }
    if (picks_.size() == 2) {
        try {
            const auto pair = kernel::measureBetween(picks_[0].shape, picks_[1].shape);
            lines << QObject::tr("Abstand %1").arg(mm(pair.distance));
            if (pair.angle)
                lines << QObject::tr("Winkel %1°")
                             .arg(fullPrecision
                                      ? QLocale().toString(*pair.angle * 180 / std::numbers::pi,
                                                           'g', 15)
                                      : QLocale().toString(*pair.angle * 180 / std::numbers::pi,
                                                           'f', 2));
        } catch (const std::exception&) {
            lines << QObject::tr("Abstand und Winkel lassen sich hier nicht messen.");
        }
    }
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
    panel_.end();
    statusChanged_(QObject::tr("Messen beendet"));
}

} // namespace voelund::desktop
