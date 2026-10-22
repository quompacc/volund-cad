// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/MeshExport.hpp>

#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <TopExp_Explorer.hxx>
#include <gp_Pln.hxx>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>

// N03 (user decision 2026-10-22): a 10 mm cube meshes to 12 outward triangles on 8 shared
// vertices with volume 1000; a cylinder r 5, h 10 is closed at every resolution and comes
// closer to pi r^2 h from coarse to fine. Binary STL has its header, count and 50 bytes per
// triangle; 3MF is a ZIP package with content types, relations and a model in millimetres with
// one named object per solid. Nothing to write, a face and an unwritable place are refused.
namespace {
using namespace voelund::kernel;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Every edge is used once in each direction: closed and consistently oriented.
bool closed(const Mesh& mesh) {
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> edges;
    for (const auto& t : mesh.triangles)
        for (int i = 0; i < 3; ++i)
            ++edges[{t[i], t[(i + 1) % 3]}];
    for (const auto& [edge, uses] : edges) {
        const auto back = edges.find({edge.second, edge.first});
        if (uses != 1 || back == edges.end() || back->second != 1)
            return false;
    }
    return true;
}

double volume(const Mesh& mesh) {
    double total = 0;
    for (const auto& t : mesh.triangles) {
        const auto& a = mesh.vertices[t[0]];
        const auto& b = mesh.vertices[t[1]];
        const auto& c = mesh.vertices[t[2]];
        total += (a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
                  a[2] * (b[0] * c[1] - b[1] * c[0])) /
                 6.0;
    }
    return total;
}

std::uint32_t read32(const std::string& bytes, std::size_t at) {
    std::uint32_t value = 0;
    for (int i = 3; i >= 0; --i)
        value = (value << 8U) | static_cast<unsigned char>(bytes[at + static_cast<std::size_t>(i)]);
    return value;
}
std::uint32_t read16(const std::string& bytes, std::size_t at) {
    return static_cast<unsigned char>(bytes[at]) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + 1])) << 8U);
}

// The entries of a stored ZIP archive, from its local headers.
std::map<std::string, std::string> unzip(const std::string& bytes) {
    std::map<std::string, std::string> entries;
    std::size_t at = 0;
    while (read32(bytes, at) == 0x04034B50U) {
        require(read16(bytes, at + 8) == 0, "Entries are stored");
        const auto size = read32(bytes, at + 18);
        const auto nameLength = read16(bytes, at + 26);
        const auto extra = read16(bytes, at + 28);
        const std::string name = bytes.substr(at + 30, nameLength);
        entries[name] = bytes.substr(at + 30 + nameLength + extra, size);
        at += 30 + nameLength + extra + size;
    }
    require(read32(bytes, bytes.size() - 22) == 0x06054B50U, "End of central directory");
    return entries;
}

std::size_t count(const std::string& text, const std::string& part) {
    std::size_t found = 0;
    for (auto at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
        ++found;
    return found;
}

void cube() {
    const auto mesh = meshSolid(BRepPrimAPI_MakeBox(10, 10, 10).Shape(), MeshResolution::medium);
    require(mesh.triangles.size() == 12 && mesh.vertices.size() == 8 && closed(mesh) &&
                std::abs(volume(mesh) - 1000) < 1e-9,
            "Cube: 12 outward triangles on 8 vertices, volume 1000: " +
                std::to_string(volume(mesh)));
}

void cylinder() {
    const TopoDS_Shape shape = BRepPrimAPI_MakeCylinder(5, 10).Shape();
    const double exact = std::numbers::pi * 25 * 10;
    double previous = 1;
    for (const auto resolution :
         {MeshResolution::coarse, MeshResolution::medium, MeshResolution::fine}) {
        const auto mesh = meshSolid(shape, resolution);
        const double error = (exact - volume(mesh)) / exact;
        require(closed(mesh) && error >= 0 && error < previous && error < 0.05,
                "Cylinder closed, inside, closer with each step: " + std::to_string(error));
        previous = error;
    }
    require(previous < 0.003, "Fine is within 0.3 %");
}

void files() {
    const Mesh meshes[] = {
        meshSolid(BRepPrimAPI_MakeBox(10, 10, 10).Shape(), MeshResolution::medium, "Würfel"),
        meshSolid(BRepPrimAPI_MakeCylinder(5, 10).Shape(), MeshResolution::coarse, "Rohr & <Co>")};
    const auto triangles = meshes[0].triangles.size() + meshes[1].triangles.size();
    const auto stl = stlBytes(meshes);
    require(stl.size() == 84 + 50 * triangles && read32(stl, 80) == triangles,
            "Binary STL: header, count, 50 bytes a triangle");

    const auto package = unzip(threeMfBytes(meshes));
    require(package.contains("[Content_Types].xml") && package.contains("_rels/.rels") &&
                package.contains("3D/3dmodel.model"),
            "3MF package parts");
    const auto& model = package.at("3D/3dmodel.model");
    require(model.find("unit=\"millimeter\"") != std::string::npos &&
                count(model, "<object ") == 2 && count(model, "<item ") == 2 &&
                count(model, "<triangle ") == triangles &&
                model.find("name=\"Würfel\"") != std::string::npos &&
                model.find("name=\"Rohr &amp; &lt;Co&gt;\"") != std::string::npos,
            "3MF model: millimetres, two named objects, every triangle");
}

void writing() {
    const auto directory = std::filesystem::temp_directory_path() / "voelund-n03-mesh-Ä";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    const StepSolid solids[] = {{BRepPrimAPI_MakeBox(10, 10, 10).Shape(), "Würfel"}};
    const auto written =
        writeMesh(directory / "würfel.stl", solids, MeshFormat::stl, MeshResolution::medium);
    require(!written.error && written.triangles == 12 &&
                std::filesystem::file_size(directory / "würfel.stl") == 84 + 50 * 12,
            "STL file written");
    require(!writeMesh(directory / "würfel.3mf", solids, MeshFormat::threeMf, MeshResolution::fine)
                 .error,
            "3MF file written");
    require(writeMesh(directory / "leer.stl", {}, MeshFormat::stl, MeshResolution::medium).error ==
                MeshExportError::nothingToWrite,
            "Nothing to write");
    const StepSolid face[] = {{BRepBuilderAPI_MakeFace(gp_Pln(), 0, 1, 0, 1).Shape(), "Fläche"}};
    require(
        writeMesh(directory / "fläche.stl", face, MeshFormat::stl, MeshResolution::medium).error ==
            MeshExportError::notASolid,
        "A face is no solid");
    require(
        writeMesh(directory / "fehlt" / "x.stl", solids, MeshFormat::stl, MeshResolution::medium)
                .error == MeshExportError::writeFailed,
        "An unwritable place is reported");
    std::filesystem::remove_all(directory);
}
} // namespace

int main() {
    try {
        cube();
        cylinder();
        files();
        writing();
        std::cout << "PASS N03 meshes: closed outward meshes, volumes per resolution, binary STL, "
                     "3MF package with named objects, refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
