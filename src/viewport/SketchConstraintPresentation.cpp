// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchConstraintPresentation.hpp"

#include <Font_NameOfFont.hxx>
#include <Graphic3d_Group.hxx>
#include <Graphic3d_TransformPers.hxx>
#include <Prs3d_Text.hxx>
#include <Prs3d_TextAspect.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <SelectMgr_Selection.hxx>
#include <TCollection_ExtendedString.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace voelund::viewport {
namespace {

const Quantity_Color conflictingColor(1.0, 0.15, 0.12, Quantity_TOC_RGB);
// S13: yellow, so a redundant label differs from a selected (orange) one.
const Quantity_Color redundantColor(0.95, 0.85, 0.1, Quantity_TOC_RGB);
const Quantity_Color selectedColor(1.0, 0.55, 0.05, Quantity_TOC_RGB);

// U11 (A-VIS-01): a label drawn a few screen pixels beside its anchor. Labels on one point
// (a corner's coincidence and coordinates, a side's direction and length) are stacked below
// each other instead of on top of each other, at every zoom.
class OffsetLabel final : public AIS_TextLabel {
  public:
    OffsetLabel(double offsetX, double offsetY) : offset_(offsetX, offsetY, 0.0) {}

  protected:
    void Compute(const occ::handle<PrsMgr_PresentationManager>&,
                 const occ::handle<Prs3d_Presentation>& presentation, const int mode) override {
        if (mode != 0)
            return;
        // Zoom and rotation persistent: local units are screen pixels around the anchor.
        SetTransformPersistence(
            new Graphic3d_TransformPers(Graphic3d_TMF_ZoomRotatePers, Position()));
        const auto text =
            Prs3d_Text::Draw(presentation->CurrentGroup(), myDrawer->TextAspect(), Text(), offset_);
        static_cast<void>(text);
    }
    // Picked by SketchConstraintPresentation::constraintAt, not by OCCT.
    void ComputeSelection(const occ::handle<SelectMgr_Selection>&, const int) override {}

  private:
    gp_Pnt offset_;
};

} // namespace

void SketchConstraintPresentation::setLabelColor(const Quantity_Color& color) {
    labelColor_ = color;
}

void SketchConstraintPresentation::setConstraints(
    const occ::handle<AIS_InteractiveContext>& context,
    std::span<const SceneSketchConstraint> constraints) {
    clear(context);
    if (context.IsNull())
        return;
    constraints_.reserve(constraints.size());
    // Labels on one anchor stack downwards, right of the point marker.
    std::map<std::tuple<long long, long long, long long>, int> stacked;
    const auto key = [](const SceneSketchConstraint& constraint) {
        const auto round = [](double value) { return std::llround(value * 1.0e4); };
        return std::tuple{round(constraint.x), round(constraint.y), round(constraint.z)};
    };
    for (const SceneSketchConstraint& constraint : constraints) {
        const int row = stacked[key(constraint)]++;
        const double offsetX = 12.0, offsetY = -6.0 - 18.0 * row;
        const double height = constraint.selected ? 17.0 : 15.0;
        occ::handle<AIS_TextLabel> label = new OffsetLabel(offsetX, offsetY);
        label->SetText(TCollection_ExtendedString(constraint.label.c_str(), true));
        label->SetPosition(gp_Pnt(constraint.x, constraint.y, constraint.z));
        label->SetColor(constraint.selected      ? selectedColor
                        : constraint.conflicting ? conflictingColor
                        : constraint.redundant   ? redundantColor
                                                 : labelColor_);
        label->SetFont(Font_NOF_SANS_SERIF);
        label->SetHeight(height);
        label->SetZoomable(false);
        context->Display(label, false);
        const TCollection_ExtendedString text(constraint.label.c_str(), true);
        constraints_.push_back({constraint.id, label, constraint.editable,
                                gp_Pnt(constraint.x, constraint.y, constraint.z), offsetX, offsetY,
                                height, text.Length()});
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

std::optional<std::uint64_t>
SketchConstraintPresentation::constraintAt(const occ::handle<V3d_View>& view, int x, int y) const {
    if (view.IsNull())
        return std::nullopt;
    for (const ConstraintPresentation& constraint : constraints_) {
        int anchorX = 0, anchorY = 0;
        view->Convert(constraint.anchor.X(), constraint.anchor.Y(), constraint.anchor.Z(), anchorX,
                      anchorY);
        // The text box in pixels (screen y downwards): the baseline at the offset, the text
        // above it, a descender below, about 0.6 of the height per character wide; a few
        // pixels of tolerance around it.
        const double tolerance = 3.0;
        const double left = anchorX + constraint.offsetX - tolerance;
        const double right = anchorX + constraint.offsetX +
                             0.6 * constraint.height * std::max(1, constraint.characters) +
                             tolerance;
        const double baseline = anchorY - constraint.offsetY;
        const double top = baseline - constraint.height - tolerance;
        const double bottom = baseline + 0.25 * constraint.height + tolerance;
        if (x >= left && x <= right && y >= top && y <= bottom)
            return constraint.id;
    }
    return std::nullopt;
}

} // namespace voelund::viewport
