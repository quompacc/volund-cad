#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

bool boxPreservesRequestedVolume()
{
    constexpr voelund::kernel::BoxDimensions dimensions{40.0, 30.0, 20.0};
    const TopoDS_Shape shape = voelund::kernel::ShapeFactory::makeBox(dimensions);

    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);

    return std::abs(properties.Mass() - 24'000.0) < 1.0e-9;
}

bool boxRejectsNonPositiveDimensions()
{
    constexpr voelund::kernel::BoxDimensions dimensions{0.0, 30.0, 20.0};
    try {
        static_cast<void>(voelund::kernel::ShapeFactory::makeBox(dimensions));
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

} // namespace

int main()
{
    if (!boxPreservesRequestedVolume()) {
        std::cerr << "Box volume test failed\n";
        return 1;
    }
    if (!boxRejectsNonPositiveDimensions()) {
        std::cerr << "Box validation test failed\n";
        return 1;
    }
    return 0;
}
