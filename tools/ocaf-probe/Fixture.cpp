// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "Fixture.hpp"
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BinDrivers.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedMap.hxx>
#include <PCDM_ReaderStatus.hxx>
#include <PCDM_StoreStatus.hxx>
#include <TDF_Reference.hxx>
#include <TDataStd_AsciiString.hxx>
#include <TDataStd_Integer.hxx>
#include <TDataStd_Name.hxx>
#include <TDataStd_Real.hxx>
#include <TNaming_Builder.hxx>
#include <TNaming_Iterator.hxx>
#include <TNaming_NamedShape.hxx>
#include <TNaming_Tool.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace probe {
namespace {
constexpr const char* documentId = "779d0b24-02ba-47f5-9d74-3261f62f5f86";
constexpr const char* bodyId = "39f37c74-024e-4eb9-8b52-a1590f3589d4";
constexpr const char* featureId = "1e55f614-a86a-4360-b944-04211a6fdf55";
TCollection_ExtendedString title() {
    return TCollection_ExtendedString("Völund D08 – Prüfkörper", true);
}
TDF_Label body(const Handle(TDocStd_Document) & doc) { return doc->Main().FindChild(10, false); }
TDF_Label feature(const Handle(TDocStd_Document) & doc) { return body(doc).FindChild(20, false); }
template <class Attribute> Handle(Attribute) attribute(const TDF_Label& label) {
    Handle(Attribute) value;
    require(!label.IsNull() && label.FindAttribute(Attribute::GetID(), value),
            "Required attribute missing");
    return value;
}
void checkId(const TDF_Label& label, const char* expected) {
    require(attribute<TDataStd_AsciiString>(label)->Get().IsEqual(expected), "Identity changed");
}
void checkShape(const TopoDS_Shape& shape, double width) {
    require(!shape.IsNull() && BRepCheck_Analyzer(shape).IsValid(), "Invalid BRep");
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    require(std::abs(properties.Mass() - width * 5.0 * 3.0) < 1e-8, "Volume changed");
    Bnd_Box bounds;
    BRepBndLib::AddOptimal(shape, bounds, false, false);
    double x0, y0, z0, x1, y1, z1;
    bounds.Get(x0, y0, z0, x1, y1, z1);
    require(std::abs(x0) < 1e-8 && std::abs(y0) < 1e-8 && std::abs(z0) < 1e-8 &&
                std::abs(x1 - width) < 1e-8 && std::abs(y1 - 5) < 1e-8 && std::abs(z1 - 3) < 1e-8,
            "Bounds changed");
    for (const auto& [kind, count] :
         {std::pair{TopAbs_SOLID, 1}, std::pair{TopAbs_FACE, 6}, std::pair{TopAbs_EDGE, 12}}) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> shapes;
        TopExp::MapShapes(shape, kind, shapes);
        require(shapes.Extent() == count, "Topology count changed");
    }
}
} // namespace
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
Handle(TDocStd_Application) application() {
    Handle(TDocStd_Application) app = new TDocStd_Application;
    BinDrivers::DefineFormat(app);
    return app;
}
Handle(TDocStd_Document) create(const Handle(TDocStd_Application) & app) {
    Handle(TDocStd_Document) doc;
    app->NewDocument("BinOcaf", doc);
    require(!doc.IsNull(), "NewDocument failed");
    doc->SetUndoLimit(10);
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    doc->NewCommand();
    const auto root = doc->Main();
    TDataStd_Integer::Set(root, 1); // Probe schema only, never a product format number.
    TDataStd_AsciiString::Set(root, documentId);
    TDataStd_Name::Set(root, title());
    const auto bodyLabel = root.FindChild(10);
    TDataStd_AsciiString::Set(bodyLabel, bodyId);
    const auto featureLabel = bodyLabel.FindChild(20);
    TDataStd_AsciiString::Set(featureLabel, featureId);
    TDataStd_Name::Set(featureLabel, TCollection_ExtendedString("Box D08"));
    TDataStd_Real::Set(featureLabel.FindChild(1), 10);
    TDF_Reference::Set(featureLabel.FindChild(2), bodyLabel);
    TNaming_Builder(featureLabel.FindChild(3)).Generated(BRepPrimAPI_MakeBox(10, 5, 3).Shape());
    require(doc->CommitCommand(), "Initial transaction failed");
    check(doc, 10);
    change(doc, 20);
    check(doc, 20);
    require(doc->Undo(), "Initial undo failed");
    check(doc, 10);
    require(doc->Redo(), "Initial redo failed");
    check(doc, 20);
    return doc;
}
void check(const Handle(TDocStd_Document) & doc, double width) {
    require(attribute<TDataStd_Integer>(doc->Main())->Get() == 1, "Unsupported probe schema");
    checkId(doc->Main(), documentId);
    checkId(body(doc), bodyId);
    checkId(feature(doc), featureId);
    require(attribute<TDataStd_Name>(doc->Main())->Get().IsEqual(title()), "Unicode title changed");
    require(attribute<TDataStd_Name>(feature(doc))
                ->Get()
                .IsEqual(TCollection_ExtendedString("Box D08")),
            "Feature name changed");
    require(attribute<TDataStd_Real>(feature(doc).FindChild(1, false))->Get() == width,
            "Parameter changed");
    require(attribute<TDF_Reference>(feature(doc).FindChild(2, false))->Get() == body(doc),
            "Reference changed");
    const auto named = attribute<TNaming_NamedShape>(feature(doc).FindChild(3, false));
    checkShape(TNaming_Tool::GetShape(named), width);
    if (width == 20) {
        TNaming_Iterator history(named);
        require(history.More() && history.IsModification(), "Naming modification history missing");
        checkShape(history.OldShape(), 10);
        checkShape(history.NewShape(), 20);
        history.Next();
        require(!history.More(), "Unexpected naming history entry");
    }
}
void change(const Handle(TDocStd_Document) & doc, double width) {
    const auto label = feature(doc).FindChild(3, false);
    const auto old = TNaming_Tool::GetShape(attribute<TNaming_NamedShape>(label));
    doc->NewCommand();
    TDataStd_Real::Set(feature(doc).FindChild(1, false), width);
    TNaming_Builder(label).Modify(old, BRepPrimAPI_MakeBox(width, 5, 3).Shape());
    require(doc->CommitCommand(), "Edit transaction failed");
}
void verifyHistory(const Handle(TDocStd_Document) & doc) {
    doc->SetUndoLimit(10);
    change(doc, 30);
    check(doc, 30);
    require(doc->Undo(), "Loaded undo failed");
    check(doc, 20);
    require(doc->Redo(), "Loaded redo failed");
    check(doc, 30);
    require(doc->Undo(), "Restore baseline failed");
    check(doc, 20);
}
void save(const Handle(TDocStd_Application) & app, const Handle(TDocStd_Document) & doc,
          const std::filesystem::path& path) {
    require(doc->StorageFormatVersion() == TDocStd_FormatVersion_VERSION_12,
            "Unpinned writer version");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    require(output.is_open(), "Could not create output stream");
    require(app->SaveAs(doc, output) == PCDM_SS_OK, "SaveAs stream failed");
    output.flush();
    require(output.good(), "Output flush failed");
    output.close();
    require(!output.fail(), "Output close failed");
}
Handle(TDocStd_Document)
    read(const Handle(TDocStd_Application) & app, const std::filesystem::path& path) {
    Handle(TDocStd_Document) doc;
    std::ifstream input(path, std::ios::binary);
    require(input.is_open(), "Could not open input stream");
    const auto status = app->Open(input, doc);
    if (status != PCDM_RS_OK || doc.IsNull())
        throw std::runtime_error("Open stream failed with status " +
                                 std::to_string(static_cast<int>(status)));
    check(doc, 20);
    require(doc->GetAvailableUndos() == 0, "Unexpected persisted undo stack");
    doc->ChangeStorageFormatVersion(TDocStd_FormatVersion_VERSION_12);
    return doc;
}
} // namespace probe
