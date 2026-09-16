#include "../../src/document/NativePrimitiveCodec.hpp"
#include "../../src/document/NativeExtrusion.hpp"
#include <voelund/features/FeatureGraph.hpp>
#include <BinDrivers.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <TDataStd_Real.hxx>
#include <TDF_Reference.hxx>
#include <TDocStd_Application.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

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
    for (int index = 0; index < 3; ++index) {
        auto support = model.addBoxFeature("Körper Ω", {40, 30, 20});
        const auto sketch = model.addSketchFeature("Profile Ω", support,
            {{0, 0, 20, 0, 0, 1, 1, 0, 0}, {{
                {SketchElementId{9}, SketchRectangle{1, 1, 5, 5}},
                {SketchElementId{2}, SketchRectangle{10, 10, 13, 14}}}, 10}});
        std::vector<SketchProfileId> profiles;
        if (index > 0) profiles.emplace_back(9);
        if (index > 1) profiles.emplace_back(2);
        support = model.addExtrusionFeature("Extrusion Ω", sketch, {3, profiles});
        if (index == 2) {
            const auto next = model.addSketchFeature("Auf Extrusion", support,
                {{0, 0, 23, 0, 0, 1, 1, 0, 0}, {{
                    {SketchElementId{7}, SketchRectangle{2, 2, 4, 4}}}, 8}});
            (void)model.addExtrusionFeature("Folgeextrusion", next, {2, {SketchProfileId{7}}});
        }
    }
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, 0x0123456789abcdefULL},
                                               DocumentSnapshot::capture(model));
}
TDF_Label feature(const Handle(TDocStd_Document)& doc, int position) {
    Handle(TDF_Reference) ref;
    require(doc->Main().FindChild(20).FindChild(position).FindAttribute(TDF_Reference::GetID(), ref),
            "Missing order reference");
    return ref->Get();
}
void check(const Handle(TDocStd_Document)& doc, const IdentifiedDocumentSnapshot& expected) {
    const auto actual = NativePrimitiveCodec::read(doc);
    require(actual == expected, "Extrusion snapshot changed");
    const auto result = features::FeatureGraph{}.evaluate(actual.snapshot().toDocument());
    require(result.bodies.size() == 3, "Extrusions changed body count");
    const double volumes[]{24084, 24048, 24092};
    const std::uint64_t bodies[]{1, 4, 7}, producers[]{3, 6, 11};
    for (std::size_t index = 0; index < result.bodies.size(); ++index) {
        const auto& body = result.bodies[index];
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body.shape, properties);
        require(body.bodyId.value() == bodies[index] && body.sourceFeature.value() == producers[index] &&
            BRepCheck_Analyzer(body.shape).IsValid() && std::abs(properties.Mass() - volumes[index]) < 1e-7,
            "Extrusion identity, BRep or analytical volume changed");
    }
    for (std::size_t index : {1U, 4U, 7U, 9U})
        require(!result.features[index].bodyId, "Sketch acquired a solid body output");
}
void historicalProfiles(const Handle(TDocStd_Application)& app) {
    auto model = fixture().snapshot().toDocument();
    require(model.setExtrusionParameters(FeatureId{3},
        {3, {SketchProfileId{UINT64_MAX}, SketchProfileId{UINT64_MAX - 1}, SketchProfileId{99}}}),
        "Could not set historical selection");
    const auto expected = IdentifiedDocumentSnapshot::restore(fixture().id(), DocumentSnapshot::capture(model));
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    require(NativePrimitiveCodec::read(loaded) == expected, "Historical profiles lost or repaired");
    bool failed = false;
    try { (void)features::FeatureGraph{}.evaluate(NativePrimitiveCodec::read(loaded).snapshot().toDocument()); }
    catch (const features::FeatureEvaluationError&) { failed = true; }
    require(failed, "Unknown profile selection unexpectedly evaluated");
    app->Close(loaded);
    app->Close(doc);
}
void regressions(const Handle(TDocStd_Application)& app) {
    historicalProfiles(app);
    auto doc = fresh(app);
    const auto expected = fixture();
    NativePrimitiveCodec::write(doc, expected);
    check(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    check(loaded, expected);
    app->Close(loaded);
    auto other = fresh(app);
    NativePrimitiveCodec::write(other, expected);
    const auto extrusion = feature(doc, 9);
    const auto payload = extrusion.FindChild(4);
    const auto profiles = payload.FindChild(1);
    doc->SetUndoLimit(10);
    const auto corrupt = [&](auto mutate) {
        doc->NewCommand();
        mutate();
        rejected([&] { (void)NativePrimitiveCodec::read(doc); });
        doc->AbortCommand();
        require(NativePrimitiveCodec::read(doc) == expected, "Rejected edit damaged document");
    };
    for (double distance : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()})
        corrupt([&] { TDataStd_Real::Set(payload, distance); });
    corrupt([&] { payload.ForgetAttribute(TDataStd_Real::GetID()); });
    for (int count : {-1, 0, 3, 1000001})
        corrupt([&] { TDataStd_Integer::Set(profiles, count); });
    for (const char* id : {"0", "02", "-1", "18446744073709551616", "2"})
        corrupt([&] { TDataStd_AsciiString::Set(profiles.FindChild(1), id); });
    corrupt([&] { profiles.FindChild(1).ForgetAllAttributes(); });
    corrupt([&] { extrusion.FindChild(5).ForgetAllAttributes(); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), feature(other, 8)); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), doc->Main()); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), feature(doc, 7)); });
    corrupt([&] { TDF_Reference::Set(extrusion.FindChild(5), feature(doc, 5)); }); // wrong body
    corrupt([&] { TDF_Reference::Set(feature(doc, 8).FindChild(5), extrusion); }); // cycle
    rejected([&] { (void)detail::readNativeExtrusion(payload, 1); });
    rejected([&] { detail::validateNativeExtrusion({0, {}}); });
    rejected([&] { detail::validateNativeExtrusion({1, {SketchProfileId{0}}}); });
    rejected([&] { detail::validateNativeExtrusion({1, {SketchProfileId{2}, SketchProfileId{2}}}); });
    doc->NewCommand();
    TDataStd_Real::Set(payload, 4);
    require(doc->CommitCommand(), "Extrusion edit failed");
    const auto edited = NativePrimitiveCodec::read(doc);
    require(edited != expected && edited.id() == expected.id() && doc->Undo() &&
        NativePrimitiveCodec::read(doc) == expected && doc->Redo() &&
        NativePrimitiveCodec::read(doc) == edited && doc->Undo(), "Extrusion history lost identity");
    doc->NewCommand();
    extrusion.FindChild(5).ForgetAllAttributes();
    std::stringstream malformed(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, malformed);
    loaded = open(app, malformed);
    rejected([&] { (void)NativePrimitiveCodec::read(loaded); });
    app->Close(loaded);
    doc->AbortCommand();
    app->Close(other);
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
        std::cout << "PASS native extrusions: profiles, distance, references, identities, BRep, history\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
