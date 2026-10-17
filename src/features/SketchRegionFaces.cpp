#include "SketchPlaneGuard.hpp"
#include "SketchProfileFace.hpp"
#include "SketchRegionIntervalFaces.hpp"
#include <voelund/features/GeometryEvaluationError.hpp>
#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchProfiles.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepTools.hxx>
#include <GProp_GProps.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <string>

namespace voelund::features {
namespace {

std::string reason(SketchRegionReferenceStatus status) {
    switch (status) {
    case SketchRegionReferenceStatus::elementsRemoved:
        return "a boundary element was removed";
    case SketchRegionReferenceStatus::topologyChanged:
        return "the boundary was split, merged or cut";
    case SketchRegionReferenceStatus::ambiguous:
        return "several cells share its boundary";
    case SketchRegionReferenceStatus::invalidSketch:
        return "the sketch regions are invalid";
    case SketchRegionReferenceStatus::resolved:
        break;
    }
    return "unknown";
}

// R10: failures name the elements a user has to look at.
std::string elementList(const std::set<std::uint64_t>& elements) {
    std::string text = " [elements";
    for (const std::uint64_t element : elements)
        text += " " + std::to_string(element);
    return text + "]";
}

std::set<std::uint64_t> failingElements(const SketchRegionIdentities& identities,
                                        std::span<const document::SketchElement> elements,
                                        const document::SketchRegionId& id,
                                        SketchRegionReferenceStatus status) {
    std::set<std::uint64_t> result;
    if (status == SketchRegionReferenceStatus::invalidSketch) {
        for (const SketchRegionDiagnostic& diagnostic : identities.regions.diagnostics)
            for (document::SketchElementId element : diagnostic.elements)
                result.insert(element.value());
        return result;
    }
    for (document::SketchElementId element : id.elements()) {
        const bool material = std::ranges::any_of(elements, [&](const auto& candidate) {
            return candidate.id == element && !candidate.construction;
        });
        if (status != SketchRegionReferenceStatus::elementsRemoved || !material)
            result.insert(element.value());
    }
    return result;
}

std::set<std::uint64_t> boundaryElements(const SketchRegionResult& regions, std::size_t boundary) {
    std::set<std::uint64_t> elements;
    for (const RegionHalfEdge& edge : regions.boundaries[boundary].edges)
        elements.insert(
            regions.split.pieces[regions.split.intervals[edge.interval].piece].element.value());
    return elements;
}

double area(const TopoDS_Face& face) {
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    return properties.Mass();
}

// Adds the hole loop as an inner wire. The orientation is chosen geometrically: only a
// valid face whose area is exactly outer minus hole is accepted.
TopoDS_Face withHole(const TopoDS_Face& outer, const TopoDS_Face& hole) {
    const double expected = area(outer) - area(hole);
    const TopoDS_Wire wire = BRepTools::OuterWire(hole);
    for (const bool reversed : {true, false}) {
        BRepBuilderAPI_MakeFace face(outer);
        face.Add(reversed ? TopoDS::Wire(wire.Reversed()) : wire);
        if (!face.IsDone())
            continue;
        // MakeFace rebuilds a FORWARD face; keep the loop face's orientation for the prism.
        TopoDS_Face result = face.Face();
        result.Orientation(outer.Orientation());
        if (BRepCheck_Analyzer(result).IsValid() &&
            std::abs(area(result) - expected) <= 1e-9 * std::max(1.0, area(outer)))
            return result;
    }
    throw GeometryEvaluationError("Region did not produce a valid face", "profile-invalid");
}

// The legacy loop face whose loop carries exactly these elements; the bridge's only source.
// L06: built from the loops evaluateRegionFaces detected once, not by detecting them again
// for every region (quadratic in the number of regions).
EvaluatedProfileFace loopFace(const document::SketchParameters& sketch,
                              std::span<const SketchProfile> loops,
                              const std::set<std::uint64_t>& elements) {
    for (const SketchProfile& loop : loops) {
        std::set<std::uint64_t> loopElements;
        for (document::SketchElementId element : loop.elementIds)
            loopElements.insert(element.value());
        if (loopElements == elements)
            return detail::profileFaceOf(sketch, loop);
    }
    throw GeometryEvaluationError("Region boundary is no closed loop" + elementList(elements),
                                  "region-open", {elements.begin(), elements.end()});
}

} // namespace

std::vector<EvaluatedProfileFace>
evaluateRegionFaces(const document::SketchParameters& sketch,
                    std::span<const document::SketchRegionId> regions) {
    if (!sketch.elements.values.empty())
        document::validateSketchElements(sketch.elements);
    const auto identities = identifySketchRegions(sketch.elements.values);
    const auto loops = detectClosedProfiles(sketch.elements.values);
    const auto& cells = identities.regions.regions;
    std::vector<std::size_t> selected;
    for (const document::SketchRegionId& id : regions) {
        const auto resolved = resolveSketchRegion(identities, sketch.elements.values, id);
        if (resolved.status != SketchRegionReferenceStatus::resolved) {
            const auto failing =
                failingElements(identities, sketch.elements.values, id, resolved.status);
            throw GeometryEvaluationError(
                "Extrusion region needs repair: " + reason(resolved.status) + elementList(failing),
                "region-needs-repair", {failing.begin(), failing.end()});
        }
        selected.push_back(*resolved.region);
    }
    // R10b: cells of crossing or branching contours are no whole closed loops; build them
    // directly from their intervals. Whole loops keep the R10 path and its references.
    const auto loopBacked = [&](std::size_t boundary) {
        const auto elements = boundaryElements(identities.regions, boundary);
        return std::ranges::any_of(loops, [&](const SketchProfile& loop) {
            std::set<std::uint64_t> loopElements;
            for (document::SketchElementId element : loop.elementIds)
                loopElements.insert(element.value());
            return loopElements == elements;
        });
    };
    const bool wholeLoops = detail::wholeLoopCells(identities.regions, selected) &&
                            std::ranges::all_of(selected, [&](std::size_t cell) {
                                return loopBacked(cells[cell].outer) &&
                                       std::ranges::all_of(cells[cell].holes, loopBacked);
                            });
    if (!wholeLoops) {
        detail::requireCanonicalPlane(sketch.plane);
        return detail::intervalRegionFaces(sketch, identities.regions, selected);
    }
    detail::requireCanonicalPlane(sketch.plane); // once for all loop faces below
    const auto innerRegion = [&](std::size_t hole) -> std::optional<std::size_t> {
        const auto found = std::ranges::find(cells, hole, &SketchRegion::outer);
        if (found == cells.end())
            return std::nullopt;
        return static_cast<std::size_t>(found - cells.begin());
    };
    const auto isSelected = [&](std::optional<std::size_t> region) {
        return region && std::ranges::find(selected, *region) != selected.end();
    };
    // A selected region inside a selected hole fills that hole instead of becoming a
    // second solid that only touches the first one along a separately built surface.
    std::set<std::size_t> absorbed;
    for (std::size_t region : selected)
        for (std::size_t hole : cells[region].holes)
            if (isSelected(innerRegion(hole)))
                absorbed.insert(*innerRegion(hole));
    std::vector<EvaluatedProfileFace> faces;
    for (std::size_t index : selected) {
        if (absorbed.contains(index))
            continue;
        const SketchRegion& region = cells[index];
        auto result = loopFace(sketch, loops, boundaryElements(identities.regions, region.outer));
        std::vector<std::size_t> holes(region.holes.begin(), region.holes.end());
        while (!holes.empty()) {
            const std::size_t hole = holes.back();
            holes.pop_back();
            if (const auto inner = innerRegion(hole); isSelected(inner)) {
                holes.insert(holes.end(), cells[*inner].holes.begin(), cells[*inner].holes.end());
                continue;
            }
            auto inner = loopFace(sketch, loops, boundaryElements(identities.regions, hole));
            result.face = withHole(result.face, inner.face);
            result.boundary.insert(result.boundary.end(), inner.boundary.begin(),
                                   inner.boundary.end());
            result.edges.insert(result.edges.end(), inner.edges.begin(), inner.edges.end());
        }
        std::ranges::sort(result.boundary, {}, &document::SketchElementId::value);
        faces.push_back(std::move(result));
    }
    return faces;
}

std::vector<EvaluatedProfileFace>
evaluateExtrusionFaces(const document::SketchParameters& sketch,
                       const document::ExtrusionParameters& parameters) {
    if (!parameters.regions.empty())
        return evaluateRegionFaces(sketch, parameters.regions);
    return evaluateNamedProfileFaces(sketch, parameters.profiles);
}

} // namespace voelund::features
