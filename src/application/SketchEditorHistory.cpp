#include <voelund/application/SketchEditorSession.hpp>

namespace voelund::application {

void SketchEditorSession::commitHistory() {
    if (dragStart_)
        return;
    if (!history_.empty() && history_[historyIndex_].elements == elements_ &&
        history_[historyIndex_].constraints == constraints_)
        return;
    if (!history_.empty())
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(historyIndex_ + 1),
                       history_.end());
    history_.push_back({elements_, constraints_});
    historyIndex_ = history_.size() - 1;
}

void SketchEditorSession::beginDrag() {
    if (!dragStart_)
        dragStart_ = HistoryState{elements_, constraints_};
}

bool SketchEditorSession::dragSelected(double totalX, double totalY) {
    if (!dragStart_)
        return false;
    elements_ = dragStart_->elements;
    constraints_ = dragStart_->constraints;
    if (totalX == 0.0 && totalY == 0.0) {
        static_cast<void>(solveConstraints());
        return true;
    }
    return translateSelected(totalX, totalY);
}

void SketchEditorSession::endDrag() {
    dragStart_.reset();
    commitHistory();
}

void SketchEditorSession::cancelDrag() {
    if (!dragStart_)
        return;
    elements_ = dragStart_->elements;
    constraints_ = dragStart_->constraints;
    dragStart_.reset();
    static_cast<void>(solveConstraints());
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
