// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/ModelExport.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

// N03 (user decisions 2026-10-22): a plate "Platte" 40 x 20 x 10 and a cylinder of its own.
// STL of the visible bodies holds both in one file with their volume within the resolution; 3MF
// of the picked plate holds one object "Platte" in millimetres. With every body hidden nothing
// is written; a revision with an error is refused and writes no file. N04: IGES of both bodies
// with the plate's name; DXF of a sketch, refused for a body.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::StepExportStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

std::string bytesOf(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// The volume a binary STL encloses.
double stlVolume(const std::string& bytes) {
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + 80, 4);
    require(bytes.size() == 84 + 50 * std::size_t{count}, "STL size");
    double total = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        std::array<float, 9> v{};
        std::memcpy(v.data(), bytes.data() + 84 + 50 * std::size_t{index} + 12, sizeof v);
        total += (v[0] * (v[4] * v[8] - v[5] * v[7]) - v[1] * (v[3] * v[8] - v[5] * v[6]) +
                  v[2] * (v[3] * v[7] - v[4] * v[6])) /
                 6.0;
    }
    return total;
}

std::size_t count(const std::string& text, const std::string& part) {
    std::size_t found = 0;
    for (auto at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
        ++found;
    return found;
}

document::FeatureId buildPart(application::CadSession& session) {
    session.newDocument();
    const auto plate = session.createBoxFeature({40, 20, 10}).feature();
    require(plate && session.renameBody(document::BodyId{plate->value()}, "Platte").status() ==
                         CommandStatus::committed,
            "Plate named");
    require(session.createNewBodyCylinder().status() == CommandStatus::committed &&
                session.validBodies().size() == 2,
            "A cylinder of its own");
    return *plate;
}

void written(application::CadSession& session, document::FeatureId plate,
             const std::filesystem::path& directory) {
    double exact = 0;
    for (const auto& body : session.validBodies())
        exact += test::volumeOf(body.shape);
    const auto stl = application::exportMesh(
        session, directory / "teil.stl", kernel::MeshFormat::stl, kernel::MeshResolution::medium);
    const double meshed = stlVolume(bytesOf(directory / "teil.stl"));
    require(stl.status == StepExportStatus::written && stl.solids == 2 &&
                std::abs(meshed - exact) < 0.01 * exact && meshed > 8000,
            "STL of both visible bodies: " + std::to_string(meshed) + " of " +
                std::to_string(exact));
    const document::BodyId picked[] = {document::BodyId{plate.value()}};
    const auto threeMf =
        application::exportMesh(session, directory / "platte.3mf", kernel::MeshFormat::threeMf,
                                kernel::MeshResolution::fine, picked);
    const auto package = bytesOf(directory / "platte.3mf");
    require(threeMf.status == StepExportStatus::written && threeMf.solids == 1 &&
                count(package, "<object ") == 1 &&
                package.find("name=\"Platte\"") != std::string::npos &&
                package.find("unit=\"millimeter\"") != std::string::npos,
            "3MF of the picked plate: one object named Platte, in mm");
}

// N04: IGES of the visible bodies; DXF of a sketch with a rectangle and a circle, refused for
// the plate, which is no sketch.
void igesAndDxf(application::CadSession& session, document::FeatureId plate,
                const std::filesystem::path& directory) {
    const auto iges = application::exportIges(session, directory / "teil.igs");
    const auto igesText = bytesOf(directory / "teil.igs");
    require(iges.status == StepExportStatus::written && iges.solids == 2 &&
                igesText.find("Platte") != std::string::npos,
            "IGES of both visible bodies with the plate's name: " + iges.message);
    document::SketchParameters drawing{document::datumSketchPlane(document::DatumPlane::xy), {}};
    drawing.datum = document::DatumPlane::xy;
    drawing.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{0, 0, 30, 20}});
    drawing.elements.values.push_back(
        {document::SketchElementId{2}, document::SketchCircle{{15, 10}, 4}});
    drawing.elements.nextId = 3;
    const auto sketch = session.createDatumSketchFeature(drawing).feature();
    require(sketch.has_value(), "A sketch");
    const auto dxf = application::exportSketchDxf(session, directory / "skizze.dxf", *sketch);
    const auto dxfText = bytesOf(directory / "skizze.dxf");
    require(dxf.status == StepExportStatus::written && dxf.solids == 2 &&
                count(dxfText, "\nLINE\n") == 4 && count(dxfText, "\nCIRCLE\n") == 1,
            "DXF of the sketch: four lines and a circle");
    require(application::exportSketchDxf(session, directory / "platte.dxf", plate).status ==
                    StepExportStatus::notASketch &&
                !std::filesystem::exists(directory / "platte.dxf"),
            "The plate is no sketch");
    require(session.undo().status() == CommandStatus::committed, "Sketch undone");
}

void refused(application::CadSession& session, document::FeatureId plate,
             const std::filesystem::path& directory) {
    std::optional<document::FilletEdge> edge;
    for (TopExp_Explorer explorer(*session.featureShape(plate), TopAbs_EDGE); explorer.More();
         explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (std::abs(middle.Y()) < 1e-9 && std::abs(middle.Z() - 10) < 1e-9)
            edge = session.filletEdge(plate, explorer.Current());
    }
    require(edge.has_value(), "An edge of the plate");
    const auto fillet = session.createFillet(plate, std::span{&*edge, 1}, 2);
    auto parameters = session.filletParameters(*fillet.feature());
    parameters.radius = 50;
    require(session.editFilletParameters(*fillet.feature(), parameters,
                                         application::ErrorDisposition::keepEditableErrors)
                    .status() == CommandStatus::committedWithErrors,
            "A radius too large is kept with its error");
    const auto faulty = application::exportMesh(
        session, directory / "fehler.stl", kernel::MeshFormat::stl, kernel::MeshResolution::medium);
    require(faulty.status == StepExportStatus::invalidRevision &&
                !std::filesystem::exists(directory / "fehler.stl"),
            "A revision with an error is not exported");
    require(session.undo().status() == CommandStatus::committed, "Undo the radius");
    for (const auto& body : session.modelOutline().bodies)
        require(session.setBodyHidden(body.id, true), "Hidden");
    require(application::exportMesh(session, directory / "leer.stl", kernel::MeshFormat::stl,
                                    kernel::MeshResolution::medium)
                    .status == StepExportStatus::noBodies,
            "Nothing visible, nothing written");
}
} // namespace

int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / "voelund-n03-Ä";
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        application::CadSession session;
        const auto plate = buildPart(session);
        written(session, plate, directory);
        igesAndDxf(session, plate, directory);
        refused(session, plate, directory);
        std::filesystem::remove_all(directory);
        std::cout << "PASS N03 mesh export from the session: STL of the visible bodies, 3MF of "
                     "the picked one, faulty revision and nothing visible refused\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
