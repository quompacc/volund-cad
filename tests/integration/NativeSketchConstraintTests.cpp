// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/document/NativePrimitiveCodec.hpp"
#include "../../src/document/NativeSketchConstraints.hpp"
#include "../../src/document/NativeSketchGeometry.hpp"
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
#include <voelund/document/SketchPlaneValidation.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>

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
SketchGeometryReference ref(std::uint64_t id,
                            SketchPointPosition point = SketchPointPosition::start) {
    return {SketchElementId{id}, point};
}
SketchParameters geometry() {
    return {{0, 0, 20, 0, 0, 1, 1, 0, 0},
            {{{SketchElementId{9}, SketchLine{{0, 0}, {3, 4}}},
              {SketchElementId{2}, SketchLine{{1, 2}, {5, 7}}, true},
              {SketchElementId{3}, SketchLine{{10, 0}, {10, 10}}, true},
              {SketchElementId{4}, SketchCircle{{8, 8}, 2}},
              {SketchElementId{5}, SketchArc{{12, 12}, 3, 0.2, 2.1}},
              {SketchElementId{6}, SketchEllipse{{20, 20}, 4, 2, 0.3}, true}},
             20},
            {{}, UINT64_MAX - 1}};
}
SketchParameters allKinds() {
    auto sketch = geometry();
    const auto append = [&](SketchConstraintKind kind, SketchGeometryReference first,
                            std::optional<SketchGeometryReference> second = std::nullopt,
                            double value = 0,
                            std::optional<SketchGeometryReference> third = std::nullopt,
                            std::vector<double> extra = {}) {
        sketch.constraints.values.emplace_back(
            SketchConstraintId{100 + sketch.constraints.values.size()}, kind, first, second, value,
            third, std::move(extra));
    };
    for (auto kind : {SketchConstraintKind::horizontal, SketchConstraintKind::vertical})
        append(kind, ref(9));
    append(SketchConstraintKind::distance, ref(9), {}, 5);
    append(SketchConstraintKind::radius, ref(4, SketchPointPosition::center), {}, 2);
    for (auto kind : {SketchConstraintKind::coincident, SketchConstraintKind::parallel,
                      SketchConstraintKind::perpendicular, SketchConstraintKind::pointOnObject,
                      SketchConstraintKind::equal, SketchConstraintKind::midpoint,
                      SketchConstraintKind::perpendicularBisector, SketchConstraintKind::collinear,
                      SketchConstraintKind::horizontalPoints, SketchConstraintKind::verticalPoints})
        append(kind, ref(9, SketchPointPosition::end), ref(2));
    append(SketchConstraintKind::coordinateX, ref(9), {}, -2.5);
    append(SketchConstraintKind::coordinateY, ref(6, SketchPointPosition::center), {}, 20);
    append(SketchConstraintKind::tangent, ref(9), ref(5));
    append(SketchConstraintKind::symmetric, ref(9), ref(2), 0, ref(3));
    append(SketchConstraintKind::pointDistance, ref(9), ref(2, SketchPointPosition::end), 4);
    append(SketchConstraintKind::angle, ref(9), ref(2));
    sketch.constraints.values.back().angleValue = kernel::Radians{-0.75};
    append(SketchConstraintKind::pointDistanceX, ref(9), ref(2), -2);
    append(SketchConstraintKind::pointDistanceY, ref(9), ref(5, SketchPointPosition::end), 3);
    append(SketchConstraintKind::concentric, ref(4), ref(5));
    append(SketchConstraintKind::diameter, ref(5), {}, 6);
    append(SketchConstraintKind::fixed, ref(9), {}, 0, {}, {0, 3, 4});
    append(SketchConstraintKind::pointLineDistance, ref(4, SketchPointPosition::center), ref(2), 3);
    require(sketch.constraints.values.size() == 26, "Missing constraint kind fixture");
    append(SketchConstraintKind::fixed, ref(4), {}, 8, {}, {8, 2});
    append(SketchConstraintKind::coordinateX, ref(9), {}, -3, {}, {2, 0.25});
    sketch.constraints.values.front().id = SketchConstraintId{UINT64_MAX - 2};
    return sketch;
}
IdentifiedDocumentSnapshot snapshot(const SketchParameters& sketch) {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Körper Ω", {40, 30, 20});
    (void)model.addSketchFeature("Constraints Ω", box, sketch);
    (void)model.addSketchFeature("Other sketch", box, geometry());
    return IdentifiedDocumentSnapshot::restore(DocumentId{UINT64_MAX, 0x0123456789abcdefULL},
                                               DocumentSnapshot::capture(model));
}
IdentifiedDocumentSnapshot fixture() { return snapshot(allKinds()); }
TDF_Label feature(const Handle(TDocStd_Document) & doc, int position) {
    Handle(TDF_Reference) value;
    require(
        doc->Main().FindChild(20).FindChild(position).FindAttribute(TDF_Reference::GetID(), value),
        "Missing feature reference");
    return value->Get();
}
Handle(TDataStd_RealArray) array(const TDF_Label& label) {
    Handle(TDataStd_RealArray) value;
    require(label.FindAttribute(TDataStd_RealArray::GetID(), value), "Missing array");
    return value;
}
void check(const Handle(TDocStd_Document) & doc, const IdentifiedDocumentSnapshot& expected) {
    require(NativePrimitiveCodec::read(doc) == expected, "Constraint snapshot changed");
}
void solverParity(const Handle(TDocStd_Application) & app) {
    for (bool conflicting : {false, true}) {
        auto sketch = geometry();
        sketch.constraints.values = {
            {SketchConstraintId{1}, SketchConstraintKind::distance, ref(9), {}, 5},
            {SketchConstraintId{2}, SketchConstraintKind::horizontal, ref(9), {}}};
        if (conflicting)
            sketch.constraints.values.emplace_back(
                SketchConstraintId{3}, SketchConstraintKind::vertical, ref(9), std::nullopt);
        auto doc = fresh(app);
        const auto expected = snapshot(sketch);
        NativePrimitiveCodec::write(doc, expected);
        std::stringstream bytes(std::ios::in | std::ios::out | std::ios::binary);
        save(app, doc, bytes);
        auto loaded = open(app, bytes);
        check(loaded, expected);
        auto restored = std::get<SketchParameters>(
            NativePrimitiveCodec::read(loaded).snapshot().parameters()[1]);
        const auto before = features::solveSketchConstraints(sketch);
        const auto after = features::solveSketchConstraints(restored);
        require(
            before.status == after.status && before.degreesOfFreedom == after.degreesOfFreedom &&
                before.conflictingConstraints == after.conflictingConstraints && sketch == restored,
            "Native round trip changed solver result");
        require(after.status == (conflicting ? features::SketchSolveStatus::conflicting
                                             : features::SketchSolveStatus::success),
                "Unexpected fixture solver status");
        app->Close(loaded);
        app->Close(doc);
    }
}
void regressions(const Handle(TDocStd_Application) & app) {
    solverParity(app);
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
    const auto payload = feature(doc, 2).FindChild(4);
    const auto constraints = payload.FindChild(3);
    const auto first = constraints.FindChild(1);
    doc->SetUndoLimit(10);
    const auto corrupt = [&](auto mutate) {
        doc->NewCommand();
        mutate();
        rejected([&] { (void)NativePrimitiveCodec::read(doc); });
        doc->AbortCommand();
        check(doc, expected);
    };
    for (const char* id : {"0", "01", "101", "18446744073709551614", "18446744073709551615"})
        corrupt([&] { TDataStd_AsciiString::Set(first.FindChild(1), id); });
    corrupt([&] { TDataStd_AsciiString::Set(payload.FindChild(2), "100"); });
    corrupt([&] { TDataStd_AsciiString::Set(first.FindChild(2), "future"); });
    for (int count : {-1, 0, 29, 2000001})
        corrupt([&] { TDataStd_Integer::Set(constraints, count); });
    for (int point : {-1, 0, 4})
        corrupt([&] { TDataStd_Integer::Set(first.FindChild(3), point); });
    corrupt([&] { first.FindChild(3).ForgetAttribute(TDF_Reference::GetID()); });
    corrupt([&] { TDF_Reference::Set(first.FindChild(3), doc->Main()); });
    corrupt([&] {
        TDF_Reference::Set(first.FindChild(3),
                           feature(other, 2).FindChild(4).FindChild(10).FindChild(1));
    });
    corrupt([&] {
        TDF_Reference::Set(first.FindChild(3),
                           feature(doc, 3).FindChild(4).FindChild(10).FindChild(1));
    });
    corrupt([&] { TDF_Reference::Set(first.FindChild(4), payload.FindChild(10).FindChild(1)); });
    corrupt([&] { TDataStd_Integer::Set(first.FindChild(4), 1); });
    corrupt([&] { array(first.FindChild(6))->SetValue(2, 0.5); });
    corrupt(
        [&] { array(first.FindChild(6))->SetValue(1, std::numeric_limits<double>::quiet_NaN()); });
    corrupt([&] { array(first.FindChild(6))->Init(0, 1); });
    corrupt([&] { TDataStd_Integer::Set(first.FindChild(7), 4); });
    corrupt([&] { TDataStd_RealArray::Set(first.FindChild(7), 1, 1); });
    corrupt([&] { array(constraints.FindChild(28).FindChild(7))->SetValue(2, 2); });
    corrupt([&] { array(constraints.FindChild(27).FindChild(7))->SetValue(2, -1); });
    corrupt([&] { array(constraints.FindChild(20).FindChild(6))->SetValue(2, 4); });
    corrupt(
        [&] { constraints.FindChild(18).FindChild(5).ForgetAttribute(TDF_Reference::GetID()); });
    rejected([&] { (void)detail::readNativeSketchGeometry(payload, 33); });
    auto invalid = allKinds();
    invalid.constraints.values[0].kind = static_cast<SketchConstraintKind>(99);
    rejected([&] { detail::validateNativeSketchConstraints(invalid); });
    invalid = allKinds();
    invalid.constraints.values[0].first.point = static_cast<SketchPointPosition>(99);
    rejected([&] { detail::validateNativeSketchConstraints(invalid); });
    auto candidate = fresh(app);
    rejected([&] { NativePrimitiveCodec::write(candidate, snapshot(invalid)); });
    require(candidate->Main().NbChildren() == 0, "Invalid constraint write modified root");
    app->Close(candidate);
    doc->NewCommand();
    array(constraints.FindChild(4).FindChild(6))->SetValue(1, 2.5);
    require(doc->CommitCommand(), "Constraint edit failed");
    const auto edited = NativePrimitiveCodec::read(doc);
    require(edited != expected && edited.id() == expected.id() && doc->Undo() &&
                NativePrimitiveCodec::read(doc) == expected && doc->Redo() &&
                NativePrimitiveCodec::read(doc) == edited && doc->Undo(),
            "Constraint history lost identity");
    doc->NewCommand();
    first.FindChild(3).ForgetAttribute(TDF_Reference::GetID());
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
        std::cout << "PASS native sketch constraints: kinds, references, values, history, solver "
                     "parity\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
