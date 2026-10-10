#include "TransformManipulator.hpp"
#include "RotationInput.hpp"
#include <voelund/kernel/Angle.hpp>

#include <AIS_Manipulator.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gp_XYZ.hxx>

#include <cmath>
#include <numbers>
#include <utility>

namespace voelund::viewport {

class TransformManipulator::Impl final {
public:
    Impl()
        : manipulator(new AIS_Manipulator())
    {
        manipulator->SetPart(AIS_MM_Scaling, false);
        manipulator->SetModeActivationOnDetection(true);
        manipulator->SetZoomPersistence(true);
        manipulator->SetSize(80.0f);
    }

    [[nodiscard]] gp_Trsf snappedTransformation(const gp_Trsf& transformation) const
    {
        const AIS_ManipulatorMode mode = manipulator->ActiveMode();
        if (mode == AIS_MM_Translation || mode == AIS_MM_TranslationPlane) {
            const gp_XYZ translation = transformation.TranslationPart();
            gp_Trsf snapped = transformation;
            snapped.SetTranslationPart(gp_Vec(
                std::round(translation.X() / linearSnapStep) * linearSnapStep,
                std::round(translation.Y() / linearSnapStep) * linearSnapStep,
                std::round(translation.Z() / linearSnapStep) * linearSnapStep));
            return snapped;
        }
        if (mode != AIS_MM_Rotation) {
            return transformation;
        }
        return detail::snapRotation(transformation, transformPivot,
                                    kernel::Degrees(angularSnapStep));
    }

    [[nodiscard]] bool rotationDelta(
        const gp_Trsf& transformation,
        int axisIndex,
        double& angleDegrees) const
    {
        const auto delta = detail::rotationDeltaDegrees(transformation, axisIndex);
        if (!delta) {
            return false;
        }
        angleDegrees = delta->value();
        return true;
    }

    void notifyPreview(const gp_Trsf& transformation) const
    {
        const AIS_ManipulatorMode mode = manipulator->ActiveMode();
        if ((mode == AIS_MM_Translation || mode == AIS_MM_TranslationPlane)
            && movePreviewHandler) {
            const gp_XYZ translation = transformation.TranslationPart();
            movePreviewHandler(
                bodyId, translation.X(), translation.Y(), translation.Z());
            return;
        }
        const int axisIndex = manipulator->ActiveAxisIndex();
        double angleDegrees = 0.0;
        if (mode == AIS_MM_Rotation && rotationPreviewHandler
            && rotationDelta(transformation, axisIndex, angleDegrees)) {
            rotationPreviewHandler(bodyId, axisIndex, angleDegrees);
        }
    }

    void commit(const AIS_ManipulatorMode mode, int axisIndex, const gp_Trsf& transformation)
    {
        constexpr double movementTolerance = 1.0e-9;
        const gp_XYZ translation = transformation.TranslationPart();
        if ((mode == AIS_MM_Translation || mode == AIS_MM_TranslationPlane)
            && translation.SquareModulus() > movementTolerance && moveHandler) {
            moveHandler(bodyId, translation.X(), translation.Y(), translation.Z());
            return;
        }
        double angleDegrees = 0.0;
        if (mode == AIS_MM_Rotation && rotationHandler
            && rotationDelta(transformation, axisIndex, angleDegrees)
            && std::abs(angleDegrees) > movementTolerance) {
            rotationHandler(bodyId, axisIndex, angleDegrees);
        }
    }

    occ::handle<AIS_Manipulator> manipulator;
    MoveHandler moveHandler;
    MoveHandler movePreviewHandler;
    RotationHandler rotationHandler;
    RotationHandler rotationPreviewHandler;
    std::uint64_t bodyId{0};
    bool enabled{false};
    bool snappingEnabled{true};
    double linearSnapStep{1.0};
    double angularSnapStep{5.0};
    gp_Pnt transformPivot;
};

TransformManipulator::TransformManipulator()
    : impl_(std::make_unique<Impl>())
{
}

TransformManipulator::~TransformManipulator()
{
    detach();
}

void TransformManipulator::setEnabled(bool enabled)
{
    impl_->enabled = enabled;
    if (!enabled) {
        detach();
    }
}

bool TransformManipulator::isEnabled() const noexcept
{
    return impl_->enabled;
}

void TransformManipulator::attach(
    const occ::handle<AIS_Shape>& presentation,
    std::uint64_t bodyId)
{
    if (!impl_->enabled) {
        return;
    }
    detach();
    impl_->manipulator->SetPart(AIS_MM_Translation, true);
    impl_->manipulator->SetPart(AIS_MM_TranslationPlane, true);
    impl_->manipulator->SetPart(AIS_MM_Rotation, true);
    impl_->manipulator->Attach(presentation);
    impl_->manipulator->EnableMode(AIS_MM_Translation);
    impl_->manipulator->EnableMode(AIS_MM_TranslationPlane);
    impl_->manipulator->EnableMode(AIS_MM_Rotation);
    impl_->bodyId = bodyId;
}

void TransformManipulator::detach()
{
    if (impl_->manipulator->IsAttached()) {
        impl_->manipulator->Detach();
    }
}

void TransformManipulator::resetPresentation() {
    detach();
    impl_->manipulator = new AIS_Manipulator();
    impl_->manipulator->SetPart(AIS_MM_Scaling, false);
    impl_->manipulator->SetModeActivationOnDetection(true);
    impl_->manipulator->SetZoomPersistence(true);
    impl_->manipulator->SetSize(80.0f);
}

bool TransformManipulator::matches(
    const occ::handle<AIS_InteractiveObject>& interactiveObject) const noexcept
{
    return interactiveObject == impl_->manipulator;
}

bool TransformManipulator::hasActiveMode() const noexcept
{
    return impl_->manipulator->HasActiveMode();
}

void TransformManipulator::start(int x, int y, const occ::handle<V3d_View>& view)
{
    impl_->transformPivot = impl_->manipulator->Position().Location();
    impl_->manipulator->StartTransform(x, y, view);
}

void TransformManipulator::update(
    int x,
    int y,
    const occ::handle<V3d_View>& view,
    bool bypassSnapping)
{
    if (!impl_->snappingEnabled || bypassSnapping) {
        impl_->manipulator->Transform(x, y, view);
    } else {
        gp_Trsf transformation;
        if (!impl_->manipulator->ObjectTransformation(x, y, view, transformation)) {
            return;
        }
        impl_->manipulator->Transform(impl_->snappedTransformation(transformation));
    }
    impl_->notifyPreview(impl_->manipulator->Object()->LocalTransformation());
}

void TransformManipulator::finish()
{
    const AIS_ManipulatorMode mode = impl_->manipulator->ActiveMode();
    const int axisIndex = impl_->manipulator->ActiveAxisIndex();
    const gp_Trsf transformation = impl_->manipulator->Object()->LocalTransformation();
    impl_->manipulator->StopTransform(false);
    impl_->manipulator->DeactivateCurrentMode();
    impl_->commit(mode, axisIndex, transformation);
}

void TransformManipulator::cancel()
{
    impl_->notifyPreview(gp_Trsf());
    impl_->manipulator->StopTransform(false);
    impl_->manipulator->DeactivateCurrentMode();
}

void TransformManipulator::setMoveHandler(MoveHandler handler)
{
    impl_->moveHandler = std::move(handler);
}

void TransformManipulator::setMovePreviewHandler(MoveHandler handler)
{
    impl_->movePreviewHandler = std::move(handler);
}

void TransformManipulator::setRotationHandler(RotationHandler handler)
{
    impl_->rotationHandler = std::move(handler);
}

void TransformManipulator::setRotationPreviewHandler(RotationHandler handler)
{
    impl_->rotationPreviewHandler = std::move(handler);
}

void TransformManipulator::setSnapping(bool enabled, double linearStep, double angularStep)
{
    impl_->snappingEnabled = enabled;
    if (linearStep > 0.0) {
        impl_->linearSnapStep = linearStep;
    }
    if (angularStep > 0.0) {
        impl_->angularSnapStep = angularStep;
    }
}

} // namespace voelund::viewport
