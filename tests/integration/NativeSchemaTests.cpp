#include "../../src/document/NativePrimitiveCodec.hpp"
#include <voelund/features/FeatureGraph.hpp>
#include <BinDrivers.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <TDF_Reference.hxx>
#include <TDocStd_Application.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <map>
#include <numbers>

namespace {
using namespace voelund;
using namespace document;
using detail::NativePrimitiveCodec;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Action> void rejected(Action action) {
    bool failed = false;
    try { action(); } catch (const std::invalid_argument&) { failed = true; }
    require(failed, "Invalid native cylinder data accepted");
}
Handle(TDocStd_Document) fresh(const Handle(TDocStd_Application)& app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
void save(const Handle(TDocStd_Application)& app, const Handle(TDocStd_Document)& doc,
          std::ostream& output) {
    require(app->SaveAs(doc, output) == PCDM_SS_OK && output.good(), "Native save failed");
}
Handle(TDocStd_Document) open(const Handle(TDocStd_Application)& app, std::istream& input) {
    Handle(TDocStd_Document) doc;
    require(app->Open(input, doc) == PCDM_RS_OK && !doc.IsNull(), "Native open failed");
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
IdentifiedDocumentSnapshot fixture() {
    auto model = ModelDocument::empty();
    const auto a = model.addBoxFeature("A", {40, 30, 20});
    const auto b = model.addBoxFeature("B", {10, 10, 10});
    const auto sketch = model.addSketchFeature("Skizze Ω", a,
        {{0, 0, 20, 0, 0, 1, 1, 0, 0}, {{
            {SketchElementId{9}, SketchRectangle{1, 1, 5, 5}},
            {SketchElementId{2}, SketchCircle{{8, 8}, 2}, true}}, 10},
            {{{SketchConstraintId{8}, SketchConstraintKind::radius,
                {SketchElementId{2}, SketchPointPosition::center}, std::nullopt, 2}}, 11}});
    (void)model.addCylinderFeature("C", b, {2, 5, {60, 0, 0, {}, {}, {}}, CylinderResultMode::separate});
    (void)model.addExtrusionFeature("A erweitert", sketch, {3, {SketchProfileId{9}}});
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, 0x0123456789abcdefULL},
        DocumentSnapshot::capture(model));
}
using Bodies = std::map<std::uint64_t, std::pair<std::uint64_t, double>>;
Bodies bodies(const IdentifiedDocumentSnapshot& snapshot) {
    const auto result = features::FeatureGraph{}.evaluate(snapshot.snapshot().toDocument());
    Bodies values;
    for (const auto& body : result.bodies) {
        require(BRepCheck_Analyzer(body.shape).IsValid(), "Invalid mixed-model BRep");
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body.shape, properties);
        values.emplace(body.bodyId.value(), std::pair{body.sourceFeature.value(), properties.Mass()});
    }
    require(values.size() == 3, "Independent bodies merged or lost");
    return values;
}
void sameBodies(const IdentifiedDocumentSnapshot& a, const IdentifiedDocumentSnapshot& b) {
    const auto before = bodies(a), after = bodies(b);
    require(before.size() == after.size(), "Body count changed");
    for (const auto& [id, value] : before) {
        const auto found = after.find(id);
        require(found != after.end() && found->second.first == value.first &&
            std::abs(found->second.second - value.second) < 1e-7, "Body identity or volume changed");
    }
}
void check(const Handle(TDocStd_Document)& doc, const IdentifiedDocumentSnapshot& expected) {
    const auto actual = NativePrimitiveCodec::read(doc);
    require(actual == expected, "Mixed snapshot fields changed");
    sameBodies(expected, actual);
}
void roundTrip(const Handle(TDocStd_Application)& app, const IdentifiedDocumentSnapshot& expected) {
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    check(loaded, expected);
    app->Close(loaded);
    app->Close(doc);
}
void regressions(const Handle(TDocStd_Application)& app) {
    const auto expected = fixture();
    const auto analytic = bodies(expected);
    require(std::abs(analytic.at(1).second - 24048) < 1e-7 &&
        std::abs(analytic.at(2).second - 1000) < 1e-7 &&
        std::abs(analytic.at(4).second - 20 * std::numbers::pi) < 1e-7,
        "Unexpected analytical volumes");
    roundTrip(app, expected);
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    require(doc->Main().FindChild(10).NbChildren() == 3, "Body groups require component wrapper");
    const auto order = doc->Main().FindChild(20);
    std::vector<TDF_Label> labels;
    for (int index = 1; index <= 5; ++index) {
        Handle(TDF_Reference) ref;
        require(order.FindChild(index).FindAttribute(TDF_Reference::GetID(), ref), "Missing reference");
        labels.push_back(ref->Get());
    }
    // Reorder independent branches, retaining each branch's dependency order.
    const int permutation[]{2, 4, 1, 3, 5};
    for (int index = 0; index < 5; ++index)
        TDF_Reference::Set(order.FindChild(index + 1), labels[static_cast<std::size_t>(permutation[index] - 1)]);
    TDataStd_AsciiString::Set(labels[0].FindChild(3), "Umbenannt Ω");
    const auto reordered = NativePrimitiveCodec::read(doc);
    require(reordered.id() == expected.id() && reordered.snapshot().features()[2].id == FeatureId{1} &&
        reordered.snapshot().features()[2].name == "Umbenannt Ω", "Reorder or rename lost identity");
    sameBodies(expected, reordered);
    roundTrip(app, reordered);
    const auto copy = reordered;
    roundTrip(app, copy);
    const auto duplicate = reordered.duplicate();
    require(duplicate.id() != reordered.id() && duplicate.snapshot() == reordered.snapshot(),
        "Duplicate did not isolate document namespace");
    roundTrip(app, duplicate);
    auto edited = reordered.snapshot().toDocument();
    const auto transient = edited.addBoxFeature("Undo", {1, 1, 1});
    require(edited.undo(), "Undo failed");
    const auto revision = reordered.revised(DocumentSnapshot::capture(edited));
    require(revision.id() == reordered.id() &&
        revision.snapshot().nextFeatureId() > transient.value(), "Revision lost allocation watermark");
    roundTrip(app, revision);
    auto restored = fresh(app);
    NativePrimitiveCodec::write(restored, revision);
    auto mutableModel = NativePrimitiveCodec::read(restored).snapshot().toDocument();
    require(mutableModel.addBoxFeature("Next", {1, 1, 1}).value() > transient.value(),
        "Native restore reused published ID");
    rejected([&] { (void)revision.revised(reordered.snapshot()); });
    app->Close(restored);
    app->Close(doc);
}
} // namespace
int main(int argc, char** argv) {
    try {
        Handle(TDocStd_Application) app = new TDocStd_Application;
        BinDrivers::DefineFormat(app);
        if (argc == 1) regressions(app);
        else {
            require(argc == 3 || argc == 4, "Usage: create FILE | read FILE [REWRITE]");
            Handle(TDocStd_Document) doc;
            const std::string mode = argv[1];
            if (mode == "create") {
                doc = fresh(app);
                NativePrimitiveCodec::write(doc, fixture());
            } else {
                require(mode == "read", "Unknown mode");
                std::ifstream input(argv[2], std::ios::binary);
                doc = open(app, input);
            }
            check(doc, fixture());
            if (mode == "create" || argc == 4) {
                std::ofstream output(argv[argc - 1], std::ios::binary);
                save(app, doc, output);
                output.close();
                require(!output.fail(), "Fixture flush failed");
            }
            app->Close(doc);
        }
        std::cout << "PASS native schema: mixed features, independent bodies, reorder, identity, watermark\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
