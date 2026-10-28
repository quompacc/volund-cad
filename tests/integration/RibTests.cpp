// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <BRepClass3d_SolidClassifier.hxx>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// N12b (schema 51, packet acceptance): a box 40 x 30 x 20 shelled 2 inward with its top open
// (7152 mm³; the cavity runs x 2..38, y 2..28, z 2..20). A sketch on the construction plane
// y = 15 holds a line across the top at z = 20. Growing upwards the rib meets nothing and is
// refused; flipped it fills the cavity down to the floor between the side walls: 36 x 18 x 2 =
// 1296 more, one body. A 4 mm rib adds 2592. Undo and Save/Open keep it.
namespace {
using namespace voelund;
using application::CommandStatus;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}
bool solidAt(const application::CadSession& session, double x, double y, double z) {
    for (const auto& body : session.validBodies())
        if (BRepClass3d_SolidClassifier(body.shape, gp_Pnt(x, y, z), 1e-7).State() == TopAbs_IN)
            return true;
    return false;
}

struct Shell final {
    application::CadSession session;
    document::FeatureId box{1};
    document::FeatureId sketch{1};
};
void make(Shell& made) {
    auto& session = made.session;
    session.newDocument();
    made.box = *session.createBoxFeature({40, 30, 20}).feature();
    for (const auto& face : session.supportFaces(made.box))
        if (face.plane.normalZ > 0.5) {
            const std::array top{face.origin};
            require(session.createShell(made.box, top, 2, false).status() ==
                        CommandStatus::committed,
                    "Shell");
        }
    // The XZ plane's normal points to -Y: 15 against it is y = 15.
    const auto plane = *session
                            .createDatumPlane(document::DatumPlane::xz, std::nullopt, std::nullopt,
                                              -15, document::DatumRotationAxis::none, 0)
                            .feature();
    document::SketchParameters sketch{*session.datumFrame(plane), {}};
    sketch.constructionPlane = plane;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchLine{{0, 20}, {40, 20}}});
    sketch.elements.nextId = 2;
    require(near(sketch.plane.originY, 15) &&
                session.createDatumSketchFeature(sketch).status() == CommandStatus::committed,
            "A line across the top on y = 15");
    made.sketch = session.features().back().id;
}

void rib() {
    Shell made;
    make(made);
    auto& session = made.session;
    require(near(volume(session), 7152), "The open shell");
    const document::BodyId body{made.box.value()};
    require(session.createRib(made.sketch, body, {2, false}).status() != CommandStatus::committed,
            "Growing upwards the rib meets nothing");
    require(session.createRib(made.sketch, body, {2, true}).status() == CommandStatus::committed &&
                session.validBodies().size() == 1 && near(volume(session), 7152 + 1296) &&
                solidAt(session, 20, 15, 10) && !solidAt(session, 20, 17, 10),
            "Flipped: 36 x 18 x 2 joined into the shell");
    const auto ribId = session.features().back().id;
    auto parameters = session.identifiedSnapshot().snapshot().extrusionParameters(ribId);
    parameters.rib->thickness = 4;
    require(session.editExtrusionParameters(ribId, parameters).status() ==
                    CommandStatus::committed &&
                near(volume(session), 7152 + 2592),
            "4 mm thick: 2592");
    require(session.undo().status() == CommandStatus::committed &&
                near(volume(session), 7152 + 1296),
            "Undo returns to 2 mm");
    const auto path = test::temporaryFilePath("voelund-n12b-rib.volund");
    session.saveDocument(path);
    application::CadSession loaded;
    loaded.loadDocument(path);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    require(near(volume(loaded), 7152 + 1296) &&
                loaded.identifiedSnapshot().snapshot().extrusionParameters(ribId).rib ==
                    session.identifiedSnapshot().snapshot().extrusionParameters(ribId).rib,
            "Save/Open keeps the rib");
}
} // namespace

int main() {
    try {
        rib();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS N12b rib in a shell: refused upwards, flipped 1296, 4 mm, undo, open\n";
    return 0;
}
