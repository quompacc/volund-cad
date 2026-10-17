#include <voelund/kernel/PlaneNormal.hpp>
#include <BRepAdaptor_Surface.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Pln.hxx>
#include <gp_Vec.hxx>
#include <tuple>
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchReferenceReview.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

namespace voelund::features {
namespace {
bool onSupport(const TopoDS_Shape& face, const EvaluatedFeature& support) {
    for (TopExp_Explorer it(support.shape, TopAbs_FACE); it.More(); it.Next())
        if (face.IsSame(it.Current()))
            return true;
    return false;
}
bool before(const SketchSupportCandidate& a, const SketchSupportCandidate& b) {
    const auto& x = a.binding.reference.origin;
    const auto& y = b.binding.reference.origin;
    const auto key = [](const auto& origin) {
        return std::pair{origin.feature.value(), origin.role ? static_cast<int>(*origin.role) : -1};
    };
    if (key(x) != key(y))
        return key(x) < key(y);
    return std::lexicographical_compare(
        x.extrusionBoundary.begin(), x.extrusionBoundary.end(), y.extrusionBoundary.begin(),
        y.extrusionBoundary.end(),
        [](auto first, auto second) { return first.value() < second.value(); });
}
} // namespace
SketchReferenceReview reviewSketchReferences(document::DocumentId document,
                                             const document::SketchParameters& original,
                                             const EvaluatedFeature* support) {
    SketchReferenceReview result{
        SketchReferenceReviewStatus::legacyNeedsExplicitChoice, original, {}, {}, 0,
        "Legacy plane requires explicit support choice"};
    for (const auto& constraint : original.constraints.values)
        if ((constraint.kind == document::SketchConstraintKind::coordinateX ||
             constraint.kind == document::SketchConstraintKind::coordinateY) &&
            constraint.auxiliaryValues.size() == 2)
            result.legacyEdgeConstraints.push_back(constraint.id);
    bool canonical = true;
    try {
        document::validateCanonicalSketchPlane(original.plane);
    } catch (const std::invalid_argument&) {
        canonical = false;
    }
    if (!support || !support->bodyId || support->shape.IsNull()) {
        result.status = SketchReferenceReviewStatus::supportUnavailable;
        result.diagnostic = "Current support output is unavailable";
        return result;
    }
    if (original.supportBinding) {
        result.status = SketchReferenceReviewStatus::brokenBinding;
        result.diagnostic = "Stored support or edge binding requires repair";
        if (original.supportBinding->reference.document == document) {
            auto resolved = original;
            try {
                resolveBoundSketchSupport(resolved, *support);
                result.status = SketchReferenceReviewStatus::current;
                result.diagnostic = "Stored references resolve without substitution";
            } catch (const GeometryEvaluationError&) {
            } catch (const std::invalid_argument&) {
            }
        }
    } else if (!canonical) {
        result.status = SketchReferenceReviewStatus::invalidLegacyPlane;
        result.diagnostic = "Legacy plane is noncanonical; original values retained";
    }
    for (const auto& origin : support->supportOrigins) {
        const auto aliases = std::ranges::count_if(support->supportOrigins, [&](const auto& other) {
            if (other.origin == origin.origin)
                return true;
            // T05: second names of a merged face are no aliases.
            return origin.faces.size() == 1 && !other.secondary &&
                   std::ranges::any_of(other.faces, [&](const auto& face) {
                       return face.IsSame(origin.faces.front());
                   });
        });
        if (origin.coverage != kernel::HistoryCoverage::complete || origin.faces.size() != 1 ||
            aliases != 1 || !onSupport(origin.faces.front(), *support)) {
            ++result.unresolvedOrigins;
            continue;
        }
        const BRepAdaptor_Surface surface(TopoDS::Face(origin.faces.front()));
        if (surface.GetType() != GeomAbs_Plane)
            continue;
        const auto p = surface.Plane();
        const gp_Vec normal(kernel::planeNormal(p, origin.faces.front().Orientation()));
        auto position = p.Location();
        gp_Vec x(p.XAxis().Direction());
        bool matches = false;
        if (canonical) {
            const auto& old = original.plane;
            const gp_Pnt oldPosition(old.originX, old.originY, old.originZ);
            const double distance = gp_Vec(position, oldPosition).Dot(normal);
            const double scale =
                std::max({std::abs(old.originX), std::abs(old.originY), std::abs(old.originZ),
                          std::abs(position.X()), std::abs(position.Y()), std::abs(position.Z())});
            matches = std::abs(distance) <= kernel::connectionTolerance(scale).millimetres &&
                      gp_Vec(old.normalX, old.normalY, old.normalZ).Angle(normal) <=
                          kernel::angularTolerance.radians;
            if (matches) {
                position = oldPosition.Translated(normal * (-distance));
                x = gp_Vec(old.xDirectionX, old.xDirectionY, old.xDirectionZ);
            }
        }
        const document::SketchPlane plane{position.X(), position.Y(), position.Z(),
                                          normal.X(),   normal.Y(),   normal.Z(),
                                          x.X(),        x.Y(),        x.Z()};
        try {
            auto binding =
                bindSketchSupport(document, *support, origin.origin, origin.faces.front(), plane);
            result.candidates.push_back({std::move(binding), plane, matches});
        } catch (const GeometryEvaluationError&) {
            ++result.unresolvedOrigins;
        }
    }
    std::ranges::sort(result.candidates, before);
    return result;
}
} // namespace voelund::features
