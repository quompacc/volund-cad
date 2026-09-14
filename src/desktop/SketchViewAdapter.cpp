#include "SketchViewAdapter.hpp"
#include <voelund/document/SketchPlaneValidation.hpp>

#include <voelund/application/CadSession.hpp>

#include <QObject>

namespace voelund::desktop {
namespace {

viewport::SketchPointKind pointKind(document::SketchPointPosition kind) {
    if (kind == document::SketchPointPosition::end)
        return viewport::SketchPointKind::end;
    if (kind == document::SketchPointPosition::center)
        return viewport::SketchPointKind::center;
    return viewport::SketchPointKind::start;
}

} // namespace

viewport::SceneSketchElement makeSceneSketchElement(const features::EvaluatedSketchElement& element,
                                                    bool conflicting) {
    std::vector<viewport::SceneSketchPoint> points;
    points.reserve(element.points.size());
    for (const auto& point : element.points) {
        points.push_back({element.id.value(), pointKind(point.kind), point.local.x, point.local.y,
                          point.world.X(), point.world.Y(), point.world.Z()});
        if (point.kind == document::SketchPointPosition::center)
            points.back().radialRadius = element.radialRadius;
    }
    if (element.points.size() == 2) {
        const auto& first = element.points[0];
        const auto& second = element.points[1];
        points.push_back(
            {element.id.value(), viewport::SketchPointKind::midpoint,
             (first.local.x + second.local.x) * 0.5, (first.local.y + second.local.y) * 0.5,
             (first.world.X() + second.world.X()) * 0.5, (first.world.Y() + second.world.Y()) * 0.5,
             (first.world.Z() + second.world.Z()) * 0.5});
    }
    return {element.id.value(), element.shape, conflicting, element.construction,
            std::move(points)};
}

std::vector<viewport::SceneSketchElement>
makeSceneSketchElements(const application::CadSession& session) {
    std::vector<viewport::SceneSketchElement> result;
    const auto selected = session.selectedFeature();
    if (!selected.has_value() ||
        session.selectedFeatureDescriptor().kind != document::FeatureKind::sketch) {
        return result;
    }
    try {
        document::validateCanonicalSketchPlane(session.selectedSketchParameters().plane);
    } catch (const std::invalid_argument&) {
        // The tree/status carries the migration diagnostic; no guessed overlay.
        return result;
    }
    const auto evaluated = session.selectedSketchElements();
    result.reserve(evaluated.size());
    for (const features::EvaluatedSketchElement& element : evaluated) {
        result.push_back(makeSceneSketchElement(element));
    }
    return result;
}

QString sketchSelectionSummary(const application::CadSession& session) {
    const auto& sketch = session.selectedSketchParameters();
    const auto selectedId = session.selectedSketchElement();
    QString summary = QObject::tr("Sketch — %1 elements").arg(sketch.elements.values.size());
    const auto profiles = session.selectedSketchProfiles();
    if (!profiles.empty()) {
        QString ids;
        for (document::SketchProfileId profile : profiles) {
            if (!ids.isEmpty())
                ids += QStringLiteral(", ");
            ids += QString::number(profile.value());
        }
        return summary + QObject::tr("\n\nSelected profiles: %1\nStable IDs: %2")
                             .arg(profiles.size())
                             .arg(ids);
    }
    if (!selectedId.has_value()) {
        return summary;
    }
    for (const document::SketchElement& element : sketch.elements.values) {
        if (element.id != *selectedId) {
            continue;
        }
        QString kind = QObject::tr("Circle");
        if (std::holds_alternative<document::SketchLine>(element.geometry)) {
            kind = QObject::tr("Line");
        } else if (std::holds_alternative<document::SketchRectangle>(element.geometry)) {
            kind = QObject::tr("Rectangle");
        } else if (std::holds_alternative<document::SketchArc>(element.geometry)) {
            kind = QObject::tr("Arc");
        } else if (std::holds_alternative<document::SketchEllipse>(element.geometry)) {
            kind = QObject::tr("Ellipse");
        }
        return summary + QObject::tr("\n\nSelected: %1 #%2").arg(kind).arg(element.id.value());
    }
    return summary;
}

} // namespace voelund::desktop
