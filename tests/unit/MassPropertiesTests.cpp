// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/MassProperties.hpp>

#include <BRepPrimAPI_MakeBox.hxx>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

// N07: a 10 mm cube of steel weighs 7.85 g; centroid and inertia against the analytic values;
// two bodies together by the parallel axis theorem.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) <= 1e-6 * std::max(1.0, std::abs(b)); }

void run() {
    const auto cube = kernel::massProperties(BRepPrimAPI_MakeBox(10, 10, 10).Shape());
    require(near(cube.volume, 1000) && near(cube.area, 600), "Cube volume and area");
    require(near(cube.centroid[0], 5) && near(cube.centroid[1], 5) && near(cube.centroid[2], 5),
            "Cube centroid");
    // About the centroid: V·(b² + c²)/12, no products.
    require(near(cube.inertia[0], 1000 * 200.0 / 12) && near(cube.inertia[2], 1000 * 200.0 / 12) &&
                std::abs(cube.inertia[3]) < 1e-6,
            "Cube inertia");
    const kernel::WeightedBody steel{cube, 7.85};
    const auto one = kernel::combineMasses(std::span(&steel, 1));
    require(near(one.mass, 7.85), "10 mm steel cube: 7.85 g");
    require(near(one.inertia[0], 7.85 * 200.0 / 12), "Inertia in g·mm²: m·(b² + c²)/12");

    // A plate 40 x 20 x 10 at (10, 0, 0): centroid (30, 10, 5).
    gp_Pnt corner(10, 0, 0);
    const auto plate = kernel::massProperties(BRepPrimAPI_MakeBox(corner, 40, 20, 10).Shape());
    require(near(plate.centroid[0], 30) && near(plate.centroid[1], 10) &&
                near(plate.centroid[2], 5),
            "Plate centroid");
    const std::array<kernel::WeightedBody, 2> both{{{cube, 7.85}, {plate, 2.7}}};
    const auto together = kernel::combineMasses(both);
    const double cubeMass = 7.85, plateMass = 8000 * 2.7 / 1000;
    const double centerX = (cubeMass * 5 + plateMass * 30) / (cubeMass + plateMass);
    require(near(together.mass, cubeMass + plateMass) && near(together.center[0], centerX),
            "Two bodies: total mass and common centre");
    // Izz: each about its own centroid, plus m·(dx² + dy²).
    const double cubeZ = cubeMass * 200.0 / 12, plateZ = plateMass * (1600.0 + 400.0) / 12;
    const double centerY = (cubeMass * 5 + plateMass * 10) / (cubeMass + plateMass);
    const auto shifted = [&](double mass, double x, double y) {
        return mass * ((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
    };
    require(near(together.inertia[2],
                 cubeZ + plateZ + shifted(cubeMass, 5, 5) + shifted(plateMass, 30, 10)),
            "Two bodies: Izz by the parallel axis theorem");
    std::cout << "PASS mass properties: cube, plate, two bodies\n";
}
} // namespace

int main() {
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
