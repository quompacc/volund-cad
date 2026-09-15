#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/IdCapacity.hpp>
#include <algorithm>

namespace voelund::application {
bool SketchEditorSession::canAllocateIds(std::uint64_t elements,
                                         std::uint64_t constraints) const noexcept {
    return document::hasIdCapacity(elements_.nextId, elements) &&
           document::hasIdCapacity(constraints_.nextId, constraints);
}

bool SketchEditorSession::canAllocatePreviewIds(
    std::span<const document::SketchGeometry> preview) const noexcept {
    if (preview.empty())
        return canAllocateIds(0, 0);
    if (std::holds_alternative<document::SketchRectangle>(preview.front()))
        return canAllocateIds(4, 8);
    const auto count = static_cast<std::uint64_t>(preview.size());
    if (tool_ == SketchDrawingTool::slot && count == 4)
        return canAllocateIds(4, 10);
    if (tool_ == SketchDrawingTool::polygon && count > 1) {
        // Check multiplication before forming the complete count (2*n - 1).
        return document::hasIdCapacity(elements_.nextId, count) &&
               document::hasIdCapacity(constraints_.nextId, count) &&
               document::hasIdCapacity(constraints_.nextId + count, count - 1);
    }
    return canAllocateIds(count, 0);
}

bool SketchEditorSession::canUpsertUnaryConstraint(document::SketchElementId element,
                                                   document::SketchConstraintKind kind) const noexcept {
    return std::ranges::any_of(constraints_.values, [element, kind](const auto& item) {
        return item.kind == kind && item.first.element == element && !item.second.has_value();
    }) || canAllocateIds(0, 1);
}
} // namespace voelund::application
