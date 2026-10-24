// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchConstraintPresentation.hpp"

#include "SketchDimensionDrawing.hpp"

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

// W12: draws a dimension's lines and arrows; its value centred above the dimension line, or
// beside an upright one on its outer side.
SketchConstraintPresentation::Placement SketchConstraintPresentation::placeDimension(
    const occ::handle<AIS_InteractiveContext>& context, const SceneDimension& dimension,
    const Quantity_Color& color, double height, int characters) {
    const auto lines = drawDimension(dimension);
    occ::handle<AIS_InteractiveObject> drawing =
        makeDimensionGraphic(lines, dimension.normal, color);
    context->Display(drawing, 0, -1, false);
    const double width = 0.6 * height * characters;
    const int beside = dimension.textBeside;
    return {lines.text,
            beside > 0   ? 6.0
            : beside < 0 ? -width - 6.0
                         : -width / 2,
            beside != 0 ? -0.35 * height : 4.0, drawing};
}

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
        const Quantity_Color color = constraint.selected      ? selectedColor
                                     : constraint.conflicting ? conflictingColor
                                     : constraint.redundant   ? redundantColor
                                                              : labelColor_;
        const double height = constraint.selected ? 17.0 : 15.0;
        // W12: a dimension draws its lines and arrows; its value sits on the dimension line.
        // Relations keep their symbols beside the point, stacked.
        const std::string& shown =
            constraint.dimension ? constraint.dimension->text : constraint.label;
        const TCollection_ExtendedString text(shown.c_str(), true);
        Placement place{gp_Pnt(constraint.x, constraint.y, constraint.z), 12.0, 0.0, {}};
        if (constraint.dimension)
            place = placeDimension(context, *constraint.dimension, color, height, text.Length());
        else
            place.offsetY = -6.0 - 18.0 * stacked[key(constraint)]++;
        const auto& [anchor, offsetX, offsetY, drawing] = place;
        occ::handle<AIS_TextLabel> label = new OffsetLabel(offsetX, offsetY);
        label->SetText(text);
        label->SetPosition(anchor);
        label->SetColor(color);
        label->SetFont(Font_NOF_SANS_SERIF);
        label->SetHeight(height);
        label->SetZoomable(false);
        context->Display(label, false);
        constraints_.push_back({constraint.id, label, drawing, constraint.editable, anchor, offsetX,
                                offsetY, height, text.Length()});
    }
}

void SketchConstraintPresentation::clear(const occ::handle<AIS_InteractiveContext>& context) {
    if (!context.IsNull()) {
        for (const ConstraintPresentation& constraint : constraints_) {
            context->Remove(constraint.label, false);
            if (!constraint.drawing.IsNull())
                context->Remove(constraint.drawing, false);
        }
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
