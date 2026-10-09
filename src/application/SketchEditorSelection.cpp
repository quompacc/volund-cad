#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>

// S13: whole elements from a selection box, and a selected relation (user decision
// 2026-10-09: a click on a relation label selects it; Delete or "Bedingung entfernen" removes
// it). A selected relation and selected elements exclude each other.
namespace voelund::application {

bool SketchEditorSession::selectElements(std::span<const document::SketchElementId> ids,
                                         bool additive) noexcept {
    for (const auto id : ids)
        if (std::ranges::find(elements_.values, id, &document::SketchElement::id) ==
            elements_.values.end())
            return false;
    selectedConstraint_.reset();
    std::vector<document::SketchElementId> all;
    if (additive)
        all = selectedElementIds();
    for (const auto id : ids)
        if (std::ranges::find(all, id) == all.end())
            all.push_back(id);
    if (all.empty()) {
        clearSelection();
        return true;
    }
    // One element is selected as a whole (no point reference, so it moves as a whole).
    if (all.size() == 1)
        return selectElement(all.front());
    selectedReferences_.clear();
    for (const auto id : all)
        selectedReferences_.push_back({id, document::SketchPointPosition::start});
    selectedElement_ = all.back();
    selectionPoint_.reset();
    return true;
}

bool SketchEditorSession::selectConstraint(document::SketchConstraintId id) noexcept {
    if (std::ranges::find(constraints_.values, id, &document::SketchConstraint::id) ==
        constraints_.values.end())
        return false;
    selectedElement_.reset();
    selectedReferences_.clear();
    selectionPoint_.reset();
    selectedConstraint_ = id;
    return true;
}

std::optional<document::SketchConstraintId>
SketchEditorSession::selectedConstraint() const noexcept {
    if (selectedConstraint_ &&
        std::ranges::find(constraints_.values, *selectedConstraint_,
                          &document::SketchConstraint::id) != constraints_.values.end())
        return selectedConstraint_;
    return std::nullopt;
}

} // namespace voelund::application
