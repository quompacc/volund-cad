// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace voelund::viewport {

enum class SketchElementTool;

// D01 (user report 2026-10-23, as in common CAD tools): values typed while drawing. After the
// first point, fields follow the pointer (line: length and angle; rectangle: width and height;
// circle: diameter). Typing goes into the current field, Tab moves to the next, a typed value
// is fixed while the pointer still sets the others, Enter places the element.
enum class DynamicField { length, angle, width, height, diameter };

// A value the user typed for an element: the shell turns it into a dimension. `text` is what
// was typed ("25", "=Breite*2"); `value` its value in mm or degrees.
struct SketchTypedDimension final {
    DynamicField field;
    double value;
    std::string text;
    bool operator==(const SketchTypedDimension&) const = default;
};

class SketchDynamicInput final {
  public:
    using Point = std::pair<double, double>;
    using Evaluator = std::function<std::optional<double>(const std::string&)>;
    struct Field final {
        DynamicField kind;
        double live{0.0};   // from the pointer, shown until something is typed
        std::string text{}; // typed so far
    };

    // The fields of `tool` once its first point is set; tools without fields get none.
    void begin(SketchElementTool tool);
    void reset() noexcept;
    [[nodiscard]] bool active() const noexcept { return !fields_.empty(); }
    [[nodiscard]] std::span<const Field> fields() const noexcept { return fields_; }
    [[nodiscard]] std::size_t current() const noexcept { return current_; }
    // A character for the current field (digits, sign, decimal point or comma, "=" and the
    // letters of an expression); false when it is not taken.
    bool type(char character);
    // Backspace: the last character of the current field; false when it is empty.
    bool erase();
    // Tab: the next field (around at the end); false when there is only one.
    bool next();
    // N01: "=Name*2" is resolved by the shell; without an evaluator only numbers count.
    void setEvaluator(Evaluator evaluator) { evaluator_ = std::move(evaluator); }
    // The typed value of a field, if it has one that parses (lengths positive).
    [[nodiscard]] std::optional<double> value(DynamicField field) const;
    [[nodiscard]] bool hasTyped() const noexcept;
    // Something typed that is no value: Enter is refused until it is fixed.
    [[nodiscard]] bool hasInvalidText() const;
    // The point that ends the element: typed values fixed, the others from `pointer`.
    [[nodiscard]] Point constrain(Point first, Point pointer) const;
    // The live values shown for the element from `first` to `second`.
    void updateLive(Point first, Point second);
    [[nodiscard]] std::vector<SketchTypedDimension> typedDimensions() const;

  private:
    [[nodiscard]] std::optional<double> parsed(const Field& field) const;
    [[nodiscard]] Point linePoint(Point first, double dx, double dy) const;

    std::vector<Field> fields_;
    std::size_t current_{0};
    bool centered_{false}; // a center rectangle: width and height are twice the offset
    Evaluator evaluator_;
};

} // namespace voelund::viewport
