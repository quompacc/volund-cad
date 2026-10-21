#include "../../src/document/NativeParameters.hpp"
#include "../../src/document/NativePrimitiveCodec.hpp"
#include <BinDrivers.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDocStd_Application.hxx>
#include <iostream>
#include <sstream>

// PAR01 schema 7: parameter table in Main() slot 30, bindings as optional children
// (constraint child 9, extrusion payload child 3); older schemas reject them.
namespace {
using namespace voelund;
using namespace document;
using detail::NativePrimitiveCodec;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action, const char* message) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, message);
}
Handle(TDocStd_Document) fresh(const Handle(TDocStd_Application) & app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
SketchParameters circleOnXy() {
    SketchParameters sketch{datumSketchPlane(DatumPlane::xy),
                            {{{SketchElementId{1}, SketchCircle{{0, 0}, 10}}}, 2}};
    sketch.datum = DatumPlane::xy;
    return sketch;
}
IdentifiedDocumentSnapshot fixture(bool bindConstraint, bool bindExtrusion) {
    auto model = ModelDocument::empty();
    ParameterTable table{{{ParameterId{1}, "A", ParameterUnit::length, 10, "Breite"},
                          {ParameterId{3}, "Winkel_2", ParameterUnit::angle, 30}},
                         4};
    require(model.setParameterTable(table), "Table set");
    auto sketch = circleOnXy();
    sketch.constraints = {{{SketchConstraintId{1},
                            SketchConstraintKind::radius,
                            {SketchElementId{1}, SketchPointPosition::center},
                            std::nullopt,
                            10}},
                          2};
    if (bindConstraint)
        sketch.constraints.values.front().parameter = ParameterId{1};
    const auto sketchId = model.addDatumSketchFeature("S", sketch);
    ExtrusionParameters extrusion{10, {}};
    if (bindExtrusion)
        extrusion.distanceParameter = ParameterId{1};
    (void)model.addExtrusionFeature("E", sketchId, extrusion);
    return IdentifiedDocumentSnapshot::restore(DocumentId{5, 6}, DocumentSnapshot::capture(model));
}

void roundTripAndGates(const Handle(TDocStd_Application) & app) {
    for (const auto [constraint, extrusion] : {std::pair{true, true}, std::pair{false, true},
                                               std::pair{true, false}, std::pair{false, false}}) {
        const auto expected = fixture(constraint, extrusion);
        auto doc = fresh(app);
        NativePrimitiveCodec::write(doc, expected);
        std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
        require(app->SaveAs(doc, bytes) == PCDM_SS_OK, "Native save failed");
        Handle(TDocStd_Document) loaded;
        require(app->Open(bytes, loaded) == PCDM_RS_OK, "Native open failed");
        require(NativePrimitiveCodec::read(loaded) == expected, "Parameter roundtrip");
        app->Close(loaded);
        // The table, and every binding, needs schema 7.
        TDataStd_Integer::Set(doc->Main().FindChild(2), 6);
        rejected([&] { (void)NativePrimitiveCodec::read(doc); }, "Schema 6 accepted a table");
        app->Close(doc);
    }
    // Without a table nothing is written at slot 30; such a document reads as schema 6.
    auto model = ModelDocument::empty();
    (void)model.addDatumSketchFeature("S", circleOnXy());
    const auto plain =
        IdentifiedDocumentSnapshot::restore(DocumentId{7, 8}, DocumentSnapshot::capture(model));
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, plain);
    require(doc->Main().FindChild(detail::nativeParameterSlot, false).IsNull(),
            "Empty table written");
    TDataStd_Integer::Set(doc->Main().FindChild(2), 6);
    require(NativePrimitiveCodec::read(doc) == plain, "Schema 6 document without parameters");
    app->Close(doc);
    std::cout << "schema 7 parameter table and bindings round-trip; schema 6 gate passed\n";
}

// N01 schema 37: an expression parameter and a plain number round-trip; schema 36 refuses them.
void expressionsAndNumbers(const Handle(TDocStd_Application) & app) {
    auto model = ModelDocument::empty();
    ParameterTable table;
    table.values = {{ParameterId{1}, "A", ParameterUnit::length, 10, "", ""},
                    {ParameterId{2}, "B", ParameterUnit::length, 25, "", "A*2 + 5"},
                    {ParameterId{3}, "N", ParameterUnit::number, 6, "", ""}};
    table.nextId = 4;
    require(model.setParameterTable(table), "Expression table set");
    const auto expected =
        IdentifiedDocumentSnapshot::restore(DocumentId{9, 10}, DocumentSnapshot::capture(model));
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    require(NativePrimitiveCodec::read(doc) == expected, "Expression round trip");
    TDataStd_Integer::Set(doc->Main().FindChild(2), 36);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }, "Schema 36 accepted an expression");
    app->Close(doc);
    rejected(
        [&] {
            auto wrong = table;
            wrong.values[1].value = 99; // not what A*2 + 5 gives
            static_cast<void>(model.setParameterTable(wrong));
        },
        "A stored value that differs from its expression was accepted");
    std::cout << "schema 37 expressions and plain numbers round-trip; schema 36 gate passed\n";
}

// N01b schema 37: a box's width bound to W follows it and round-trips; schema 36 refuses the
// list; a stored binding to a field the feature lacks is refused.
void fieldBindings(const Handle(TDocStd_Application) & app) {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Quader", {10, 10, 10});
    ParameterTable table;
    table.values = {{ParameterId{1}, "W", ParameterUnit::length, 25, "", ""}};
    table.nextId = 2;
    table.bindings = {{box, "width", ParameterId{1}}};
    require(model.setParameterTable(table), "Binding set");
    const auto snapshot = DocumentSnapshot::capture(model);
    require(std::get<BoxParameters>(snapshot.featureParameters(box)).dimensions.width == 25,
            "The bound width follows W");
    const auto expected = IdentifiedDocumentSnapshot::restore(DocumentId{11, 12}, snapshot);
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    require(NativePrimitiveCodec::read(doc) == expected, "Binding round trip");
    const auto binding =
        doc->Main().FindChild(detail::nativeParameterSlot).FindChild(3).FindChild(1);
    TDataStd_AsciiString::Set(binding.FindChild(2), "colour");
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }, "A binding to no field was accepted");
    TDataStd_AsciiString::Set(binding.FindChild(2), "width");
    TDataStd_Integer::Set(doc->Main().FindChild(2), 36);
    rejected([&] { (void)NativePrimitiveCodec::read(doc); }, "Schema 36 accepted field bindings");
    app->Close(doc);
    std::cout << "schema 37 field bindings round-trip; schema 36 gate and unknown field passed\n";
}

void corruptions(const Handle(TDocStd_Application) & app) {
    const auto expected = fixture(true, true);
    const auto corrupt = [&](auto change, const char* message) {
        auto doc = fresh(app);
        NativePrimitiveCodec::write(doc, expected);
        change(doc);
        rejected([&] { (void)NativePrimitiveCodec::read(doc); }, message);
        app->Close(doc);
    };
    const auto parameter = [](const Handle(TDocStd_Document) & doc, int index) {
        return doc->Main().FindChild(detail::nativeParameterSlot).FindChild(2).FindChild(index);
    };
    corrupt([&](auto doc) { TDataStd_AsciiString::Set(parameter(doc, 2).FindChild(2), "a"); },
            "Duplicate name ignoring case");
    corrupt([&](auto doc) { TDataStd_AsciiString::Set(parameter(doc, 2).FindChild(2), "mm"); },
            "Reserved name");
    corrupt([&](auto doc) { TDataStd_AsciiString::Set(parameter(doc, 2).FindChild(2), "2B"); },
            "Name starting with a digit");
    corrupt([&](auto doc) { TDataStd_Integer::Set(parameter(doc, 1).FindChild(3), 3); },
            "Unknown unit");
    corrupt([&](auto doc) { TDataStd_AsciiString::Set(parameter(doc, 2).FindChild(1), "4"); },
            "ID at the watermark");
    corrupt([&](auto doc) { TDataStd_AsciiString::Set(parameter(doc, 2).FindChild(1), "1"); },
            "Duplicate ID");
    corrupt([&](auto doc) { TDataStd_Integer::Set(parameter(doc, 1).FindChild(3), 2); },
            "Angle parameter bound to a radius and a distance");
    corrupt(
        [&](auto doc) {
            TDataStd_AsciiString::Set(
                doc->Main().FindChild(detail::nativeParameterSlot).FindChild(1), "0");
        },
        "Zero watermark");
    corrupt(
        [&](auto doc) {
            TDataStd_Integer::Set(doc->Main().FindChild(detail::nativeParameterSlot).FindChild(2),
                                  3);
        },
        "Count mismatch");

    // Bindings to unknown parameters or of the wrong unit are rejected before writing.
    auto model = expected.snapshot().toDocument(DocumentId{5, 6});
    auto table = model.parameterTable();
    table.values.erase(table.values.begin());
    rejected([&] { (void)model.setParameterTable(table); }, "Bound parameter deleted");
    auto sketch = model.sketchParameters(FeatureId{1});
    sketch.constraints.values.front().parameter = ParameterId{3};
    rejected([&] { (void)model.setSketchParameters(FeatureId{1}, sketch); }, "Unit mismatch");
    std::cout << "parameter corruption, unit and dangling-binding checks passed\n";
}
} // namespace

int main() {
    try {
        Handle(TDocStd_Application) app = new TDocStd_Application;
        BinDrivers::DefineFormat(app);
        roundTripAndGates(app);
        expressionsAndNumbers(app);
        fieldBindings(app);
        corruptions(app);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
