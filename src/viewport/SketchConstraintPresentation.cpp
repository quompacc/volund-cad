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

#include "SketchGlyphOverlay.hpp"
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
const Quantity_Color drivenColor(0.5, 0.53, 0.58, Quantity_TOC_RGB);

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
    const Quantity_Color& color, double height, int characters) const {
    const auto lines = drawDimension(dimension);
    occ::handle<AIS_InteractiveObject> drawing =
        makeDimensionGraphic(lines, dimension.normal, color, pixelScale_);
    context->Display(drawing, 0, -1, false);
    const double width = 0.6 * height * characters;
    const int beside = dimension.textBeside;
    return {lines.text,
            beside > 0   ? 6.0 * pixelScale_
            : beside < 0 ? -width - 6.0 * pixelScale_
                         : -width / 2,
            beside != 0 ? -0.35 * height : 4.0 * pixelScale_, drawing};
}

void SketchConstraintPresentation::setLabelColor(const Quantity_Color& color) {
    labelColor_ = color;
}

Quantity_Color
SketchConstraintPresentation::colorOf(const SceneSketchConstraint& constraint) const {
    const bool driven = constraint.dimension && constraint.dimension->text.starts_with('(');
    return constraint.selected      ? selectedColor
           : constraint.conflicting ? conflictingColor
           : constraint.redundant   ? redundantColor
           : driven                 ? drivenColor // W21: a reference measures, grey as SolidCraft
                                    : labelColor_;
}

// W18: a relation with anchors is a symbol box, when there is an overlay to draw it.
bool SketchConstraintPresentation::asSymbol(const SceneSketchConstraint& constraint) const {
    return glyphs_ != nullptr && !constraint.glyphs.empty() && !constraint.dimension;
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
    std::vector<SketchGlyphOverlay::Glyph> glyphs;
    const auto key = [](const SceneSketchConstraint& constraint) {
        const auto round = [](double value) { return std::llround(value * 1.0e4); };
        return std::tuple{round(constraint.x), round(constraint.y), round(constraint.z)};
    };
    for (const SceneSketchConstraint& constraint : constraints) {
        if (asSymbol(constraint)) {
            glyphs.push_back({constraint.id, constraint.glyph, constraint.glyphs,
                              constraint.conflicting, constraint.redundant, constraint.selected,
                              constraint.elements});
            continue;
        }
        const Quantity_Color color = colorOf(constraint);
        // OCCT scales the text by the view's resolution; the pixel offsets and the hit box
        // below use the height on screen.
        const double textHeight = constraint.selected ? 17.0 : 15.0;
        const double height = textHeight * pixelScale_;
        // W12: a dimension draws its lines and arrows; its value sits on the dimension line.
        // Relations keep their symbols beside the point, stacked.
        const std::string& shown =
            constraint.dimension ? constraint.dimension->text : constraint.label;
        const TCollection_ExtendedString text(shown.c_str(), true);
        Placement place{
            gp_Pnt(constraint.x, constraint.y, constraint.z), 12.0 * pixelScale_, 0.0, {}};
        if (constraint.dimension)
            place = placeDimension(context, *constraint.dimension, color, height, text.Length());
        else
            place.offsetY = (-6.0 - 18.0 * stacked[key(constraint)]++) * pixelScale_;
        const auto& [anchor, offsetX, offsetY, drawing] = place;
        const auto label = showText(context, constraint, place, color, textHeight);
        constraints_.push_back({constraint.id, label, drawing, constraint.editable, anchor, offsetX,
                                offsetY, height, text.Length()});
    }
    glyphCount_ = glyphs.size();
    if (glyphs_ != nullptr) {
        glyphs_->setGlyphs(std::move(glyphs));
        glyphs_->setValues(std::move(values_));
    }
    values_.clear();
}

occ::handle<AIS_TextLabel> SketchConstraintPresentation::showText(
    const occ::handle<AIS_InteractiveContext>& context, const SceneSketchConstraint& constraint,
    const Placement& place, const Quantity_Color& color, double textHeight) {
    const std::string& shown = constraint.dimension ? constraint.dimension->text : constraint.label;
    if (constraint.dimension && glyphs_ != nullptr) {
        values_.push_back(
            {QString::fromStdString(shown), place.anchor.X(), place.anchor.Y(), place.anchor.Z(),
             QPointF(place.offsetX, place.offsetY),
             QColor::fromRgbF(static_cast<float>(color.Red()), static_cast<float>(color.Green()),
                              static_cast<float>(color.Blue())),
             textHeight});
        return {};
    }
    occ::handle<AIS_TextLabel> label = new OffsetLabel(place.offsetX, place.offsetY);
    label->SetText(TCollection_ExtendedString(shown.c_str(), true));
    label->SetPosition(place.anchor);
    label->SetColor(color);
    label->SetFont(Font_NOF_SANS_SERIF);
    label->SetHeight(textHeight);
    label->SetZoomable(false);
    context->Display(label, false);
    return label;
}

void SketchConstraintPresentation::clear(const occ::handle<AIS_InteractiveContext>& context) {
    if (!context.IsNull()) {
        for (const ConstraintPresentation& constraint : constraints_) {
            if (!constraint.label.IsNull())
                context->Remove(constraint.label, false);
            if (!constraint.drawing.IsNull())
                context->Remove(constraint.drawing, false);
        }
    }
    constraints_.clear();
    glyphCount_ = 0;
    if (glyphs_ != nullptr) {
        glyphs_->setGlyphs({});
        glyphs_->setValues({});
    }
}

bool SketchConstraintPresentation::hasDimension(std::uint64_t id) const {
    return std::ranges::any_of(constraints_, [id](const ConstraintPresentation& constraint) {
        return constraint.id == id && !constraint.drawing.IsNull();
    });
}

std::size_t SketchConstraintPresentation::size() const noexcept {
    return constraints_.size() + glyphCount_;
}

std::vector<std::array<double, 4>>
SketchConstraintPresentation::labelBoxes(const occ::handle<V3d_View>& view) const {
    std::vector<std::array<double, 4>> result;
    if (view.IsNull())
        return result;
    for (const ConstraintPresentation& constraint : constraints_) {
        int anchorX = 0, anchorY = 0;
        view->Convert(constraint.anchor.X(), constraint.anchor.Y(), constraint.anchor.Z(), anchorX,
                      anchorY);
        const double left = anchorX + constraint.offsetX;
        const double baseline = anchorY - constraint.offsetY;
        result.push_back({left, baseline - constraint.height,
                          left + 0.6 * constraint.height * std::max(1, constraint.characters),
                          baseline + 0.25 * constraint.height});
    }
    return result;
}

std::optional<std::uint64_t>
SketchConstraintPresentation::constraintAt(const occ::handle<V3d_View>& view, int x, int y) const {
    if (view.IsNull())
        return std::nullopt;
    if (glyphs_ != nullptr)
        if (const auto glyph = glyphs_->glyphAt(QPointF(x, y) / pixelScale_))
            return glyph; // W18: a relation's symbol box
    for (const ConstraintPresentation& constraint : constraints_) {
        int anchorX = 0, anchorY = 0;
        view->Convert(constraint.anchor.X(), constraint.anchor.Y(), constraint.anchor.Z(), anchorX,
                      anchorY);
        // The text box in pixels (screen y downwards): the baseline at the offset, the text
        // above it, a descender below, about 0.6 of the height per character wide; a few
        // pixels of tolerance around it.
        const double tolerance = 3.0 * pixelScale_;
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
