// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionTestSupport.hpp"

#include <voelund/features/SketchConstraintAnnotations.hpp>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// W21 (schema 41, as SolidCraft): a dragged dimension keeps its line where it was put - on
// either side of what it measures - and Save and Open keep it; the pointer gives the place.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-6; }

// A horizontal line from (0, 0) to (40, 0) with its length dimensioned.
document::SketchParameters lineWithLength(std::optional<double> position) {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchLine{{0, 0}, {40, 0}}});
    sketch.elements.nextId = 2;
    document::SketchConstraint length{document::SketchConstraintId{1},
                                      document::SketchConstraintKind::distance,
                                      {document::SketchElementId{1}},
                                      std::nullopt,
                                      40.0};
    length.dimensionPosition = position;
    sketch.constraints.values.push_back(length);
    sketch.constraints.nextId = 2;
    return sketch;
}

// Where the dimension line runs: its distance from the measured line, signed by side (+Y up).
double lineSide(const document::SketchParameters& sketch) {
    const auto annotations = features::evaluateSketchConstraintAnnotations(sketch);
    require(!annotations.empty() && annotations.front().dimension.has_value(), "A dimension");
    const auto& shape = *annotations.front().dimension;
    return shape.outward.Y() * shape.offset;
}

void placement() {
    const double automatic = lineSide(lineWithLength(std::nullopt));
    require(std::abs(automatic) > 0, "The automatic place stands off the line");
    require(near(lineSide(lineWithLength(12.0)), 12.0 * (automatic > 0 ? 1 : -1)),
            "Dragged 12 mm out: the line runs there");
    require(near(lineSide(lineWithLength(-7.0)), -7.0 * (automatic > 0 ? 1 : -1)),
            "Dragged to the other side: it runs there");
    const auto at = features::sketchDimensionPositionAt(lineWithLength(std::nullopt),
                                                        document::SketchConstraintId{1},
                                                        {20.0, automatic > 0 ? 9.0 : -9.0});
    require(at && near(*at, 9.0), "The pointer 9 mm off the line gives the place 9");
}

// A circle's radius dimension dragged round to 90 degrees points straight up.
void radius() {
    document::SketchParameters sketch{document::datumSketchPlane(document::DatumPlane::xy), {}};
    sketch.datum = document::DatumPlane::xy;
    sketch.elements.values.push_back(
        {document::SketchElementId{1}, document::SketchCircle{{0, 0}, 10}});
    sketch.elements.nextId = 2;
    sketch.constraints.values.push_back({document::SketchConstraintId{1},
                                         document::SketchConstraintKind::radius,
                                         {document::SketchElementId{1}},
                                         std::nullopt,
                                         10.0});
    sketch.constraints.nextId = 2;
    const auto at =
        features::sketchDimensionPositionAt(sketch, document::SketchConstraintId{1}, {0.0, 25.0});
    require(at && near(*at, std::numbers::pi / 2), "Dragged above the centre: 90 degrees");
    sketch.constraints.values.front().dimensionPosition = at;
    const auto annotations = features::evaluateSketchConstraintAnnotations(sketch);
    const auto& shape = *annotations.front().dimension;
    require(near(shape.second.X(), 0.0) && near(shape.second.Y(), 10.0),
            "The radius line meets the circle at the top");
}

void savedAndOpened(const std::filesystem::path& work) {
    application::CadSession session;
    session.newDocument();
    const auto created = session.createDatumSketchFeature(lineWithLength(-7.5));
    require(created.feature().has_value(), "Sketch: " + created.diagnostic());
    const auto path = work / "dimension.volund";
    session.saveDocument(path);
    session.newDocument();
    session.loadDocument(path);
    const auto sketch = session.sketchParameters(session.features().front().id);
    require(sketch.constraints.values.size() == 1 &&
                sketch.constraints.values.front().dimensionPosition == -7.5,
            "Save and Open keep the dragged place");
}
} // namespace

int main() {
    try {
        const auto work = std::filesystem::temp_directory_path() / "voelund-w21";
        std::filesystem::create_directories(work);
        placement();
        radius();
        savedAndOpened(work);
        std::filesystem::remove_all(work);
        std::cout << "PASS W21 dimension position: either side, pointer, Save and Open\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
