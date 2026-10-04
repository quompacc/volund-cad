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
    throw GeometryEvaluationError("Region did not produce a valid face");
}

// The legacy loop face whose loop carries exactly these elements; the bridge's only source.
EvaluatedProfileFace loopFace(const document::SketchParameters& sketch,
                              std::span<const SketchProfile> loops,
                              const std::set<std::uint64_t>& elements) {
    for (const SketchProfile& loop : loops) {
        std::set<std::uint64_t> loopElements;
        for (document::SketchElementId element : loop.elementIds)
            loopElements.insert(element.value());
        if (loopElements == elements) {
            const document::SketchProfileId id[]{loop.id};
            return evaluateNamedProfileFaces(sketch, id).front();
        }
    }
    throw GeometryEvaluationError(
        "Region on intersecting boundaries requires the R10 face construction");
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
        if (resolved.status != SketchRegionReferenceStatus::resolved)
            throw GeometryEvaluationError("Extrusion region needs repair: " +
                                          reason(resolved.status));
        selected.push_back(*resolved.region);
    }
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
