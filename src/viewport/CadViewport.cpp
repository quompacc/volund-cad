#include "ViewportContextGuard.hpp"
#include <QDebug>
#include <stdexcept>
#include <voelund/viewport/CadViewport.hpp>
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include "SketchBoxSelection.hpp"
#include "SketchConstraintPresentation.hpp"
#include "SketchEditorInteraction.hpp"
#include "SketchInteraction.hpp"
#include "SketchPresentation.hpp"
#include "TransformManipulator.hpp"
#include "ViewportHighlightStyle.hpp"

#include <AIS_DisplayMode.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_SelectionScheme.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_GradientFillMethod.hxx>
#include <Aspect_Grid.hxx>
#include <Aspect_GridDrawMode.hxx>
#include <Aspect_GridType.hxx>
#include <Aspect_TypeOfTriedronPosition.hxx>
#include <Aspect_Window.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <NCollection_Vec2.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <QMouseEvent>
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include <QOpenGLContext>
#include <QSurfaceFormat>
#else
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#endif
#include <QRubberBand>
#include <QWheelEvent>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <StdSelect_BRepOwner.hxx>
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include <Standard_Failure.hxx>
#endif
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace voelund::viewport {

CadViewport::CadViewport(QWidget* parent)
    : CadViewportBase(parent), impl_(std::make_unique<Impl>()) {
#if !defined(VOELUND_VIEWPORT_QT_OPENGL)
    // OCCT owns framebuffer rendering; Qt only owns the native window and event routing.
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_PaintOnScreen);
#else
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
#endif
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(640, 480);
    impl_->selectionBand = new QRubberBand(QRubberBand::Rectangle, this);
}

CadViewport::~CadViewport() {
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    cleanupGlResources();
#endif
}

bool CadViewport::isRenderingReady() const noexcept { return impl_->ready; }

std::size_t CadViewport::displayedSketchElementCount() const noexcept {
    return impl_->sketchPresentation.size();
}

std::size_t CadViewport::displayedSketchConstraintCount() const noexcept {
    return impl_->sketchConstraintPresentation.size();
}

std::pair<int, int> CadViewport::renderingPixelSize() const noexcept {
    if (!impl_->ready)
        return {0, 0};
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    if (impl_->view->Window().IsNull())
        return {0, 0};
#else
    if (impl_->window.IsNull())
        return {0, 0};
#endif
    int width = 0;
    int height = 0;
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    impl_->view->Window()->Size(width, height);
#else
    impl_->window->Size(width, height);
#endif
    return {width, height};
}

ViewportLifecycleDiagnostics CadViewport::lifecycleDiagnostics() const noexcept {
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    return {impl_->contextGeneration, impl_->cleanupGeneration};
#else
    return {impl_->ready ? 1U : 0U, 0U};
#endif
}

void CadViewport::setShape(const TopoDS_Shape& shape) {
    performViewportOperation(*this, impl_->ready, [&] {
        const SceneBody body{0, shape, false};
        setBodies(std::span<const SceneBody>(&body, 1));
    });
}

void CadViewport::setBodies(std::span<const SceneBody> bodies) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->pendingBodies.assign(bodies.begin(), bodies.end());
        impl_->displayPendingBodies();
        update();
    });
}

void CadViewport::setSketchElements(std::span<const SceneSketchElement> elements,
                                    std::optional<std::uint64_t> selectedId) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->pendingSketchElements.assign(elements.begin(), elements.end());
        std::vector<SceneSketchPoint> snapPoints;
        for (const SceneSketchElement& element : elements)
            snapPoints.insert(snapPoints.end(), element.points.begin(), element.points.end());
        impl_->sketchEditor.setSnapPoints(snapPoints);
        impl_->pendingSketchConstraints.clear();
        impl_->sketchConstraintPresentation.clear(impl_->context);
        impl_->selectedSketchElements.clear();
        if (selectedId.has_value())
            impl_->selectedSketchElements.push_back(*selectedId);
        if (impl_->ready)
            impl_->sketchPresentation.setElements(impl_->context, impl_->pendingSketchElements,
                                                  impl_->selectedSketchElements,
                                                  impl_->sketchEditor.isActive());
        update();
    });
}

void CadViewport::setSketchConstraints(std::span<const SceneSketchConstraint> constraints) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->pendingSketchConstraints.assign(constraints.begin(), constraints.end());
        if (impl_->ready)
            impl_->sketchConstraintPresentation.setConstraints(impl_->context,
                                                               impl_->pendingSketchConstraints);
        update();
    });
}

void CadViewport::setSelectedSketchElement(std::optional<std::uint64_t> elementId) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->selectedSketchElements.clear();
        if (elementId.has_value())
            impl_->selectedSketchElements.push_back(*elementId);
        impl_->sketchPresentation.select(impl_->context, impl_->selectedSketchElements);
        update();
    });
}

void CadViewport::setSelectedSketchElements(std::span<const std::uint64_t> elementIds) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->selectedSketchElements.assign(elementIds.begin(), elementIds.end());
        impl_->sketchPresentation.select(impl_->context, impl_->selectedSketchElements);
        update();
    });
}

void CadViewport::setTransformTarget(std::optional<std::uint64_t> bodyId) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->transformTarget = bodyId;
        impl_->attachTransformTarget();
        update();
    });
}

void CadViewport::beginRectangleSketch(const PlanarFaceSelection& support) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->sketchEditor.cancel();
        impl_->manipulator.setEnabled(false);
        impl_->manipulator.detach();
        impl_->activeSketchSupport = support;
        if (!impl_->context.IsNull())
            impl_->basePlanes.hide(impl_->context);
        impl_->sketch.begin(support, impl_->context, impl_->view);
        update();
    });
}

void CadViewport::beginSketchEditor(const PlanarFaceSelection& support, SketchElementTool tool) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->sketch.cancel();
        impl_->manipulator.setEnabled(false);
        impl_->manipulator.detach();
        impl_->activeSketchSupport = support;
        if (!impl_->context.IsNull())
            impl_->basePlanes.hide(impl_->context);
        impl_->sketchEditor.begin(support, tool, impl_->view);
        impl_->sketchPresentation.setReferences(impl_->context, support, true);
        update();
    });
}

void CadViewport::setSketchEditorTool(SketchElementTool tool) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->sketchEditor.setTool(tool);
        if (impl_->sketchEditorPreviewHandler) {
            impl_->sketchEditorPreviewHandler(std::nullopt);
        }
        update();
    });
}

void CadViewport::continueSketchPolyline(double x, double y, std::uint64_t elementId) {
    performViewportOperation(*this, impl_->ready,
                             [&] { impl_->sketchEditor.continuePolyline(x, y, elementId); });
}

bool CadViewport::cancelSketchEditorStep() { return impl_->sketchEditor.cancelStep(); }

void CadViewport::updateSketchExtrusionPreview(double distance) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->sketch.updateDistance(distance);
        update();
    });
}

void CadViewport::cancelSketch() {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->activeSketchSupport.reset();
        impl_->sketch.cancel();
        impl_->sketchEditor.cancel();
        impl_->sketchPresentation.setReferences(impl_->context, {}, false);
        impl_->refreshBasePlanes();
        update();
    });
}

std::size_t CadViewport::displayedBasePlaneCount() const noexcept {
    return impl_->basePlanes.displayedCount();
}

void CadViewport::setTransformToolEnabled(bool enabled) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->setTransformToolEnabled(enabled);
        if (!impl_->ready) {
            return;
        }
        update();
    });
}

void CadViewport::setTransformSnapping(bool enabled, double linearStep, double angularStep) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->manipulator.setSnapping(enabled, linearStep, angularStep);
        impl_->sketch.setSnapping(enabled, linearStep);
        impl_->sketchEditor.setSnapping(enabled, linearStep);
    });
}

void CadViewport::setTheme(const ViewportTheme& theme) {
    impl_->theme = theme;
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready)
            return;
        impl_->applyTheme();
        update();
    });
}

const ViewportTheme& CadViewport::theme() const noexcept { return impl_->theme; }

void CadViewport::setCameraView(CameraView cameraView) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready)
            return;
        V3d_TypeOfOrientation orientation = V3d_TypeOfOrientation_Zup_AxoRight;
        switch (cameraView) {
        case CameraView::top:
            orientation = V3d_TypeOfOrientation_Zup_Top;
            break;
        case CameraView::front:
            orientation = V3d_TypeOfOrientation_Zup_Front;
            break;
        case CameraView::right:
            orientation = V3d_TypeOfOrientation_Zup_Right;
            break;
        case CameraView::left:
            orientation = V3d_TypeOfOrientation_Zup_Left;
            break;
        case CameraView::back:
            orientation = V3d_TypeOfOrientation_Zup_Back;
            break;
        case CameraView::bottom:
            orientation = V3d_TypeOfOrientation_Zup_Bottom;
            break;
        case CameraView::isometric:
            break;
        }
        impl_->view->SetProj(orientation);
        fitView();
    });
}

void CadViewport::setPerspectiveProjection(bool enabled) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready)
            return;
        impl_->view->Camera()->SetProjectionType(enabled
                                                     ? Graphic3d_Camera::Projection_Perspective
                                                     : Graphic3d_Camera::Projection_Orthographic);
        update();
    });
}

void CadViewport::fitView() {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready)
            return;
        impl_->view->FitAll(0.08, false);
        impl_->view->ZFitAll();
        update();
    });
}

} // namespace voelund::viewport
