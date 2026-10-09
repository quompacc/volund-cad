#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>

namespace voelund::application {

bool SketchEditorSession::isSelectedConstruction() const noexcept {
    if (!selectedElement_.has_value())
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    return match != elements_.values.end() && match->construction;
}

bool SketchEditorSession::toggleSelectedConstruction() {
    if (!active_ || !selectedElement_.has_value() || patternLock(false))
        return false;
    const auto match =
        std::ranges::find(elements_.values, *selectedElement_, &document::SketchElement::id);
    if (match == elements_.values.end())
        return false;
    match->construction = !match->construction;
    return true;
}

} // namespace voelund::application
