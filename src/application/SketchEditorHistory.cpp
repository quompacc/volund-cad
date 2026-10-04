#include <voelund/application/SketchEditorSession.hpp>

namespace voelund::application {

void SketchEditorSession::commitHistory() {
    if (!history_.empty() && history_[historyIndex_].elements == elements_ &&
        history_[historyIndex_].constraints == constraints_)
        return;
    if (!history_.empty())
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(historyIndex_ + 1),
                       history_.end());
    history_.push_back({elements_, constraints_});
    historyIndex_ = history_.size() - 1;
}

bool SketchEditorSession::canUndo() const noexcept { return historyIndex_ > 0; }

bool SketchEditorSession::canRedo() const noexcept {
    return !history_.empty() && historyIndex_ + 1 < history_.size();
}

bool SketchEditorSession::undo() {
    if (!canUndo())
        return false;
    --historyIndex_;
    elements_ = history_[historyIndex_].elements;
    constraints_ = history_[historyIndex_].constraints;
    clearPreview();
    selectedElement_.reset();
    selectedReferences_.clear();
    if (elements_.values.empty()) {
        lastSolveResult_ = {features::SketchSolveStatus::success, -1, {}};
    } else if (solveConstraints()) {
        // Entries are already solved; the solve only refreshes its result. PlaneGCS orders
        // work by heap address, so writing its geometry back could perturb the last bits.
        elements_ = history_[historyIndex_].elements;
        constraints_ = history_[historyIndex_].constraints;
    }
    return true;
}

bool SketchEditorSession::redo() {
    if (!canRedo())
        return false;
    ++historyIndex_;
    elements_ = history_[historyIndex_].elements;
    constraints_ = history_[historyIndex_].constraints;
    clearPreview();
    selectedElement_.reset();
    selectedReferences_.clear();
    if (elements_.values.empty()) {
        lastSolveResult_ = {features::SketchSolveStatus::success, -1, {}};
    } else if (solveConstraints()) {
        // Entries are already solved; the solve only refreshes its result. PlaneGCS orders
        // work by heap address, so writing its geometry back could perturb the last bits.
        elements_ = history_[historyIndex_].elements;
        constraints_ = history_[historyIndex_].constraints;
    }
    return true;
}

} // namespace voelund::application
