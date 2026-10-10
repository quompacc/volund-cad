#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"
#include <cmath>
#include <string>

namespace voelund::application {
using namespace detail;
CommandResult CadSession::createBoxFeature() {
    return createBoxFeature(kernel::BoxDimensions{20.0, 20.0, 40.0});
}

CommandResult CadSession::createBoxFeature(const kernel::BoxDimensions& dimensions) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::size_t sequence = featureCount(document_.features(), document::FeatureKind::box) + 1;
    const auto idInput = detail::prepareInput(
        [&] { return candidate.addBoxFeature("Quader " + std::to_string(sequence), dimensions); });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::createCylinderFeature(document::CylinderResultMode resultMode) {
    if (document_.features().empty() || !canAddCylinder(document_.features().back().id))
        return CommandResult::invalidInput("A cylinder requires a current input feature");
    return createCylinderFeature(document_.features().back().id, resultMode);
}

CommandResult CadSession::createCylinderFeature(document::FeatureId inputFeature,
                                                document::CylinderResultMode resultMode) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!canAddCylinder(inputFeature)) {
        return CommandResult::invalidInput("A cylinder requires a current input feature");
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::cylinder) + 1;
    document::CylinderParameters parameters{
        8.0, 25.0, kernel::RigidTransform{0.0, 0.0, 0.0, {}, {}, {}}, resultMode};
    if (resultMode == document::CylinderResultMode::separate) {
        translateCylinderBetweenInputAndReference(
            parameters, evaluatedFeature(evaluation_->result.features, inputFeature), 1.0);
    }
    const auto idInput = detail::prepareInput([&] {
        return candidate.addCylinderFeature("Zylinder " + std::to_string(sequence), inputFeature,
                                            parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    const auto id = *idInput.value;
    return finishCandidate(transaction, id, CommandKind::newFeature, ErrorDisposition::reject,
                           true);
}

CommandResult CadSession::createNewBodyCylinder() {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::cylinder) + 1;
    const document::CylinderParameters parameters{
        8.0, 25.0, kernel::RigidTransform{0.0, 0.0, 0.0, {}, {}, {}},
        document::CylinderResultMode::separate};
    const auto idInput = detail::prepareInput([&] {
        return candidate.addCylinderFeature("Zylinder " + std::to_string(sequence), std::nullopt,
                                            parameters);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::createSketchExtrusion(document::FeatureId supportFeature,
                                                const document::SketchPlane& plane,
                                                const document::SketchRectangle& rectangle,
                                                double distance) {
    const document::SketchElement rectangleElement{document::SketchElementId{1}, rectangle};
    return createSketchExtrusion(
        supportFeature,
        document::SketchParameters{plane, document::SketchElements{{rectangleElement}, 2}},
        distance);
}

CommandResult CadSession::createSketchFeature(document::FeatureId supportFeature,
                                              const document::SketchParameters& sketch) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!document_.contains(supportFeature)) {
        return CommandResult::invalidInput("Sketch support is not in the document");
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::sketch) + 1;
    const auto idInput = detail::prepareInput([&] {
        return candidate.addSketchFeature("Skizze " + std::to_string(sequence), supportFeature,
                                          sketch);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    const auto id = *idInput.value;
    return finishCandidate(transaction, id, CommandKind::newFeature, ErrorDisposition::reject,
                           true);
}

CommandResult CadSession::createDatumSketchFeature(const document::SketchParameters& sketch) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::sketch) + 1;
    const auto idInput = detail::prepareInput([&] {
        return candidate.addDatumSketchFeature("Skizze " + std::to_string(sequence), sketch);
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::createSketchExtrusion(document::FeatureId supportFeature,
                                                const document::SketchParameters& sketch,
                                                double distance) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    if (!document_.contains(supportFeature)) {
        return CommandResult::invalidInput("Sketch support is not in the document");
    }
    if (!std::isfinite(distance) || distance <= 0.0) {
        return CommandResult::invalidInput("Sketch extrusion distance must be positive and finite");
    }
    const std::size_t sketchSequence =
        featureCount(document_.features(), document::FeatureKind::sketch) + 1;
    const auto sketchIdInput = detail::prepareInput([&] {
        return candidate.addSketchFeature("Skizze " + std::to_string(sketchSequence),
                                          supportFeature, sketch);
    });
    if (!sketchIdInput.value)
        return CommandResult::invalidInput(sketchIdInput.diagnostic);
    const auto sketchId = *sketchIdInput.value;
    const std::size_t extrusionSequence =
        featureCount(document_.features(), document::FeatureKind::extrusion) + 1;
    const auto extrusionIdInput = detail::prepareInput([&] {
        return candidate.addExtrusionFeature("Extrusion " + std::to_string(extrusionSequence),
                                             sketchId, document::ExtrusionParameters{distance, {}});
    });
    if (!extrusionIdInput.value)
        return CommandResult::invalidInput(extrusionIdInput.diagnostic);
    const auto extrusionId = *extrusionIdInput.value;
    return finishCandidate(transaction, extrusionId, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

} // namespace voelund::application
