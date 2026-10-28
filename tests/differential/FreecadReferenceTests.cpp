// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

// Q01: the FreeCAD reference parts (tools/reference-freecad/make_references.py) rebuilt with
// Völund's own features. Same volume, area, centroid and face count as FreeCAD's result shows
// that fillet, chamfer, hole and mirror land where they should and remove what they should.
//
//   voelund_freecad_reference_tests <table> <known differences>

#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>
#include <voelund/features/SketchSupportBinding.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopExp.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace voelund;
using application::CommandStatus;

document::FeatureId committed(const application::CommandResult& result, const std::string& what) {
    if (result.status() != CommandStatus::committed || !result.feature())
        throw std::runtime_error(what + " was not committed");
    return *result.feature();
}

// The planar face of `support` whose outward normal is (nx, ny, nz), with its frame.
features::SupportFaceFrame face(const application::CadSession& session, document::FeatureId support,
                                double nx, double ny, double nz) {
    for (const auto& candidate : session.supportFaces(support))
        if (!candidate.secondary && std::abs(candidate.plane.normalX - nx) < 1e-9 &&
            std::abs(candidate.plane.normalY - ny) < 1e-9 &&
            std::abs(candidate.plane.normalZ - nz) < 1e-9)
            return candidate;
    throw std::runtime_error("No face with this normal");
}

document::FilletEdge edge(const application::CadSession& session, document::FeatureId support,
                          std::array<double, 3> first, std::array<double, 3> second) {
    return {face(session, support, first[0], first[1], first[2]).origin,
            face(session, support, second[0], second[1], second[2]).origin};
}

// A hole in the top face whose axis meets it at the world point (x, y).
struct TopHole {
    double x, y, diameter;
    std::optional<double> depth;
};

document::FeatureId hole(application::CadSession& session, document::FeatureId support,
                         const TopHole& spec) {
    const auto top = face(session, support, 0, 0, 1);
    const auto& plane = top.plane;
    const double dx = spec.x - plane.originX;
    const double dy = spec.y - plane.originY;
    // Face frame coordinates; its y axis is normal x xDirection with the normal +Z.
    const double x = dx * plane.xDirectionX + dy * plane.xDirectionY;
    const double y = dx * -plane.xDirectionY + dy * plane.xDirectionX;
    return committed(session.createHole(support, top.origin, x, y, spec.diameter, spec.depth),
                     "Hole");
}

void plate(application::CadSession& session) {
    const auto box = committed(session.createBoxFeature({80, 50, 12}), "Box");
    std::vector<document::FilletEdge> corners;
    for (const double sx : {1.0, -1.0})
        for (const double sy : {1.0, -1.0})
            corners.push_back(edge(session, box, {sx, 0, 0}, {0, sy, 0}));
    const auto rounded = committed(session.createFillet(box, corners, 6), "Fillet");
    (void)hole(session, rounded, {40, 25, 10, std::nullopt});
}

void blindHole(application::CadSession& session) {
    const auto box = committed(session.createBoxFeature({60, 40, 20}), "Box");
    (void)hole(session, box, {15, 20, 8, 6.0});
}

void chamfered(application::CadSession& session) {
    const auto box = committed(session.createBoxFeature({40, 30, 20}), "Box");
    std::vector<document::FilletEdge> top;
    for (const auto& side :
         std::vector<std::array<double, 3>>{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}})
        top.push_back(edge(session, box, {0, 0, 1}, side));
    (void)committed(session.createChamfer(box, top, 2), "Chamfer");
}

void mirrored(application::CadSession& session) {
    const auto box = committed(session.createBoxFeature({30, 20, 10}), "Box");
    const auto corner = edge(session, box, {1, 0, 0}, {0, 1, 0});
    const auto rounded = committed(session.createFillet(box, std::span(&corner, 1), 4), "Fillet");
    (void)committed(session.createMirror(application::CreateMirrorAction{
                        rounded, document::DatumPlane::yz, std::nullopt, std::nullopt, std::nullopt,
                        document::MirrorMode::join}),
                    "Mirror");
}

void patterned(application::CadSession& session) {
    const auto box = committed(session.createBoxFeature({10, 10, 5}), "Box");
    application::PatternInput input;
    input.kind = document::PatternKind::linear;
    input.baseAxis = document::BaseAxis::x;
    input.count = 4;
    input.spacing = 8;
    input.mode = document::PatternMode::join;
    (void)committed(session.createPattern(box, input), "Pattern");
}

void shelled(application::CadSession& session) {
    const auto box = committed(session.createBoxFeature({40, 30, 20}), "Box");
    const auto top = face(session, box, 0, 0, 1).origin;
    (void)committed(session.createShell(box, std::span(&top, 1), 2, false), "Shell");
}

void revolved(application::CadSession& session) {
    // XZ sketch: x along +X, sketch y along +Z. The distance is unused by a revolve but must
    // be positive.
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xz), {}};
    sketch.datum = document::DatumPlane::xz;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{10, 0, 20, 15}});
    sketch.elements.nextId = 2;
    const auto identities = features::identifySketchRegions(sketch.elements.values);
    if (identities.ids.size() != 1)
        throw std::runtime_error("One rectangle region");
    application::ExtrusionMode mode;
    mode.revolve = document::RevolveParameters{
        document::RevolveAxis{std::nullopt, document::BaseAxis::z, std::nullopt},
        kernel::Radians{2 * std::numbers::pi}};
    (void)committed(session.createDatumSketchExtrusion(sketch, identities.ids, 1, mode), "Revolve");
}

const std::map<std::string, std::function<void(application::CadSession&)>> builders{
    {"plate", plate},       {"blind_hole", blindHole}, {"chamfered", chamfered},
    {"mirrored", mirrored}, {"patterned", patterned},  {"shelled", shelled},
    {"revolved", revolved}};

struct Reference {
    std::string part;
    double volume{0}, area{0}, x{0}, y{0}, z{0};
    int faces{0};
};

std::vector<Reference> readTable(const std::string& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("No reference table: " + path);
    std::vector<Reference> table;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line.front() == '#')
            continue;
        std::replace(line.begin(), line.end(), '|', ' ');
        std::stringstream fields(line);
        Reference reference;
        if (!(fields >> reference.part >> reference.volume >> reference.area >> reference.x >>
              reference.y >> reference.z >> reference.faces))
            throw std::runtime_error("Malformed reference line: " + line);
        table.push_back(reference);
    }
    if (table.empty())
        throw std::runtime_error("Empty reference table");
    return table;
}

// "part | faces N | reason": the face count Völund knowingly produces instead.
std::map<std::string, int> readKnownDifferences(const std::string& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("No list of known differences: " + path);
    std::map<std::string, int> faces;
    for (std::string line; std::getline(input, line);) {
        if (line.empty() || line.front() == '#')
            continue;
        std::replace(line.begin(), line.end(), '|', ' ');
        std::stringstream fields(line);
        std::string part, kind;
        int count = 0;
        if (!(fields >> part >> kind >> count) || kind != "faces")
            throw std::runtime_error("Malformed known difference: " + line);
        faces[part] = count;
    }
    return faces;
}

bool close(double actual, double expected, double tolerance) {
    return std::abs(actual - expected) <= tolerance * std::max(1.0, std::abs(expected));
}

std::string checkPart(Reference reference, const std::map<std::string, int>& known) {
    const auto difference = known.find(reference.part);
    if (difference != known.end())
        reference.faces = difference->second;
    const auto builder = builders.find(reference.part);
    if (builder == builders.end())
        return "unknown part";
    application::CadSession session;
    session.newDocument();
    builder->second(session);
    const auto bodies = session.validBodies();
    if (bodies.size() != 1 || !BRepCheck_Analyzer(bodies.front().shape).IsValid())
        return std::to_string(bodies.size()) + " bodies, not one valid solid";
    const auto& shape = bodies.front().shape;
    GProp_GProps volume;
    BRepGProp::VolumeProperties(shape, volume);
    GProp_GProps area;
    BRepGProp::SurfaceProperties(shape, area);
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    TopExp::MapShapes(shape, TopAbs_FACE, faces);
    const auto centre = volume.CentreOfMass();
    const double offset =
        std::hypot(centre.X() - reference.x, centre.Y() - reference.y, centre.Z() - reference.z);
    std::cout << std::setprecision(12) << reference.part << ": volume " << volume.Mass() << " ("
              << reference.volume << "), area " << area.Mass() << " (" << reference.area
              << "), centroid off by " << offset << " mm, " << faces.Extent() << " faces ("
              << reference.faces << ")\n";
    if (!close(volume.Mass(), reference.volume, 1e-7) || !close(area.Mass(), reference.area, 1e-7))
        return "volume or area differs";
    if (offset > 1e-5)
        return "centroid differs";
    if (faces.Extent() != reference.faces)
        return "face count differs";
    return {};
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "usage: voelund_freecad_reference_tests <table> <known differences>\n";
            return 2;
        }
        const auto table = readTable(argv[1]);
        const auto known = readKnownDifferences(argv[2]);
        int failed = 0;
        for (const auto& reference : table) {
            const auto problem = checkPart(reference, known);
            if (!problem.empty()) {
                std::cerr << "FAIL " << reference.part << ": " << problem << '\n';
                ++failed;
            }
        }
        if (failed != 0)
            return 1;
        std::cout << "PASS FreeCAD reference parts: " << table.size()
                  << " rebuilt with Völund features, same volume, area, centroid, faces\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
