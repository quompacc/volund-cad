// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorInteraction.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

// W13b (user report 2026-10-24, as in FreeCAD): a rectangle with rounded corners or a frame is
// drawn in steps. First the corners (width and height), then, with rounded corners, the corner
// radius - the pointer's distance from the nearest corner -, then, with a frame, its thickness
// - the pointer's distance from the nearest side, inwards inside the rectangle and outwards
// outside it. Each step takes a typed value; Enter or a click goes on, Escape goes back.
namespace voelund::viewport {
namespace {
using Point = std::pair<double, double>;

struct Box final {
    double low[2];
    double high[2];
    [[nodiscard]] double shorter() const { return std::min(high[0] - low[0], high[1] - low[1]); }
};

Box boxOf(Point first, Point second, bool centered) {
    if (centered)
        first = {2 * first.first - second.first, 2 * first.second - second.second};
    return {{std::min(first.first, second.first), std::min(first.second, second.second)},
            {std::max(first.first, second.first), std::max(first.second, second.second)}};
}

// The pointer's distance from the nearest corner, short of half the shorter side.
double cornerDistance(const Box& box, Point pointer) {
    double nearest = std::numeric_limits<double>::max();
    for (const double x : {box.low[0], box.high[0]})
        for (const double y : {box.low[1], box.high[1]})
            nearest = std::min(nearest, std::hypot(pointer.first - x, pointer.second - y));
    return std::clamp(nearest, 0.01, 0.499 * box.shorter());
}

// The pointer's distance from the nearest side: positive inside, negative outside.
double sideDistance(const Box& box, Point pointer) {
    const double inside = std::min({pointer.first - box.low[0], box.high[0] - pointer.first,
                                    pointer.second - box.low[1], box.high[1] - pointer.second});
    if (inside >= 0.0)
        return std::clamp(inside, 0.01, 0.499 * box.shorter());
    const double dx = std::max({box.low[0] - pointer.first, 0.0, pointer.first - box.high[0]});
    const double dy = std::max({box.low[1] - pointer.second, 0.0, pointer.second - box.high[1]});
    return -std::max(std::hypot(dx, dy), 0.01);
}

std::vector<Point> roundedOutline(const Box& box, double radius);

// The frame `width` off the box (inwards above zero), corners concentric with the box's.
std::optional<std::vector<Point>> frameOutline(const Box& box, double radius,
                                               std::optional<double> width) {
    if (!width)
        return std::nullopt;
    const Box frame{{box.low[0] + *width, box.low[1] + *width},
                    {box.high[0] - *width, box.high[1] - *width}};
    if (frame.shorter() <= 0.0)
        return std::nullopt;
    return roundedOutline(frame, std::max(0.0, radius - *width));
}

// A rectangle with corners of `radius`, as a closed polyline.
std::vector<Point> roundedOutline(const Box& box, double radius) {
    std::vector<Point> points;
    const std::array<Point, 4> centres{Point{box.high[0] - radius, box.high[1] - radius},
                                       Point{box.low[0] + radius, box.high[1] - radius},
                                       Point{box.low[0] + radius, box.low[1] + radius},
                                       Point{box.high[0] - radius, box.low[1] + radius}};
    constexpr int steps = 8;
    for (std::size_t corner = 0; corner < centres.size(); ++corner)
        for (int step = 0; step <= steps; ++step) {
            const double angle =
                std::numbers::pi / 2 * (static_cast<double>(corner) + double(step) / steps);
            points.push_back({centres[corner].first + radius * std::cos(angle),
                              centres[corner].second + radius * std::sin(angle)});
        }
    points.push_back(points.front());
    return points;
}
} // namespace

std::vector<Point> SketchEditorInteraction::onScreen(const std::vector<Point>& sketch) const {
    std::vector<Point> screen;
    for (const auto& point : sketch)
        if (const auto pixel = screenPoint(point.first, point.second))
            screen.push_back(*pixel);
    return screen;
}

bool SketchEditorInteraction::rectangleStaged() const noexcept {
    return (tool_ == SketchElementTool::rectangle || tool_ == SketchElementTool::centerRectangle) &&
           (rectangleRounded_ || rectangleFrame_);
}

double SketchEditorInteraction::stageValue(Point pointer) const {
    const Box box = boxOf(*firstPoint_, *secondPoint_, tool_ == SketchElementTool::centerRectangle);
    if (stage_ == RectangleStage::radius)
        return dynamic_.value(DynamicField::radius).value_or(cornerDistance(box, pointer));
    const double side = sideDistance(box, pointer);
    const auto typed = dynamic_.value(DynamicField::thickness);
    return typed ? (side < 0.0 ? -*typed : *typed) : side;
}

// The element of a later step: the corners fixed, the step's value from the pointer.
std::optional<SketchElementIntent> SketchEditorInteraction::stageIntent(Point pointer) const {
    SketchElementIntent intent{firstPoint_->first,   firstPoint_->second, secondPoint_->first,
                               secondPoint_->second, std::nullopt,        firstReference_,
                               secondReference_,     std::nullopt,        std::nullopt,
                               std::nullopt};
    intent.typed = cornerTyped_;
    intent.cornerRadius = cornerRadius_;
    if (stage_ == RectangleStage::radius)
        intent.cornerRadius = stageValue(pointer);
    else if (stage_ == RectangleStage::frame)
        intent.frameWidth = stageValue(pointer);
    dynamic_.updateLiveValue(std::abs(stageValue(pointer)));
    return valid(intent) ? std::optional{related(intent)} : std::nullopt;
}

// A click (or Enter at `pointer`) of a staged rectangle: the corners, then each step.
std::optional<SketchElementIntent>
SketchEditorInteraction::stageClick(Point pointer,
                                    std::optional<SketchPointReferenceIntent> reference) {
    if (stage_ == RectangleStage::corners) {
        auto corners = finalIntent(pointer, reference);
        if (!corners)
            return std::nullopt;
        secondPoint_ = {corners->secondX, corners->secondY};
        secondReference_ = corners->secondReference;
        cornerTyped_ = corners->typed;
        cornerRadius_.reset();
        stage_ = rectangleRounded_ ? RectangleStage::radius : RectangleStage::frame;
        dynamic_.beginStage(rectangleRounded_ ? DynamicField::radius : DynamicField::thickness);
        return std::nullopt;
    }
    if (stage_ == RectangleStage::radius && rectangleFrame_) {
        cornerRadius_ = stageValue(pointer);
        stage_ = RectangleStage::frame;
        dynamic_.beginStage(DynamicField::thickness);
        return std::nullopt;
    }
    auto intent = stageIntent(pointer);
    if (intent)
        finishElement();
    return intent;
}

// Escape: back to the step before.
bool SketchEditorInteraction::stageBack() {
    if (stage_ == RectangleStage::frame && rectangleRounded_) {
        stage_ = RectangleStage::radius;
        dynamic_.beginStage(DynamicField::radius);
        return true;
    }
    if (stage_ == RectangleStage::corners)
        return false;
    stage_ = RectangleStage::corners;
    secondPoint_.reset();
    secondReference_.reset();
    dynamic_.begin(tool_);
    return true;
}

// The rounded outline and the frame as the next click would place them, in framebuffer pixels.
std::vector<std::vector<Point>> SketchEditorInteraction::stageOutline(int x, int y) const {
    std::vector<std::vector<Point>> outlines;
    if (!rectangleStaged() || stage_ == RectangleStage::corners || !secondPoint_)
        return outlines;
    const auto pointer = interactionPoint(x, y);
    const auto intent = pointer ? stageIntent(pointer->position) : std::nullopt;
    if (!intent)
        return outlines;
    const Box box = boxOf(*firstPoint_, *secondPoint_, tool_ == SketchElementTool::centerRectangle);
    const double radius = intent->cornerRadius.value_or(0.0);
    outlines.push_back(onScreen(roundedOutline(box, radius)));
    if (const auto frame = frameOutline(box, radius, intent->frameWidth))
        outlines.push_back(onScreen(*frame));
    return outlines;
}

} // namespace voelund::viewport
