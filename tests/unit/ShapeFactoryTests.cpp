// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/ShapeFactory.hpp>
#include <voelund/kernel/ShapeOperations.hpp>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <utility>

namespace {

bool boxPreservesRequestedVolume() {
    constexpr voelund::kernel::BoxDimensions dimensions{40.0, 30.0, 20.0};
    const TopoDS_Shape shape = voelund::kernel::ShapeFactory::makeBox(dimensions);

    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);

    return std::abs(properties.Mass() - 24'000.0) < 1.0e-9;
}

bool boxRejectsNonPositiveDimensions() {
    constexpr voelund::kernel::BoxDimensions dimensions{0.0, 30.0, 20.0};
    try {
        static_cast<void>(voelund::kernel::ShapeFactory::makeBox(dimensions));
    } catch (const voelund::kernel::KernelError& error) {
        return error.operation() == voelund::kernel::KernelOperation::makeBox &&
               error.diagnostic().find("[0.001, 1000000]") != std::string::npos;
    }
    return false;
}

bool rejectsNonFiniteDimensions() {
    try {
        static_cast<void>(voelund::kernel::ShapeFactory::makeBox(
            {std::numeric_limits<double>::infinity(), 30.0, 20.0}));
    } catch (const voelund::kernel::KernelError& error) {
        return error.operation() == voelund::kernel::KernelOperation::makeBox;
    }
    return false;
}

bool cylinderRejectsZeroAxis() {
    try {
        static_cast<void>(
            voelund::kernel::ShapeFactory::makeCylinder({5.0, 20.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}));
    } catch (const voelund::kernel::KernelError& error) {
        return error.operation() == voelund::kernel::KernelOperation::makeCylinder &&
               error.diagnostic().find("axis") != std::string::npos;
    }
    return false;
}

bool intersectRejectsDisjointBodies() {
    const TopoDS_Shape left = voelund::kernel::ShapeFactory::makeBox({10.0, 10.0, 10.0});
    const TopoDS_Shape right =
        voelund::kernel::ShapeFactory::makeCylinder({2.0, 5.0, 100.0, 0.0, 0.0});
    try {
        static_cast<void>(voelund::kernel::ShapeOperations::intersect(left, right));
    } catch (const voelund::kernel::KernelError& error) {
        return error.operation() == voelund::kernel::KernelOperation::intersect &&
               error.diagnostic().find("overlap") != std::string::npos;
    }
    return false;
}

template <typename Operation>
bool rejectsWith(voelund::kernel::KernelOperation expected, Operation&& operation) {
    try {
        static_cast<void>(operation());
    } catch (const voelund::kernel::KernelError& error) {
        if (error.operation() != expected || error.diagnostic().empty()) {
            std::cerr << "Unexpected kernel error: " << error.what() << '\n';
        }
        return error.operation() == expected && !error.diagnostic().empty();
    }
    std::cerr << "Expected rejection for " << voelund::kernel::kernelOperationName(expected)
              << '\n';
    return false;
}

bool cutKeepsPublicOperationId() {
    using namespace voelund::kernel;
    const auto input = ShapeFactory::makeBox({10.0, 10.0, 10.0});
    const auto tool = ShapeFactory::makeCylinder({2.0, 5.0, 100.0, 0.0, 0.0});
    return rejectsWith(KernelOperation::cut, [&] { return ShapeOperations::cut(input, tool); });
}

bool rejectsNullShapes() {
    using namespace voelund::kernel;
    const auto box = ShapeFactory::makeBox({10.0, 10.0, 10.0});
    const TopoDS_Shape nullShape;
    const std::array shapes{box, nullShape};
    return rejectsWith(KernelOperation::fuse,
                       [&] { return ShapeOperations::fuse(nullShape, box); }) &&
           rejectsWith(KernelOperation::fuse,
                       [&] { return ShapeOperations::fuse(box, nullShape); }) &&
           rejectsWith(KernelOperation::cut,
                       [&] { return ShapeOperations::cut(nullShape, box); }) &&
           rejectsWith(KernelOperation::cut,
                       [&] { return ShapeOperations::cut(box, nullShape); }) &&
           rejectsWith(KernelOperation::intersect,
                       [&] { return ShapeOperations::intersect(nullShape, box); }) &&
           rejectsWith(KernelOperation::intersect,
                       [&] { return ShapeOperations::intersect(box, nullShape); }) &&
           rejectsWith(KernelOperation::compound,
                       [&] { return ShapeOperations::compound(shapes); }) &&
           rejectsWith(
               KernelOperation::compound,
               [&] { return ShapeOperations::compound(std::span<const TopoDS_Shape>{}); }) &&
           rejectsWith(KernelOperation::applyTransform,
                       [&] { return ShapeFactory::applyTransform(nullShape, 0.0, 0.0, 0.0, {}); });
}

bool rejectsNonFiniteDefinitions() {
    using namespace voelund::kernel;
    const auto box = ShapeFactory::makeBox({10.0, 10.0, 10.0});
    for (const double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity()}) {
        if (!rejectsWith(KernelOperation::makeBox,
                         [&] { return ShapeFactory::makeBox({10.0, value, 10.0}); }) ||
            !rejectsWith(KernelOperation::makeCylinder,
                         [&] { return ShapeFactory::makeCylinder({value, 5.0, 0.0, 0.0, 0.0}); }) ||
            !rejectsWith(KernelOperation::makeCylinder,
                         [&] { return ShapeFactory::makeCylinder({2.0, 5.0, value, 0.0, 0.0}); }) ||
            !rejectsWith(KernelOperation::makeCylinder,
                         [&] {
                             return ShapeFactory::makeCylinder(
                                 {2.0, 5.0, 0.0, 0.0, 0.0, value, 0.0, 1.0});
                         }) ||
            !rejectsWith(KernelOperation::applyTransform,
                         [&] { return ShapeFactory::applyTransform(box, value, 0.0, 0.0, {}); }) ||
            !rejectsWith(KernelOperation::applyTransform, [&] {
                return ShapeFactory::applyTransform(box, 0.0, 0.0, 0.0,
                                                    {value, 0.0, 0.0, {}, {}, {}});
            })) {
            return false;
        }
    }
    return true;
}

bool translatesOcctFailure() {
    using namespace voelund::kernel;
    const Standard_Failure namedFailure("A06 diagnostic fixture");
    const Standard_Failure emptyFailure;
    if (occtFailureDiagnostic(namedFailure) != "A06 diagnostic fixture" ||
        occtFailureDiagnostic(emptyFailure).empty()) {
        return false;
    }
    // Below the supported input range: rejected before reaching the OCCT builder.
    return rejectsWith(KernelOperation::makeBox,
                       [&] { return ShapeFactory::makeBox({1.0e-20, 10.0, 10.0}); });
}

bool cylinderAndTransformPreserveGeometry() {
    using namespace voelund::kernel;
    const auto cylinder = ShapeFactory::makeCylinder({2.0, 5.0, 1.0, 2.0, 3.0, 1.0, 0.0, 0.0});
    const auto moved = ShapeFactory::applyTransform(
        cylinder, 0.0, 0.0, 0.0,
        {10.0, 20.0, 30.0, {}, {}, voelund::kernel::toRadians(voelund::kernel::Degrees(90))});
    GProp_GProps originalProperties;
    GProp_GProps movedProperties;
    BRepGProp::VolumeProperties(cylinder, originalProperties);
    BRepGProp::VolumeProperties(moved, movedProperties);
    const auto center = movedProperties.CentreOfMass();
    return std::abs(originalProperties.Mass() - 20.0 * std::numbers::pi) < 1.0e-9 &&
           std::abs(movedProperties.Mass() - originalProperties.Mass()) < 1.0e-9 &&
           std::abs(center.X() - 8.0) < 1.0e-9 && std::abs(center.Y() - 23.5) < 1.0e-9 &&
           std::abs(center.Z() - 33.0) < 1.0e-9;
}

} // namespace

int main() {
    const std::pair<const char*, bool (*)()> reviewCases[] = {
        {"Cut operation identity", cutKeepsPublicOperationId},
        {"Null shape inputs", rejectsNullShapes},
        {"Non-finite definitions", rejectsNonFiniteDefinitions},
        {"OCCT exception translation", translatesOcctFailure},
        {"Cylinder and transform geometry", cylinderAndTransformPreserveGeometry},
    };
    bool reviewPassed = true;
    for (const auto& [name, test] : reviewCases) {
        if (!test()) {
            std::cerr << name << " failed\n";
            reviewPassed = false;
        }
    }
    if (!reviewPassed) {
        return 1;
    }
    if (!boxPreservesRequestedVolume()) {
        std::cerr << "Box volume test failed\n";
        return 1;
    }
    if (!boxRejectsNonPositiveDimensions()) {
        std::cerr << "Box validation test failed\n";
        return 1;
    }
    if (!rejectsNonFiniteDimensions()) {
        std::cerr << "Non-finite dimension validation test failed\n";
        return 1;
    }
    if (!cylinderRejectsZeroAxis()) {
        std::cerr << "Zero axis validation test failed\n";
        return 1;
    }
    if (!intersectRejectsDisjointBodies()) {
        std::cerr << "Failed operation error test failed\n";
        return 1;
    }
    return 0;
}
