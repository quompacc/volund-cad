#pragma once
#include <optional>
#include <variant>
#include <voelund/document/BodyId.hpp>
#include <voelund/document/DocumentId.hpp>
#include <voelund/document/FeatureId.hpp>
#include <voelund/document/SketchElementId.hpp>
#include <voelund/kernel/OperationHistory.hpp>
namespace voelund::document {
// Canonical pair of adjacent semantic primitive faces, never a traversal index.
struct PrimitiveEdgeOrigin final {
    FeatureId feature;
    kernel::PrimitiveRole first;
    kernel::PrimitiveRole second;
    bool operator==(const PrimitiveEdgeOrigin&) const = default;
};
// Roles belong to the generating geometry, never to wire traversal order.
enum class ContourRole {
    line,
    circle,
    arc,
    ellipse,
    rectangleFirst,
    rectangleSecond,
    rectangleThird,
    rectangleFourth
};
enum class ContourStage { sketch, extrusionStart, extrusionEnd };
struct ContourEdgeOrigin final {
    FeatureId sketch;
    SketchElementId element;
    ContourRole role;
    std::optional<FeatureId> extrusion{};
    ContourStage stage{ContourStage::sketch};
    bool operator==(const ContourEdgeOrigin&) const = default;
};
struct EdgeOrigin final {
    std::variant<PrimitiveEdgeOrigin, ContourEdgeOrigin> value;
    EdgeOrigin(PrimitiveEdgeOrigin origin) : value(origin) {}
    EdgeOrigin(ContourEdgeOrigin origin) : value(origin) {}
    EdgeOrigin(FeatureId feature, kernel::PrimitiveRole first, kernel::PrimitiveRole second)
        : value(PrimitiveEdgeOrigin{feature, first, second}) {}
    bool operator==(const EdgeOrigin&) const = default;
};
enum class EdgePointKind { start, end, parameter };
struct EdgePointSelection final {
    EdgePointKind kind{EdgePointKind::parameter};
    double parameter{0.0}; // normalized on the original oriented analytic curve
    bool operator==(const EdgePointSelection&) const = default;
};
struct ProjectedEdgeReference final {
    DocumentId document;
    BodyId body;
    EdgeOrigin origin;
    EdgePointSelection selection;
    bool operator==(const ProjectedEdgeReference&) const = default;
};
void validateEdgeOrigin(const EdgeOrigin& origin);
void validateProjectedEdgeReference(const ProjectedEdgeReference& reference);
} // namespace voelund::document
