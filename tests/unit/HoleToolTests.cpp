// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/HoleTool.hpp>
#include <voelund/kernel/KernelError.hpp>
#include <voelund/kernel/MassProperties.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

// N09: the turned hole tools against their analytic volumes, with one named face per role.
namespace {
using namespace voelund;
using kernel::PrimitiveRole;
constexpr double pi = std::numbers::pi;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-7 * std::max(1.0, std::abs(b)); }
double volume(const kernel::HoleProfile& profile) {
    return kernel::massProperties(kernel::makeHoleToolWithHistory(profile).shape).volume;
}
bool hasRoles(const kernel::HoleProfile& profile, std::initializer_list<PrimitiveRole> roles) {
    const auto tool = kernel::makeHoleToolWithHistory(profile);
    return tool.origins.size() == roles.size() &&
           std::ranges::all_of(roles, [&](PrimitiveRole role) {
               return std::ranges::count(tool.origins, role, &kernel::PrimitiveOrigin::role) == 1;
           });
}

void run() {
    // Counterbore Ø11 x 6 over a Ø6.6 hole 20 deep.
    kernel::HoleProfile counterbore{3.3, 20.0};
    counterbore.headRadius = 5.5;
    counterbore.headDepth = 6.0;
    require(near(volume(counterbore), pi * 3.3 * 3.3 * 20 + pi * (5.5 * 5.5 - 3.3 * 3.3) * 6),
            "Counterbore volume");
    require(hasRoles(counterbore, {PrimitiveRole::cylinderStart, PrimitiveRole::holeCounterboreSide,
                                   PrimitiveRole::holeCounterboreFloor, PrimitiveRole::cylinderSide,
                                   PrimitiveRole::cylinderEnd}),
            "Counterbore faces named");
    // Countersink Ø12, 90°, over a Ø6.4 hole 15 deep: cone height 2.8.
    kernel::HoleProfile countersink{3.2, 15.0};
    countersink.headRadius = 6.0;
    countersink.countersinkAngle = pi / 2;
    const double cone = 6.0 - 3.2;
    require(near(volume(countersink),
                 pi * 3.2 * 3.2 * (15 - cone) + pi * cone * (36 + 6 * 3.2 + 3.2 * 3.2) / 3),
            "Countersink volume");
    require(hasRoles(countersink, {PrimitiveRole::cylinderStart, PrimitiveRole::holeCountersink,
                                   PrimitiveRole::cylinderSide, PrimitiveRole::cylinderEnd}),
            "Countersink faces named");
    // Ø5 blind hole 12 deep with a 118° drill point.
    kernel::HoleProfile pointed{2.5, 12.0};
    pointed.tipAngle = 118.0 * pi / 180.0;
    const double tip = 2.5 / std::tan(59.0 * pi / 180.0);
    require(near(volume(pointed), pi * 2.5 * 2.5 * 12 + pi * 2.5 * 2.5 * tip / 3),
            "Drill point volume");
    require(hasRoles(pointed, {PrimitiveRole::cylinderStart, PrimitiveRole::cylinderSide,
                               PrimitiveRole::holeTip}),
            "Drill point faces named");
    kernel::HoleProfile tooDeep{3.0, 2.0};
    tooDeep.headRadius = 10.0;
    tooDeep.countersinkAngle = pi / 2;
    bool refused = false;
    try {
        static_cast<void>(kernel::makeHoleToolWithHistory(tooDeep));
    } catch (const kernel::KernelError&) {
        refused = true;
    }
    require(refused, "A countersink deeper than the hole is refused");
    std::cout << "PASS hole tools: counterbore, countersink, drill point\n";
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
