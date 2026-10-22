// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/features/SketchProfiles.hpp>

#include <iostream>
#include <numbers>

namespace {

voelund::document::SketchElement line(std::uint64_t id, double x1, double y1, double x2,
                                      double y2) {
    return {voelund::document::SketchElementId{id},
            voelund::document::SketchLine{{x1, y1}, {x2, y2}}};
}

bool detectsPrimitiveAndLineProfiles() {
    const std::vector<voelund::document::SketchElement> elements{
        {voelund::document::SketchElementId{10},
         voelund::document::SketchRectangle{0.0, 0.0, 4.0, 3.0}},
        {voelund::document::SketchElementId{11}, voelund::document::SketchCircle{{8.0, 2.0}, 1.5}},
        line(20, 0.0, 6.0, 3.0, 6.0),
        line(21, 3.0, 9.0, 3.0, 6.0),
        line(22, 0.0, 9.0, 3.0, 9.0),
        line(23, 0.0, 6.0, 0.0, 9.0),
        line(30, 10.0, 10.0, 12.0, 10.0)};
    const auto profiles = voelund::features::detectClosedProfiles(elements);
    return profiles.size() == 3 && profiles[0].id.value() == 10 && profiles[1].id.value() == 11 &&
           profiles[2].id.value() == 20 && profiles[2].elementIds.size() == 4;
}

bool openLinesDoNotFormAProfile() {
    const std::vector<voelund::document::SketchElement> elements{line(1, 0.0, 0.0, 1.0, 0.0),
                                                                 line(2, 1.0, 0.0, 1.0, 1.0)};
    return voelund::features::detectClosedProfiles(elements).empty();
}

bool constructionGeometryDoesNotFormProfiles() {
    const std::vector<voelund::document::SketchElement> elements{
        {voelund::document::SketchElementId{1},
         voelund::document::SketchRectangle{0.0, 0.0, 4.0, 3.0}, true},
        {voelund::document::SketchElementId{2}, voelund::document::SketchCircle{{8.0, 2.0}, 1.5},
         true},
        {voelund::document::SketchElementId{3},
         voelund::document::SketchLine{{0.0, 6.0}, {3.0, 6.0}}, true},
        line(4, 3.0, 6.0, 3.0, 9.0),
        line(5, 3.0, 9.0, 0.0, 9.0),
        line(6, 0.0, 9.0, 0.0, 6.0)};
    return voelund::features::detectClosedProfiles(elements).empty();
}

bool arcAndLineFormAnEvaluatedProfile() {
    using namespace voelund;
    document::SketchParameters sketch{
        {0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0},
        {{{document::SketchElementId{1},
           document::SketchArc{{0.0, 0.0}, 2.0, 0.0, std::numbers::pi}},
          line(2, -2.0, 0.0, 2.0, 0.0)},
         3}};
    const auto profiles = features::detectClosedProfiles(sketch.elements.values);
    return profiles.size() == 1 && profiles.front().elementIds.size() == 2 &&
           !features::evaluateSketchProfileFaces(sketch).IsNull();
}

} // namespace

int main() {
    if (!detectsPrimitiveAndLineProfiles()) {
        std::cerr << "Closed sketch profile detection failed\n";
        return 1;
    }
    if (!openLinesDoNotFormAProfile()) {
        std::cerr << "Open sketch line validation failed\n";
        return 1;
    }
    if (!constructionGeometryDoesNotFormProfiles()) {
        std::cerr << "Construction geometry formed a sketch profile\n";
        return 1;
    }
    if (!arcAndLineFormAnEvaluatedProfile()) {
        std::cerr << "Arc and line profile evaluation failed\n";
        return 1;
    }
    return 0;
}
