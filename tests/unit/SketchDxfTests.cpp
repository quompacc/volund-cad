// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/document/SketchDxf.hpp>

#include <cmath>
#include <iostream>
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// N04 (user decision 2026-10-22): a sketch with every element kind as DXF, read back by an own
// reader of group codes. A line, a rectangle (four lines), a circle, an arc from 0° to 90°, an
// ellipse turned by 30° with ratio 0.5, a point and a cubic B-spline of five poles (clamped
// knots 0 0 0 0 1 2 2 2 2) come out with their values in mm; a construction line lies on layer
// "Konstruktion". The header names AutoCAD 2000 and millimetres; the file ends with EOF.
namespace {
using namespace voelund::document;
using Pairs = std::vector<std::pair<int, std::string>>;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }

struct Entity final {
    std::string type;
    std::multimap<int, std::string> groups;
    [[nodiscard]] double number(int code, std::size_t index = 0) const {
        auto range = groups.equal_range(code);
        for (std::size_t skip = 0; skip < index && range.first != range.second; ++skip)
            ++range.first;
        require(range.first != range.second, type + " lacks group " + std::to_string(code));
        return std::stod(range.first->second);
    }
    [[nodiscard]] std::string text(int code) const { return groups.find(code)->second; }
};

Pairs pairsOf(const std::string& dxf) {
    std::istringstream input(dxf);
    Pairs pairs;
    for (std::string code, value; std::getline(input, code) && std::getline(input, value);)
        pairs.emplace_back(std::stoi(code), value);
    return pairs;
}

// The value of a header variable.
std::string header(const Pairs& pairs, const std::string& name) {
    for (std::size_t index = 0; index + 1 < pairs.size(); ++index)
        if (pairs[index] == std::pair{9, name})
            return pairs[index + 1].second;
    return {};
}

// The entities of the ENTITIES section, each with its groups.
std::vector<Entity> entitiesOf(const Pairs& pairs) {
    std::vector<Entity> entities;
    std::size_t index = 0;
    while (index < pairs.size() && pairs[index] != std::pair{2, std::string("ENTITIES")})
        ++index;
    for (++index; index < pairs.size() && pairs[index].second != "ENDSEC"; ++index) {
        if (pairs[index].first == 0)
            entities.push_back({pairs[index].second, {}});
        else
            entities.back().groups.emplace(pairs[index]);
    }
    return entities;
}

SketchElements everyKind() {
    SketchElements elements;
    std::uint64_t id = 1;
    const auto add = [&](SketchGeometry geometry, bool construction = false) {
        elements.values.push_back({SketchElementId{id++}, std::move(geometry), construction});
    };
    add(SketchLine{{0, 0}, {40, 0}});
    add(SketchRectangle{0, 10, 20, 30});
    add(SketchCircle{{50, 50}, 5});
    add(SketchArc{{0, 0}, 10, 0, std::numbers::pi / 2});
    add(SketchEllipse{{100, 0}, 20, 10, std::numbers::pi / 6});
    add(SketchPointGeometry{{7, 8}});
    add(SketchBSpline{{{0, 0}, {10, 10}, {20, 0}, {30, 10}, {40, 0}}, {}});
    add(SketchLine{{0, -5}, {40, -5}}, true);
    elements.nextId = id;
    return elements;
}

std::size_t count(const std::vector<Entity>& entities, const std::string& type) {
    std::size_t found = 0;
    for (const auto& entity : entities)
        found += entity.type == type ? 1 : 0;
    return found;
}
Entity first(const std::vector<Entity>& entities, const std::string& type) {
    for (const auto& entity : entities)
        if (entity.type == type)
            return entity;
    throw std::runtime_error("No " + type);
}

void headerAndKinds(const Pairs& pairs, const std::vector<Entity>& entities) {
    require(header(pairs, "$ACADVER") == "AC1015", "AutoCAD 2000");
    require(header(pairs, "$INSUNITS") == "4", "Millimetres");
    require(pairs.back() == std::pair{0, std::string("EOF")}, "EOF at the end");
    const std::pair<const char*, std::size_t> kinds[] = {
        {"LINE", 6}, {"CIRCLE", 1}, {"ARC", 1}, {"ELLIPSE", 1}, {"POINT", 1}, {"SPLINE", 1}};
    for (const auto& [type, expected] : kinds)
        require(count(entities, type) == expected, std::string("Count of ") + type);
    require(entities.size() == 11, "Eleven entities");
}

void lines(const std::vector<Entity>& entities) {
    const Entity line = entities.front();
    require(line.text(8) == "0" && near(line.number(10), 0) && near(line.number(11), 40),
            "The line from 0,0 to 40,0 on layer 0");
    const Entity construction = entities.back();
    require(construction.text(8) == "Konstruktion" && near(construction.number(20), -5),
            "The construction line on layer Konstruktion");
}

void curves(const std::vector<Entity>& entities) {
    const Entity circle = first(entities, "CIRCLE");
    require(near(circle.number(10), 50) && near(circle.number(40), 5), "Circle 50,50 r 5");
    const Entity arc = first(entities, "ARC");
    require(near(arc.number(40), 10) && near(arc.number(50), 0) && near(arc.number(51), 90),
            "Arc radius 10 from 0° to 90°");
    const Entity ellipse = first(entities, "ELLIPSE");
    require(near(ellipse.number(11), 20 * std::cos(std::numbers::pi / 6)) &&
                near(ellipse.number(21), 10) && near(ellipse.number(40), 0.5),
            "Ellipse: major axis 20 turned by 30°, ratio 0.5");
    const Entity point = first(entities, "POINT");
    require(near(point.number(10), 7) && near(point.number(20), 8), "Point 7,8");
}

void spline(const std::vector<Entity>& entities) {
    const Entity curve = first(entities, "SPLINE");
    require(near(curve.number(71), 3) && near(curve.number(72), 9) && near(curve.number(73), 5),
            "Spline: degree 3, nine knots, five poles");
    const double knots[] = {0, 0, 0, 0, 1, 2, 2, 2, 2};
    for (std::size_t index = 0; index < 9; ++index)
        require(near(curve.number(40, index), knots[index]), "Clamped uniform knots");
    require(near(curve.number(10, 4), 40) && near(curve.number(20, 1), 10),
            "Spline poles as in the sketch");
}
} // namespace

int main() {
    try {
        const auto pairs = pairsOf(sketchDxf(everyKind()));
        const auto entities = entitiesOf(pairs);
        headerAndKinds(pairs, entities);
        lines(entities);
        curves(entities);
        spline(entities);
        std::cout << "PASS N04 sketch DXF: every element kind with its values in mm, "
                     "construction on its own layer, AutoCAD 2000 header\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
