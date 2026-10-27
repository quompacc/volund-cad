// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"
#include <cmath>
#include <string>
#include <voelund/features/FilletEdges.hpp>

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

CommandResult CadSession::createFillet(document::FeatureId support,
                                       std::span<const document::FilletEdge> edges, double radius) {
    if (!canAddCylinder(support))
        return CommandResult::invalidInput("A fillet requires a body with a current result");
    const auto body = evaluatedFeature(evaluation_->result.features, support).bodyId;
    if (!body)
        return CommandResult::invalidInput("A fillet requires a body with a current result");
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::fillet) + 1;
    std::vector<document::FilletEdge> numbered(edges.begin(), edges.end());
    features::numberFilletEdges(numbered);
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addFilletFeature("Verrundung " + std::to_string(sequence),
                                          document::FilletParameters{*body, numbered, radius});
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editFilletParameters(document::FeatureId featureId,
                                               const document::FilletParameters& parameters,
                                               ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.setFilletParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

document::FilletParameters CadSession::filletParameters(document::FeatureId featureId) const {
    return document_.filletParameters(featureId);
}

CommandResult CadSession::createChamfer(document::FeatureId support,
                                        std::span<const document::FilletEdge> edges,
                                        double distance) {
    if (!canAddCylinder(support))
        return CommandResult::invalidInput("A chamfer requires a body with a current result");
    const auto body = evaluatedFeature(evaluation_->result.features, support).bodyId;
    if (!body)
        return CommandResult::invalidInput("A chamfer requires a body with a current result");
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::chamfer) + 1;
    std::vector<document::FilletEdge> numbered(edges.begin(), edges.end());
    features::numberFilletEdges(numbered);
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addChamferFeature("Fase " + std::to_string(sequence),
                                           document::ChamferParameters{*body, numbered, distance});
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editChamferParameters(document::FeatureId featureId,
                                                const document::ChamferParameters& parameters,
                                                ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.setChamferParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

document::ChamferParameters CadSession::chamferParameters(document::FeatureId featureId) const {
    return document_.chamferParameters(featureId);
}

CommandResult CadSession::createHole(document::FeatureId support,
                                     const document::SubshapeOrigin& face, double x, double y,
                                     double diameter, std::optional<double> depth) {
    return createHole(
        support, face,
        document::HoleParameters{{document::BodyId{support.value()}, face}, x, y, diameter, depth});
}

CommandResult CadSession::createHole(document::FeatureId support,
                                     const document::SubshapeOrigin& face,
                                     const document::HoleParameters& hole) {
    std::optional<document::BodyFaceReference> reference;
    try {
        reference = faceReference(support, face);
    } catch (const std::invalid_argument& error) {
        return CommandResult::invalidInput(error.what());
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::hole) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addHoleFeature("Bohrung " + std::to_string(sequence),
                                        document::HoleParameters{*reference, hole.x, hole.y,
                                                                 hole.diameter, hole.depth,
                                                                 hole.variant});
    });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

CommandResult CadSession::editHoleParameters(document::FeatureId featureId,
                                             const document::HoleParameters& parameters,
                                             ErrorDisposition disposition) {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input =
        detail::prepareInput([&] { return candidate.setHoleParameters(featureId, parameters); });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    if (!*input.value)
        return {CommandStatus::rejected, RejectionReason::noChange};
    return finishCandidate(transaction, selectedFeature_, CommandKind::editFeature, disposition,
                           false);
}

document::HoleParameters CadSession::holeParameters(document::FeatureId featureId) const {
    return document_.holeParameters(featureId);
}

std::optional<document::FilletEdge> CadSession::filletEdge(document::FeatureId support,
                                                           const TopoDS_Shape& edge) const {
    if (!canAddCylinder(support) || edge.IsNull() || edge.ShapeType() != TopAbs_EDGE)
        return std::nullopt;
    return features::filletEdgeAt(
        evaluatedFeature(evaluation_->result.features, support).supportOrigins, edge);
}

std::vector<TopoDS_Shape>
CadSession::filletEdgeShapes(document::FeatureId support,
                             std::span<const document::FilletEdge> edges) const {
    std::vector<TopoDS_Shape> result;
    if (!canAddCylinder(support))
        return result;
    const auto& origins = evaluatedFeature(evaluation_->result.features, support).supportOrigins;
    for (const auto& edge : edges)
        for (const auto& shape : features::filletEdgeShapes(origins, edge))
            result.push_back(shape);
    return result;
}

CommandResult CadSession::createNewBodyCylinder() {
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::cylinder) + 1;
    const document::CylinderParameters parameters{8.0, 25.0,
                                                  kernel::RigidTransform{0.0, 0.0, 0.0, {}, {}, {}},
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
