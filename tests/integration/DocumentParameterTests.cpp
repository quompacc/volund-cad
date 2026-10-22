// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// PAR01 acceptance: two sketches and one extrusion bound to A = 10 mm; A -> 20 mm changes
// every dimension and volume in one undo step; renaming keeps bindings; a used parameter
// cannot be deleted; save/open is identical; an unsolvable result is a visible error.
namespace {
using namespace voelund;
using application::CommandStatus;
using application::RejectionReason;
using document::DatumPlane;
using document::ParameterId;
using document::ParameterUnit;
using document::SketchConstraintId;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
void committed(const application::CommandResult& result, const std::string& message) {
    require(result.status() == CommandStatus::committed, message + ": " + result.diagnostic());
}
document::FeatureId created(const application::CommandResult& result) {
    require(result.status() == CommandStatus::committed && result.feature().has_value(),
            "Command was not committed: " + result.diagnostic());
    return *result.feature();
}
void invalid(const application::CommandResult& result, const std::string& message) {
    require(result.status() == CommandStatus::rejected &&
                result.reason() == RejectionReason::invalidInput,
            message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }

// A circle with a radius dimension (constraint 1) and, optionally, a fixed diameter (2).
document::SketchParameters circle(DatumPlane datum, double x, double radius,
                                  std::optional<double> diameter = std::nullopt) {
    document::SketchParameters sketch{
        document::datumSketchPlane(datum),
        {{{document::SketchElementId{1}, document::SketchCircle{{x, 0}, radius}}}, 2}};
    sketch.datum = datum;
    const document::SketchGeometryReference center{document::SketchElementId{1},
                                                   document::SketchPointPosition::center};
    sketch.constraints.values.emplace_back(SketchConstraintId{1},
                                           document::SketchConstraintKind::radius, center,
                                           std::nullopt, radius);
    if (diameter)
        sketch.constraints.values.emplace_back(SketchConstraintId{2},
                                               document::SketchConstraintKind::diameter, center,
                                               std::nullopt, *diameter);
    sketch.constraints.nextId = sketch.constraints.values.size() + 1;
    return sketch;
}
document::FeatureId extrude(application::CadSession& session, document::FeatureId sketch,
                            double distance) {
    require(session.selectFeature(sketch) &&
                session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "Select the circle region");
    const std::vector<document::SketchRegionId> regions(session.selectedSketchProfiles().begin(),
                                                        session.selectedSketchProfiles().end());
    return created(session.createSketchProfileExtrusion(sketch, regions, distance));
}
ParameterId create(application::CadSession& session, const std::string& name, ParameterUnit unit,
                   double value) {
    committed(session.createParameter(name, unit, value), "Create " + name);
    const auto* parameter = document::findParameter(session.parameterTable(), name);
    require(parameter != nullptr && parameter->value == value, "Created parameter " + name);
    return parameter->id;
}
double radiusOf(const application::CadSession& session, document::FeatureId sketch) {
    const auto parameters = session.sketchParameters(sketch);
    return std::get<document::SketchCircle>(parameters.elements.values.front().geometry).radius;
}
bool allCurrent(const application::CadSession& session) {
    return std::ranges::all_of(session.featureStates(), [](const auto& state) {
        return state.status == application::FeatureRuntimeStatus::current;
    });
}

void acceptance() {
    application::CadSession session;
    session.newDocument();
    const auto a = create(session, "A", ParameterUnit::length, 10);
    const auto first = created(session.createDatumSketchFeature(circle(DatumPlane::xy, 0, 10)));
    const auto second = created(session.createDatumSketchFeature(circle(DatumPlane::xz, 60, 10)));
    const auto extrusion = extrude(session, first, 10);
    static_cast<void>(extrude(session, second, 5));
    committed(session.bindSketchDimension(first, SketchConstraintId{1}, a), "Bind first");
    committed(session.bindSketchDimension(second, SketchConstraintId{1}, a), "Bind second");
    committed(session.bindExtrusionDistance(extrusion, a), "Bind extrusion");
    require(session.parameterUseCount(a) == 3,
            "Three uses of A, found " + std::to_string(session.parameterUseCount(a)));
    const double before = std::numbers::pi * (100 * 10 + 100 * 5);
    require(near(test::volumeOf(session.currentShape()), before), "Volume with A = 10");

    committed(session.setParameterValue(a, 20), "A -> 20");
    const double after = std::numbers::pi * (400 * 20 + 400 * 5);
    require(near(radiusOf(session, first), 20) && near(radiusOf(session, second), 20) &&
                session.sketchParameters(first).constraints.values.front().value == 20 &&
                session.identifiedSnapshot().snapshot().extrusionParameters(extrusion).distance ==
                    20,
            "Every bound dimension follows A");
    require(near(test::volumeOf(session.currentShape()), after) && allCurrent(session),
            "Volumes follow A");
    committed(session.undo(), "Undo A");
    require(near(test::volumeOf(session.currentShape()), before) &&
                document::findParameter(session.parameterTable(), a)->value == 10 &&
                near(radiusOf(session, second), 10),
            "One undo step restores A = 10 and every dimension");
    committed(session.redo(), "Redo A");
    require(near(test::volumeOf(session.currentShape()), after), "Redo restores A = 20");

    committed(session.renameParameter(a, "Breite"), "Rename");
    require(document::findParameter(session.parameterTable(), "breite")->id == a &&
                session.parameterUseCount(a) == 3,
            "Renaming keeps the bindings");
    const auto b = create(session, "B", ParameterUnit::length, 5);
    invalid(session.renameParameter(b, "BREITE"), "Duplicate name ignoring case accepted");
    for (const char* name : {"2x", "mm", "Pi", "a-b", ""})
        invalid(session.createParameter(name, ParameterUnit::length, 1),
                std::string("Invalid name accepted: ") + name);
    const auto angle = create(session, "W", ParameterUnit::angle, 30);
    invalid(session.bindSketchDimension(first, SketchConstraintId{1}, angle),
            "Angle parameter bound to a radius");
    invalid(session.bindExtrusionDistance(extrusion, angle), "Angle bound to a distance");

    const auto table = session.parameterTable();
    invalid(session.deleteParameter(a), "A used parameter was deleted");
    require(session.parameterTable() == table, "Refused deletion changed the table");
    committed(session.deleteParameter(b), "Unused parameter deleted");

    const auto path = std::filesystem::temp_directory_path() / "voelund-parameters.volund";
    session.saveDocument(path);
    application::CadSession reopened;
    reopened.loadDocument(path);
    std::filesystem::remove(path);
    const auto saved = session.identifiedSnapshot().snapshot();
    const auto loaded = reopened.identifiedSnapshot().snapshot();
    require(loaded.parameterTable() == saved.parameterTable() &&
                std::ranges::equal(loaded.parameters(), saved.parameters()) &&
                near(test::volumeOf(reopened.currentShape()), after) && allCurrent(reopened),
            "Save/open keeps table, bindings and volumes");
    committed(reopened.setParameterValue(a, 15), "Reopened bindings are live");
    require(near(radiusOf(reopened, second), 15), "Reopened binding follows");

    // Unbinding keeps the current value as a fixed one; the parameter is then free.
    committed(session.bindSketchDimension(first, SketchConstraintId{1}, std::nullopt), "Unbind");
    committed(session.bindSketchDimension(second, SketchConstraintId{1}, std::nullopt), "Unbind");
    committed(session.deleteParameter(a, application::ParameterDeletion::unbindUses),
              "Delete after switching the remaining use to its fixed value");
    require(near(radiusOf(session, second), 20) &&
                session.identifiedSnapshot().snapshot().extrusionParameters(extrusion).distance ==
                    20 &&
                !session.identifiedSnapshot()
                     .snapshot()
                     .extrusionParameters(extrusion)
                     .distanceParameter,
            "Fixed values remain after unbinding");
    std::cout << "PAR01 binding, one-step change, rename, delete guard and save/open passed\n";
}

void unsolvableIsVisible() {
    application::CadSession session;
    session.newDocument();
    const auto r = create(session, "R", ParameterUnit::length, 10);
    const auto sketch =
        created(session.createDatumSketchFeature(circle(DatumPlane::xy, 0, 10, 20.0)));
    committed(session.bindSketchDimension(sketch, SketchConstraintId{1}, r), "Bind radius");
    const auto result = session.setParameterValue(r, 15);
    require(result.status() == CommandStatus::committedWithErrors,
            "A radius conflicting with the fixed diameter is committed with errors");
    require(std::ranges::any_of(session.featureStates(),
                                [&](const auto& state) {
                                    return state.id == sketch &&
                                           state.status == application::FeatureRuntimeStatus::error;
                                }),
            "The unsolvable sketch shows as an error");
    require(session.sketchParameters(sketch).constraints.values.front().value == 15,
            "The new value is kept, nothing is discarded silently");
    const auto rejectedChange =
        session.setParameterValue(r, 12, application::ErrorDisposition::reject);
    require(rejectedChange.status() == CommandStatus::rejected,
            "Reject disposition refuses the unsolvable change");
    committed(session.undo(), "Undo the unsolvable change");
    require(allCurrent(session), "Undo restores the solvable sketch");
    std::cout << "PAR01 unsolvable change is committed with a visible error\n";
}
} // namespace

int main() {
    try {
        acceptance();
        unsolvableIsVisible();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
