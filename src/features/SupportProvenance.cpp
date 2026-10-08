#include <algorithm>
#include <stdexcept>
#include <voelund/features/SupportProvenance.hpp>

namespace voelund::features {
namespace {
void appendUnique(std::vector<TopoDS_Shape>& faces, const TopoDS_Shape& face) {
    if (face.IsNull() || face.ShapeType() != TopAbs_FACE)
        throw std::logic_error("Support provenance requires a face");
    if (std::ranges::none_of(faces, [&](const auto& item) { return item.IsSame(face); }))
        faces.push_back(face);
}
SupportProvenance continueOrigin(const SupportProvenance& previous,
                                 const kernel::ShapeOperationResult& operation,
                                 kernel::HistoryOperand operand) {
    SupportProvenance next{previous.origin, {}, previous.coverage, previous.frameFace};
    for (const auto& source : previous.faces) {
        const auto entry = std::ranges::find_if(operation.history, [&](const auto& item) {
            return item.operand == operand && item.source.IsSame(source);
        });
        if (entry == operation.history.end() ||
            entry->coverage != kernel::HistoryCoverage::complete) {
            next.coverage = kernel::HistoryCoverage::unavailable;
            continue;
        }
        if (entry->unchanged)
            appendUnique(next.faces, source);
        for (const auto& modified : entry->modified)
            appendUnique(next.faces, modified);
        // Generated geometry never stands in for a continuation of the original face.
        // A complete deleted entry contributes no descendants, retaining its tombstone.
    }
    return next;
}
} // namespace
std::vector<SupportProvenance>
primitiveSupportOrigins(document::FeatureId feature,
                        const kernel::ShapeOperationResult& primitive) {
    std::vector<SupportProvenance> result;
    for (const auto& origin : primitive.origins) {
        SupportProvenance item{{feature, origin.role}, {}};
        item.frameFace = origin.shape;
        appendUnique(item.faces, origin.shape);
        result.push_back(std::move(item));
    }
    return result;
}
std::vector<SupportProvenance> continueSupportOrigins(std::span<const SupportProvenance> previous,
                                                      const kernel::ShapeOperationResult& operation,
                                                      kernel::HistoryOperand operand) {
    std::vector<SupportProvenance> result;
    result.reserve(previous.size());
    for (const auto& origin : previous)
        result.push_back(continueOrigin(origin, operation, operand));
    return result;
}
void addSupportFace(std::vector<SupportProvenance>& origins, const SupportOrigin& origin,
                    const TopoDS_Shape& face) {
    const auto found = std::ranges::find(origins, origin, &SupportProvenance::origin);
    if (found == origins.end()) {
        origins.push_back({origin, {face}, kernel::HistoryCoverage::complete, face});
        return;
    }
    if (std::ranges::none_of(found->faces, [&](const auto& item) { return item.IsSame(face); }))
        found->faces.push_back(face);
}
SupportResolution resolveSupportOrigin(std::span<const SupportProvenance> origins,
                                       SupportOrigin requested) {
    const auto found = std::ranges::find(origins, requested, &SupportProvenance::origin);
    if (found == origins.end())
        return {SupportResolutionStatus::missing, {}};
    if (found->coverage != kernel::HistoryCoverage::complete)
        return {SupportResolutionStatus::unavailable, found->faces};
    return {found->faces.empty()       ? SupportResolutionStatus::missing
            : found->faces.size() == 1 ? SupportResolutionStatus::resolved
                                       : SupportResolutionStatus::ambiguous,
            found->faces};
}
} // namespace voelund::features
