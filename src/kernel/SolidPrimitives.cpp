// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/kernel/SolidPrimitives.hpp>

#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRepPrimAPI_MakeWedge.hxx>
#include <BRepPrim_Wedge.hxx>
#include <Standard_Failure.hxx>
#include <Standard_OutOfMemory.hxx>
#include <Standard_ProgramError.hxx>
#include <TopExp_Explorer.hxx>
#include <gp.hxx>
#include <gp_Ax2.hxx>

#include <cmath>
#include <string>
#include <utility>

namespace voelund::kernel {
namespace {

KernelOperation operationOf(PrimitiveShape shape) {
    switch (shape) {
    case PrimitiveShape::sphere:
        return KernelOperation::makeSphere;
    case PrimitiveShape::cone:
        return KernelOperation::makeCone;
    case PrimitiveShape::torus:
        return KernelOperation::makeTorus;
    case PrimitiveShape::wedge:
        return KernelOperation::makeWedge;
    }
    return KernelOperation::makeSphere;
}

bool length(double value) { return std::isfinite(value) && modelLength(value); }

} // namespace

const char* primitiveSizeProblem(const SolidPrimitiveDefinition& d) noexcept {
    switch (d.shape) {
    case PrimitiveShape::sphere:
        return length(d.first) ? nullptr : "the radius must be a positive model length";
    case PrimitiveShape::cone:
        if (!std::isfinite(d.first) || !std::isfinite(d.second) || d.first < 0.0 ||
            d.second < 0.0 || (d.first == 0.0 && d.second == 0.0) || !length(d.third))
            return "a cone needs radii of at least 0 (not both 0) and a positive height";
        if ((d.first > 0.0 && !modelLength(d.first)) || (d.second > 0.0 && !modelLength(d.second)))
            return "a radius is outside the model range";
        return std::abs(d.first - d.second) <= 1e-6 ? "equal radii make a cylinder" : nullptr;
    case PrimitiveShape::torus:
        if (!length(d.first) || !length(d.second))
            return "a torus needs positive radii";
        return d.second < d.first ? nullptr : "the tube radius must be below the ring radius";
    case PrimitiveShape::wedge:
        if (!length(d.first) || !length(d.second) || !length(d.third) || !length(d.fourth))
            return "a wedge needs positive sizes";
        return d.fourth <= d.first ? nullptr : "the top width is at most the width";
    }
    return "unknown primitive";
}

double primitiveHeight(const SolidPrimitiveDefinition& d) noexcept {
    switch (d.shape) {
    case PrimitiveShape::sphere:
        return 2.0 * d.first;
    case PrimitiveShape::cone:
        return d.third;
    case PrimitiveShape::torus:
        return 2.0 * d.second;
    case PrimitiveShape::wedge:
        return d.third;
    }
    return 0.0;
}

ShapeOperationResult makeSolidPrimitiveWithHistory(const SolidPrimitiveDefinition& d) {
    const KernelOperation operation = operationOf(d.shape);
    if (const char* problem = primitiveSizeProblem(d))
        throw KernelError(operation, problem);
    if (!modelCoordinate(d.originX) || !modelCoordinate(d.originY) || !modelCoordinate(d.originZ))
        throw KernelError(operation, "the origin is outside the model range");
    try {
        ShapeOperationResult result;
        const gp_Pnt base(d.originX, d.originY, d.originZ);
        const auto onlyFace = [&result] {
            return TopExp_Explorer(result.shape, TopAbs_FACE).Current();
        };
        switch (d.shape) {
        case PrimitiveShape::sphere: {
            BRepPrimAPI_MakeSphere maker(base.Translated(gp_Vec(0, 0, d.first)), d.first);
            result.shape = maker.Shape();
            result.origins.push_back({PrimitiveRole::sphereSurface, onlyFace()});
            break;
        }
        case PrimitiveShape::torus: {
            BRepPrimAPI_MakeTorus maker(gp_Ax2(base.Translated(gp_Vec(0, 0, d.second)), gp::DZ()),
                                        d.first, d.second);
            result.shape = maker.Shape();
            result.origins.push_back({PrimitiveRole::torusSurface, onlyFace()});
            break;
        }
        case PrimitiveShape::cone: {
            BRepPrimAPI_MakeCone maker(gp_Ax2(base, gp::DZ()), d.first, d.second, d.third);
            result.shape = maker.Shape();
            auto& cone = maker.Cone();
            if (cone.HasBottom())
                result.origins.push_back({PrimitiveRole::cylinderStart, cone.BottomFace()});
            if (cone.HasTop())
                result.origins.push_back({PrimitiveRole::cylinderEnd, cone.TopFace()});
            result.origins.push_back({PrimitiveRole::cylinderSide, cone.LateralFace()});
            break;
        }
        case PrimitiveShape::wedge: {
            // OCCT narrows the wedge along its y; turned so it narrows towards +z: x stays,
            // its y is our z and its z is our -y.
            const gp_Ax2 frame(base.Translated(gp_Vec(-d.first / 2, d.second / 2, 0)),
                               gp_Dir(0, -1, 0), gp_Dir(1, 0, 0));
            BRepPrimAPI_MakeWedge maker(frame, d.first, d.third, d.second, d.fourth);
            result.shape = maker.Shape();
            auto& wedge = maker.Wedge();
            const std::pair<BRepPrim_Direction, PrimitiveRole> faces[]{
                {BRepPrim_XMin, PrimitiveRole::boxNegativeX},
                {BRepPrim_XMax, PrimitiveRole::boxPositiveX},
                {BRepPrim_ZMax, PrimitiveRole::boxNegativeY},
                {BRepPrim_ZMin, PrimitiveRole::boxPositiveY},
                {BRepPrim_YMin, PrimitiveRole::boxNegativeZ},
                {BRepPrim_YMax, PrimitiveRole::boxPositiveZ}};
            for (const auto& [direction, role] : faces)
                if (wedge.HasFace(direction))
                    result.origins.push_back({role, wedge.Face(direction)});
            break;
        }
        }
        if (result.shape.IsNull() || !BRepCheck_Analyzer(result.shape).IsValid())
            throw KernelError(operation, "operation produced an invalid shape");
        return result;
    } catch (const KernelError&) {
        throw;
    } catch (const Standard_OutOfMemory&) {
        throw;
    } catch (const Standard_ProgramError&) {
        throw;
    } catch (const Standard_Failure& failure) {
        throw KernelError(operation, occtFailureDiagnostic(failure));
    }
}

} // namespace voelund::kernel
