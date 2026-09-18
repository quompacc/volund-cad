#pragma once

#include <span>
#include <voelund/document/SubshapeReference.hpp>
#include <voelund/kernel/OperationHistory.hpp>

namespace voelund::features {

// Evaluation-local provenance. Persistent document namespace/binding follows R04b.
using SupportOrigin = document::SubshapeOrigin;
struct SupportProvenance final {
    SupportOrigin origin;
    std::vector<TopoDS_Shape> faces;
    kernel::HistoryCoverage coverage{kernel::HistoryCoverage::complete};
    // Source coordinate frame is carried unchanged through Booleans.
    // Primitive placement and prism generation supply the already transformed face.
    TopoDS_Shape frameFace{};
};
enum class SupportResolutionStatus { resolved, missing, ambiguous, unavailable };
struct SupportResolution final {
    SupportResolutionStatus status;
    std::vector<TopoDS_Shape> candidates;
};

[[nodiscard]] std::vector<SupportProvenance>
primitiveSupportOrigins(document::FeatureId feature, const kernel::ShapeOperationResult& primitive);
[[nodiscard]] std::vector<SupportProvenance>
continueSupportOrigins(std::span<const SupportProvenance> previous,
                       const kernel::ShapeOperationResult& operation,
                       kernel::HistoryOperand operand = kernel::HistoryOperand::first);
[[nodiscard]] SupportResolution resolveSupportOrigin(std::span<const SupportProvenance> origins,
                                                     SupportOrigin requested);

} // namespace voelund::features
