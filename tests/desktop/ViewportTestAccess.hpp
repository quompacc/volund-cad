#pragma once
#include "../../src/viewport/CadViewportQtOpenGlBackend.hpp"
#include <AIS_Manipulator.hxx>
#include <Aspect_Window.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <cmath>
#include <stdexcept>

namespace voelund::viewport {
class ViewportTestAccess {
  public:
    static std::uint64_t frames(CadViewport& v) { return v.impl_->renderedFrames; }
    static void cleanup(CadViewport& v) { v.cleanupGlResources(); }
    static void initialize(CadViewport& v, bool fail = false) {
        v.impl_->failNextInitialization = fail;
        v.makeCurrent();
        v.initializeGL();
        v.doneCurrent();
        v.update();
    }
    static std::string error(CadViewport& v) { return v.impl_->lastError; }
    static int selectedCount(CadViewport& v) {
        int count = 0;
        const auto& context = v.impl_->context;
        for (context->InitSelected(); context->MoreSelected(); context->NextSelected())
            ++count;
        return count;
    }
    // Shape type of the selected owner: a face pick must not select the whole body.
    static std::optional<TopAbs_ShapeEnum> selectedShapeType(CadViewport& v) {
        const auto& context = v.impl_->context;
        context->InitSelected();
        if (!context->MoreSelected())
            return std::nullopt;
        const auto owner = occ::handle<StdSelect_BRepOwner>::DownCast(context->SelectedOwner());
        if (owner.IsNull())
            return std::nullopt;
        return owner->Shape().ShapeType();
    }
    static std::optional<Quantity_Color> sketchElementColor(CadViewport& v, std::uint64_t id) {
        return v.impl_->sketchPresentation.color(id);
    }
    static std::optional<bool> sketchElementDashed(CadViewport& v, std::uint64_t id) {
        return v.impl_->sketchPresentation.dashed(id);
    }
    static occ::handle<Graphic3d_Camera> camera(CadViewport& v) {
        return new Graphic3d_Camera(v.impl_->view->Camera());
    }
    static bool sceneRestored(CadViewport& v) {
        return v.impl_->presentations.size() == 1 && v.impl_->presentations.front().id == 42 &&
               v.impl_->selectedSketchElements == std::vector<std::uint64_t>{7} &&
               v.displayedSketchElementCount() == 1 && v.displayedSketchConstraintCount() == 1;
    }
    // T04c: the pointer rests on the push/pull arrow (after a MouseMove).
    static bool pushPullArrowHovered(CadViewport& v) {
        const auto& context = v.impl_->context;
        return context->HasDetected() && v.impl_->pushPull.matches(context->DetectedInteractive()) &&
               v.impl_->pushPull.hasActiveMode();
    }
    static std::size_t bodyCount(CadViewport& v) { return v.impl_->presentations.size(); }
    static double pushPullArrowZ(CadViewport& v) { return v.impl_->pushPull.arrowDirection().Z(); }
    static QPoint project(CadViewport& v, double x, double y, double z) {
        int px = 0, py = 0;
        v.impl_->view->Convert(x, y, z, px, py);
        return (QPointF(px, py) / v.devicePixelRatioF()).toPoint();
    }
    static QSize windowSize(CadViewport& v) {
        int w = 0, h = 0;
        v.impl_->view->Window()->Size(w, h);
        return {w, h};
    }
    static double windowRatio(CadViewport& v) {
        return v.impl_->view->Window()->DevicePixelRatio();
    }
    static QRect selectionBand(CadViewport& v) { return v.impl_->selectionBand->geometry(); }
    // S13: what the view switches change.
    static bool gridVisible(CadViewport& v) { return v.impl_->gridVisible; }
    static std::size_t constraintLabels(CadViewport& v) {
        return v.impl_->sketchConstraintPresentation.size();
    }
    static std::size_t projectedEdges(CadViewport& v) {
        return v.impl_->sketchPresentation.projectedEdgeCount();
    }
    static bool bindFramebuffer(CadViewport& v, double ratio) {
        return qtgl::bindCurrentFramebuffer(v.impl_->view, ratio);
    }
    static std::optional<QPoint> translationHandle(CadViewport& v) {
        v.makeCurrent();
        const QPoint center = project(v, 10, 10, 10);
        for (int y = -90; y <= 90; y += 3) {
            for (int x = -90; x <= 90; x += 3) {
                const QPoint point = center + QPoint(x, y);
                const QPoint physical = (QPointF(point) * v.devicePixelRatioF()).toPoint();
                v.impl_->context->MoveTo(physical.x(), physical.y(), v.impl_->view, false);
                const auto detected = v.impl_->context->DetectedInteractive();
                const auto manipulator = occ::handle<AIS_Manipulator>::DownCast(detected);
                if (!manipulator.IsNull() && manipulator->ActiveMode() == AIS_MM_Translation) {
                    v.doneCurrent();
                    return point;
                }
            }
        }
        v.doneCurrent();
        return std::nullopt;
    }
    static QPoint translationDestination(CadViewport& v, QPoint start) {
        const auto manipulator =
            occ::handle<AIS_Manipulator>::DownCast(v.impl_->context->DetectedInteractive());
        if (manipulator.IsNull())
            throw std::runtime_error("No detected manipulator for translation");
        const int axis = manipulator->ActiveAxisIndex();
        const QPoint origin = project(v, 10, 10, 10);
        const QPoint along =
            project(v, axis == 0 ? 30 : 10, axis == 1 ? 30 : 10, axis == 2 ? 30 : 10) - origin;
        const double length = std::hypot(along.x(), along.y());
        if (length < 1)
            throw std::runtime_error("Translation axis projects to a point");
        return start + (QPointF(along) * (80.0 / length)).toPoint();
    }
};
} // namespace voelund::viewport
