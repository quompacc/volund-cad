#include "SketchConstraintPresentation.hpp"

#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <TCollection_ExtendedString.hxx>
#include <gp_Pnt.hxx>

namespace voelund::viewport {
namespace {

const Quantity_Color regularColor(0.2, 0.85, 1.0, Quantity_TOC_RGB);
const Quantity_Color conflictingColor(1.0, 0.15, 0.12, Quantity_TOC_RGB);

} // namespace

void SketchConstraintPresentation::setConstraints(
    const occ::handle<AIS_InteractiveContext>& context,
    std::span<const SceneSketchConstraint> constraints) {
    clear(context);
    if (context.IsNull())
        return;
    constraints_.reserve(constraints.size());
    for (const SceneSketchConstraint& constraint : constraints) {
        occ::handle<AIS_TextLabel> label = new AIS_TextLabel();
        label->SetText(TCollection_ExtendedString(constraint.label.c_str(), true));
        label->SetPosition(gp_Pnt(constraint.x, constraint.y, constraint.z));
        label->SetColor(constraint.conflicting ? conflictingColor : regularColor);
        label->SetHeight(15.0);
        label->SetZoomable(false);
        context->Display(label, false);
        context->Activate(label, 0);
        constraints_.push_back({constraint.id, label, constraint.editable});
    }
}

void SketchConstraintPresentation::clear(const occ::handle<AIS_InteractiveContext>& context) {
    if (!context.IsNull()) {
        for (const ConstraintPresentation& constraint : constraints_)
            context->Remove(constraint.label, false);
    }
    constraints_.clear();
}

std::size_t SketchConstraintPresentation::size() const noexcept { return constraints_.size(); }

std::optional<std::uint64_t> SketchConstraintPresentation::detectedEditableConstraint(
    const occ::handle<AIS_InteractiveObject>& detected) const noexcept {
    for (const ConstraintPresentation& constraint : constraints_) {
        if (constraint.editable && constraint.label == detected)
            return constraint.id;
    }
    return std::nullopt;
}

} // namespace voelund::viewport
