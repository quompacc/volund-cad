#pragma once

#include "../TestPaths.hpp"
#include <voelund/application/CadSession.hpp>
#include <voelund/features/FeatureGraph.hpp>
#include <voelund/features/SketchSupportResolver.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <string>

namespace voelund::test {
inline double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return properties.Mass();
}

inline double maximumXOf(const TopoDS_Shape& shape) {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    double minimumX = 0.0;
    double minimumY = 0.0;
    double minimumZ = 0.0;
    double maximumX = 0.0;
    double maximumY = 0.0;
    double maximumZ = 0.0;
    bounds.Get(minimumX, minimumY, minimumZ, maximumX, maximumY, maximumZ);
    return maximumX;
}

inline double maximumZOf(const TopoDS_Shape& shape) {
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    double minimumX = 0.0;
    double minimumY = 0.0;
    double minimumZ = 0.0;
    double maximumX = 0.0;
    double maximumY = 0.0;
    double maximumZ = 0.0;
    bounds.Get(minimumX, minimumY, minimumZ, maximumX, maximumY, maximumZ);
    return maximumZ;
}

} // namespace voelund::test
