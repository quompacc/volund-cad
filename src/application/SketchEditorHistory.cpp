#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>

#include <algorithm>
#include <stdexcept>

namespace voelund::application {

SketchEditorSession::EditorState SketchEditorSession::snapshot() const {
    return {elements_,        constraints_,        patterns_,       projections_,
            selectedElement_, selectedReferences_, selectionPoint_, lastSolveResult_};
}

void SketchEditorSession::restore(EditorState state) noexcept {
    elements_ = std::move(state.elements);
    constraints_ = std::move(state.constraints);
    patterns_ = std::move(state.patterns);
    projections_ = std::move(state.projections);
    selectedElement_ = state.selectedElement;
    selectedReferences_ = std::move(state.selectedReferences);
    selectionPoint_ = state.selectionPoint;
    lastSolveResult_ = std::move(state.solve);
}

void SketchEditorSession::restoreSelection(const EditorState& state) {
    const auto exists = [this](document::SketchElementId id) {
        return std::ranges::find(elements_.values, id, &document::SketchElement::id) !=
               elements_.values.end();
    };
    selectedElement_.reset();
    selectedReferences_.clear();
    selectionPoint_.reset();
    if (state.selectedElement && exists(*state.selectedElement)) {
        selectedElement_ = state.selectedElement;
        selectionPoint_ = state.selectionPoint;
    }
    for (const auto& reference : state.selectedReferences)
        if (exists(reference.element))
            selectedReferences_.push_back(reference);
}

bool SketchEditorSession::solveConstraints() {
    auto sketch = parameters(false);
    try {
        if (supportBinding_) {
            if (!supportStamp_ || *supportStamp_ != session_.sceneStatus().document)
                throw std::invalid_argument("the support changed");
            session_.resolveSketchSupport(*supportFeature_, sketch);
        }
        lastSolveResult_ = features::solveSketchConstraints(sketch);
    } catch (const std::invalid_argument&) {
        lastSolveResult_ = {features::SketchSolveStatus::failed, -1, {}};
        return false;
    } catch (const features::GeometryEvaluationError&) {
        lastSolveResult_ = {features::SketchSolveStatus::failed, -1, {}};
        return false;
    }
    if (lastSolveResult_.status != features::SketchSolveStatus::success)
        return false;
    elements_ = std::move(sketch.elements);
    constraints_ = std::move(sketch.constraints);
    plane_ = sketch.plane;
    return true;
}

void SketchEditorSession::refreshSolve() {
    if (elements_.values.empty()) {
        lastSolveResult_ = {features::SketchSolveStatus::success, 0, {}};
        return;
    }
    // The state is already solved; the solve only refreshes its result. PlaneGCS orders work
    // by heap address, so writing its geometry back could perturb the last bits.
    const auto elements = elements_;
    const auto constraints = constraints_;
    if (solveConstraints()) {
        elements_ = elements;
        constraints_ = constraints;
    }
}

bool SketchEditorSession::keepIfValid(Rollback& rollback) {
    try {
        document::validateSketchElements(elements_);
        document::validateSketchConstraints(elements_, constraints_);
    } catch (const std::invalid_argument&) {
        return false;
    }
    if (!solveConstraints())
        return false;
    rollback.keep();
    return true;
}

bool SketchEditorSession::loadDraft(document::SketchElements elements,
                                    document::SketchConstraints constraints,
                                    document::SketchPatterns patterns) {
    if (!active_ || dragStart_)
        return false;
    Rollback rollback(*this);
    elements_ = std::move(elements);
    constraints_ = std::move(constraints);
    patterns_ = std::move(patterns);
    clearPreview();
    selectedElement_.reset();
    selectedReferences_.clear();
    selectionPoint_.reset();
    try {
        document::validateSketchParameters(parameters(false));
    } catch (const std::invalid_argument&) {
        return false;
    }
    refreshSolve();
    rollback.keep();
    commitHistory();
    return true;
}

void SketchEditorSession::commitHistory() {
    if (dragStart_)
        return;
    if (!history_.empty() && history_[historyIndex_].elements == elements_ &&
        history_[historyIndex_].constraints == constraints_ &&
        history_[historyIndex_].patterns == patterns_ &&
        history_[historyIndex_].projections == projections_) {
        // Same geometry: the step keeps the latest selection.
        history_[historyIndex_] = snapshot();
        return;
    }
    if (!history_.empty())
        history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(historyIndex_ + 1),
                       history_.end());
    history_.push_back(snapshot());
    historyIndex_ = history_.size() - 1;
}

void SketchEditorSession::beginDrag() {
    if (!dragStart_)
        dragStart_ = snapshot();
}

bool SketchEditorSession::dragSelected(double totalX, double totalY) {
    if (!dragStart_)
        return false;
    elements_ = dragStart_->elements;
    constraints_ = dragStart_->constraints;
    patterns_ = dragStart_->patterns;
    projections_ = dragStart_->projections;
    lastSolveResult_ = dragStart_->solve;
    if (totalX == 0.0 && totalY == 0.0)
        return true;
    return translateSelected(totalX, totalY);
}

void SketchEditorSession::endDrag() {
    dragStart_.reset();
    commitHistory();
}

void SketchEditorSession::cancelDrag() {
    if (!dragStart_)
        return;
    auto start = std::move(*dragStart_);
    dragStart_.reset();
    restore(std::move(start));
}

bool SketchEditorSession::canUndo() const noexcept { return historyIndex_ > 0; }

bool SketchEditorSession::canRedo() const noexcept {
    return !history_.empty() && historyIndex_ + 1 < history_.size();
}

bool SketchEditorSession::undo() {
    if (!canUndo() || dragStart_)
        return false;
    --historyIndex_;
    elements_ = history_[historyIndex_].elements;
    constraints_ = history_[historyIndex_].constraints;
    patterns_ = history_[historyIndex_].patterns;
    projections_ = history_[historyIndex_].projections;
    clearPreview();
    restoreSelection(history_[historyIndex_]);
    refreshSolve();
    return true;
}

bool SketchEditorSession::redo() {
    if (!canRedo() || dragStart_)
        return false;
    ++historyIndex_;
    elements_ = history_[historyIndex_].elements;
    constraints_ = history_[historyIndex_].constraints;
    patterns_ = history_[historyIndex_].patterns;
    projections_ = history_[historyIndex_].projections;
    clearPreview();
    restoreSelection(history_[historyIndex_]);
    refreshSolve();
    return true;
}

} // namespace voelund::application
