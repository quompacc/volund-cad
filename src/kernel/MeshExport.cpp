#include "ZipStore.hpp"

#include <voelund/kernel/MeshExport.hpp>

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>

#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <map>
#include <numbers>
#include <stdexcept>

namespace voelund::kernel {
namespace {

// Joins the vertices faces repeat along their shared edges: points closer than `step` (in every
// coordinate) are one vertex.
class VertexWeld final {
  public:
    explicit VertexWeld(std::vector<std::array<double, 3>>& vertices) : vertices_(vertices) {}

    std::uint32_t add(const gp_Pnt& point) {
        const Key key{cell(point.X()), cell(point.Y()), cell(point.Z())};
        for (long long dx = -1; dx <= 1; ++dx)
            for (long long dy = -1; dy <= 1; ++dy)
                for (long long dz = -1; dz <= 1; ++dz)
                    if (const auto found = cells_.find({key[0] + dx, key[1] + dy, key[2] + dz});
                        found != cells_.end() && near(vertices_[found->second], point))
                        return found->second;
        const auto index = static_cast<std::uint32_t>(vertices_.size());
        vertices_.push_back({point.X(), point.Y(), point.Z()});
        cells_.emplace(key, index);
        return index;
    }

  private:
    using Key = std::array<long long, 3>;
    static constexpr double step = 1e-6;
    static long long cell(double value) { return std::llround(value / step); }
    static bool near(const std::array<double, 3>& vertex, const gp_Pnt& point) {
        return std::abs(vertex[0] - point.X()) <= step && std::abs(vertex[1] - point.Y()) <= step &&
               std::abs(vertex[2] - point.Z()) <= step;
    }

    std::vector<std::array<double, 3>>& vertices_;
    std::map<Key, std::uint32_t> cells_;
};

// The triangles of one meshed face, turned outward with the face's orientation.
void addFace(const TopoDS_Face& face, VertexWeld& weld, Mesh& mesh) {
    TopLoc_Location location;
    const auto triangulation = BRep_Tool::Triangulation(face, location);
    if (triangulation.IsNull())
        throw std::invalid_argument("A face could not be meshed");
    const gp_Trsf transformation = location.Transformation();
    std::vector<std::uint32_t> nodes(static_cast<std::size_t>(triangulation->NbNodes()) + 1);
    for (int node = 1; node <= triangulation->NbNodes(); ++node)
        nodes[static_cast<std::size_t>(node)] =
            weld.add(triangulation->Node(node).Transformed(transformation));
    const bool reversed = face.Orientation() == TopAbs_REVERSED;
    for (int index = 1; index <= triangulation->NbTriangles(); ++index) {
        int first = 0;
        int second = 0;
        int third = 0;
        triangulation->Triangle(index).Get(first, second, third);
        if (reversed)
            std::swap(second, third);
        const std::array<std::uint32_t, 3> triangle{nodes[static_cast<std::size_t>(first)],
                                                    nodes[static_cast<std::size_t>(second)],
                                                    nodes[static_cast<std::size_t>(third)]};
        if (triangle[0] != triangle[1] && triangle[1] != triangle[2] && triangle[0] != triangle[2])
            mesh.triangles.push_back(triangle);
    }
}

void putFloat(std::string& out, double value) {
    const auto bits = std::bit_cast<std::uint32_t>(static_cast<float>(value));
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((bits >> shift) & 0xFFU));
}

std::string number(double value) {
    char buffer[32];
    const auto written = std::to_chars(buffer, buffer + sizeof buffer, value);
    return {buffer, written.ptr};
}

std::string escaped(const std::string& text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out.push_back(c);
        }
    }
    return out;
}

void appendObject(std::string& model, const Mesh& mesh, std::size_t id) {
    model += "  <object id=\"" + std::to_string(id) + "\" type=\"model\" name=\"" +
             escaped(mesh.name) + "\">\n   <mesh>\n    <vertices>\n";
    for (const auto& vertex : mesh.vertices)
        model += "     <vertex x=\"" + number(vertex[0]) + "\" y=\"" + number(vertex[1]) +
                 "\" z=\"" + number(vertex[2]) + "\"/>\n";
    model += "    </vertices>\n    <triangles>\n";
    for (const auto& triangle : mesh.triangles)
        model += "     <triangle v1=\"" + std::to_string(triangle[0]) + "\" v2=\"" +
                 std::to_string(triangle[1]) + "\" v3=\"" + std::to_string(triangle[2]) + "\"/>\n";
    model += "    </triangles>\n   </mesh>\n  </object>\n";
}

} // namespace

MeshTolerance meshTolerance(MeshResolution resolution) noexcept {
    constexpr double degree = std::numbers::pi / 180.0;
    switch (resolution) {
    case MeshResolution::coarse:
        return {0.1, 30 * degree};
    case MeshResolution::fine:
        return {0.005, 5 * degree};
    case MeshResolution::medium:
        break;
    }
    return {0.025, 15 * degree};
}

Mesh meshSolid(const TopoDS_Shape& solid, MeshResolution resolution, std::string name) {
    if (solid.IsNull() || solid.ShapeType() != TopAbs_SOLID || !BRepCheck_Analyzer(solid).IsValid())
        throw std::invalid_argument("The shape is no valid solid");
    // A copy, so the document's shapes keep the triangulation the view gave them.
    TopoDS_Shape copy = BRepBuilderAPI_Copy(solid, true, false).Shape();
    BRepTools::Clean(copy);
    const auto tolerance = meshTolerance(resolution);
    BRepMesh_IncrementalMesh meshing(copy, tolerance.chordal, false, tolerance.angle, true);
    if (!meshing.IsDone())
        throw std::invalid_argument("The solid could not be meshed");
    Mesh mesh{std::move(name), {}, {}};
    VertexWeld weld(mesh.vertices);
    for (TopExp_Explorer explorer(copy, TopAbs_FACE); explorer.More(); explorer.Next())
        addFace(TopoDS::Face(explorer.Current()), weld, mesh);
    return mesh;
}

std::string stlBytes(std::span<const Mesh> meshes) {
    std::string out = "Voelund CAD binary STL, millimetres";
    out.resize(80, ' ');
    std::size_t count = 0;
    for (const auto& mesh : meshes)
        count += mesh.triangles.size();
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((count >> shift) & 0xFFU));
    for (const auto& mesh : meshes)
        for (const auto& triangle : mesh.triangles) {
            const auto& a = mesh.vertices[triangle[0]];
            const auto& b = mesh.vertices[triangle[1]];
            const auto& c = mesh.vertices[triangle[2]];
            const gp_Vec u(b[0] - a[0], b[1] - a[1], b[2] - a[2]);
            const gp_Vec v(c[0] - a[0], c[1] - a[1], c[2] - a[2]);
            gp_Vec normal = u.Crossed(v);
            if (normal.Magnitude() > 0)
                normal.Normalize();
            for (const double value : {normal.X(), normal.Y(), normal.Z()})
                putFloat(out, value);
            for (const auto* vertex : {&a, &b, &c})
                for (const double value : *vertex)
                    putFloat(out, value);
            out.append(2, '\0'); // attribute byte count
        }
    return out;
}

std::string threeMfBytes(std::span<const Mesh> meshes) {
    std::string model = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<model unit=\"millimeter\" xml:lang=\"de-DE\" "
                        "xmlns=\"http://schemas.microsoft.com/3dmanufacturing/core/2015/02\">\n"
                        " <resources>\n";
    for (std::size_t index = 0; index < meshes.size(); ++index)
        appendObject(model, meshes[index], index + 1);
    model += " </resources>\n <build>\n";
    for (std::size_t index = 0; index < meshes.size(); ++index)
        model += "  <item objectid=\"" + std::to_string(index + 1) + "\"/>\n";
    model += " </build>\n</model>\n";
    const std::string types =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" "
        "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"model\" "
        "ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/></Types>\n";
    const std::string relations =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Target=\"/3D/3dmodel.model\" Id=\"rel0\" "
        "Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\"/>"
        "</Relationships>\n";
    const detail::ZipEntry entries[] = {
        {"[Content_Types].xml", types}, {"_rels/.rels", relations}, {"3D/3dmodel.model", model}};
    return detail::zipStore(entries);
}

MeshExport writeMesh(const std::filesystem::path& path, std::span<const StepSolid> solids,
                     MeshFormat format, MeshResolution resolution) {
    if (solids.empty())
        return {MeshExportError::nothingToWrite, "No solid to write"};
    std::vector<Mesh> meshes;
    std::size_t triangles = 0;
    for (const auto& solid : solids) {
        try {
            meshes.push_back(meshSolid(solid.solid, resolution, solid.name));
        } catch (const std::invalid_argument& error) {
            return {MeshExportError::notASolid, "\"" + solid.name + "\": " + error.what()};
        }
        triangles += meshes.back().triangles.size();
    }
    const auto bytes = format == MeshFormat::stl ? stlBytes(meshes) : threeMfBytes(meshes);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file)
        return {MeshExportError::writeFailed, "The file could not be written"};
    return {std::nullopt, {}, triangles};
}

} // namespace voelund::kernel
