// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/viewport/CadViewport.hpp>
#include <voelund/viewport/SketchDynamicInput.hpp>

#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// D01: the fields of each tool, typing, Tab, locked and free values, signs, expressions and
// refused text, without a window.
namespace {
using namespace voelund::viewport;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
void type(SketchDynamicInput& input, const std::string& text) {
    for (const char character : text)
        require(input.type(character), std::string("typed ") + character);
}

void line() {
    SketchDynamicInput input;
    input.begin(SketchElementTool::line);
    require(input.fields().size() == 2 && input.fields()[0].kind == DynamicField::length &&
                input.fields()[1].kind == DynamicField::angle,
            "A line has length and angle");
    input.updateLive({0, 0}, {3, 4});
    require(near(input.fields()[0].live, 5) && near(input.fields()[1].live, 53.13010235415598),
            "Live values follow the pointer");
    type(input, "40");
    // Length fixed, the angle follows the pointer.
    auto end = input.constrain({0, 0}, {0, 7});
    require(near(end.first, 0) && near(end.second, 40), "Length 40 along the pointer");
    require(input.next() && input.current() == 1, "Tab moves to the angle");
    type(input, "30");
    end = input.constrain({0, 0}, {-9, -9});
    require(near(end.first, 40 * std::cos(std::numbers::pi / 6)) && near(end.second, 20),
            "Length and angle fixed: the pointer no longer matters");
    const auto typed = input.typedDimensions();
    require(typed.size() == 2 && typed[0].field == DynamicField::length && near(typed[0].value, 40),
            "Both typed values are reported");
    require(input.erase() && input.erase() && !input.erase(), "Backspace empties the angle");
    require(input.next() && input.current() == 0, "Tab wraps to the length");
}

void rectangleAndCircle() {
    SketchDynamicInput input;
    input.begin(SketchElementTool::rectangle);
    type(input, "50");
    auto corner = input.constrain({10, 10}, {2, 15});
    require(near(corner.first, -40) && near(corner.second, 15),
            "A fixed width keeps the side the pointer is on; the height stays free");
    SketchDynamicInput centered;
    centered.begin(SketchElementTool::centerRectangle);
    type(centered, "50");
    corner = centered.constrain({0, 0}, {3, 3});
    require(near(corner.first, 25), "A center rectangle's width is twice the offset");
    SketchDynamicInput circle;
    circle.begin(SketchElementTool::circle);
    require(circle.fields().size() == 1 && !circle.next(), "A circle has one field");
    type(circle, "16");
    corner = circle.constrain({0, 0}, {0, -3});
    require(near(corner.first, 0) && near(corner.second, -8),
            "Diameter 16: radius 8 to the pointer");
    SketchDynamicInput none;
    none.begin(SketchElementTool::arc);
    require(!none.active() && !none.type('5'), "Tools without fields take no keys");
}

void textRules() {
    SketchDynamicInput input;
    input.begin(SketchElementTool::line);
    require(!input.type('x'), "Letters are tool shortcuts, not values");
    type(input, "2,5");
    require(input.value(DynamicField::length) == 2.5, "A decimal comma counts");
    input.erase();
    input.erase();
    input.erase();
    type(input, "-3");
    require(!input.value(DynamicField::length) && input.hasInvalidText(),
            "A negative length is refused and blocks Enter");
    input.erase();
    input.erase();
    type(input, "=B*2");
    require(input.hasInvalidText(), "An expression needs the shell to resolve it");
    input.setEvaluator([](const std::string& text) -> std::optional<double> {
        return text == "=B*2" ? std::optional{24.0} : std::nullopt;
    });
    require(input.value(DynamicField::length) == 24.0 && !input.hasInvalidText(),
            "The shell resolves =B*2 to 24");
    require(input.typedDimensions().front().text == "=B*2", "The typed text is kept");
}
} // namespace

int main() {
    try {
        line();
        rectangleAndCircle();
        textRules();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS D01 dynamic input fields\n";
    return 0;
}
