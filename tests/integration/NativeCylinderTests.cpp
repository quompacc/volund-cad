// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/document/NativePrimitiveCodec.hpp"
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BinDrivers.hxx>
#include <GProp_GProps.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDF_Reference.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_RealArray.hxx>
#include <TDocStd_Application.hxx>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <voelund/features/FeatureGraph.hpp>

namespace {
using namespace voelund;
using namespace document;
using detail::NativePrimitiveCodec;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Action> void rejected(Action action) {
    bool failed = false;
    try {
        action();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    require(failed, "Invalid native cylinder data accepted");
}
Handle(TDocStd_Document) fresh(const Handle(TDocStd_Application) & app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
void save(const Handle(TDocStd_Application) & app, const Handle(TDocStd_Document) & doc,
          std::ostream& output) {
    require(app->SaveAs(doc, output) == PCDM_SS_OK && output.good(), "Native save failed");
}
Handle(TDocStd_Document) open(const Handle(TDocStd_Application) & app, std::istream& input) {
    Handle(TDocStd_Document) doc;
    require(app->Open(input, doc) == PCDM_RS_OK && !doc.IsNull(), "Native open failed");
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
IdentifiedDocumentSnapshot fixture() {
    auto model = ModelDocument::empty();
    std::vector<FeatureId> boxes;
    for (int index = 0; index < 4; ++index)
        boxes.push_back(model.addBoxFeature("Körper Ω", {40, 30, 20}));
    std::size_t index = 0;
    for (auto mode : {CylinderResultMode::join, CylinderResultMode::cut,
                      CylinderResultMode::intersect, CylinderResultMode::separate}) {
        const auto placement = mode == CylinderResultMode::separate
                                   ? kernel::RigidTransform{3,
                                                            -2,
                                                            1,
                                                            kernel::Radians{0.1},
                                                            kernel::Radians{-0.2},
                                                            kernel::Radians{0.3}}
                                   : kernel::RigidTransform{};
        (void)model.addCylinderFeature("Körper Ω", boxes[index++], {2, 5, placement, mode});
    }
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, 0x0123456789abcdefULL},
                                               DocumentSnapshot::capture(model));
}
TDF_Label feature(const Handle(TDocStd_Document) & doc, int position) {
    Handle(TDF_Reference) ref;
    require(
        doc->Main().FindChild(20).FindChild(position).FindAttribute(TDF_Reference::GetID(), ref),
        "Missing test order reference");
    return ref->Get();
}
double volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}
void check(const Handle(TDocStd_Document) & doc, const IdentifiedDocumentSnapshot& expected) {
    const auto actual = NativePrimitiveCodec::read(doc);
    require(actual == expected, "Native cylinder snapshot changed");
    const auto before = features::FeatureGraph{}.evaluate(expected.snapshot().toDocument());
    const auto after = features::FeatureGraph{}.evaluate(actual.snapshot().toDocument());
    require(before.bodies.size() == after.bodies.size(), "Body count changed");
    for (std::size_t index = 0; index < after.bodies.size(); ++index) {
        const auto& a = before.bodies[index];
        const auto& b = after.bodies[index];
        require(a.bodyId == b.bodyId && a.sourceFeature == b.sourceFeature &&
                    BRepCheck_Analyzer(b.shape).IsValid() &&
                    std::abs(volume(a.shape) - volume(b.shape)) < 1e-8,
                "Native round trip changed body identity, BRep or volume");
    }
}
void roundTrip(const Handle(TDocStd_Application) & app,
               const IdentifiedDocumentSnapshot& expected) {
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    check(doc, expected);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto loaded = open(app, bytes);
    check(loaded, expected);
    app->Close(loaded);
    app->Close(doc);
}
void modesAndHistory(const Handle(TDocStd_Application) & app) {
    ModelDocument model;
    const auto identity = generateDocumentId();
    auto parameters = model.cylinderParameters(FeatureId{2});
    for (auto mode :
         {CylinderResultMode::separate, CylinderResultMode::cut, CylinderResultMode::intersect,
          CylinderResultMode::join, CylinderResultMode::separate}) {
        parameters.resultMode = mode;
        require(model.setCylinderParameters(FeatureId{2}, parameters), "Mode change failed");
        const auto current = DocumentSnapshot::capture(model);
        roundTrip(app, IdentifiedDocumentSnapshot::restore(identity, current));
        require(model.undo() && model.redo() && DocumentSnapshot::capture(model) == current,
                "Mode undo/redo changed snapshot");
    }
    // A cylinder may feed another cylinder; the input is a native reference to its producer.
    (void)model.addCylinderFeature("Second", FeatureId{2}, {1, 3, {}, CylinderResultMode::join});
    roundTrip(app, IdentifiedDocumentSnapshot::restore(identity, DocumentSnapshot::capture(model)));
    // Structurally valid but currently unevaluable input remains editable, not discarded on read.
    ModelDocument historical;
    (void)historical.addCylinderFeature("Historical", FeatureId{1},
                                        {1, 3, {}, CylinderResultMode::join});
    const auto invalidGeometry =
        IdentifiedDocumentSnapshot::restore(identity, DocumentSnapshot::capture(historical));
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, invalidGeometry);
    std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, bytes);
    auto restored = open(app, bytes);
    require(NativePrimitiveCodec::read(restored) == invalidGeometry,
            "Unevaluable input was repaired or discarded");
    bool evaluationFailed = false;
    try {
        (void)features::FeatureGraph{}.evaluate(
            NativePrimitiveCodec::read(restored).snapshot().toDocument());
    } catch (const features::FeatureEvaluationError&) {
        evaluationFailed = true;
    }
    require(evaluationFailed, "Historical input unexpectedly became valid");
    app->Close(restored);
    app->Close(doc);
}
void regressions(const Handle(TDocStd_Application) & app) {
    modesAndHistory(app);
    const auto expected = fixture();
    roundTrip(app, expected);
    auto doc = fresh(app);
    NativePrimitiveCodec::write(doc, expected);
    const auto cylinder = feature(doc, 5);
    auto other = fresh(app);
    NativePrimitiveCodec::write(other, expected);
    doc->SetUndoLimit(10);
    const auto corrupt = [&](auto mutate) {
        doc->NewCommand();
        mutate();
        rejected([&] { (void)NativePrimitiveCodec::read(doc); });
        doc->AbortCommand();
        require(NativePrimitiveCodec::read(doc) == expected,
                "Rejected edit damaged native document");
    };
    for (int mode : {0, 5, -1})
        corrupt([&] { TDataStd_Integer::Set(cylinder.FindChild(6), mode); });
    corrupt([&] { TDataStd_Integer::Set(cylinder.FindChild(6), 4); }); // wrong body owner
    corrupt([&] { cylinder.FindChild(5).ForgetAllAttributes(); });
    corrupt([&] { TDF_Reference::Set(cylinder.FindChild(5), doc->Main()); });
    corrupt([&] { TDF_Reference::Set(cylinder.FindChild(5), feature(other, 1)); });
    corrupt([&] { TDF_Reference::Set(cylinder.FindChild(5), cylinder); });
    corrupt([&] {
        TDF_Reference::Set(cylinder.FindChild(5), feature(doc, 6));
        TDF_Reference::Set(feature(doc, 6).FindChild(5), cylinder);
    });
    corrupt([&] { TDF_Reference::Set(doc->Main().FindChild(20).FindChild(1), cylinder); });
    corrupt([&] { TDF_Reference::Set(doc->Main().FindChild(20).FindChild(1), doc->Main()); });
    corrupt([&] { TDataStd_AsciiString::Set(cylinder.FindChild(1), "sketch"); });
    corrupt([&] { TDataStd_RealArray::Set(cylinder.FindChild(4), 1, 9); });
    for (int index : {1, 2, 8}) {
        corrupt([&] {
            Handle(TDataStd_RealArray) data;
            cylinder.FindChild(4).FindAttribute(TDataStd_RealArray::GetID(), data);
            data->SetValue(index, index == 8 ? std::numeric_limits<double>::quiet_NaN() : -1);
        });
    }
    doc->NewCommand();
    Handle(TDataStd_RealArray) data;
    cylinder.FindChild(4).FindAttribute(TDataStd_RealArray::GetID(), data);
    data->SetValue(1, 2.5);
    require(doc->CommitCommand(), "Native radius edit failed");
    const auto edited = NativePrimitiveCodec::read(doc);
    require(edited.id() == expected.id() && edited != expected && doc->Undo() &&
                NativePrimitiveCodec::read(doc) == expected && doc->Redo() &&
                NativePrimitiveCodec::read(doc) == edited && doc->Undo(),
            "Native history lost references");
    // Valid native container with a broken input must fail domain validation after Open.
    doc->NewCommand();
    cylinder.FindChild(5).ForgetAllAttributes();
    std::stringstream malformed(std::ios::in | std::ios::out | std::ios::binary);
    save(app, doc, malformed);
    auto restored = open(app, malformed);
    rejected([&] { (void)NativePrimitiveCodec::read(restored); });
    app->Close(restored);
    doc->AbortCommand();
    app->Close(other);
    app->Close(doc);
}
} // namespace
int main(int argc, char** argv) {
    try {
        Handle(TDocStd_Application) app = new TDocStd_Application;
        BinDrivers::DefineFormat(app);
        if (argc == 1)
            regressions(app);
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
        std::cout << "PASS native cylinders: modes, inputs, identities, order, parameters, BRep\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
