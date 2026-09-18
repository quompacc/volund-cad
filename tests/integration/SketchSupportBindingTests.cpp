#include "../../src/document/NativePrimitiveCodec.hpp"
#include "../../src/document/NativeSketchGeometry.hpp"
#include <BinDrivers.hxx>
#include <TDataStd_Integer.hxx>
#include <TDocStd_Application.hxx>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numbers>
#include <sstream>
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/SketchSupportBinding.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

using namespace voelund;
using namespace document;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception&) {
        failed = true;
    }
    require(failed, "Unsafe support accepted");
}
void near(double a, double b) { require(std::abs(a - b) < 1e-7, "Support frame mismatch"); }
const features::EvaluatedFeature& output(const features::FeatureGraphEvaluation& evaluation,
                                         FeatureId id) {
    const auto found =
        std::ranges::find(evaluation.result.features, id, &features::EvaluatedFeature::id);
    require(found != evaluation.result.features.end(), "Missing output");
    return *found;
}
SketchParameters sketch(double z) {
    return {{0, 0, z, 0, 0, 1, 1, 0, 0}, {{{SketchElementId{1}, SketchRectangle{1, 1, 3, 3}}}, 2}};
}
void bind(ModelDocument& model, SketchParameters& p, FeatureId support, SubshapeOrigin origin) {
    const auto evaluation =
        features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(model));
    const auto& item = output(*evaluation, support);
    const auto resolution = features::resolveSupportOrigin(item.supportOrigins, origin);
    require(resolution.status == features::SupportResolutionStatus::resolved, "Origin not unique");
    p.supportBinding = features::bindSketchSupport(model.documentId(), item, origin,
                                                   resolution.candidates.front(), p.plane);
}
ModelDocument fixture() {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 10, 10});
    auto p = sketch(10);
    bind(model, p, box, {box, kernel::PrimitiveRole::boxPositiveZ});
    const auto s = model.addSketchFeature("Bound", box, p);
    const auto extrusion = model.addExtrusionFeature("Pad", s, {2, {}});
    auto top = sketch(12);
    bind(model, top, extrusion, {extrusion, std::nullopt, {SketchElementId{1}}});
    (void)model.addSketchFeature("Cap", extrusion, top);
    return model;
}
void checkFixture(const ModelDocument& model) {
    const auto expected = fixture();
    require(DocumentSnapshot::capture(expected).remapSupportNamespace(
                expected.documentId(), model.documentId()) == DocumentSnapshot::capture(model),
            "Exchanged parameters changed");
    const auto result = features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(model));
    require(result->valid(), "Bound model failed");
    auto cap = model.sketchParameters(FeatureId{4});
    require(cap.supportBinding && cap.supportBinding->reference.document == model.documentId(),
            "Lost namespace");
    features::resolveBoundSketchSupport(cap, output(*result, FeatureId{3}));
    near(cap.plane.originZ, 12);
}
void persistenceAndEditing() {
    auto model = fixture();
    checkFixture(model);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    DocumentSerializer::write(model, bytes);
    auto opened = DocumentSerializer::read(bytes);
    require(opened.documentId() == model.documentId() &&
                DocumentSnapshot::capture(opened) == DocumentSnapshot::capture(model),
            "Roundtrip changed binding");
    checkFixture(opened);
    const auto snapshot = DocumentSnapshot::capture(opened);
    rejected([&] { (void)snapshot.toDocument(); });
    const auto duplicate =
        IdentifiedDocumentSnapshot::restore(opened.documentId(), snapshot).duplicate();
    require(duplicate.id() != opened.documentId(), "Duplicate retained identity");
    auto copy = duplicate.snapshot().toDocument(duplicate.id());
    checkFixture(copy);
    auto bad = opened.sketchParameters(FeatureId{2});
    bad.supportBinding->reference.document = generateDocumentId();
    rejected([&] { (void)opened.setSketchParameters(FeatureId{2}, bad); });
    require(DocumentSnapshot::capture(opened) == snapshot,
            "Rejected namespace edit mutated document");
    bad = opened.sketchParameters(FeatureId{2});
    bad.supportBinding->reference.body = BodyId{99};
    rejected([&] { (void)opened.setSketchParameters(FeatureId{2}, bad); });
    application::CadSession session;
    application::SketchEditorSession editor(session, FeatureId{2}, FeatureId{1},
                                            model.sketchParameters(FeatureId{2}));
    require(editor.draftParameters().supportBinding ==
                model.sketchParameters(FeatureId{2}).supportBinding,
            "Editor dropped binding");
    auto extrusion = opened.extrusionParameters(FeatureId{3});
    extrusion.distance = 4;
    require(opened.setExtrusionParameters(FeatureId{3}, extrusion), "Resize extrusion");
    const auto edited =
        features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(opened));
    require(edited->valid(), "Cap edit failed");
    auto cap = opened.sketchParameters(FeatureId{4});
    features::resolveBoundSketchSupport(cap, output(*edited, FeatureId{3}));
    near(cap.plane.originZ, 14);
    require(opened.undo(), "Undo cap edit");
    checkFixture(opened);
    require(opened.redo(), "Redo cap edit");
    auto baseSketch = opened.sketchParameters(FeatureId{2});
    baseSketch.elements.values.front().id = SketchElementId{2};
    baseSketch.elements.nextId = 3;
    require(opened.setSketchParameters(FeatureId{2}, baseSketch), "Change profile boundary");
    const auto changed =
        features::FeatureGraph{}.evaluateDetailed(DocumentSnapshot::capture(opened));
    require(!changed->valid() &&
                changed->report.features.back().status == features::EvaluationStatus::failed,
            "Changed boundary silently retargeted cap");
    std::cout << "persistence, namespace isolation, duplicate, editor retention, cap edit and "
                 "boundary loss passed\n";
}
void placementAndFailures() {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 10, 10});
    auto p = sketch(10);
    bind(model, p, box, {box, kernel::PrimitiveRole::boxPositiveZ});
    (void)model.addSketchFeature("Bound", box, p);
    features::FeatureGraph graph;
    const auto before = graph.evaluateDetailed(DocumentSnapshot::capture(model));
    auto dimensions = model.boxParameters(box);
    dimensions.dimensions.height = 12;
    dimensions.placement.rotationY = kernel::Radians{std::numbers::pi / 2};
    require(model.setBoxParameters(box, dimensions), "Rotate box");
    const auto after = graph.evaluateDetailed(DocumentSnapshot::capture(model), before);
    require(after->valid(), "Rotated bound graph failed");
    features::resolveBoundSketchSupport(p, output(*after, box));
    near(p.plane.normalX, 1);
    near(p.plane.originX, 11);
    const auto& old = output(*before, box);
    const auto oldFace = features::resolveSupportOrigin(old.supportOrigins,
                                                        {box, kernel::PrimitiveRole::boxPositiveZ})
                             .candidates.front();
    rejected([&] {
        (void)features::bindSketchSupport(model.documentId(), output(*after, box),
                                          {box, kernel::PrimitiveRole::boxPositiveZ}, oldFace,
                                          p.plane);
    });
    auto cutterModel = ModelDocument::empty();
    const auto b = cutterModel.addBoxFeature("Base", {10, 10, 10});
    const auto cut = cutterModel.addCylinderFeature("Cut", b, {2, 5, {}, CylinderResultMode::cut});
    auto cutSketch = sketch(10);
    bind(cutterModel, cutSketch, cut, {b, kernel::PrimitiveRole::boxPositiveZ});
    const auto s = cutterModel.addSketchFeature("Bound cut", cut, cutSketch);
    (void)cutterModel.addExtrusionFeature("Dependent", s, {2, {}});
    const auto independent = cutterModel.addBoxFeature("Independent", {2, 2, 2});
    auto cutting = cutterModel.cylinderParameters(cut);
    cutting.radius = 20;
    require(cutterModel.setCylinderParameters(cut, cutting), "Delete top");
    const auto failed = graph.evaluateDetailed(DocumentSnapshot::capture(cutterModel));
    require(!failed->valid(), "Deleted support succeeded");
    require(failed->report.features[2].status == features::EvaluationStatus::failed &&
                failed->report.features[3].status ==
                    features::EvaluationStatus::blockedByDependency,
            "Failure did not propagate");
    (void)output(*failed, independent);
    const auto tool = kernel::ShapeFactory::applyTransform(
        kernel::ShapeFactory::makeBox({2, 12, 2}), 0, 0, 0, {4, -1, 9, {}, {}, {}});
    const auto split = kernel::ShapeOperations::cutWithHistory(old.shape, tool);
    auto splitOutput = old;
    splitOutput.supportOrigins = features::continueSupportOrigins(old.supportOrigins, split);
    require(features::resolveSupportOrigin(splitOutput.supportOrigins,
                                           {box, kernel::PrimitiveRole::boxPositiveZ})
                    .status == features::SupportResolutionStatus::ambiguous,
            "Split fixture not ambiguous");
    auto original = sketch(10);
    original.supportBinding = cutSketch.supportBinding;
    rejected([&] { features::resolveBoundSketchSupport(original, splitOutput); });
    require(original.plane == sketch(10).plane, "Failed resolve mutated plane");
    auto unavailable = old;
    unavailable.supportOrigins =
        features::continueSupportOrigins(old.supportOrigins, {old.shape, {}, {}});
    rejected([&] { features::resolveBoundSketchSupport(original, unavailable); });
    auto projected = sketch(10);
    projected.supportBinding = original.supportBinding;
    projected.constraints.values.push_back({SketchConstraintId{1},
                                            SketchConstraintKind::coordinateX,
                                            {SketchElementId{1}, SketchPointPosition::start},
                                            std::nullopt,
                                            1,
                                            std::nullopt,
                                            {0, 0}});
    projected.constraints.nextId = 2;
    rejected([&] { features::resolveBoundSketchSupport(projected, old); });
    std::cout << "rotated frame, stale selection, deletion, blocked descendants, independent body, "
                 "split and missing history passed\n";
}
void schemaCompatibility() {
    Handle(TDocStd_Application) app = new TDocStd_Application;
    BinDrivers::DefineFormat(app);
    Handle(TDocStd_Document) native;
    app->NewDocument("BinOcaf", native);
    auto model = ModelDocument::empty();
    const auto b = model.addBoxFeature("Legacy", {10, 10, 10});
    (void)model.addSketchFeature("Unbound", b, sketch(10));
    const auto expected =
        IdentifiedDocumentSnapshot::restore(model.documentId(), DocumentSnapshot::capture(model));
    detail::NativePrimitiveCodec::write(native, expected);
    TDataStd_Integer::Set(native->Main().FindChild(2), 1);
    require(detail::NativePrimitiveCodec::read(native) == expected, "Schema 1 no longer readable");
    const auto bound = fixture().sketchParameters(FeatureId{2});
    const auto label = native->Main().FindChild(99);
    detail::writeNativeSketchGeometry(label, bound);
    rejected([&] { (void)detail::readNativeSketchGeometry(label, 100, 1); });
    require(detail::readNativeSketchGeometry(label, 100, 2) == bound, "Schema2 lost binding");
    TDataStd_Integer::Set(label.FindChild(11).FindChild(3), 99);
    rejected([&] { (void)detail::readNativeSketchGeometry(label, 100, 2); });
    const auto cap = fixture().sketchParameters(FeatureId{4});
    const auto capLabel = native->Main().FindChild(100);
    detail::writeNativeSketchGeometry(capLabel, cap);
    rejected([&] { (void)detail::readNativeSketchGeometry(capLabel, 1, 2); });
    require(detail::readNativeSketchGeometry(capLabel, 2, 2) == cap,
            "Cap boundary budget mismatch");
    app->Close(native);
    std::cout << "schema 1 compatibility, schema 2 binding and invalid role rejection passed\n";
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 1) {
            persistenceAndEditing();
            placementAndFailures();
            schemaCompatibility();
        } else {
            require(argc == 3 || argc == 4, "Usage: create FILE | read FILE [REWRITE]");
            auto model = fixture();
            if (std::string(argv[1]) == "read") {
                std::ifstream input(argv[2], std::ios::binary);
                model = DocumentSerializer::read(input);
            } else
                require(std::string(argv[1]) == "create", "Unknown exchange mode");
            checkFixture(model);
            if (std::string(argv[1]) == "create" || argc == 4) {
                std::ofstream out(argv[argc - 1], std::ios::binary);
                DocumentSerializer::write(model, out);
                out.close();
                require(!out.fail(), "Fixture write failed");
            }
            std::cout << "bound primitive and extrusion cap file exchange passed\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
