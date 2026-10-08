#include "PushPullHandle.hpp"

#include <AIS_Manipulator.hxx>
#include <AIS_Shape.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Ax2.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>

#include <cmath>
#include <utility>

namespace voelund::viewport {

class PushPullHandle::Impl final {
  public:
    [[nodiscard]] gp_Trsf along(double length) const {
        gp_Trsf shift;
        shift.SetTranslation(gp_Vec(normal) * length);
        return shift;
    }
    // The arrow points out of the body for a pull and into it for a push (user request
    // 2026-10-08); the distance is always measured along the outward normal.
    void place(double value) {
        distance = value;
        context->SetLocation(face, TopLoc_Location(along(value)));
        manipulator->SetPosition(
            gp_Ax2(center.Translated(gp_Vec(normal) * value), value < 0 ? normal.Reversed() : normal));
        context->Redisplay(manipulator, false);
    }

    occ::handle<AIS_InteractiveContext> context;
    occ::handle<AIS_Manipulator> manipulator;
    occ::handle<AIS_Shape> face;
    gp_Pnt center;
    gp_Dir normal{0, 0, 1};
    double distance{0.0};
    double startDistance{0.0};
    Handler handler;
    bool snapping{true};
    double step{1.0};
};

PushPullHandle::PushPullHandle() : impl_(std::make_unique<Impl>()) {}

PushPullHandle::~PushPullHandle() = default;

void PushPullHandle::show(const occ::handle<AIS_InteractiveContext>& context,
                          const TopoDS_Shape& face, const gp_Dir& normal,
                          const Quantity_Color& color) {
    hide();
    auto& impl = *impl_;
    impl.context = context;
    impl.normal = normal;
    impl.distance = 0.0;
    GProp_GProps properties;
    BRepGProp::SurfaceProperties(face, properties);
    impl.center = properties.CentreOfMass();
    impl.face = new AIS_Shape(face);
    impl.face->SetColor(color);
    impl.face->SetTransparency(0.35);
    context->Display(impl.face, false);
    context->Deactivate(impl.face);
    impl.manipulator = new AIS_Manipulator();
    impl.manipulator->SetPart(AIS_MM_Scaling, false);
    impl.manipulator->SetPart(AIS_MM_Rotation, false);
    impl.manipulator->SetPart(AIS_MM_TranslationPlane, false);
    impl.manipulator->SetPart(0, AIS_MM_Translation, false);
    impl.manipulator->SetPart(1, AIS_MM_Translation, false);
    impl.manipulator->SetModeActivationOnDetection(true);
    impl.manipulator->SetZoomPersistence(true);
    impl.manipulator->SetSize(120.0f);
    AIS_Manipulator::OptionsForAttach options;
    options.SetAdjustPosition(false).SetAdjustSize(false).SetEnableModes(false);
    impl.manipulator->Attach(impl.face, options);
    impl.manipulator->SetPosition(gp_Ax2(impl.center, normal));
    impl.manipulator->EnableMode(AIS_MM_Translation);
}

void PushPullHandle::hide() {
    auto& impl = *impl_;
    if (!impl.manipulator.IsNull() && impl.manipulator->IsAttached())
        impl.manipulator->Detach();
    if (!impl.context.IsNull()) {
        if (!impl.manipulator.IsNull())
            impl.context->Remove(impl.manipulator, false);
        if (!impl.face.IsNull())
            impl.context->Remove(impl.face, false);
    }
    impl.manipulator.Nullify();
    impl.face.Nullify();
    impl.context.Nullify();
}

void PushPullHandle::reset() {
    impl_->manipulator.Nullify();
    impl_->face.Nullify();
    impl_->context.Nullify();
}

bool PushPullHandle::isShown() const noexcept { return !impl_->face.IsNull(); }

void PushPullHandle::setDistance(double distance) {
    if (isShown() && distance != impl_->distance)
        impl_->place(distance);
}

gp_Dir PushPullHandle::arrowDirection() const {
    return impl_->manipulator.IsNull() ? impl_->normal
                                       : impl_->manipulator->Position().Direction();
}

bool PushPullHandle::matches(const occ::handle<AIS_InteractiveObject>& object) const noexcept {
    return !impl_->manipulator.IsNull() && object == impl_->manipulator;
}

bool PushPullHandle::hasActiveMode() const noexcept {
    return !impl_->manipulator.IsNull() && impl_->manipulator->HasActiveMode();
}

void PushPullHandle::start(int x, int y, const occ::handle<V3d_View>& view) {
    impl_->startDistance = impl_->distance;
    impl_->manipulator->StartTransform(x, y, view);
}

void PushPullHandle::update(int x, int y, const occ::handle<V3d_View>& view,
                            bool bypassSnapping) {
    auto& impl = *impl_;
    gp_Trsf delta;
    if (!impl.manipulator->ObjectTransformation(x, y, view, delta))
        return;
    const double raw =
        impl.startDistance + gp_Vec(delta.TranslationPart()).Dot(gp_Vec(impl.normal));
    const double value = impl.snapping && !bypassSnapping && impl.step > 0.0
                             ? std::round(raw / impl.step) * impl.step
                             : raw;
    impl.manipulator->Transform(impl.along(value - impl.startDistance));
    impl.distance = value;
    if (impl.handler)
        impl.handler(value, false);
}

void PushPullHandle::finish() {
    auto& impl = *impl_;
    impl.manipulator->StopTransform(true);
    impl.manipulator->DeactivateCurrentMode();
    impl.place(impl.distance); // Turns the arrow when the drag crossed the face.
    if (impl.handler)
        impl.handler(impl.distance, true);
}

void PushPullHandle::setHandler(Handler handler) { impl_->handler = std::move(handler); }

void PushPullHandle::setSnapping(bool enabled, double step) {
    impl_->snapping = enabled;
    if (step > 0.0)
        impl_->step = step;
}

} // namespace voelund::viewport
