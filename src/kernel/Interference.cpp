// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/Interference.hpp>

#include "OcctGuard.hpp"

#include <BRepAlgoAPI_Common.hxx>
#include <BRepBndLib.hxx>
#include <BRepGProp.hxx>
#include <Bnd_Box.hxx>
#include <GProp_GProps.hxx>

#include <cmath>

namespace voelund::kernel {
namespace {

// Below this a common part is a touch, not an overlap (mm³).
constexpr double minimumVolume = 1e-6;

Bnd_Box boxOf(const TopoDS_Shape& shape) {
    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    return box;
}

double volumeOf(const TopoDS_Shape& shape) {
    GProp_GProps properties;
    BRepGProp::VolumeProperties(shape, properties);
    return std::abs(properties.Mass());
}

} // namespace

std::vector<Interference> findInterferences(std::span<const TopoDS_Shape> bodies) {
    return detail::guarded(KernelOperation::evaluation, [&] {
        std::vector<Bnd_Box> boxes;
        boxes.reserve(bodies.size());
        for (const auto& body : bodies)
            boxes.push_back(boxOf(body));
        std::vector<Interference> found;
        for (std::size_t first = 0; first < bodies.size(); ++first)
            for (std::size_t second = first + 1; second < bodies.size(); ++second) {
                if (bodies[first].IsNull() || bodies[second].IsNull() ||
                    boxes[first].IsOut(boxes[second]))
                    continue;
                BRepAlgoAPI_Common common(bodies[first], bodies[second]);
                if (!common.IsDone())
                    continue;
                const double volume = volumeOf(common.Shape());
                if (volume > minimumVolume)
                    found.push_back({first, second, volume, common.Shape()});
            }
        return found;
    });
}

} // namespace voelund::kernel
