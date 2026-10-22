// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/application/Actions.hpp>

#include <TopExp_Explorer.hxx>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

// M03 (A-EXT-02): join, cut/pocket and intersect of base-plane sketches on a 40 x 30 x 10
// box (body A). A through hole r3 removes 90 pi, a blind 10 x 8 x 4 pocket 320 mm^3. A join
// that touches a face is one solid; a separate join, a cut that misses or only touches, a
// cut of the whole body and an intersect without common volume are refused with their code,
// and A stays unchanged. A cut that splits A keeps both pieces in A. Preview and commit agree.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::ExtrusionMode;
using document::DatumPlane;
using document::ExtrusionOperation;
using document::FeatureId;
using document::SketchElementId;
constexpr double pi = std::numbers::pi;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

document::BodyId boxA(application::CadSession& session) {
    session.newDocument();
    const auto box = session.createBoxFeature({40, 30, 10});
    require(box.status() == CommandStatus::committed, "Box A");
    return document::BodyId{box.feature()->value()};
}

struct Fixture final {
    application::CadSession session;
    document::BodyId a{boxA(session)};
    [[nodiscard]] double volume() const {
        for (const auto& body : session.validBodies())
            if (body.bodyId == a)
                return test::volumeOf(body.shape);
        throw std::runtime_error("Body A missing");
    }
    [[nodiscard]] int solids() const {
        int count = 0;
        for (const auto& body : session.validBodies())
            if (body.bodyId == a)
                for (TopExp_Explorer it(body.shape, TopAbs_SOLID); it.More(); it.Next())
                    ++count;
        return count;
    }
    application::CreateProfileExtrusionAction action(document::SketchGeometry geometry,
                                                     ExtrusionOperation operation, double distance,
                                                     double offset = 0) {
        document::SketchParameters sketch{
            document::datumSketchPlane(DatumPlane::xy),
            document::SketchElements{{{SketchElementId{1}, std::move(geometry)}}, 2}};
        sketch.datum = DatumPlane::xy;
        const auto created = session.createDatumSketchFeature(sketch);
        require(created.status() == CommandStatus::committed, "Sketch");
        const auto id = *created.feature();
        require(session.selectFeature(id) &&
                    session.selectSketchProfileByElement(SketchElementId{1}, false),
                "Region");
        ExtrusionMode mode;
        mode.operation = operation;
        mode.target = a;
        mode.offset = offset;
        return {id,
                {session.selectedSketchProfiles().begin(), session.selectedSketchProfiles().end()},
                {distance, application::QuantityUnit::millimetre},
                mode};
    }
    application::ActionResult run(const application::CreateProfileExtrusionAction& action) {
        return application::executeAction(session, application::prepareRequest(session, action));
    }
    // Refused with `code`; A keeps its volume.
    void refused(const application::CreateProfileExtrusionAction& action, const std::string& code) {
        const double before = volume();
        const auto result = run(action).command;
        bool coded = false;
        for (const auto& issue : result.evaluation().features)
            coded = coded || issue.code == code;
        require(result.status() == CommandStatus::rejected && coded && near(volume(), before),
                "Refused as " + code + ": " + result.diagnostic());
    }
};

document::SketchRectangle rect(double x0, double y0, double x1, double y1) {
    return {x0, y0, x1, y1};
}

void cutsAndPocket() {
    Fixture f;
    const double box = 40 * 30 * 10;
    const auto hole = f.action(document::SketchCircle{{20, 15}, 3}, ExtrusionOperation::cut, 10);
    const auto preview =
        application::previewAction(f.session, application::prepareRequest(f.session, hole));
    double previewed = 0;
    for (const auto& body : preview.bodies)
        if (body.bodyId == f.a)
            previewed = test::volumeOf(body.shape);
    require(f.run(hole).command.status() == CommandStatus::committed &&
                near(f.volume(), box - 90 * pi) && near(previewed, f.volume()),
            "Through hole r3 removes 90 pi; preview and commit agree");
    // A blind pocket from the top: 10 x 8 x 4, z 6..10.
    require(f.run(f.action(rect(5, 5, 15, 13), ExtrusionOperation::cut, 4, 6)).command.status() ==
                    CommandStatus::committed &&
                near(f.volume(), box - 90 * pi - 320),
            "Blind pocket removes 320");
}

void joinsAndRefusals() {
    Fixture f;
    const double box = 40 * 30 * 10;
    f.refused(f.action(rect(100, 0, 110, 10), ExtrusionOperation::join, 10), "join-disjoint");
    f.refused(f.action(rect(40, 0, 50, 10), ExtrusionOperation::cut, 10), "cut-no-material");
    f.refused(f.action(rect(100, 0, 110, 10), ExtrusionOperation::cut, 10), "cut-no-material");
    f.refused(f.action(rect(-1, -1, 41, 31), ExtrusionOperation::cut, 10), "cut-whole-body");
    f.refused(f.action(rect(100, 0, 110, 10), ExtrusionOperation::intersect, 10),
              "intersect-empty");
    // A join that shares a face is one solid.
    require(f.run(f.action(rect(40, 0, 50, 10), ExtrusionOperation::join, 10)).command.status() ==
                    CommandStatus::committed &&
                near(f.volume(), box + 1000) && f.solids() == 1,
            "A touching join is one solid");
    // A cut through the middle splits A into two solids; both stay in body A.
    require(f.run(f.action(rect(18, -1, 22, 31), ExtrusionOperation::cut, 10)).command.status() ==
                    CommandStatus::committed &&
                near(f.volume(), box + 1000 - 4 * 30 * 10) && f.solids() == 2 &&
                f.session.validBodies().size() == 1,
            "A splitting cut keeps both pieces in A");
}
} // namespace

int main() {
    try {
        cutsAndPocket();
        joinsAndRefusals();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS M03 join, cut, pocket, intersect (A-EXT-02)\n";
    return 0;
}
