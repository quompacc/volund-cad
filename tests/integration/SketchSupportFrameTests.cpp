#include "CadSessionTestSupport.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <BRepClass3d_SolidClassifier.hxx>
#include <gp_Pnt.hxx>

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// S03: a sketch on every planar face of a box turned by 30 degrees about Z. Provenance keeps
// three of its faces reversed against their occurrence in the solid; binding and evaluation
// must accept the outward frame, and a joined extrusion grows the box outwards.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

document::FeatureId turnedBox(application::CadSession& session) {
    session.newDocument();
    require(session.createBoxFeature({40, 20, 30}).status() == CommandStatus::committed, "Box");
    auto box = session.selectedBoxParameters();
    box.placement.rotationZ = kernel::toRadians(kernel::Degrees(30));
    require(session.editSelectedBoxParameters(box).status() == CommandStatus::committed,
            "Turn the box");
    return session.features().front().id;
}

bool inside(const TopoDS_Shape& shape, const gp_Pnt& point) {
    BRepClass3d_SolidClassifier classifier(shape, point, 1e-7);
    return classifier.State() == TopAbs_IN;
}

void everyFace() {
    application::CadSession probe;
    const auto faceCount = probe.supportFaces(turnedBox(probe)).size();
    require(faceCount == 6, "The turned box offers six planar faces");
    for (std::size_t index = 0; index < faceCount; ++index) {
        const std::string name = "face " + std::to_string(index);
        application::CadSession session;
        const auto box = turnedBox(session);
        const double boxVolume = test::volumeOf(session.currentShape());
        const auto face = session.supportFaces(box)[index];
        auto sketch = session.sketchOnFace(box, face.origin);
        const double cx = (face.minX + face.maxX) / 2, cy = (face.minY + face.maxY) / 2;
        sketch.elements = {{{document::SketchElementId{1},
                             document::SketchRectangle{cx - 5, cy - 5, cx + 5, cy + 5}}},
                           2};
        const auto created = session.createSketchFeature(box, sketch);
        require(created.status() == CommandStatus::committed && created.feature().has_value(),
                "Sketch on " + name + ": " + created.diagnostic());
        const auto& p = session.sketchParameters(*created.feature()).plane;
        require(near(p.normalX, face.plane.normalX) && near(p.normalY, face.plane.normalY) &&
                    near(p.normalZ, face.plane.normalZ),
                "The sketch keeps the outward normal of " + name);
        // The centre of the profile, 2.5 mm out of the face.
        const double yx = p.normalY * p.xDirectionZ - p.normalZ * p.xDirectionY;
        const double yy = p.normalZ * p.xDirectionX - p.normalX * p.xDirectionZ;
        const double yz = p.normalX * p.xDirectionY - p.normalY * p.xDirectionX;
        const gp_Pnt outside(p.originX + cx * p.xDirectionX + cy * yx + 2.5 * p.normalX,
                             p.originY + cx * p.xDirectionY + cy * yy + 2.5 * p.normalY,
                             p.originZ + cx * p.xDirectionZ + cy * yz + 2.5 * p.normalZ);
        require(!inside(session.currentShape(), outside), name + ": probe starts outside");
        const auto regions = features::identifySketchRegions(
                                 session.sketchParameters(*created.feature()).elements.values)
                                 .ids;
        const auto extruded = session.createSketchProfileExtrusion(*created.feature(), regions, 5);
        require(extruded.status() == CommandStatus::committed, "Join on " + name);
        require(near(test::volumeOf(session.currentShape()), boxVolume + 500) &&
                    inside(session.currentShape(), outside),
                "The extrusion on " + name + " grows the box outwards");
        std::cout << name << " n(" << p.normalX << ", " << p.normalY << ", " << p.normalZ
                  << "): sketch, join outwards\n";
    }
}
} // namespace

int main() {
    try {
        everyFace();
        std::cout << "PASS S03 sketch on every face of a turned box\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
