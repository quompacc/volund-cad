#include "../../src/document/NativePrimitiveCodec.hpp"
#include "../../src/document/NativeSketchGeometry.hpp"
#include <voelund/document/SketchPlaneValidation.hpp>
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
SketchParameters geometry() {
    return {{1, 2, 20, 0, 0, 1, 1, 0, 0}, {{
        {SketchElementId{9}, SketchLine{{0, 0}, {2, 3}}, true},
        {SketchElementId{2}, SketchLine{{0, 4}, {2, 4}, SketchLineConstraint::horizontal}, true},
        {SketchElementId{3}, SketchLine{{4, 0}, {4, 3}, SketchLineConstraint::vertical}, true},
        {SketchElementId{4}, SketchRectangle{1, 1, 5, 6}},
        {SketchElementId{5}, SketchCircle{{10, 10}, 2}, true},
        {SketchElementId{6}, SketchArc{{15, 15}, 3, 0.2, 2.1}, true},
        {SketchElementId{UINT64_MAX - 2}, SketchEllipse{{20, 20}, 4, 2, 0.3}, true}
    }, UINT64_MAX - 1}, {{}, UINT64_MAX - 1}};
}
IdentifiedDocumentSnapshot fixture(bool constrained = false) {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Körper Ω", {40, 30, 20});
    auto sketch = geometry();
    if (constrained) sketch.constraints.values.emplace_back(SketchConstraintId{7},
        SketchConstraintKind::radius, SketchGeometryReference{SketchElementId{5}}, std::nullopt, 2);
    (void)model.addSketchFeature("Skizze Ω", box, sketch);
    const auto cylinder = model.addCylinderFeature("Zylinder", box,
        {2, 5, {}, CylinderResultMode::separate});
    (void)model.addSketchFeature("Skizze Ω", cylinder, geometry());
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, 0x0123456789abcdefULL},
                                               DocumentSnapshot::capture(model));
}
TDF_Label feature(const Handle(TDocStd_Document)& doc, int position) {
    Handle(TDF_Reference) ref;
    require(doc->Main().FindChild(20).FindChild(position).FindAttribute(TDF_Reference::GetID(), ref),
            "Missing order reference");
    return ref->Get();
}
Handle(TDataStd_RealArray) array(const TDF_Label& label) {
    Handle(TDataStd_RealArray) value;
    require(label.FindAttribute(TDataStd_RealArray::GetID(), value), "Missing array");
    return value;
}
void check(const Handle(TDocStd_Document)& doc, const IdentifiedDocumentSnapshot& expected) {
    const auto actual = NativePrimitiveCodec::read(doc);
    require(actual == expected, "Sketch snapshot changed");
    const auto result = features::FeatureGraph{}.evaluate(actual.snapshot().toDocument());
    require(result.bodies.size() == 2 && result.features.size() == 4 &&
        !result.features[1].bodyId && !result.features[3].bodyId,
        "Sketch acquired a solid body output");
    for (const auto& body : result.bodies)
        require(BRepCheck_Analyzer(body.shape).IsValid(), "Invalid reconstructed body");
}
void regressions(const Handle(TDocStd_Application)& app) {
    const auto expected = fixture();
    auto doc = fresh(app);
    // Unsupported constraints must fail before changing even the header.
    rejected([&] { NativePrimitiveCodec::write(doc, fixture(true)); });
    require(doc->Main().NbChildren() == 0, "Rejected write modified root");
    NativePrimitiveCodec::write(doc, expected);
    check(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    check(loaded, expected);
    app->Close(loaded);
    const auto sketch = feature(doc, 2);
    const auto payload = sketch.FindChild(4);
    const auto elements = payload.FindChild(10);
    auto other = fresh(app);
    NativePrimitiveCodec::write(other, expected);
    doc->SetUndoLimit(10);
    const auto corrupt = [&](auto mutate) {
        doc->NewCommand();
        mutate();
        rejected([&] { (void)NativePrimitiveCodec::read(doc); });
        doc->AbortCommand();
        require(NativePrimitiveCodec::read(doc) == expected, "Rejected edit damaged document");
    };
    corrupt([&] { sketch.FindChild(5).ForgetAllAttributes(); });
    corrupt([&] { TDF_Reference::Set(sketch.FindChild(5), feature(other, 1)); });
    corrupt([&] { TDF_Reference::Set(sketch.FindChild(5), feature(doc, 4)); });
    corrupt([&] { TDF_Reference::Set(sketch.FindChild(5), doc->Main()); });
    for (const char* id : {"0", "02", "18446744073709551614", "18446744073709551615", "-1", "2"})
        corrupt([&] { TDataStd_AsciiString::Set(elements.FindChild(1).FindChild(1), id); });
    for (int tag : {1, 2})
        for (const char* counter : {"0", "01", "18446744073709551615"})
            corrupt([&] { TDataStd_AsciiString::Set(payload.FindChild(tag), counter); });
    corrupt([&] { TDataStd_AsciiString::Set(payload.FindChild(1), "9"); });
    corrupt([&] { TDataStd_Integer::Set(payload.FindChild(3), 1); });
    for (int count : {0, 8, 1000001})
        corrupt([&] { TDataStd_Integer::Set(elements, count); });
    corrupt([&] { TDataStd_AsciiString::Set(elements.FindChild(1).FindChild(2), "future"); });
    corrupt([&] { TDataStd_Integer::Set(elements.FindChild(1).FindChild(3), 2); });
    corrupt([&] { TDataStd_Integer::Set(elements.FindChild(1).FindChild(5), 3); });
    corrupt([&] { TDataStd_Integer::Set(elements.FindChild(1).FindChild(5), 1); });
    corrupt([&] { array(elements.FindChild(5).FindChild(4))->SetValue(3, -1); });
    corrupt([&] { array(elements.FindChild(6).FindChild(4))->SetValue(5, 0.2); });
    corrupt([&] { array(elements.FindChild(7).FindChild(4))->SetValue(4, 5); });
    corrupt([&] { array(payload)->SetValue(1, std::numeric_limits<double>::quiet_NaN()); });
    corrupt([&] { array(payload)->SetValue(6, 0); });
    corrupt([&] { array(elements.FindChild(1).FindChild(4))->Init(0, 3); });
    rejected([&] { (void)detail::readNativeSketchGeometry(payload, 6); });
    auto invalid = geometry();
    std::get<SketchLine>(invalid.elements.values[0].geometry).constraint =
        static_cast<SketchLineConstraint>(99);
    rejected([&] { detail::validateNativeSketchGeometry(invalid); });
    doc->NewCommand();
    array(elements.FindChild(5).FindChild(4))->SetValue(3, 2.5);
    require(doc->CommitCommand(), "Edit did not commit");
    const auto edited = NativePrimitiveCodec::read(doc);
    require(edited != expected && edited.id() == expected.id() && doc->Undo() &&
        NativePrimitiveCodec::read(doc) == expected && doc->Redo() &&
        NativePrimitiveCodec::read(doc) == edited && doc->Undo(), "Undo/Redo changed identity");
    // Legacy perpendicular axes retain their magnitude through native read/write.
    doc->NewCommand();
    array(payload)->SetValue(6, 2);
    array(payload)->SetValue(7, 3);
    const auto legacy = NativePrimitiveCodec::read(doc);
    auto preserved = fresh(app);
    NativePrimitiveCodec::write(preserved, legacy);
    const auto preservedPlane = detail::readNativeSketchGeometry(feature(preserved, 2).FindChild(4), 7).plane;
    require(preservedPlane.normalZ == 2 && preservedPlane.xDirectionX == 3,
            "Legacy axes silently normalized");
    rejected([&] { validateCanonicalSketchPlane(preservedPlane); });
    app->Close(preserved);
    doc->AbortCommand();
    doc->NewCommand();
    TDataStd_Integer::Set(payload.FindChild(3), 1);
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
        std::cout << "PASS native sketch geometry: elements, planes, support, identity, validation, history\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
