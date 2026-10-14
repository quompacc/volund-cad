#include "SketchViewAdapter.hpp"
#include <voelund/document/SketchPlaneValidation.hpp>

#include <voelund/application/CadSession.hpp>

#include <voelund/features/SketchConstraintSolver.hpp>

#include <QObject>

#include <algorithm>
#include <cmath>
#include <gp_Vec.hxx>

namespace voelund::desktop {
void setSupportProjection(viewport::PlanarFaceSelection& support,
                          const application::SketchSupportProjection& projection) {
    support.referenceSegments.clear();
    support.referencePoints.clear();
    support.stableReferences = true;
    const auto& p = projection.plane;
    support.originX = p.originX;
    support.originY = p.originY;
    support.originZ = p.originZ;
    support.normalX = p.normalX;
    support.normalY = p.normalY;
    support.normalZ = p.normalZ;
    support.xDirectionX = p.xDirectionX;
    support.xDirectionY = p.xDirectionY;
    support.xDirectionZ = p.xDirectionZ;
    const gp_Pnt origin(p.originX, p.originY, p.originZ);
    const gp_Vec x(p.xDirectionX, p.xDirectionY, p.xDirectionZ);
    const auto y = gp_Vec(p.normalX, p.normalY, p.normalZ).Crossed(x);
    for (const auto& segment : projection.segments) {
        const gp_Vec a(origin, segment.first), b(origin, segment.second);
        support.referenceSegments.push_back(
            {a.Dot(x), a.Dot(y), b.Dot(x), b.Dot(y), segment.first.X(), segment.first.Y(),
             segment.first.Z(), segment.second.X(), segment.second.Y(), segment.second.Z()});
    }
    for (const auto& point : projection.points)
        support.referencePoints.push_back({0, viewport::SketchPointKind::projected, point.local.x,
                                           point.local.y, point.world.X(), point.world.Y(),
                                           point.world.Z(), 0, 0, 0, point.reference});
}
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
                                                    bool conflicting, bool determined) {
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
            std::move(points), determined};
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
    // The determination of the stored sketch, from a solve of a copy.
    features::SketchSolveResult solved;
    try {
        auto copy = session.selectedSketchParameters();
        solved = features::solveSketchConstraints(copy);
    } catch (const std::exception&) {
    }
    const auto evaluated = session.selectedSketchElements();
    result.reserve(evaluated.size());
    for (const features::EvaluatedSketchElement& element : evaluated) {
        const bool determined =
            solved.status == features::SketchSolveStatus::success &&
            std::ranges::find(solved.freeElements, element.id) == solved.freeElements.end();
        result.push_back(makeSceneSketchElement(element, false, determined));
    }
    return result;
}

QString sketchSelectionSummary(const application::CadSession& session) {
    const auto& sketch = session.selectedSketchParameters();
    const auto selectedId = session.selectedSketchElement();
    QString summary = QObject::tr("Skizze — %1 Elemente").arg(sketch.elements.values.size());
    const auto profiles = session.selectedSketchProfiles();
    if (!profiles.empty()) {
        QString ids;
        for (const document::SketchRegionId& profile : profiles) {
            if (!ids.isEmpty())
                ids += QStringLiteral("; ");
            ids += QString::fromStdString(profile.text());
        }
        return summary + QObject::tr("\n\nGewählte Bereiche: %1\nStabile IDs: %2")
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
        QString kind = QObject::tr("Kreis");
        if (std::holds_alternative<document::SketchLine>(element.geometry)) {
            kind = QObject::tr("Linie");
        } else if (std::holds_alternative<document::SketchRectangle>(element.geometry)) {
            kind = QObject::tr("Rechteck");
        } else if (std::holds_alternative<document::SketchArc>(element.geometry)) {
            kind = QObject::tr("Bogen");
        } else if (std::holds_alternative<document::SketchEllipse>(element.geometry)) {
            kind = QObject::tr("Ellipse");
        } else if (std::holds_alternative<document::SketchPointGeometry>(element.geometry)) {
            kind = QObject::tr("Punkt");
        } else if (std::holds_alternative<document::SketchBSpline>(element.geometry)) {
            kind = QObject::tr("B-Spline");
        }
        return summary + QObject::tr("\n\nGewählt: %1 #%2").arg(kind).arg(element.id.value());
    }
    return summary;
}

std::optional<SketchPlaneCrossing> sketchPlaneCrossing(const application::CadSession& session,
                                                       document::FeatureId sketch,
                                                       const viewport::ViewRay& ray) {
    const auto& plane = session.sketchParameters(sketch).plane;
    const double along = plane.normalX * ray.direction[0] + plane.normalY * ray.direction[1] +
                         plane.normalZ * ray.direction[2];
    if (std::abs(along) < 1e-9)
        return std::nullopt; // Looking along the plane.
    const double distance = (plane.normalX * (plane.originX - ray.origin[0]) +
                             plane.normalY * (plane.originY - ray.origin[1]) +
                             plane.normalZ * (plane.originZ - ray.origin[2])) /
                            along;
    return SketchPlaneCrossing{distance,
                               {ray.origin[0] + distance * ray.direction[0],
                                ray.origin[1] + distance * ray.direction[1],
                                ray.origin[2] + distance * ray.direction[2]}};
}

bool pickSketchArea(application::CadSession& session, const viewport::ViewRay& ray,
                    const std::set<std::uint64_t>& hiddenBodies) {
    const auto outline = session.modelOutline();
    const auto hidden = [&](document::FeatureId feature) {
        for (const auto& body : outline.bodies)
            if (std::ranges::find(body.features, feature) != body.features.end())
                return hiddenBodies.contains(body.id.value());
        return false;
    };
    std::vector<std::pair<SketchPlaneCrossing, document::FeatureId>> crossings;
    for (const auto& feature : session.features())
        if (feature.kind == document::FeatureKind::sketch && !hidden(feature.id))
            if (const auto crossing = sketchPlaneCrossing(session, feature.id, ray))
                crossings.emplace_back(*crossing, feature.id);
    std::ranges::sort(crossings, {}, [](const auto& item) { return item.first.distance; });
    // S14: a sketch on the clicked face lies at its distance; one behind the face is hidden.
    const double reach = ray.nearestSurface + 1.0e-6 * (1.0 + std::abs(ray.nearestSurface));
    for (const auto& [crossing, sketch] : crossings) {
        if (crossing.distance > reach)
            break;
        if (session.selectSketchProfileAt(sketch, crossing.point, ray.additive))
            return true;
    }
    return false;
}

} // namespace voelund::desktop
