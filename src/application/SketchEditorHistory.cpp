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
    preview_.clear();
    selectedElement_.reset();
    selectedReferences_.clear();
    if (elements_.values.empty())
        lastSolveResult_ = {features::SketchSolveStatus::success, -1, {}};
    else
        static_cast<void>(solveConstraints());
    return true;
}

bool SketchEditorSession::redo() {
    if (!canRedo())
        return false;
    ++historyIndex_;
    elements_ = history_[historyIndex_].elements;
    constraints_ = history_[historyIndex_].constraints;
    preview_.clear();
    selectedElement_.reset();
    selectedReferences_.clear();
    if (elements_.values.empty())
        lastSolveResult_ = {features::SketchSolveStatus::success, -1, {}};
    else
        static_cast<void>(solveConstraints());
    return true;
}

} // namespace voelund::application
