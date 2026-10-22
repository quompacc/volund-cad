// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "../../src/features/FeatureEvaluationDetail.hpp"
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <voelund/application/CadSession.hpp>
#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>

namespace {
using namespace voelund;
using namespace document;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
double volume(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}
DocumentSnapshot detachedSnapshot() {
    auto model = ModelDocument::empty();
    const auto box = model.addBoxFeature("Box", {10, 20, 30});
    static_cast<void>(
        model.addCylinderFeature("Separate", box, {2, 5, {}, CylinderResultMode::separate}));
    const auto sketch = model.addSketchFeature(
        "Sketch", box,
        {{0, 0, 30, 0, 0, 1, 1, 0, 0},
         SketchElements{{{SketchElementId{1}, SketchRectangle{1, 1, 4, 4}}}, 2}});
    static_cast<void>(model.addExtrusionFeature("Extrude", sketch, {3, {}}));
    return DocumentSnapshot::capture(model);
}
void detachedQueries() {
    const auto snapshot = detachedSnapshot(); // Source document is already destroyed.
    require(snapshot.features().size() == 4 && snapshot.boxDimensions(FeatureId{1}).width == 10 &&
                snapshot.cylinderParameters(FeatureId{2}).radius == 2 &&
                snapshot.sketchParameters(FeatureId{3}).elements.values.size() == 1 &&
                snapshot.extrusionParameters(FeatureId{4}).distance == 3,
            "Snapshot lost typed parameters after source destruction");
    auto restored = snapshot.toDocument();
    require(restored.setBoxDimensions(FeatureId{1}, {12, 20, 30}), "edit restored document");
    require(snapshot.boxDimensions(FeatureId{1}).width == 10 && restored.undo() &&
                snapshot.matchesContent(restored),
            "Snapshot shares mutable restored state");
    bool missing = false;
    try {
        (void)snapshot.featureParameters(FeatureId{99});
    } catch (const std::out_of_range&) {
        missing = true;
    }
    bool wrongKind = false;
    try {
        (void)snapshot.sketchParameters(FeatureId{1});
    } catch (const std::bad_variant_access&) {
        wrongKind = true;
    }
    require(missing && wrongKind, "Snapshot accepted unknown ID or wrong parameter kind");
    const auto evaluated = features::FeatureGraph{}.evaluate(snapshot);
    require(evaluated.features.size() == 4 && evaluated.bodies.size() == 2,
            "Mixed snapshot evaluation lost features/bodies");
}
ModelDocument* mutationTarget = nullptr;
void mutateDuringEvaluation(FeatureId) {
    if (!mutationTarget)
        return;
    auto* target = mutationTarget;
    mutationTarget = nullptr;
    *target = ModelDocument::empty();
    static_cast<void>(target->addBoxFeature("Replacement", {100, 100, 100}));
}
void oneCapturedState() {
    auto model = ModelDocument::empty();
    static_cast<void>(model.addBoxFeature("First", {2, 3, 4}));
    static_cast<void>(model.addBoxFeature("Second", {3, 4, 5}));
    const auto previous = features::detail::beforeFeatureEvaluation;
    struct RestoreHook {
        decltype(previous) value;
        ~RestoreHook() {
            features::detail::beforeFeatureEvaluation = value;
            mutationTarget = nullptr;
        }
    } restore{previous};
    mutationTarget = &model;
    features::detail::beforeFeatureEvaluation = mutateDuringEvaluation;
    const auto result = features::FeatureGraph{}.evaluate(model);
    require(model.features().size() == 1 && result.features.size() == 2 &&
                result.bodies.size() == 2 && std::abs(volume(result.resultShape) - 84.0) < 1e-7,
            "Evaluation mixed live document states instead of using one snapshot");
}
void ownedSessionValues() {
    application::CadSession session;
    require(session.selectFeature(ModelDocument::initialBoxId()), "select box");
    const auto list = session.features();
    const auto descriptor = session.selectedFeatureDescriptor();
    const auto parameters = session.selectedBoxParameters();
    const auto dimensions = session.selectedBoxDimensions();
    const auto placement = session.selectedFeaturePlacement();
    static_assert(!std::is_reference_v<decltype(session.selectedBoxParameters())>);
    static_assert(!std::is_reference_v<decltype(session.selectedFeatureDescriptor())>);
    static_assert(!std::is_reference_v<decltype(session.selectedSketchParameters())>);
    static_assert(!std::is_reference_v<decltype(session.selectedCylinderParameters())>);
    static_assert(!std::is_reference_v<decltype(session.selectedExtrusionParameters())>);
    session.newDocument();
    require(session.features().empty() && list.size() == 2 && descriptor.name == "Box" &&
                parameters.dimensions == dimensions && dimensions.width == 40 &&
                parameters.placement == placement,
            "UI query values depended on replaced document storage");
}
} // namespace
int main() {
    try {
        detachedQueries();
        oneCapturedState();
        ownedSessionValues();
        std::cout << "PASS owned read boundaries and frozen evaluation\n";
        return 0;
    } catch (const Standard_Failure& error) {
        std::cerr << kernel::occtFailureDiagnostic(error) << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 1;
}
