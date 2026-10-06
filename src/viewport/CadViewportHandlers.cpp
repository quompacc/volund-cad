#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

namespace voelund::viewport {
void CadViewport::setSketchConstraintEditHandler(std::function<void(std::uint64_t)> handler) {
    impl_->sketchConstraintEditHandler = std::move(handler);
}

void CadViewport::setSketchElementSelectionHandler(
    std::function<void(const SketchSelectionIntent&)> handler) {
    impl_->sketchElementSelectionHandler = std::move(handler);
}

void CadViewport::setBodySelectionHandler(std::function<void(std::uint64_t)> handler) {
    impl_->bodySelectionHandler = std::move(handler);
}

void CadViewport::setBasePlaneSelectionHandler(std::function<void(BasePlane)> handler) {
    impl_->basePlaneSelectionHandler = std::move(handler);
}

void CadViewport::setEmptySelectionHandler(std::function<void()> handler) {
    impl_->emptySelectionHandler = std::move(handler);
}

void CadViewport::setAreaClickHandler(std::function<bool(const ViewRay&)> handler) {
    impl_->areaClickHandler = std::move(handler);
}

void CadViewport::setPlanarFaceSelectionHandler(
    std::function<void(const PlanarFaceSelection&)> handler) {
    impl_->planarFaceSelectionHandler = std::move(handler);
}

void CadViewport::setSketchEditorPreviewHandler(
    std::function<void(const std::optional<SketchElementIntent>&)> handler) {
    impl_->sketchEditorPreviewHandler = std::move(handler);
}

void CadViewport::setSketchEditorElementHandler(
    std::function<void(const SketchElementIntent&)> handler) {
    impl_->sketchEditorElementHandler = std::move(handler);
}

void CadViewport::setSketchEditorTranslationHandler(std::function<void(double, double)> handler) {
    impl_->sketchEditorTranslationHandler = std::move(handler);
}

void CadViewport::setBodyMoveHandler(
    std::function<void(std::uint64_t, double, double, double)> handler) {
    impl_->manipulator.setMoveHandler(std::move(handler));
}

void CadViewport::setBodyMovePreviewHandler(
    std::function<void(std::uint64_t, double, double, double)> handler) {
    impl_->manipulator.setMovePreviewHandler(std::move(handler));
}

void CadViewport::setBodyRotationHandler(std::function<void(std::uint64_t, int, double)> handler) {
    impl_->manipulator.setRotationHandler(std::move(handler));
}

void CadViewport::setBodyRotationPreviewHandler(
    std::function<void(std::uint64_t, int, double)> handler) {
    impl_->manipulator.setRotationPreviewHandler(std::move(handler));
}
} // namespace voelund::viewport
