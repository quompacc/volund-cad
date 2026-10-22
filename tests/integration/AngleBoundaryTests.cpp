// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <type_traits>
#include <voelund/document/DocumentSerializer.hpp>
#include <voelund/features/SketchConstraintSolver.hpp>
#include <voelund/kernel/Angle.hpp>

namespace {
using namespace voelund;
static_assert(!std::is_convertible_v<double, kernel::Degrees>);
static_assert(!std::is_convertible_v<double, kernel::Radians>);
static_assert(!std::is_convertible_v<kernel::Degrees, kernel::Radians>);
static_assert(!std::is_convertible_v<kernel::Radians, double>);
static_assert(std::is_same_v<decltype(kernel::RigidTransform{}.rotationX), kernel::Radians>);
static_assert(std::is_same_v<decltype(std::declval<document::SketchConstraint>().angleValue),
                             kernel::Radians>);
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Fixture missing");
    return {std::istreambuf_iterator<char>(input), {}};
}
document::ModelDocument read(const std::string& text) {
    std::istringstream input(text);
    return document::DocumentSerializer::read(input);
}
void replace(std::string& text, const std::string& before, const std::string& after) {
    const auto position = text.find(before);
    require(position != std::string::npos, "Fixture marker missing");
    text.replace(position, before.size(), after);
}
document::ModelDocument roundtrip(const document::ModelDocument& doc) {
    std::ostringstream output;
    document::DocumentSerializer::write(doc, output);
    return read(output.str());
}
void conversions() {
    for (double degrees : {-720.0, -90.0, 0.0, 90.0, 450.0, 720.0}) {
        const auto radians = kernel::toRadians(kernel::Degrees(degrees));
        require(std::abs(radians.value() - degrees * std::numbers::pi / 180.0) < 1e-13 &&
                    std::abs(kernel::toDegrees(radians).value() - degrees) < 1e-12,
                "Unit conversion wrapped turns or changed sign");
    }
    for (double invalid :
         {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN()}) {
        bool degreeRejected = false, radianRejected = false;
        try {
            static_cast<void>(kernel::Degrees(invalid));
        } catch (const std::invalid_argument&) {
            degreeRejected = true;
        }
        try {
            static_cast<void>(kernel::Radians(invalid));
        } catch (const std::invalid_argument&) {
            radianRejected = true;
        }
        require(degreeRejected && radianRejected, "Nonfinite angle accepted");
    }
    bool rejected = false;
    try {
        static_cast<void>(kernel::toDegrees(kernel::Radians(std::numeric_limits<double>::max())));
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "Overflowing conversion accepted");
}
void placementAxis(const std::string& original, int axis) {
    for (int degrees : {-720, -90, 90, 450, 720}) {
        auto source = original;
        const std::string rotation = std::to_string(degrees);
        replace(source, "20 20 30 0 0 0 0 0 0",
                "20 20 30 0 0 0 " + (axis == 0   ? rotation + " 0 0"
                                     : axis == 1 ? "0 " + rotation + " 0"
                                                 : "0 0 " + rotation));
        const auto doc = read(source);
        const auto loaded = roundtrip(doc);
        const auto& box = loaded.boxParameters(document::FeatureId{1});
        const kernel::Radians rotations[] = {box.placement.rotationX, box.placement.rotationY,
                                             box.placement.rotationZ};
        require(std::abs(rotations[axis].value() -
                         kernel::toRadians(kernel::Degrees(degrees)).value()) < 1e-14 &&
                    box == doc.boxParameters(document::FeatureId{1}),
                "Legacy placement degree value changed");
        const auto shape = kernel::ShapeFactory::makeBox(box.dimensions);
        const auto rotated = kernel::ShapeFactory::applyTransform(shape, 0, 0, 0, box.placement);
        GProp_GProps properties;
        BRepGProp::VolumeProperties(rotated, properties);
        const auto center = properties.CentreOfMass();
        const double angle = degrees * std::numbers::pi / 180;
        const double c = std::cos(angle), s = std::sin(angle);
        const double expectedX = axis == 0 ? 10 : axis == 1 ? 10 * c + 15 * s : 10 * c - 10 * s;
        const double expectedY = axis == 0 ? 10 * c - 15 * s : axis == 1 ? 10 : 10 * s + 10 * c;
        const double expectedZ = axis == 0 ? 10 * s + 15 * c : axis == 1 ? -10 * s + 15 * c : 15;
        require(std::abs(properties.Mass() - 12000.0) < 1e-7 &&
                    std::abs(center.X() - expectedX) < 1e-8 &&
                    std::abs(center.Y() - expectedY) < 1e-8 &&
                    std::abs(center.Z() - expectedZ) < 1e-8,
                "Legacy degrees interpreted as radians in kernel");
    }
}
void drivingAngles(const std::string& original, int version) {
    if (version < 9)
        return;
    for (int degrees : {-60, 60}) {
        auto source = original;
        const auto start = source.find("CONSTRAINT 1 ");
        require(start != std::string::npos, "Constraint fixture missing");
        const auto end = source.find('\n', start);
        source.replace(start, end - start,
                       "CONSTRAINT 1 ANGLE 1 START 2 START " + std::to_string(degrees) +
                           " 0 START" + (version >= 14 ? " 0" : ""));
        const auto doc = read(source);
        auto parameters = roundtrip(doc).sketchParameters(document::FeatureId{3});
        require(std::abs(parameters.constraints.values.front().angleValue.value() -
                         kernel::toRadians(kernel::Degrees(degrees)).value()) < 1e-14,
                "Legacy driving angle changed in persistence");
        parameters.elements.values[1].geometry = document::SketchLine{
            {0, 0}, {5, degrees < 0 ? -8.0 : 8.0}, document::SketchLineConstraint::none};
        // Preserve the legacy line's horizontal geometry explicitly in the
        // solver fixture; replacing its only constraint must not free that axis.
        parameters.constraints.values.push_back(
            {document::SketchConstraintId{2},
             document::SketchConstraintKind::horizontal,
             {document::SketchElementId{1}, document::SketchPointPosition::start},
             std::nullopt});
        parameters.constraints.nextId = 3;
        require(features::solveSketchConstraints(parameters).status ==
                    features::SketchSolveStatus::success,
                "Driving angle solve failed");
        const auto& a = std::get<document::SketchLine>(parameters.elements.values[0].geometry);
        const auto& b = std::get<document::SketchLine>(parameters.elements.values[1].geometry);
        const double ax = a.end.x - a.start.x, ay = a.end.y - a.start.y;
        const double bx = b.end.x - b.start.x, by = b.end.y - b.start.y;
        const double measured = std::atan2(ax * by - ay * bx, ax * bx + ay * by);
        require(std::abs(measured - degrees * std::numbers::pi / 180.0) < 1e-7,
                "Solver interpreted legacy driving degrees incorrectly");
    }
}
void fixtures(const std::filesystem::path& directory) {
    for (int version = 1; version <= 16; ++version) {
        std::ostringstream name;
        name << 'v' << std::setw(2) << std::setfill('0') << version << ".volund";
        const auto path = directory / name.str();
        const auto original = bytes(path);
        for (int axis = 0; axis < 3; ++axis)
            placementAxis(original, axis);
        drivingAngles(original, version);
        const auto doc = read(original);
        const auto restored = roundtrip(doc);
        require(restored.sketchParameters(document::FeatureId{3}) ==
                    doc.sketchParameters(document::FeatureId{3}),
                "Arc/ellipse radians or other sketch parameters changed");
        if (version >= 15) {
            const auto& arc = std::get<document::SketchArc>(
                restored.sketchParameters(document::FeatureId{3}).elements.values[4].geometry);
            require(arc.startAngle == 0.0 && arc.endAngle == 1.5,
                    "Arc radians were converted as degrees");
        }
        require(bytes(path) == original, "Source fixture modified");
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass fixture directory");
        conversions();
        fixtures(argv[1]);
        std::cout << "PASS explicit units, 16 placement versions, signed driving angles, geometry "
                     "radians\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
