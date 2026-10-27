// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/HoleTool.hpp>
#include <voelund/kernel/KernelError.hpp>

#include "OcctGuard.hpp"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepPrimAPI_MakeRevol.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopoDS.hxx>
#include <gp_Ax2.hxx>

#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace voelund::kernel {
namespace {

// A profile corner: distance from the axis and depth from the entrance; the role of the face
// the edge from the previous corner sweeps (none for the edge on the axis).
struct Corner final {
    double radius;
    double depth;
    std::optional<PrimitiveRole> role;
};

std::vector<Corner> corners(const HoleProfile& profile) {
    std::vector<Corner> result{{0.0, 0.0, std::nullopt}};
    const bool head = profile.headRadius > profile.radius;
    double wallStart = 0.0;
    result.push_back(
        {head ? profile.headRadius : profile.radius, 0.0, PrimitiveRole::cylinderStart});
    if (head && profile.headDepth > 0.0) { // counterbore
        result.push_back(
            {profile.headRadius, profile.headDepth, PrimitiveRole::holeCounterboreSide});
        result.push_back({profile.radius, profile.headDepth, PrimitiveRole::holeCounterboreFloor});
        wallStart = profile.headDepth;
    } else if (head) { // countersink: the cone narrows to the hole
        wallStart = (profile.headRadius - profile.radius) / std::tan(profile.countersinkAngle / 2);
        result.push_back({profile.radius, wallStart, PrimitiveRole::holeCountersink});
    }
    if (!(wallStart < profile.length))
        throw KernelError(KernelOperation::evaluation,
                          "the head of the hole is deeper than the hole");
    result.push_back({profile.radius, profile.length, PrimitiveRole::cylinderSide});
    if (profile.tipAngle > 0.0)
        result.push_back({0.0, profile.length + profile.radius / std::tan(profile.tipAngle / 2),
                          PrimitiveRole::holeTip});
    else
        result.push_back({0.0, profile.length, PrimitiveRole::cylinderEnd});
    return result;
}

} // namespace

ShapeOperationResult makeHoleToolWithHistory(const HoleProfile& profile) {
    return detail::guarded(KernelOperation::evaluation, [&] {
        const gp_Pnt origin(profile.origin[0], profile.origin[1], profile.origin[2]);
        const gp_Dir axis(profile.direction[0], profile.direction[1], profile.direction[2]);
        const gp_Ax2 frame(origin, axis);
        const auto at = [&](const Corner& corner) {
            return origin.Translated(gp_Vec(frame.XDirection()) * corner.radius +
                                     gp_Vec(axis) * corner.depth);
        };
        const auto points = corners(profile);
        BRepBuilderAPI_MakeWire wire;
        std::vector<std::pair<TopoDS_Edge, PrimitiveRole>> named;
        for (std::size_t index = 0; index < points.size(); ++index) {
            const auto& from = points[index];
            const auto& to = points[(index + 1) % points.size()];
            const TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(at(from), at(to)).Edge();
            wire.Add(edge);
            // The wire may copy an edge to join it (shared vertices): name its own edge.
            if (to.role) // the closing edge on the axis sweeps no face
                named.emplace_back(wire.Edge(), *to.role);
        }
        const TopoDS_Face face = BRepBuilderAPI_MakeFace(wire.Wire(), true).Face();
        BRepPrimAPI_MakeRevol revol(face, gp_Ax1(origin, axis), 2 * std::numbers::pi, true);
        ShapeOperationResult result{revol.Shape(), {}, {}};
        for (const auto& [edge, role] : named)
            for (const auto& generated : revol.Generated(edge))
                if (generated.ShapeType() == TopAbs_FACE)
                    result.origins.push_back({role, generated});
        return result;
    });
}

} // namespace voelund::kernel
