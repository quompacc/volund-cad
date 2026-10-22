// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// N01 (user decisions 2026-10-21): B = "A*2 + 4" follows A; an extrusion bound to B follows
// both. A cycle (A = "B/2") is refused with its path and changes nothing; a typed value
// replaces an expression. Renaming A rewrites B's expression; A cannot be deleted while B uses
// it. Undo restores the expression and the depth; save/open keeps the expression.
namespace {
using namespace voelund;
using application::CommandStatus;
using document::ParameterUnit;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

const document::DocumentParameter& named(const application::CadSession& session,
                                         const std::string& name) {
    for (const auto& entry : session.parameterTable().values)
        if (entry.name == name)
            return entry;
    throw std::runtime_error("Parameter " + name);
}
double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies())
        total += test::volumeOf(body.shape);
    return total;
}

document::FeatureId boundPlate(application::CadSession& session, document::ParameterId depth) {
    document::SketchParameters plate{document::datumSketchPlane(document::DatumPlane::xy), {}};
    plate.datum = document::DatumPlane::xy;
    plate.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchRectangle{0, 0, 10, 10}});
    plate.elements.nextId = 2;
    const auto sketch = session.createDatumSketchFeature(plate).feature();
    application::ExtrusionMode mode;
    mode.distanceParameter = depth;
    const auto regions = features::identifySketchRegions(plate.elements.values).ids;
    const auto extrusion = session.createSketchProfileExtrusion(*sketch, regions, 1, mode);
    require(extrusion.status() == CommandStatus::committed, "Plate bound to B");
    return *extrusion.feature();
}

void expressionsFollow(application::CadSession& session) {
    require(session.createParameter("A", ParameterUnit::length, 10).status() ==
                    CommandStatus::committed &&
                session.createParameter("B", ParameterUnit::length, 0, "", "A*2 + 4").status() ==
                    CommandStatus::committed,
            "A = 10, B = A*2 + 4");
    require(near(named(session, "B").value, 24), "B is 24");
    static_cast<void>(boundPlate(session, named(session, "B").id));
    require(near(volume(session), 100 * 24), "The plate is 24 deep");
    require(session.setParameterValue(named(session, "A").id, 20).status() ==
                    CommandStatus::committed &&
                near(named(session, "B").value, 44) && near(volume(session), 100 * 44),
            "A = 20: B is 44, the plate follows");
    require(session.undo().status() == CommandStatus::committed &&
                near(named(session, "B").value, 24) && near(volume(session), 100 * 24),
            "Undo returns to 24");
}

void cyclesAndValues(application::CadSession& session) {
    const auto revision = session.sceneStatus().document;
    const auto cycle = session.setParameterExpression(named(session, "A").id, "B/2");
    require(cycle.status() == CommandStatus::rejected &&
                cycle.diagnostic().find("Zyklus") != std::string::npos &&
                cycle.diagnostic().find("A") != std::string::npos &&
                session.sceneStatus().document == revision,
            "A cycle is refused with its path: " + cycle.diagnostic());
    const auto unknown = session.setParameterExpression(named(session, "A").id, "Q*2");
    require(unknown.status() == CommandStatus::rejected &&
                unknown.diagnostic().find("Unbekannter Parameter") != std::string::npos,
            "An unknown name is refused: " + unknown.diagnostic());
    require(session.setParameterValue(named(session, "B").id, 30).status() ==
                    CommandStatus::committed &&
                named(session, "B").expression.empty() && near(volume(session), 100 * 30),
            "A typed value replaces the expression");
    require(session.undo().status() == CommandStatus::committed &&
                named(session, "B").expression == "A*2 + 4",
            "Undo brings the expression back");
}

void renameAndDelete(application::CadSession& session) {
    require(session.renameParameter(named(session, "A").id, "Breite").status() ==
                    CommandStatus::committed &&
                named(session, "B").expression == "Breite*2 + 4",
            "Renaming rewrites B: " + named(session, "B").expression);
    const auto deletion = session.deleteParameter(named(session, "Breite").id,
                                                  application::ParameterDeletion::unbindUses);
    require(deletion.status() == CommandStatus::rejected &&
                deletion.diagnostic().find("B") != std::string::npos,
            "Breite cannot go while B uses it: " + deletion.diagnostic());
}

void savedAndOpened(application::CadSession& session) {
    const auto path = std::filesystem::temp_directory_path() / "voelund-expression-Ä.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    std::filesystem::remove(path);
    require(named(reopened, "B").expression == "Breite*2 + 4" &&
                near(named(reopened, "B").value, 24) && near(volume(reopened), 100 * 24),
            "Save/open keeps the expression");
}

} // namespace

int main() {
    try {
        application::CadSession session;
        session.newDocument();
        expressionsFollow(session);
        cyclesAndValues(session);
        renameAndDelete(session);
        savedAndOpened(session);
        std::cout << "PASS N01 parameter expressions in the session: dependents follow, bound "
                     "extrusion follows, cycle and unknown names refused, typed value replaces, "
                     "rename rewrites, delete refused, undo, save/open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
