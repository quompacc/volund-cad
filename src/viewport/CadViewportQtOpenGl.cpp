#include <voelund/viewport/CadViewport.hpp>

#include "QtOcctGlContext.hpp"
#include "SketchInteraction.hpp"
#include "TransformManipulator.hpp"
#include "ViewportHighlightStyle.hpp"

#include <AIS_DisplayMode.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_SelectionScheme.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <NCollection_Vec2.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <QDebug>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <Standard_Failure.hxx>
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

namespace {

const char* failureMessage(const Standard_Failure& failure)
{
#if OCC_VERSION_MAJOR >= 8
    return failure.what();
#else
    return failure.GetMessageString();
#endif
}

} // namespace

class CadViewport::Impl final {
public:
    enum class Gesture { none, pan, rotate, moveAxis };

    struct BodyPresentation final {
        std::uint64_t id;
        occ::handle<AIS_Shape> shape;
        bool transformable;
    };

    Impl()
    {
        occ::handle<Aspect_DisplayConnection> displayConnection;
        auto openGlDriver = new OpenGl_GraphicDriver(displayConnection, false);
        openGlDriver->ChangeOptions().buffersNoSwap = true;
        openGlDriver->ChangeOptions().buffersOpaqueAlpha = true;
        openGlDriver->ChangeOptions().useSystemBuffer = false;
        graphicDriver = openGlDriver;

        viewer = new V3d_Viewer(graphicDriver);
        viewer->SetDefaultLights();
        viewer->SetLightOn();
        context = new AIS_InteractiveContext(viewer);
        context->SetDisplayMode(AIS_Shaded, false);
        configureHighlightStyles(context);
        view = viewer->CreateView();
        view->SetImmediateUpdate(false);
        view->SetBackgroundColor(Quantity_Color(0.055, 0.071, 0.094, Quantity_TOC_RGB));
        view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight);
    }

    void displayPendingBodies()
    {
        if (!ready) {
            return;
        }
        manipulator.detach();
        const bool isFirstShape = presentations.empty();
        for (const BodyPresentation& presentation : presentations) {
            context->Remove(presentation.shape, false);
        }
        presentations.clear();

        for (const SceneBody& body : pendingBodies) {
            occ::handle<AIS_Shape> presentation = new AIS_Shape(body.shape);
            presentation->SetColor(Quantity_Color(0.72, 0.78, 0.88, Quantity_TOC_RGB));
            context->Display(presentation, false);
            context->Activate(presentation, AIS_Shape::SelectionMode(TopAbs_FACE));
            presentations.push_back(BodyPresentation{body.id, presentation, body.transformable});
        }
        attachTransformTarget();
        if (isFirstShape) {
            view->FitAll(0.08, false);
            view->ZFitAll();
        }
    }

    void setTransformToolEnabled(bool enabled)
    {
        manipulator.setEnabled(enabled);
        attachTransformTarget();
    }

    void attachTransformTarget()
    {
        manipulator.detach();
        if (!manipulator.isEnabled() || !transformTarget.has_value()) {
            return;
        }
        for (const BodyPresentation& presentation : presentations) {
            if (presentation.transformable && presentation.id == *transformTarget) {
                manipulator.attach(presentation.shape, presentation.id);
                break;
            }
        }
    }

    occ::handle<Graphic3d_GraphicDriver> graphicDriver;
    occ::handle<V3d_Viewer> viewer;
    occ::handle<AIS_InteractiveContext> context;
    occ::handle<V3d_View> view;
    TransformManipulator manipulator;
    SketchInteraction sketch;
    std::vector<BodyPresentation> presentations;
    std::vector<SceneBody> pendingBodies;
    std::function<void(std::uint64_t)> bodySelectionHandler;
    std::function<void(const PlanarFaceSelection&)> planarFaceSelectionHandler;
    std::function<void(const SketchRectangleResult&)> sketchRectangleHandler;
    std::optional<std::uint64_t> transformTarget;
    QPoint gestureOrigin;
    Gesture gesture{Gesture::none};
    bool ready{false};
};

CadViewport::CadViewport(QWidget* parent)
    : CadViewportBase(parent)
    , impl_(std::make_unique<Impl>())
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(640, 480);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);
}

CadViewport::~CadViewport()
{
    makeCurrent();
    impl_->manipulator.detach();
    impl_->context->RemoveAll(false);
    impl_->view->Remove();
    impl_->presentations.clear();
    impl_->context.Nullify();
    impl_->view.Nullify();
    impl_->viewer.Nullify();
    impl_->graphicDriver.Nullify();
    doneCurrent();
}

void CadViewport::setShape(const TopoDS_Shape& shape)
{
    const SceneBody body{0, shape, false};
    setBodies(std::span<const SceneBody>(&body, 1));
}

void CadViewport::setBodies(std::span<const SceneBody> bodies)
{
    impl_->pendingBodies.assign(bodies.begin(), bodies.end());
    impl_->displayPendingBodies();
    update();
}

void CadViewport::setTransformTarget(std::optional<std::uint64_t> bodyId)
{
    impl_->transformTarget = bodyId;
    impl_->attachTransformTarget();
    update();
}

void CadViewport::setBodySelectionHandler(std::function<void(std::uint64_t)> handler)
{
    impl_->bodySelectionHandler = std::move(handler);
}

void CadViewport::setPlanarFaceSelectionHandler(
    std::function<void(const PlanarFaceSelection&)> handler)
{
    impl_->planarFaceSelectionHandler = std::move(handler);
}

void CadViewport::setSketchRectangleHandler(
    std::function<void(const SketchRectangleResult&)> handler)
{
    impl_->sketchRectangleHandler = std::move(handler);
}

void CadViewport::setSketchExtrusionPreviewHandler(std::function<void(double)> handler)
{
    impl_->sketch.setDistancePreviewHandler(std::move(handler));
}

void CadViewport::beginRectangleSketch(const PlanarFaceSelection& support)
{
    impl_->manipulator.setEnabled(false);
    impl_->manipulator.detach();
    impl_->sketch.begin(support, impl_->context, impl_->view);
    update();
}

void CadViewport::updateSketchExtrusionPreview(double distance)
{
    impl_->sketch.updateDistance(distance);
    update();
}

void CadViewport::cancelSketch()
{
    impl_->sketch.cancel();
    update();
}

void CadViewport::setBodyMoveHandler(
    std::function<void(std::uint64_t, double, double, double)> handler)
{
    impl_->manipulator.setMoveHandler(std::move(handler));
}

void CadViewport::setBodyMovePreviewHandler(
    std::function<void(std::uint64_t, double, double, double)> handler)
{
    impl_->manipulator.setMovePreviewHandler(std::move(handler));
}

void CadViewport::setBodyRotationHandler(
    std::function<void(std::uint64_t, int, double)> handler)
{
    impl_->manipulator.setRotationHandler(std::move(handler));
}

void CadViewport::setBodyRotationPreviewHandler(
    std::function<void(std::uint64_t, int, double)> handler)
{
    impl_->manipulator.setRotationPreviewHandler(std::move(handler));
}

void CadViewport::setTransformToolEnabled(bool enabled)
{
    impl_->setTransformToolEnabled(enabled);
    if (!impl_->ready) {
        return;
    }
    update();
}

void CadViewport::setTransformSnapping(
    bool enabled,
    double linearStep,
    double angularStep)
{
    impl_->manipulator.setSnapping(enabled, linearStep, angularStep);
    impl_->sketch.setSnapping(enabled, linearStep);
}

bool CadViewport::isRenderingReady() const noexcept
{
    return impl_->ready;
}

void CadViewport::initializeGL()
{
    try {
        const auto fallback = static_cast<std::uintptr_t>(effectiveWinId());
        const Aspect_Drawable nativeWindow = qtgl::currentNativeWindow(fallback);
        impl_->ready = qtgl::attachCurrentContext(
            impl_->view,
            nativeWindow,
            width(),
            height(),
            devicePixelRatioF());
    } catch (const Standard_Failure& failure) {
        const QOpenGLContext* context = QOpenGLContext::currentContext();
        const QSurfaceFormat format = context != nullptr ? context->format() : QSurfaceFormat{};
        qWarning().noquote()
            << "OCCT context attachment failed:"
            << failureMessage(failure)
            << "Qt context:"
            << (context != nullptr ? (context->isOpenGLES() ? "OpenGL ES" : "desktop OpenGL")
                                   : "none")
            << format.majorVersion() << format.minorVersion();
        impl_->ready = false;
    }
    if (impl_->ready) {
        impl_->displayPendingBodies();
    }
}

void CadViewport::paintGL()
{
    if (!impl_->ready || !qtgl::bindCurrentFramebuffer(impl_->view)) {
        return;
    }
    qtgl::resetStateBeforeOcct(impl_->view);
    impl_->view->Redraw();
    qtgl::resetStateAfterOcct(impl_->view);
}

void CadViewport::mouseMoveEvent(QMouseEvent* event)
{
    if (!impl_->ready) {
        return;
    }
    const QPoint position = event->position().toPoint();
    if (impl_->sketch.isActive() && event->buttons() == Qt::NoButton) {
        impl_->sketch.move(position.x(), position.y());
        update();
        return;
    }
    if (impl_->gesture == Impl::Gesture::rotate) {
        impl_->view->Rotation(position.x(), position.y());
    } else if (impl_->gesture == Impl::Gesture::pan) {
        const QPoint delta = position - impl_->gestureOrigin;
        impl_->view->Pan(delta.x(), -delta.y(), 1.0, false);
    } else if (impl_->gesture == Impl::Gesture::moveAxis) {
        impl_->manipulator.update(
            position.x(),
            position.y(),
            impl_->view,
            event->modifiers().testFlag(Qt::ControlModifier));
    } else {
        impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
    }
    update();
}

void CadViewport::mousePressEvent(QMouseEvent* event)
{
    if (!impl_->ready) {
        return;
    }
    impl_->gestureOrigin = event->position().toPoint();
    if (event->button() == Qt::RightButton) {
        impl_->gesture = Impl::Gesture::rotate;
        impl_->view->StartRotation(impl_->gestureOrigin.x(), impl_->gestureOrigin.y());
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::MiddleButton) {
        impl_->gesture = Impl::Gesture::pan;
        impl_->view->Pan(0, 0, 1.0, true);
        setCursor(Qt::SizeAllCursor);
    } else if (event->button() == Qt::LeftButton) {
        if (impl_->sketch.isActive()) {
            return;
        }
        const QPoint position = event->position().toPoint();
        impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
        if (!impl_->context->HasDetected()) {
            return;
        }
        const occ::handle<AIS_InteractiveObject> detected =
            impl_->context->DetectedInteractive();
        if (impl_->manipulator.matches(detected) && impl_->manipulator.hasActiveMode()) {
            impl_->gesture = Impl::Gesture::moveAxis;
            impl_->manipulator.start(position.x(), position.y(), impl_->view);
            setCursor(Qt::ClosedHandCursor);
            return;
        }
        for (const Impl::BodyPresentation& presentation : impl_->presentations) {
            if (presentation.shape != detected) {
                continue;
            }
            if (impl_->bodySelectionHandler) {
                impl_->bodySelectionHandler(presentation.id);
            }
            break;
        }
    }
}

void CadViewport::mouseReleaseEvent(QMouseEvent* event)
{
    if (impl_->ready && event->button() == Qt::LeftButton
        && impl_->gesture == Impl::Gesture::moveAxis) {
        impl_->manipulator.finish();
        impl_->gesture = Impl::Gesture::none;
        unsetCursor();
        update();
        return;
    }
    if (impl_->ready && event->button() == Qt::LeftButton) {
        const QPoint position = event->position().toPoint();
        if (impl_->sketch.isActive()) {
            const auto rectangle = impl_->sketch.click(position.x(), position.y());
            if (rectangle.has_value() && impl_->sketchRectangleHandler) {
                impl_->sketchRectangleHandler(*rectangle);
            }
            update();
            return;
        }
        impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
        if (impl_->context->HasDetected()) {
            const occ::handle<AIS_InteractiveObject> detected =
                impl_->context->DetectedInteractive();
            for (const Impl::BodyPresentation& presentation : impl_->presentations) {
                if (presentation.shape == detected && impl_->bodySelectionHandler) {
                    impl_->bodySelectionHandler(presentation.id);
                    if (impl_->planarFaceSelectionHandler) {
                        const occ::handle<StdSelect_BRepOwner> owner =
                            occ::handle<StdSelect_BRepOwner>::DownCast(
                                impl_->context->DetectedOwner());
                        if (!owner.IsNull()) {
                            const auto selection =
                                planarFaceSelection(presentation.id, owner->Shape());
                            if (selection.has_value()) {
                                impl_->planarFaceSelectionHandler(*selection);
                            }
                        }
                    }
                    break;
                }
            }
        }
        impl_->context->SelectPoint(
            NCollection_Vec2<int>(position.x(), position.y()),
            impl_->view,
            AIS_SelectionScheme_Replace);
        update();
    }
    impl_->gesture = Impl::Gesture::none;
    unsetCursor();
}

void CadViewport::wheelEvent(QWheelEvent* event)
{
    if (!impl_->ready) {
        return;
    }
    constexpr double zoomStep = 1.15;
    impl_->view->SetZoom(event->angleDelta().y() > 0 ? zoomStep : 1.0 / zoomStep);
    update();
    event->accept();
}

} // namespace voelund::viewport
