// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/viewport/CadViewport.hpp>
#include <voelund/viewport/SketchDynamicInput.hpp>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <string_view>

namespace voelund::viewport {
namespace {

constexpr double degrees = 180.0 / std::numbers::pi;

// Same sign as `offset`; a zero offset counts as positive.
double signedLength(double length, double offset) {
    return offset < 0.0 ? -std::abs(length) : std::abs(length);
}

std::optional<double> number(std::string text) {
    for (auto& character : text)
        if (character == ',')
            character = '.';
    if (text.empty() || text == "-" || text == ".")
        return std::nullopt;
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(value))
        return std::nullopt;
    return value;
}

// Digits, a decimal point or comma; a sign or "=" only at the start.
bool numberCharacter(char character, bool empty) {
    const auto byte = static_cast<unsigned char>(character);
    return std::isdigit(byte) || character == '.' || character == ',' ||
           (empty && (character == '-' || character == '='));
}

// After "=": names, numbers, operators and brackets.
bool expressionCharacter(char character) {
    const auto byte = static_cast<unsigned char>(character);
    return std::isalnum(byte) ||
           std::string_view("._,+-*/()^ ").find(character) != std::string_view::npos;
}

// The pointer's direction; to the right while it sits on the first point.
double direction(double dx, double dy) { return dx == 0.0 && dy == 0.0 ? 0.0 : std::atan2(dy, dx); }

// A point at `radius` from `first` towards the pointer.
SketchDynamicInput::Point radialPoint(SketchDynamicInput::Point first, double dx, double dy,
                                      double radius) {
    const double angle = direction(dx, dy);
    return {first.first + radius * std::cos(angle), first.second + radius * std::sin(angle)};
}

} // namespace

void SketchDynamicInput::begin(SketchElementTool tool) {
    reset();
    centered_ = tool == SketchElementTool::centerRectangle;
    switch (tool) {
    case SketchElementTool::line:
    case SketchElementTool::polyline:
        fields_ = {{DynamicField::length}, {DynamicField::angle}};
        break;
    case SketchElementTool::rectangle:
    case SketchElementTool::centerRectangle:
        fields_ = {{DynamicField::width}, {DynamicField::height}};
        break;
    case SketchElementTool::circle:
        fields_ = {{DynamicField::diameter}};
        break;
    case SketchElementTool::arc:
    case SketchElementTool::ellipse:
    case SketchElementTool::polygon:
        fields_ = {{DynamicField::radius}};
        break;
    case SketchElementTool::slot:
        fields_ = {{DynamicField::length}};
        break;
    default:
        break;
    }
}

void SketchDynamicInput::beginThird(SketchElementTool tool) {
    reset();
    third_ = true;
    if (tool == SketchElementTool::arc)
        fields_ = {{DynamicField::sweep}};
    else if (tool == SketchElementTool::slot)
        fields_ = {{DynamicField::width}};
    else if (tool == SketchElementTool::ellipse)
        fields_ = {{DynamicField::minorRadius}};
    else
        third_ = false;
}

void SketchDynamicInput::beginStage(DynamicField kind) {
    fields_ = {Field{kind}};
    current_ = 0;
    centered_ = false;
    third_ = false;
}

void SketchDynamicInput::updateLiveValue(double value) {
    if (!fields_.empty())
        fields_.front().live = value;
}

void SketchDynamicInput::reset() noexcept {
    fields_.clear();
    current_ = 0;
    centered_ = false;
    third_ = false;
}

bool SketchDynamicInput::type(char character) {
    if (!active())
        return false;
    auto& text = fields_[current_].text;
    const bool expression = !text.empty() && text.front() == '=';
    if (!(expression ? expressionCharacter(character) : numberCharacter(character, text.empty())))
        return false;
    text.push_back(character);
    return true;
}

bool SketchDynamicInput::erase() {
    if (!active() || fields_[current_].text.empty())
        return false;
    fields_[current_].text.pop_back();
    return true;
}

bool SketchDynamicInput::next() {
    if (fields_.size() < 2)
        return false;
    current_ = (current_ + 1) % fields_.size();
    return true;
}

std::optional<double> SketchDynamicInput::parsed(const Field& field) const {
    if (field.text.empty())
        return std::nullopt;
    const auto value = field.text.front() == '='
                           ? (evaluator_ ? evaluator_(field.text) : std::optional<double>{})
                           : number(field.text);
    const bool signedValue = field.kind == DynamicField::angle || field.kind == DynamicField::sweep;
    if (!value || (!signedValue && *value <= 0.0))
        return std::nullopt;
    return value;
}

std::optional<double> SketchDynamicInput::value(DynamicField field) const {
    for (const auto& item : fields_)
        if (item.kind == field)
            return parsed(item);
    return std::nullopt;
}

bool SketchDynamicInput::hasTyped() const noexcept {
    for (const auto& field : fields_)
        if (!field.text.empty())
            return true;
    return false;
}

bool SketchDynamicInput::hasInvalidText() const {
    for (const auto& field : fields_)
        if (!field.text.empty() && !parsed(field))
            return true;
    return false;
}

SketchDynamicInput::Point SketchDynamicInput::constrain(Point first, Point pointer) const {
    const double dx = pointer.first - first.first;
    const double dy = pointer.second - first.second;
    if (value(DynamicField::length) || value(DynamicField::angle))
        return linePoint(first, dx, dy);
    if (const auto diameter = value(DynamicField::diameter))
        return radialPoint(first, dx, dy, *diameter / 2.0);
    if (const auto radius = value(DynamicField::radius))
        return radialPoint(first, dx, dy, *radius);
    const double scale = centered_ ? 0.5 : 1.0;
    const auto width = value(DynamicField::width);
    const auto height = value(DynamicField::height);
    return {first.first + (width ? signedLength(*width * scale, dx) : dx),
            first.second + (height ? signedLength(*height * scale, dy) : dy)};
}

SketchDynamicInput::Point SketchDynamicInput::linePoint(Point first, double dx, double dy) const {
    const double distance = value(DynamicField::length).value_or(std::hypot(dx, dy));
    const auto angle = value(DynamicField::angle);
    const double radians = angle ? *angle / degrees : direction(dx, dy);
    return {first.first + distance * std::cos(radians),
            first.second + distance * std::sin(radians)};
}

void SketchDynamicInput::updateLive(Point first, Point second) {
    const double dx = second.first - first.first;
    const double dy = second.second - first.second;
    const double scale = centered_ ? 2.0 : 1.0;
    for (auto& field : fields_) {
        switch (field.kind) {
        case DynamicField::length:
            field.live = std::hypot(dx, dy);
            break;
        case DynamicField::angle:
            field.live = direction(dx, dy) * degrees;
            break;
        case DynamicField::width:
            field.live = std::abs(dx) * scale;
            break;
        case DynamicField::height:
            field.live = std::abs(dy) * scale;
            break;
        case DynamicField::diameter:
            field.live = 2.0 * std::hypot(dx, dy);
            break;
        case DynamicField::radius:
            field.live = std::hypot(dx, dy);
            break;
        default: // the fields of the third point (updateLiveThird)
            break;
        }
    }
}

SketchDynamicInput::Point SketchDynamicInput::constrainThird(Point first, Point second,
                                                             Point pointer) const {
    const double ax = second.first - first.first;
    const double ay = second.second - first.second;
    const double length = std::hypot(ax, ay);
    if (length == 0.0)
        return pointer;
    // The side of the first two points the pointer is on.
    const double side =
        ax * (pointer.second - first.second) - ay * (pointer.first - first.first) < 0.0 ? -1.0
                                                                                        : 1.0;
    if (const auto sweep = value(DynamicField::sweep)) {
        // The end of the arc: the radius point turned by the sweep about the centre.
        const double turned = direction(ax, ay) + *sweep / degrees;
        return {first.first + length * std::cos(turned), first.second + length * std::sin(turned)};
    }
    const double nx = -ay / length * side;
    const double ny = ax / length * side;
    if (const auto width = value(DynamicField::width))
        return {second.first + nx * *width / 2.0, second.second + ny * *width / 2.0};
    if (const auto minor = value(DynamicField::minorRadius))
        return {first.first + nx * *minor, first.second + ny * *minor};
    return pointer;
}

void SketchDynamicInput::updateLiveThird(Point first, Point second, Point third) {
    const double ax = second.first - first.first;
    const double ay = second.second - first.second;
    const double length = std::hypot(ax, ay);
    const double bx = third.first - first.first;
    const double by = third.second - first.second;
    // Distance of the third point from the line through the first two.
    const double offset = length == 0.0 ? 0.0 : std::abs(ax * by - ay * bx) / length;
    for (auto& field : fields_) {
        if (field.kind == DynamicField::sweep) {
            double sweep = (direction(bx, by) - direction(ax, ay)) * degrees;
            if (sweep <= -180.0)
                sweep += 360.0;
            field.live = sweep > 180.0 ? sweep - 360.0 : sweep;
        } else if (field.kind == DynamicField::width)
            field.live = 2.0 * offset;
        else if (field.kind == DynamicField::minorRadius)
            field.live = offset;
    }
}

std::vector<SketchTypedDimension> SketchDynamicInput::typedDimensions() const {
    std::vector<SketchTypedDimension> typed;
    for (const auto& field : fields_)
        if (const auto parsedValue = parsed(field))
            typed.push_back({field.kind, *parsedValue, field.text});
    return typed;
}

} // namespace voelund::viewport
