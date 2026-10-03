#include "SketchContourGeometry.hpp"
#include "SketchIntersectionCurves.hpp"
#include <Geom2dAPI_InterCurveCurve.hxx>
#include <Standard_Failure.hxx>
#include <algorithm>
#include <cmath>
#include <gp_Vec2d.hxx>
#include <limits>
#include <numbers>
#include <tuple>
#include <voelund/features/SketchIntersections.hpp>
#include <voelund/kernel/NumericPolicy.hpp>
namespace voelund::features {
namespace {
document::SketchPoint point(const detail::PlanarCurve& c, double u) {
    const auto p = c.curve->Value(u);
    return {p.X(), p.Y()};
}
bool endpoint(const detail::PlanarCurve& c, double u) {
    return !c.periodic && (u == c.first || u == c.last);
}
} // namespace
SketchIntersectionResult
splitSketchIntersections(std::span<const document::SketchElement> elements) {
    SketchIntersectionResult result;
    result.pieces = detail::contourPieces(elements);
    std::ranges::sort(result.pieces, [](const auto& a, const auto& b) {
        return std::pair{a.element.value(), a.part} < std::pair{b.element.value(), b.part};
    });
    std::vector<detail::PlanarCurve> curves;
    std::vector<std::vector<double>> cuts;
    auto diagnostic = [&](SketchIntersectionIssue kind, std::size_t i, std::size_t j) {
        std::vector<document::SketchElementId> ids{result.pieces[i].element,
                                                   result.pieces[j].element};
        std::ranges::sort(ids, {}, &document::SketchElementId::value);
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        result.diagnostics.push_back({kind, std::move(ids)});
    };
    for (std::size_t i = 0; i < result.pieces.size(); ++i) {
        try {
            curves.push_back(detail::planarCurve(result.pieces[i]));
            cuts.push_back({curves.back().first, curves.back().last});
        } catch (const Standard_Failure&) {
            diagnostic(SketchIntersectionIssue::calculationFailed, i, i);
            return result;
        }
    }
    for (std::size_t i = 0; i < curves.size(); ++i)
        for (std::size_t j = i + 1; j < curves.size(); ++j) {
            if (detail::contourOverlap(result.pieces[i], result.pieces[j])) {
                diagnostic(SketchIntersectionIssue::overlap, i, j);
                continue;
            }
            const double eps =
                kernel::connectionTolerance(std::max(curves[i].scale, curves[j].scale)).millimetres;
            // On the same exact supporting circle, disjoint arcs can only meet
            // at their ends. OCCT may report tolerance-sized "segments" there.
            // Preserve those exact source endpoints instead of inventing a split.
            const auto* arcA = std::get_if<document::SketchArc>(&result.pieces[i].geometry);
            const auto* arcB = std::get_if<document::SketchArc>(&result.pieces[j].geometry);
            if (arcA && arcB && arcA->center == arcB->center && arcA->radius == arcB->radius) {
                const double period = 2 * std::numbers::pi;
                auto normalized = [&](double u) {
                    u = std::fmod(u, period);
                    return u < 0 ? u + period : u;
                };
                const double first = normalized(arcA->startAngle),
                             second = normalized(arcB->startAngle);
                bool overlap = false;
                for (int shift = -1; shift <= 1; ++shift) {
                    const double start = second + shift * period;
                    overlap |= std::min(first + arcA->endAngle - arcA->startAngle,
                                        start + arcB->endAngle - arcB->startAngle) -
                                   std::max(first, start) >
                               64 * std::numeric_limits<double>::epsilon() * period;
                }
                if (overlap) {
                    diagnostic(SketchIntersectionIssue::overlap, i, j);
                    continue;
                }
                for (double a : {curves[i].first, curves[i].last})
                    for (double b : {curves[j].first, curves[j].last}) {
                        const auto pa = curves[i].curve->Value(a), pb = curves[j].curve->Value(b);
                        if (pa.Distance(pb) <= eps)
                            result.intersections.push_back(
                                {i, j, a, b, {pa.X(), pa.Y()}, SketchIntersectionKind::tangent});
                    }
                continue;
            }
            try {
                Geom2dAPI_InterCurveCurve intersection(curves[i].curve, curves[j].curve, eps);
                if (!intersection.Intersector().IsDone()) {
                    diagnostic(SketchIntersectionIssue::calculationFailed, i, j);
                    continue;
                }
                if (intersection.NbSegments()) {
                    // Near-coincident intervals have no unique split topology at this tolerance.
                    diagnostic(SketchIntersectionIssue::unresolvedInterval, i, j);
                    continue;
                }
                for (int k = 1; k <= intersection.NbPoints(); ++k) {
                    const auto& hit = intersection.Intersector().Point(k);
                    const double a = detail::domainParameter(curves[i], hit.ParamOnFirst(), eps);
                    const double b = detail::domainParameter(curves[j], hit.ParamOnSecond(), eps);
                    gp_Pnt2d pa, pb;
                    gp_Vec2d da, db;
                    curves[i].curve->D1(a, pa, da);
                    curves[j].curve->D1(b, pb, db);
                    if (pa.Distance(pb) > eps) {
                        diagnostic(SketchIntersectionIssue::calculationFailed, i, j);
                        continue;
                    }
                    auto kind = SketchIntersectionKind::crossing;
                    if (std::abs(da.Crossed(db)) / (da.Magnitude() * db.Magnitude()) <=
                        kernel::angularTolerance.radians)
                        kind = SketchIntersectionKind::tangent;
                    else if (endpoint(curves[i], a) || endpoint(curves[j], b))
                        kind = SketchIntersectionKind::endpoint;
                    const bool duplicate =
                        std::ranges::any_of(result.intersections, [&](const auto& h) {
                            return h.firstPiece == i && h.secondPiece == j &&
                                   std::abs(h.firstParameter - a) <=
                                       detail::parameterTolerance(curves[i], a, eps) &&
                                   std::abs(h.secondParameter - b) <=
                                       detail::parameterTolerance(curves[j], b, eps);
                        });
                    if (duplicate)
                        continue;
                    result.intersections.push_back({i, j, a, b, {pa.X(), pa.Y()}, kind});
                    cuts[i].push_back(a);
                    cuts[j].push_back(b);
                }
            } catch (const Standard_Failure&) {
                diagnostic(SketchIntersectionIssue::calculationFailed, i, j);
            } catch (const std::runtime_error&) {
                diagnostic(SketchIntersectionIssue::calculationFailed, i, j);
            }
        }
    if (!result.valid())
        return result;
    for (std::size_t i = 0; i < curves.size(); ++i) {
        auto& values = cuts[i];
        std::ranges::sort(values);
        const double eps = kernel::connectionTolerance(curves[i].scale).millimetres;
        std::vector<double> unique;
        for (auto u : values) {
            if (unique.empty() || u - unique.back() > detail::parameterTolerance(curves[i], u, eps))
                unique.push_back(u);
            else if (u - unique.back() >
                     64 * std::numeric_limits<double>::epsilon() *
                         std::max({1.0, std::abs(u), std::abs(unique.back())})) {
                // Do not choose an arbitrary member of a near-coincident split cluster.
                diagnostic(SketchIntersectionIssue::unresolvedInterval, i, i);
            }
        }
        for (std::size_t k = 1; k < unique.size(); ++k)
            result.intervals.push_back({i, unique[k - 1], unique[k],
                                        point(curves[i], unique[k - 1]),
                                        point(curves[i], unique[k])});
    }
    if (!result.valid())
        result.intervals.clear();
    std::ranges::sort(result.intersections, [](const auto& a, const auto& b) {
        return std::tuple{a.firstPiece, a.secondPiece, a.firstParameter, a.secondParameter} <
               std::tuple{b.firstPiece, b.secondPiece, b.firstParameter, b.secondParameter};
    });
    return result;
}
} // namespace voelund::features
