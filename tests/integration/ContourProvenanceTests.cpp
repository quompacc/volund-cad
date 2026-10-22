// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Compound.hxx>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <sstream>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/ContourProvenance.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

using namespace voelund;
using namespace features;
using namespace document;
namespace {
constexpr double pi = std::numbers::pi;
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void near(double a, double b) { require(std::abs(a - b) < 1e-7, "Contour point mismatch"); }
gp_Pnt point(std::span<const EdgeProvenance> edges, EdgeOrigin origin, double parameter) {
    const auto resolved = resolveEdgePoint(edges, origin, {EdgePointKind::parameter, parameter});
    require(resolved.status == EdgeResolutionStatus::resolved && resolved.point.has_value(),
            "Contour point did not resolve");
    return *resolved.point;
}
const EvaluatedFeature& output(const FeatureGraphEvaluation& result, FeatureId id) {
    const auto found = std::ranges::find(result.result.features, id, &EvaluatedFeature::id);
    require(found != result.result.features.end(), "Missing graph output");
    return *found;
}
bool contains(const TopoDS_Shape& shape, const TopoDS_Shape& edge) {
    for (TopExp_Explorer it(shape, TopAbs_EDGE); it.More(); it.Next())
        if (it.Current().IsSame(edge))
            return true;
    return false;
}
void profilesAndFrame() {
    SketchParameters p{{10, 20, 30, 0, 0, 1, 0, 1, 0},
                       {{{SketchElementId{10}, SketchRectangle{1, 2, 5, 6}},
                         {SketchElementId{20}, SketchCircle{{20, 20}, 3}},
                         {SketchElementId{30}, SketchEllipse{{40, 20}, 4, 2, pi / 2}},
                         {SketchElementId{40}, SketchLine{{0, 0}, {1, 1}}, true}},
                        41}};
    const auto elements = evaluateSketchElements(p);
    auto edges = sketchEdgeOrigins(FeatureId{5}, elements);
    require(edges.size() == 6, "Construction geometry acquired contour origins");
    const ContourEdgeOrigin circle{FeatureId{5}, SketchElementId{20}, ContourRole::circle};
    auto start = point(edges, circle, 0);
    near(start.X(), -10);
    near(start.Y(), 43);
    near(start.Z(), 30);
    near(point(edges, circle, .25).X(), -13);
    near(point(edges, circle, .25).Y(), 40);
    near(point(edges, circle, 0).Distance(point(edges, circle, 1)), 0);
    const auto ellipse =
        point(edges, ContourEdgeOrigin{FeatureId{5}, SketchElementId{30}, ContourRole::ellipse}, 0);
    near(ellipse.X(), -14);
    near(ellipse.Y(), 60);
    const ContourEdgeOrigin rectangle{FeatureId{5}, SketchElementId{10},
                                      ContourRole::rectangleFirst};
    near(point(edges, rectangle, .5).X(), 8);
    near(point(edges, rectangle, .5).Y(), 23);
    auto profiles = evaluateNamedProfileFaces(p, {});
    require(profiles.size() == 3, "Profile selection changed");
    for (const auto& profile : profiles)
        for (const auto& edge : profile.edges)
            require(contains(profile.face, edge.edge), "Origin describes a pre-wire edge copy");
    std::ranges::reverse(p.elements.values);
    p.elements.values.insert(p.elements.values.begin(),
                             {SketchElementId{41}, SketchCircle{{60, 60}, 1}});
    p.elements.nextId = 42;
    auto reordered = sketchEdgeOrigins(FeatureId{5}, evaluateSketchElements(p));
    near(point(reordered, circle, .125).Distance(point(edges, circle, .125)), 0);
    require(resolveEdgePoint(
                edges, ContourEdgeOrigin{FeatureId{5}, SketchElementId{20}, ContourRole::arc}, {})
                    .status == EdgeResolutionStatus::missing,
            "Geometry kind silently retargeted");
    std::cout << "rectangle roles, analytic circle/ellipse frame, seam, construction exclusion and "
                 "reordered elements passed\n";
}
void connectedCurvesAndPrism() {
    SketchParameters p{{0, 0, 0, 0, 0, 1, 1, 0, 0},
                       {{{SketchElementId{7}, SketchLine{{2, 0}, {-2, 0}}},
                         {SketchElementId{9}, SketchArc{{0, 0}, 2, 0, pi}}},
                        10}};
    const auto profiles = evaluateNamedProfileFaces(p, {});
    require(profiles.size() == 1 && profiles.front().edges.size() == 2, "Mixed profile missing");
    const voelund::kernel::Prism prism(profiles.front().face, 0, 0, 1, 5);
    auto edges = extrusionEdgeOrigins(FeatureId{3}, FeatureId{2}, profiles, prism);
    require(edges.size() == 4, "Unexpected prism contour count");
    const ContourEdgeOrigin arc{FeatureId{2}, SketchElementId{9}, ContourRole::arc, FeatureId{3},
                                ContourStage::extrusionEnd};
    const auto q = point(edges, arc, .25);
    near(q.X(), std::sqrt(2.0));
    near(q.Y(), std::sqrt(2.0));
    near(q.Z(), 5);
    const auto found = std::ranges::find(edges, EdgeOrigin{arc}, &EdgeProvenance::origin);
    for (unsigned density : {3U, 12U, 96U}) {
        auto segments = tessellateEdge(*found, density);
        require(segments.size() == density, "Contour density mismatch");
        near(point(edges, segments.front().origin, .25).Distance(q), 0);
    }
    // Exact prism subshape membership, even though MakeWire joined copied vertices.
    for (const auto& edge : edges)
        require(contains(prism.shape(), edge.source), "Prism origin is foreign");
    const auto cutTool = kernel::ShapeFactory::applyTransform(
        kernel::ShapeFactory::makeBox({1, 5, 2}), 0, 0, 0, {-0.5, -1, 4, {}, {}, {}});
    const auto cut = kernel::ShapeOperations::cutWithHistory(prism.shape(), cutTool);
    const auto split = continueEdgeOrigins(edges, cut);
    require(resolveEdgePoint(split, arc, {}).status == EdgeResolutionStatus::ambiguous,
            "Split contour guessed a successor");
    const auto unknown = continueEdgeOrigins(edges, {prism.shape(), {}, {}});
    require(resolveEdgePoint(unknown, arc, {}).status == EdgeResolutionStatus::unavailable,
            "Missing contour history was accepted");
    std::cout << "connected line/arc builder identity, reversed wire orientation, prism caps, "
                 "density and Boolean split passed\n";
}
void graphAndPersistence() {
    auto model = ModelDocument::empty();
    const auto base = model.addBoxFeature("Base", {20, 20, 10});
    const auto independent = model.addBoxFeature("Independent", {3, 3, 3});
    SketchParameters p{{0, 0, 10, 0, 0, 1, 1, 0, 0},
                       {{{SketchElementId{10}, SketchRectangle{2, 2, 6, 6}},
                         {SketchElementId{20}, SketchCircle{{12, 12}, 2}}},
                        21}};
    FeatureGraph graph;
    const auto initial = graph.evaluateDetailed(DocumentSnapshot::capture(model));
    const auto& support = output(*initial, base);
    const auto face =
        resolveSupportOrigin(support.supportOrigins, {base, kernel::PrimitiveRole::boxPositiveZ});
    p.supportBinding =
        bindSketchSupport(model.documentId(), support, {base, kernel::PrimitiveRole::boxPositiveZ},
                          face.candidates.front(), p.plane);
    const auto sketch = model.addSketchFeature("Profiles", base, p);
    const auto pad = model.addExtrusionFeature("Pad", sketch, {3, {SketchProfileId{20}}});
    auto first = graph.evaluateDetailed(DocumentSnapshot::capture(model));
    require(first->valid(), "Bound extrusion failed");
    const ContourEdgeOrigin circle{sketch, SketchElementId{20}, ContourRole::circle, pad,
                                   ContourStage::extrusionEnd};
    near(point(output(*first, pad).edgeOrigins, circle, 0).Z(), 13);
    require(
        resolveEdgePoint(output(*first, pad).edgeOrigins,
                         ContourEdgeOrigin{sketch, SketchElementId{10}, ContourRole::rectangleFirst,
                                           pad, ContourStage::extrusionEnd},
                         {})
                .status == EdgeResolutionStatus::missing,
        "Unselected profile acquired extrusion edges");
    const auto cached = output(*first, independent).edgeOrigins.front().source;
    auto boxParameters = model.boxParameters(base);
    boxParameters.placement.rotationY = kernel::Radians{pi / 2};
    require(model.setBoxParameters(base, boxParameters), "Rotate base failed");
    require(model.setExtrusionParameters(pad, {5, {SketchProfileId{20}}}),
            "Resize extrusion failed");
    auto moved = graph.evaluateDetailed(DocumentSnapshot::capture(model), first);
    require(moved->valid(), "Placed contour graph failed");
    const auto& world = output(*moved, pad).edgeOrigins;
    const auto& reference = output(*moved, pad).referenceEdgeOrigins;
    near(point(reference, circle, 0).Z(), 15);
    require(point(world, circle, 0).Distance(point(reference, circle, 0)) > 1,
            "Reference contour reused world placement");
    require(output(*moved, independent).edgeOrigins.front().source.IsSame(cached),
            "Independent cache invalidated");
    require(model.undo(), "Undo length failed");
    auto undone = graph.evaluateDetailed(DocumentSnapshot::capture(model), moved);
    near(point(output(*undone, pad).referenceEdgeOrigins, circle, 0).Z(), 13);
    require(model.redo(), "Redo length failed");
    auto redone = graph.evaluateDetailed(DocumentSnapshot::capture(model), undone);
    near(point(output(*redone, pad).edgeOrigins, circle, .125).Distance(point(world, circle, .125)),
         0);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    DocumentSerializer::write(model, bytes);
    auto reopened = DocumentSerializer::read(bytes);
    auto loaded = graph.evaluateDetailed(DocumentSnapshot::capture(reopened));
    require(loaded->valid(), "Reopened contours failed");
    near(point(output(*loaded, pad).edgeOrigins, circle, .125).Distance(point(world, circle, .125)),
         0);
    auto changed = reopened.sketchParameters(sketch);
    changed.elements.values.back().id = SketchElementId{21};
    changed.elements.nextId = 22;
    require(reopened.setSketchParameters(sketch, changed), "Change element identity failed");
    require(reopened.setExtrusionParameters(pad, {5, {SketchProfileId{21}}}),
            "Select replacement failed");
    auto replacement = graph.evaluateDetailed(DocumentSnapshot::capture(reopened));
    require(replacement->valid(), "Replacement graph failed");
    require(resolveEdgePoint(output(*replacement, pad).edgeOrigins, circle, {}).status ==
                EdgeResolutionStatus::missing,
            "Replacement contour silently retargeted old origin");
    std::cout << "profile selection, placed/reference contours, independent cache, undo/redo, "
                 "native reconstruction and replacement identity passed\n";
}
} // namespace
int main() {
    try {
        profilesAndFrame();
        connectedCurvesAndPrism();
        graphAndPersistence();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
