// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/features/SketchLooseEnds.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// W19 (as SolidCraft's palette): open ends touch nothing, untied ends lie on another end by
// position only, tied ends are placed by a relation; construction lines and closed curves have
// none. The freedom without relations counts each element's parameters.
namespace {
using namespace voelund::document;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
SketchElement line(std::uint64_t id, SketchPoint from, SketchPoint to, bool construction = false) {
    return {SketchElementId{id}, SketchLine{from, to}, construction};
}
} // namespace

int main() {
    try {
        using voelund::features::sketchFreedomWithoutRelations;
        using voelund::features::sketchLooseEnds;
        // An L of two lines meeting at (10, 0) by position only, a third line tied to the second's
        // end, a construction line and a circle.
        const std::vector<SketchElement> elements{
            line(1, {0, 0}, {10, 0}), line(2, {10, 0}, {10, 10}), line(3, {10, 10}, {0, 10}),
            line(4, {-5, -5}, {5, 5}, true),
            SketchElement{SketchElementId{5}, SketchCircle{{30, 30}, 4}}};
        const std::vector<SketchConstraint> constraints{
            {SketchConstraintId{1},
             SketchConstraintKind::coincident,
             {SketchElementId{2}, SketchPointPosition::end},
             SketchGeometryReference{SketchElementId{3}, SketchPointPosition::start}}};
        const auto ends = sketchLooseEnds(elements, constraints);
        require(ends.untied.size() == 1 && ends.untied.front().first.element.value() == 1 &&
                    ends.untied.front().second.element.value() == 2,
                "The corner at (10, 0) is untied: " + std::to_string(ends.untied.size()));
        require(ends.open.size() == 2,
                "The L's free start and the third line's free end are open: " +
                    std::to_string(ends.open.size()));
        require(sketchFreedomWithoutRelations(elements) == 4 * 4 + 3,
                "Four lines and a circle: 19 parameters");
        std::cout << "PASS sketch loose ends: open, untied, tied, construction, closed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
