// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepAdaptor_Curve.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <array>
#include <cmath>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// User reports 2026-10-25: faces merged by a seamless join (schema 39) carry the names of the
// faces they came from; their edges are still picked for a fillet or a chamfer, on a plate
// continued by a block and on a tray (ring and floor joined, outer corners rounded).
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool valid(const application::CadSession& session) {
    for (const auto& body : session.validBodies())
        if (!BRepCheck_Analyzer(body.shape).IsValid())
            return false;
    return !session.validBodies().empty();
}
// The face of `support` whose outward normal points along the axis with this sign.
document::SubshapeOrigin face(const application::CadSession& session, document::FeatureId support,
                              double nx, double ny, double nz) {
    for (const auto& candidate : session.supportFaces(support))
        if (std::abs(candidate.plane.normalX - nx) < 1e-9 &&
            std::abs(candidate.plane.normalY - ny) < 1e-9 &&
            std::abs(candidate.plane.normalZ - nz) < 1e-9)
            return candidate.origin;
    throw std::runtime_error("Face with this normal missing");
}

// User report 2026-10-25: after a seamless join (schema 39) the bottom face carries the names of
// both extrusions; its edges must still be picked for a fillet or a chamfer.
void mergedFaceEdges() {
    for (const bool chamfer : {false, true}) {
        application::CadSession session;
        session.newDocument();
        const auto sketchOf = [](double x0, double x1) {
            document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy),
                                              {}};
            sketch.datum = document::DatumPlane::xy;
            sketch.elements.values.push_back(
                {document::SketchElementId{1}, document::SketchRectangle{x0, -20, x1, 20}});
            sketch.elements.nextId = 2;
            return sketch;
        };
        const auto plate = sketchOf(-40, 40);
        require(session.createDatumSketchExtrusion(
                           plate, features::identifySketchRegions(plate.elements.values).ids, 10)
                        .status() == CommandStatus::committed,
                "Plate");
        const document::BodyId body{session.features().front().id.value()};
        const auto block = sketchOf(40, 60);
        require(session.createDatumSketchExtrusion(
                           block, features::identifySketchRegions(block.elements.values).ids, 10,
                           {document::ExtrusionOperation::join, false, body})
                        .status() == CommandStatus::committed,
                "Seamless join");
        const auto support = session.features().back().id;
        std::optional<document::FilletEdge> picked;
        for (TopExp_Explorer explorer(session.validBodies().front().shape, TopAbs_EDGE);
             explorer.More() && !picked; explorer.Next()) {
            const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
            const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
            if (std::abs(middle.Y() + 20) < 1e-6 && std::abs(middle.Z()) < 1e-6)
                picked = session.filletEdge(support, explorer.Current());
        }
        require(picked.has_value(), "The bottom front edge of the joined plate is picked");
        const auto made = chamfer ? session.createChamfer(support, std::span(&*picked, 1), 1)
                                  : session.createFillet(support, std::span(&*picked, 1), 1);
        require(made.status() == CommandStatus::committed && valid(session),
                std::string(chamfer ? "Chamfer" : "Fillet") + " at the merged bottom face");
    }
}

// User report 2026-10-25 (the real model): a tray - the ring of a rectangle with a frame
// extruded 10 mm, its inner area 3 mm joined (no seam), the four outer corners rounded - takes
// a fillet or a chamfer at its bottom edge.
std::optional<document::FilletEdge> bottomFrontEdge(application::CadSession& session,
                                                    document::FeatureId support) {
    for (TopExp_Explorer explorer(session.validBodies().front().shape, TopAbs_EDGE);
         explorer.More(); explorer.Next()) {
        const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
        const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
        if (std::abs(middle.Y() + 20) < 1e-6 && std::abs(middle.Z()) < 1e-6 &&
            std::abs(middle.X()) < 1e-6)
            return session.filletEdge(support, explorer.Current());
    }
    throw std::runtime_error("The tray has no bottom front edge");
}

void trayBottomEdge() {
    for (const bool chamfer : {false, true}) {
        application::CadSession session;
        session.newDocument();
        document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
        sketch.datum = document::DatumPlane::xy;
        sketch.elements.values = {
            {document::SketchElementId{1}, document::SketchRectangle{-40, -20, 40, 20}},
            {document::SketchElementId{2}, document::SketchRectangle{-36, -16, 36, 16}}};
        sketch.elements.nextId = 3;
        const auto sketchId = *session.createDatumSketchFeature(sketch).feature();
        require(session.selectSketchProfileAt(sketchId, {0, -18, 0}, false), "The ring");
        const std::vector<document::SketchRegionId> ring(session.selectedSketchProfiles().begin(),
                                                         session.selectedSketchProfiles().end());
        require(session.createSketchProfileExtrusion(sketchId, ring, 10).status() ==
                    CommandStatus::committed,
                "Ring 10 mm");
        const document::BodyId body{sketchId.value()};
        require(session.selectSketchProfileAt(sketchId, {0, 0, 0}, false), "The inner area");
        const std::vector<document::SketchRegionId> inner(session.selectedSketchProfiles().begin(),
                                                          session.selectedSketchProfiles().end());
        application::ExtrusionMode join;
        join.operation = document::ExtrusionOperation::join;
        join.target = body;
        const auto joined = session.createSketchProfileExtrusion(sketchId, inner, 3, join);
        require(joined.status() == CommandStatus::committed,
                "Floor 3 mm joined: " + joined.diagnostic());
        auto support = session.features().back().id;
        const auto edgeOf = [&](std::array<double, 3> a, std::array<double, 3> b) {
            return document::FilletEdge{face(session, support, a[0], a[1], a[2]),
                                        face(session, support, b[0], b[1], b[2])};
        };
        const std::vector<document::FilletEdge> corners{
            edgeOf({1, 0, 0}, {0, 1, 0}), edgeOf({1, 0, 0}, {0, -1, 0}),
            edgeOf({-1, 0, 0}, {0, 1, 0}), edgeOf({-1, 0, 0}, {0, -1, 0})};
        require(session.createFillet(support, corners, 5).status() == CommandStatus::committed,
                "The outer corners rounded R 5");
        support = session.features().back().id;
        // Every outer bottom edge, straight or round at a corner, is picked.
        int bottom = 0, unpicked = 0;
        for (TopExp_Explorer explorer(session.validBodies().front().shape, TopAbs_EDGE);
             explorer.More(); explorer.Next()) {
            const BRepAdaptor_Curve curve(TopoDS::Edge(explorer.Current()));
            const auto middle = curve.Value((curve.FirstParameter() + curve.LastParameter()) / 2);
            if (std::abs(middle.Z()) > 1e-6 ||
                (std::abs(middle.X()) < 36.5 && std::abs(middle.Y()) < 16.5))
                continue;
            ++bottom;
            unpicked += !session.filletEdge(support, explorer.Current()).has_value();
        }
        require(bottom > 0 && unpicked == 0, std::to_string(unpicked) + " of " +
                                                 std::to_string(bottom) +
                                                 " outer bottom edges are not picked");
        const auto picked = bottomFrontEdge(session, support);
        require(picked.has_value(), "The tray's bottom front edge is picked");
        const auto made = chamfer ? session.createChamfer(support, std::span(&*picked, 1), 1)
                                  : session.createFillet(support, std::span(&*picked, 1), 1);
        require(made.status() == CommandStatus::committed && valid(session),
                std::string(chamfer ? "Chamfer" : "Fillet") +
                    " at the tray's bottom: " + made.diagnostic());
    }
}

} // namespace

int main() {
    try {
        mergedFaceEdges();
        trayBottomEdge();
        std::cout << "PASS fillet and chamfer at faces merged by a seamless join\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
