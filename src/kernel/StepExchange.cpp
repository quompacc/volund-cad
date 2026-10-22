#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/StepExchange.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <BinTools.hxx>
#include <DESTEP_Parameters.hxx>
#include <GProp_GProps.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <IGESCAFControl_Writer.hxx>
#include <IGESControl_Controller.hxx>
#include <Interface_Static.hxx>
#include <NCollection_Sequence.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <STEPCAFControl_Writer.hxx>
#include <STEPControl_Reader.hxx>
#include <ShapeFix_Shape.hxx>
#include <Standard_Failure.hxx>
#include <TCollection_AsciiString.hxx>
#include <TCollection_ExtendedString.hxx>
#include <TDF_Label.hxx>
#include <TDataStd_Name.hxx>
#include <TDocStd_Document.hxx>
#include <TopExp_Explorer.hxx>
#include <TopLoc_Location.hxx>
#include <UnitsMethods_LengthUnit.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>

#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>

namespace voelund::kernel {
namespace {

// OCCT's STEP translation keeps global state (static parameters, the work session's set-up);
// one translation at a time.
std::mutex& stepMutex() {
    static std::mutex mutex;
    return mutex;
}

// An XCAF document in millimetres for one translation, closed again at the end.
class XcafDocument final {
  public:
    XcafDocument() : application_(XCAFApp_Application::GetApplication()) {
        application_->NewDocument("MDTV-XCAF", document_);
        XCAFDoc_DocumentTool::SetLengthUnit(document_, 1.0, UnitsMethods_LengthUnit_Millimeter);
    }
    ~XcafDocument() {
        if (!document_.IsNull())
            application_->Close(document_);
    }
    XcafDocument(const XcafDocument&) = delete;
    XcafDocument& operator=(const XcafDocument&) = delete;
    [[nodiscard]] const occ::handle<TDocStd_Document>& get() const noexcept { return document_; }
    [[nodiscard]] occ::handle<XCAFDoc_ShapeTool> shapes() const {
        return XCAFDoc_DocumentTool::ShapeTool(document_->Main());
    }

  private:
    occ::handle<XCAFApp_Application> application_;
    occ::handle<TDocStd_Document> document_;
};

std::string labelName(const TDF_Label& label) {
    occ::handle<TDataStd_Name> name;
    if (!label.FindAttribute(TDataStd_Name::GetID(), name))
        return {};
    return TCollection_AsciiString(name->Get()).ToCString(); // UTF-8
}

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

// A valid solid of positive volume, turned outward when it came inside out; null otherwise.
TopoDS_Shape usableSolid(TopoDS_Shape shape) {
    if (shape.IsNull() || shape.ShapeType() != TopAbs_SOLID || !BRepCheck_Analyzer(shape).IsValid())
        return {};
    if (volumeOf(shape) < 0.0)
        shape.Reverse();
    return volumeOf(shape) > 1e-9 ? shape : TopoDS_Shape{};
}

// A solid as read, or healed once when it is not valid.
TopoDS_Shape healed(const TopoDS_Shape& solid) {
    if (auto usable = usableSolid(solid); !usable.IsNull())
        return usable;
    ShapeFix_Shape fix(solid);
    fix.Perform();
    TopExp_Explorer fixed(fix.Shape(), TopAbs_SOLID);
    return fixed.More() ? usableSolid(fixed.Current()) : TopoDS_Shape{};
}

// Walks the products of the file: an assembly's components with their placements, a part's
// solids under its name. Anything that is no solid is counted, not taken.
class Collector final {
  public:
    explicit Collector(StepImport& result) : result_(result) {}

    void product(const TDF_Label& label, const TopLoc_Location& location, const std::string& name,
                 int depth) {
        if (depth > 64) // no file nests that deep; guards against reference cycles
            return;
        if (XCAFDoc_ShapeTool::IsAssembly(label)) {
            NCollection_Sequence<TDF_Label> components;
            XCAFDoc_ShapeTool::GetComponents(label, components, false);
            for (int index = 1; index <= components.Length(); ++index) {
                const TDF_Label component = components.Value(index);
                TDF_Label referred;
                if (!XCAFDoc_ShapeTool::GetReferredShape(component, referred))
                    continue;
                const auto own = labelName(referred);
                product(referred, location * XCAFDoc_ShapeTool::GetLocation(component),
                        own.empty() ? labelName(component) : own, depth + 1);
            }
            return;
        }
        part(XCAFDoc_ShapeTool::GetShape(label).Moved(location), name);
    }

  private:
    void part(const TopoDS_Shape& shape, const std::string& name) {
        if (shape.IsNull())
            return;
        std::size_t taken = 0;
        for (TopExp_Explorer solids(shape, TopAbs_SOLID); solids.More(); solids.Next()) {
            const auto solid = healed(solids.Current());
            if (solid.IsNull()) {
                ++result_.invalidSolids;
                continue;
            }
            // Further solids of one product are numbered after it.
            ++taken;
            result_.solids.push_back(
                {solid, taken == 1 || name.empty() ? name : name + " " + std::to_string(taken)});
        }
        for (TopExp_Explorer loose(shape, TopAbs_SHELL, TopAbs_SOLID); loose.More(); loose.Next())
            ++result_.skippedShapes;
        for (TopExp_Explorer loose(shape, TopAbs_FACE, TopAbs_SHELL); loose.More(); loose.Next())
            ++result_.skippedShapes;
        for (TopExp_Explorer loose(shape, TopAbs_EDGE, TopAbs_FACE); loose.More(); loose.Next())
            ++result_.skippedShapes;
    }

    StepImport& result_;
};

} // namespace

StepImport readStep(const std::filesystem::path& path) {
    StepImport result;
    const auto fail = [&](StepImportError error, std::string message) {
        result.solids.clear();
        result.error = error;
        result.message = std::move(message);
        return result;
    };
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return fail(StepImportError::unreadable, "The file cannot be opened");
    const std::scoped_lock lock(stepMutex());
    try {
        XcafDocument document;
        STEPCAFControl_Reader reader;
        reader.SetNameMode(true);
        reader.SetColorMode(false);
        reader.SetLayerMode(false);
        if (reader.ReadStream("step", stream) != IFSelect_RetDone)
            return fail(StepImportError::unreadable, "The file holds no STEP data OCCT can read");
        NCollection_Sequence<TCollection_AsciiString> lengths, angles, solidAngles;
        reader.ChangeReader().FileUnits(lengths, angles, solidAngles);
        if (!lengths.IsEmpty())
            result.sourceUnit = lengths.First().ToCString();
        if (!reader.Transfer(document.get()))
            return fail(StepImportError::unreadable, "The STEP data cannot be translated");
        NCollection_Sequence<TDF_Label> roots;
        document.shapes()->GetFreeShapes(roots);
        Collector collector(result);
        for (int index = 1; index <= roots.Length(); ++index)
            collector.product(roots.Value(index), TopLoc_Location(), labelName(roots.Value(index)),
                              0);
    } catch (const Standard_Failure& failure) {
        return fail(StepImportError::unreadable,
                    "The STEP data cannot be translated: " + occtFailureDiagnostic(failure));
    }
    if (result.solids.empty())
        return fail(StepImportError::noSolids,
                    "The file holds no valid solid (" + std::to_string(result.skippedShapes) +
                        " other shapes, " + std::to_string(result.invalidSolids) +
                        " invalid solids)");
    return result;
}

namespace {
// Why the solids cannot be written, if so.
std::optional<StepExport> refusal(std::span<const StepSolid> solids) {
    if (solids.empty())
        return StepExport{StepExportError::nothingToWrite, "No solid to export"};
    for (const auto& item : solids)
        if (item.solid.IsNull() || item.solid.ShapeType() != TopAbs_SOLID ||
            !BRepCheck_Analyzer(item.solid).IsValid())
            return StepExport{StepExportError::notASolid,
                              "\"" + item.name + "\" is no valid solid"};
    return std::nullopt;
}

StepExport writeBytes(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << bytes;
    file.close();
    if (!file)
        return {StepExportError::writeFailed, "The file cannot be written"};
    return {};
}
} // namespace

StepExport writeStep(const std::filesystem::path& path, std::span<const StepSolid> solids,
                     StepLengthUnit unit) {
    StepExport result;
    const auto fail = [&](StepExportError error, std::string message) {
        result.error = error;
        result.message = std::move(message);
        return result;
    };
    if (auto refused = refusal(solids))
        return *refused;
    std::ostringstream buffer;
    {
        const std::scoped_lock lock(stepMutex());
        try {
            XcafDocument document;
            const auto shapes = document.shapes();
            for (const auto& item : solids) {
                const TDF_Label label = shapes->AddShape(item.solid, false);
                if (!item.name.empty())
                    TDataStd_Name::Set(label, TCollection_ExtendedString(item.name.c_str(), true));
            }
            DESTEP_Parameters parameters;
            parameters.InitFromStatic();
            parameters.WriteUnit = unit == StepLengthUnit::inch
                                       ? UnitsMethods_LengthUnit_Inch
                                       : UnitsMethods_LengthUnit_Millimeter;
            STEPCAFControl_Writer writer;
            writer.SetNameMode(true);
            writer.SetColorMode(false);
            writer.SetLayerMode(false);
            if (!writer.Transfer(document.get(), parameters, STEPControl_AsIs) ||
                writer.WriteStream(buffer) != IFSelect_RetDone)
                return fail(StepExportError::writeFailed,
                            "The solids cannot be translated to STEP");
        } catch (const Standard_Failure& failure) {
            return fail(StepExportError::writeFailed, "The solids cannot be translated to STEP: " +
                                                          occtFailureDiagnostic(failure));
        }
    }
    return writeBytes(path, buffer.str());
}

StepExport writeIges(const std::filesystem::path& path, std::span<const StepSolid> solids) {
    if (auto refused = refusal(solids))
        return *refused;
    std::ostringstream buffer;
    {
        const std::scoped_lock lock(stepMutex());
        try {
            IGESControl_Controller::Init();
            Interface_Static::SetCVal("write.iges.unit", "MM");
            Interface_Static::SetIVal("write.iges.brep.mode", 1);
            XcafDocument document;
            const auto shapes = document.shapes();
            for (const auto& item : solids) {
                const TDF_Label label = shapes->AddShape(item.solid, false);
                if (!item.name.empty())
                    TDataStd_Name::Set(label, TCollection_ExtendedString(item.name.c_str(), true));
            }
            IGESCAFControl_Writer writer;
            writer.SetNameMode(true);
            writer.SetColorMode(false);
            writer.SetLayerMode(false);
            if (!writer.Transfer(document.get()) || !writer.Write(buffer))
                return {StepExportError::writeFailed, "The solids cannot be translated to IGES"};
        } catch (const Standard_Failure& failure) {
            return {StepExportError::writeFailed,
                    "The solids cannot be translated to IGES: " + occtFailureDiagnostic(failure)};
        }
    }
    return writeBytes(path, buffer.str());
}

std::string writeShapeBytes(const TopoDS_Shape& shape) {
    std::ostringstream stream(std::ios::binary);
    // A fixed format version, so OCCT 7.9 and 8 write the same.
    BinTools::Write(shape, stream, false, false, BinTools_FormatVersion_VERSION_3);
    return std::move(stream).str();
}

TopoDS_Shape readShapeBytes(std::string_view bytes) {
    std::istringstream stream(std::string(bytes), std::ios::binary);
    TopoDS_Shape shape;
    try {
        BinTools::Read(shape, stream);
    } catch (const Standard_Failure&) {
        return {};
    }
    if (shape.IsNull() || shape.ShapeType() != TopAbs_SOLID || !BRepCheck_Analyzer(shape).IsValid())
        return {};
    return shape;
}

} // namespace voelund::kernel
