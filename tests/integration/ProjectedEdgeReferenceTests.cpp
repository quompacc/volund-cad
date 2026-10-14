#include "../../src/document/NativePrimitiveCodec.hpp"
#include "../../src/document/NativeProjectedEdge.hpp"
#include <BinDrivers.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Real.hxx>
#include <TDocStd_Application.hxx>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

using namespace voelund;
using namespace document;
namespace {
void require(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void near(double a, double b) { require(std::abs(a - b) < 1e-7, "Projected coordinate mismatch"); }
template <class F> void rejected(F action) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, "Invalid projected reference accepted");
}
const features::EvaluatedFeature& output(const features::FeatureGraphEvaluation& e, FeatureId id) {
    const auto found = std::ranges::find(e.result.features, id, &features::EvaluatedFeature::id);
    require(found != e.result.features.end(), "Missing feature output");
    return *found;
}
void bind(ModelDocument& model, SketchParameters& p, FeatureId support, SubshapeOrigin origin) {
    const auto e = features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(model));
    const auto& item = output(*e, support);
    const auto face = features::resolveSupportOrigin(item.supportOrigins, origin);
    require(face.candidates.size() == 1, "Support ambiguous");
    p.supportBinding = features::bindSketchSupport(model.documentId(), item, origin,
                                                   face.candidates.front(), p.plane);
}
void attach(SketchParameters& p, ProjectedEdgeReference ref) {
    for (auto kind : {SketchConstraintKind::coordinateX, SketchConstraintKind::coordinateY}) {
        SketchConstraint constraint{SketchConstraintId{p.constraints.nextId++},
                                    kind,
                                    {SketchElementId{1}, SketchPointPosition::start},
                                    std::nullopt,
                                    123};
        constraint.projectedEdge = ref;
        p.constraints.values.push_back(constraint);
    }
}
ModelDocument fixture() {
    auto m = ModelDocument::empty();
    const auto base = m.addBoxFeature("Base", {10, 10, 10});
    (void)m.addBoxFeature("Independent", {2, 2, 2});
    SketchParameters p{{0, 0, 10, 0, 0, 1, 1, 0, 0},
                       {{{SketchElementId{1}, SketchLine{{123, 123}, {7, 7}}},
                         {SketchElementId{2}, SketchRectangle{4, 4, 6, 6}}},
                        3}};
    bind(m, p, base, {base, kernel::PrimitiveRole::boxPositiveZ});
    attach(p, {m.documentId(),
               BodyId{base.value()},
               {base, kernel::PrimitiveRole::boxNegativeY, kernel::PrimitiveRole::boxPositiveZ},
               {EdgePointKind::parameter, .25}});
    auto sketch = m.addSketchFeature("Attached", base, p);
    (void)m.addExtrusionFeature("Pad", sketch, {2, {SketchProfileId{2}}});
    return m;
}
void check(const ModelDocument& m, double expectedX = 2.5) {
    auto e = features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(m));
    require(e->valid(), "Projected model failed");
    const auto& p = *output(*e, FeatureId{3}).resolvedSketch;
    const auto line = std::get<SketchLine>(p.elements.values.front().geometry);
    near(line.start.x, expectedX);
    near(line.start.y, 0);
    for (const auto& c : p.constraints.values)
        require(c.projectedEdge && c.projectedEdge->document == m.documentId(),
                "Reference namespace lost");
}
void persistenceAndGraph() {
    auto m = fixture();
    check(m);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    DocumentSerializer::write(m, bytes);
    auto opened = DocumentSerializer::read(bytes);
    check(opened);
    require(DocumentSnapshot::capture(m) == DocumentSnapshot::capture(opened),
            "Native reference roundtrip changed data");
    const auto copy =
        IdentifiedDocumentSnapshot::restore(m.documentId(), DocumentSnapshot::capture(m))
            .duplicate();
    require(copy.id() != m.documentId(), "Duplicate namespace retained");
    check(copy.snapshot().toDocument(copy.id()));
    auto bad = m.sketchParameters(FeatureId{3});
    bad.constraints.values.front().projectedEdge->document = generateDocumentId();
    const auto before = DocumentSnapshot::capture(m);
    rejected([&] { (void)m.setSketchParameters(FeatureId{3}, bad); });
    require(DocumentSnapshot::capture(m) == before, "Rejected namespace changed document");
    bad = m.sketchParameters(FeatureId{3});
    bad.constraints.values.front().projectedEdge->body = BodyId{2};
    rejected([&] { (void)m.setSketchParameters(FeatureId{3}, bad); });
    require(m.setBoxDimensions(FeatureId{1}, {20, 10, 10}), "Resize failed");
    check(m, 5);
    require(m.undo(), "Undo failed");
    check(m);
    require(m.redo(), "Redo failed");
    check(m, 5);
    bad = m.sketchParameters(FeatureId{3});
    for (auto& c : bad.constraints.values)
        c.projectedEdge->origin =
            PrimitiveEdgeOrigin{FeatureId{99}, kernel::PrimitiveRole::boxNegativeY,
                                kernel::PrimitiveRole::boxPositiveZ};
    require(m.setSketchParameters(FeatureId{3}, bad), "Missing origin fixture failed");
    auto failed = features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(m));
    require(failed->report.features[2].status == features::EvaluationStatus::failed &&
                failed->report.features[3].status ==
                    features::EvaluationStatus::blockedByDependency &&
                failed->report.features[1].status == features::EvaluationStatus::valid,
            "Missing reference used cached coordinates");
    std::cout << "primitive reference persistence, namespaces, duplication, recompute, undo/redo "
                 "and isolated failures passed\n";
}
void contourAndPlacement() {
    auto m = ModelDocument::empty();
    auto base = m.addBoxFeature("Base", {100, 100, 10});
    SketchParameters profile{{0, 0, 10, 0, 0, 1, 1, 0, 0},
                             {{{SketchElementId{10}, SketchCircle{{30, 30}, 10}}}, 11}};
    bind(m, profile, base, {base, kernel::PrimitiveRole::boxPositiveZ});
    auto sketch = m.addSketchFeature("Circle", base, profile);
    auto pad = m.addExtrusionFeature("Pad", sketch, {5, {}});
    SketchParameters p{{0, 0, 15, 0, 0, 1, 1, 0, 0},
                       {{{SketchElementId{1}, SketchLine{{1, 1}, {31, 31}}}}, 2}};
    bind(m, p, pad, {pad, std::nullopt, {SketchElementId{10}}});
    attach(p, {m.documentId(),
               BodyId{base.value()},
               ContourEdgeOrigin{sketch, SketchElementId{10}, ContourRole::circle, pad,
                                 ContourStage::extrusionEnd},
               {EdgePointKind::parameter, .125}});
    auto target = m.addSketchFeature("Circle attachment", pad, p);
    auto e = features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(m));
    require(e->valid(), "Contour attachment failed");
    auto start =
        std::get<SketchLine>(output(*e, target).resolvedSketch->elements.values.front().geometry)
            .start;
    near(start.x, 30 + 10 / std::sqrt(2.0));
    near(start.y, start.x);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    DocumentSerializer::write(m, bytes);
    auto loaded = DocumentSerializer::read(bytes);
    require(DocumentSnapshot::capture(loaded) == DocumentSnapshot::capture(m),
            "Contour persistence changed origin");
    auto parameters = loaded.boxParameters(base);
    parameters.placement.rotationY = kernel::Radians{.5};
    require(loaded.setBoxParameters(base, parameters), "Rotate failed");
    auto moved = features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(loaded));
    require(moved->valid(), "Rotated contour attachment failed");
    start = std::get<SketchLine>(
                output(*moved, target).resolvedSketch->elements.values.front().geometry)
                .start;
    near(start.x, 30 + 10 / std::sqrt(2.0));
    near(start.y, start.x);
    std::cout << "extrusion contour reference roundtrip, analytic circle parameter and placed "
                 "support frame passed\n";
}
void codecNegatives() {
    const Handle(TDocStd_Application) app = new TDocStd_Application;
    BinDrivers::DefineFormat(app);
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    auto m = fixture();
    const auto identified =
        IdentifiedDocumentSnapshot::restore(m.documentId(), DocumentSnapshot::capture(m));
    detail::NativePrimitiveCodec::write(doc, identified);
    for (int old : {1, 2, 30}) {
        TDataStd_Integer::Set(doc->Main().FindChild(2), old);
        rejected([&] { (void)detail::NativePrimitiveCodec::read(doc); });
    }
    TDataStd_Integer::Set(doc->Main().FindChild(2), 3);
    require(detail::NativePrimitiveCodec::read(doc) == identified, "Schema3 reference read failed");
    app->Close(doc);
    app->NewDocument("BinOcaf", doc);
    auto plain = ModelDocument::empty();
    (void)plain.addBoxFeature("Plain", {2, 2, 2});
    auto plainId =
        IdentifiedDocumentSnapshot::restore(plain.documentId(), DocumentSnapshot::capture(plain));
    detail::NativePrimitiveCodec::write(doc, plainId);
    for (int old : {1, 2}) {
        TDataStd_Integer::Set(doc->Main().FindChild(2), old);
        require(detail::NativePrimitiveCodec::read(doc) == plainId, "Old schema unreadable");
    }
    app->Close(doc);
    app->NewDocument("BinOcaf", doc);
    auto oldSketch = m.sketchParameters(FeatureId{3});
    for (auto& constraint : oldSketch.constraints.values)
        constraint.projectedEdge.reset();
    auto oldModel = m;
    require(oldModel.setSketchParameters(FeatureId{3}, oldSketch),
            "Old bound sketch fixture failed");
    const auto oldSnapshot = IdentifiedDocumentSnapshot::restore(
        oldModel.documentId(), DocumentSnapshot::capture(oldModel));
    detail::NativePrimitiveCodec::write(doc, oldSnapshot);
    TDataStd_Integer::Set(doc->Main().FindChild(2), 2);
    require(detail::NativePrimitiveCodec::read(doc) == oldSnapshot,
            "Schema2 bound sketch unreadable");
    app->Close(doc);
    app->NewDocument("BinOcaf", doc);
    const auto label = doc->Main().FindChild(50);
    const auto ref = *m.sketchParameters(FeatureId{3}).constraints.values.front().projectedEdge;
    detail::writeNativeProjectedEdge(label, ref);
    require(detail::readNativeProjectedEdge(label) == ref, "Direct codec mismatch");
    for (auto kind : {EdgePointKind::start, EdgePointKind::end}) {
        auto endpoint = ref;
        endpoint.selection = {kind, 0};
        const auto endpointLabel = doc->Main().FindChild(kind == EdgePointKind::start ? 51 : 52);
        detail::writeNativeProjectedEdge(endpointLabel, endpoint);
        require(detail::readNativeProjectedEdge(endpointLabel) == endpoint,
                "Endpoint kind changed");
    }
    TDataStd_Integer::Set(label.FindChild(8), 99);
    rejected([&] { (void)detail::readNativeProjectedEdge(label); });
    TDataStd_Integer::Set(label.FindChild(8), static_cast<int>(EdgePointKind::parameter));
    TDataStd_Real::Set(label.FindChild(9), 1.1);
    rejected([&] { (void)detail::readNativeProjectedEdge(label); });
    TDataStd_Real::Set(label.FindChild(9), .25);
    TDataStd_AsciiString::Set(label.FindChild(1), "01");
    rejected([&] { (void)detail::readNativeProjectedEdge(label); });
    auto invalid = ref;
    invalid.selection.parameter = std::numeric_limits<double>::quiet_NaN();
    rejected([&] { validateProjectedEdgeReference(invalid); });
    invalid = ref;
    invalid.selection.kind = EdgePointKind::end;
    rejected([&] { validateProjectedEdgeReference(invalid); });
    auto params = m.sketchParameters(FeatureId{3});
    params.constraints.values.front().auxiliaryValues = {1, .5};
    rejected([&] { validateSketchConstraints(params.elements, params.constraints); });
    app->Close(doc);
    std::cout
        << "schema1/2 compatibility, schema3 gating and malformed reference rejection passed\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 3) {
            if (std::string(argv[1]) == "write") {
                auto m = fixture();
                std::ofstream file(argv[2], std::ios::binary);
                DocumentSerializer::write(m, file);
            } else if (std::string(argv[1]) == "read") {
                std::ifstream file(argv[2], std::ios::binary);
                auto m = DocumentSerializer::read(file);
                check(m);
            } else
                throw std::invalid_argument("Unknown exchange action");
        } else {
            persistenceAndGraph();
            contourAndPlacement();
            codecNegatives();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
