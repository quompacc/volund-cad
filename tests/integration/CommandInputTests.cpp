// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/features/FeatureEvaluationDetail.hpp"
#include "../CommandResultAssertions.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>

namespace {
using namespace voelund;
using application::CommandStatus;
using application::RejectionReason;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class Execute> void rejectsInput(application::CadSession& session, Execute execute) {
    const auto stamp = session.sceneStatus();
    const auto shape = session.displayShape();
    const auto selected = session.selectedFeature();
    const auto count = session.features().size();
    const bool undo = session.canUndo(), redo = session.canRedo(), dirty = session.isModified();
    const auto previous = features::detail::beforeFeatureEvaluation;
    features::detail::beforeFeatureEvaluation = [](document::FeatureId) {
        throw std::runtime_error("Invalid input reached feature evaluation");
    };
    const auto result = execute();
    features::detail::beforeFeatureEvaluation = previous;
    require(result.status() == CommandStatus::rejected &&
                result.reason() == RejectionReason::invalidInput && !result.diagnostic().empty() &&
                result.evaluation().features.empty() && !result.feature(),
            "Invalid input lacks an explicit rejection and diagnostic");
    require(session.sceneStatus().document == stamp.document &&
                session.sceneStatus().geometry == stamp.geometry &&
                session.displayShape().IsSame(shape) && session.selectedFeature() == selected &&
                session.features().size() == count && session.canUndo() == undo &&
                session.canRedo() == redo && session.isModified() == dirty,
            "Invalid input changed the session or history");
}
const document::SketchPlane plane{0, 0, 40, 0, 0, 1, 1, 0, 0};
document::SketchParameters rectangle() {
    return {plane, {{{document::SketchElementId{1}, document::SketchRectangle{1, 1, 5, 4}}}, 2}};
}
void dimensionsAndPlacement() {
    application::CadSession session;
    const auto cylinder = session.features().back().id;
    require(session.selectFeature(document::ModelDocument::initialBoxId()), "select box");
    auto box = session.selectedBoxParameters();
    // Leave a redo branch in place throughout the rejected commands.
    test::requireCommittedFeature(session.createBoxFeature());
    require(session.undo().status() == CommandStatus::committed, "prepare redo");
    require(session.selectFeature(document::ModelDocument::initialBoxId()), "reselect box");
    for (double invalid :
         {-1.0, 0.0, std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        for (auto field : {&kernel::BoxDimensions::width, &kernel::BoxDimensions::depth,
                           &kernel::BoxDimensions::height}) {
            auto dimensions = box.dimensions;
            dimensions.*field = invalid;
            rejectsInput(session, [&] { return session.editSelectedBoxDimensions(dimensions); });
            auto parameters = box;
            parameters.dimensions = dimensions;
            rejectsInput(session, [&] {
                return session.editSelectedBoxParameters(
                    parameters, application::ErrorDisposition::keepEditableErrors);
            });
        }
    }
    for (double invalid :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        // Invalid rotations now fail at typed-value construction, before commands.
        for (auto field : {&kernel::RigidTransform::rotationX, &kernel::RigidTransform::rotationY,
                           &kernel::RigidTransform::rotationZ}) {
            const auto stamp = session.sceneStatus();
            const bool undo = session.canUndo(), redo = session.canRedo(),
                       dirty = session.isModified();
            auto placement = box.placement;
            bool rejected = false;
            try {
                placement.*field = kernel::Radians(invalid);
                static_cast<void>(session.editSelectedFeaturePlacement(placement));
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            require(rejected && placement == box.placement &&
                        session.sceneStatus().document == stamp.document &&
                        session.canUndo() == undo && session.canRedo() == redo &&
                        session.isModified() == dirty,
                    "Invalid typed rotation changed document or history");
        }
        for (auto field :
             {&kernel::RigidTransform::translationX, &kernel::RigidTransform::translationY,
              &kernel::RigidTransform::translationZ}) {
            auto parameters = box;
            parameters.placement.*field = invalid;
            rejectsInput(session, [&] { return session.editSelectedBoxParameters(parameters); });
            rejectsInput(session, [&] {
                return session.editSelectedFeaturePlacement(parameters.placement);
            });
            auto cylinderParameters = session.cylinderParameters(cylinder);
            cylinderParameters.placement.*field = invalid;
            rejectsInput(session, [&] {
                return session.editCylinderParameters(cylinder, cylinderParameters);
            });
        }
    }
    require(session.selectFeature(cylinder), "select cylinder");
    const auto original = session.selectedCylinderParameters();
    for (double invalid : {-1.0, 0.0, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
        for (auto field :
             {&document::CylinderParameters::radius, &document::CylinderParameters::height}) {
            auto parameters = original;
            parameters.*field = invalid;
            parameters.resultMode = document::CylinderResultMode::separate;
            rejectsInput(session,
                         [&] { return session.editSelectedCylinderParameters(parameters); });
        }
    }
    auto parameters = original;
    parameters.resultMode = static_cast<document::CylinderResultMode>(999);
    rejectsInput(session, [&] { return session.editCylinderParameters(cylinder, parameters); });
    rejectsInput(session, [&] { return session.createCylinderFeature(parameters.resultMode); });
    rejectsInput(session, [&] {
        return session.editCylinderParameters(document::FeatureId{9999}, original);
    });
    rejectsInput(session, [&] { return session.editSelectedBoxParameters(box); });
    require(session.redo().status() == CommandStatus::committed, "invalid inputs destroyed redo");
}
void sketchAndSelection() {
    application::CadSession session;
    session.newDocument();
    rejectsInput(session, [&] { return session.createCylinderFeature(); });
    rejectsInput(session, [&] { return session.editSelectedBoxDimensions({1, 1, 1}); });
    rejectsInput(session, [&] { return session.editSelectedBoxParameters({{1, 1, 1}, {}}); });
    rejectsInput(session, [&] {
        return session.editSelectedCylinderParameters(
            {1, 1, {}, document::CylinderResultMode::join});
    });
    rejectsInput(session, [&] { return session.editSelectedExtrusionParameters({1, {}}); });
    rejectsInput(session, [&] { return session.editSelectedFeaturePlacement({}); });
    require(session.undo().reason() == RejectionReason::noChange &&
                session.redo().reason() == RejectionReason::noChange,
            "empty history did not return noChange");
    const auto box = test::requireCommittedFeature(session.createBoxFeature());
    auto sketch = rectangle();
    rejectsInput(session,
                 [&] { return session.createSketchFeature(document::FeatureId{9999}, sketch); });
    sketch.plane.normalZ = 0;
    rejectsInput(session, [&] { return session.createSketchFeature(box, sketch); });
    rejectsInput(session, [&] { return session.createSketchExtrusion(box, sketch, 5); });
    sketch = rectangle();
    for (double invalid : {-1.0, 0.0, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()})
        rejectsInput(session, [&] { return session.createSketchExtrusion(box, sketch, invalid); });
    const auto sketchId = test::requireCommittedFeature(session.createSketchFeature(box, sketch));
    auto malformed = sketch;
    malformed.elements.nextId = 1;
    rejectsInput(session, [&] { return session.editSketchParameters(sketchId, malformed); });
    rejectsInput(session, [&] { return session.editSketchParameters(box, sketch); });
    rejectsInput(session, [&] { return session.editSelectedFeaturePlacement({}); });
    const std::vector<document::SketchRegionId> unknown{
        *document::SketchRegionId::parse("o:9999.0")};
    rejectsInput(session, [&] { return session.createSketchProfileExtrusion(box, unknown, 5); });
    rejectsInput(session,
                 [&] { return session.createSketchProfileExtrusion(sketchId, unknown, 5); });
    rejectsInput(session, [&] { return session.createSketchProfileExtrusion(sketchId, {}, 5); });
    require(session.selectSketchProfileByElement(document::SketchElementId{1}, false),
            "select profile");
    const auto id = session.selectedSketchProfiles().front();
    const std::vector<document::SketchRegionId> duplicate{id, id};
    rejectsInput(session,
                 [&] { return session.createSketchProfileExtrusion(sketchId, duplicate, 5); });
    test::requireCommittedFeature(
        session.createSketchProfileExtrusion(sketchId, session.selectedSketchProfiles(), 5));
    rejectsInput(session, [&] { return session.editSelectedExtrusionParameters({-1, {}}); });
    rejectsInput(session,
                 [&] { return session.editSelectedExtrusionParameters({5, {}, duplicate}); });
    application::SketchEditorSession editor(session, box, plane);
    rejectsInput(session, [&] { return editor.tryAccept(); });
    require(editor.isActive(), "rejected empty editor closed");
}
void unexpectedEvaluationRemainsAnException() {
    application::CadSession session;
    features::detail::beforeFeatureEvaluation = [](document::FeatureId) {
        throw std::invalid_argument("unexpected evaluation error");
    };
    bool escaped = false;
    try {
        static_cast<void>(session.createBoxFeature());
    } catch (const std::invalid_argument&) {
        escaped = true;
    }
    features::detail::beforeFeatureEvaluation = nullptr;
    require(escaped && !session.isModified() && !session.canUndo(),
            "Evaluation exception became invalidInput or changed state");
}
} // namespace
int main() {
    try {
        dimensionsAndPlacement();
        sketchAndSelection();
        unexpectedEvaluationRemainsAnException();
        std::cout << "Input results, validation boundaries and unchanged history passed\n";
        return 0;
    } catch (const std::exception& error) {
        features::detail::beforeFeatureEvaluation = nullptr;
        std::cerr << error.what() << '\n';
        return 1;
    }
}
