// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "PrimitivePlacement.hpp"
#include <voelund/features/CylinderFeature.hpp>
#include <voelund/features/GeometryEvaluationError.hpp>

#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/kernel/ShapeFactory.hpp>

#include <voelund/kernel/ShapeQueries.hpp>

#include <algorithm>
#include <stdexcept>

namespace voelund::features {

CylinderFeature::CylinderFeature(document::FeatureId featureId) noexcept : featureId_(featureId) {}

TopoDS_Shape CylinderFeature::evaluateTool(const document::ModelDocument& document,
                                           const TopoDS_Shape& inputShape,
                                           bool applyPlacement) const {
    return evaluateTool(document::DocumentSnapshot::capture(document), inputShape, applyPlacement);
}

TopoDS_Shape CylinderFeature::evaluateTool(const document::DocumentSnapshot& document,
                                           const TopoDS_Shape& inputShape,
                                           bool applyPlacement) const {
    return evaluateToolWithHistory(document, inputShape, applyPlacement).shape;
}

kernel::ShapeOperationResult
CylinderFeature::evaluateToolWithHistory(const document::DocumentSnapshot& document,
                                         const TopoDS_Shape& inputShape,
                                         bool applyPlacement) const {
    const document::CylinderParameters& cylinder = document.cylinderParameters(featureId_);
    const gp_Pnt origin = attachmentOrigin(inputShape, cylinder.height);
    const kernel::CylinderDefinition definition{cylinder.radius, cylinder.height, origin.X(),
                                                origin.Y(), origin.Z()};
    const auto cylinderShape = kernel::ShapeFactory::makeCylinderWithHistory(definition);
    if (!applyPlacement) {
        return cylinderShape;
    }
    return detail::placePrimitive(cylinderShape, origin.X(), origin.Y(),
                                  origin.Z() + cylinder.height * 0.5, cylinder.placement);
}

gp_Pnt CylinderFeature::attachmentOrigin(const TopoDS_Shape& inputShape, double cylinderHeight) {
    if (inputShape.IsNull())
        return gp_Pnt(0.0, 0.0, 0.0); // M01: a new body without input stands at the origin.
    const auto bounds = kernel::tightBoundingBox(inputShape);
    if (!bounds) {
        throw GeometryEvaluationError("Cylinder input feature has no geometry");
    }
    const double overlap = std::min(2.0, cylinderHeight * 0.25);
    const double originX = (bounds->minimum[0] + bounds->maximum[0]) * 0.5;
    const double originY = (bounds->minimum[1] + bounds->maximum[1]) * 0.5;
    const double originZ = bounds->maximum[2] - overlap;
    return gp_Pnt(originX, originY, originZ);
}

} // namespace voelund::features
