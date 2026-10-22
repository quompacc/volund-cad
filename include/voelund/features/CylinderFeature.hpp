// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/FeatureId.hpp>
#include <voelund/kernel/OperationHistory.hpp>

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

namespace voelund::document {
class ModelDocument;
class DocumentSnapshot;
} // namespace voelund::document

namespace voelund::features {

class CylinderFeature final {
  public:
    explicit CylinderFeature(document::FeatureId featureId) noexcept;
    [[nodiscard]] kernel::ShapeOperationResult
    evaluateToolWithHistory(const document::DocumentSnapshot& document,
                            const TopoDS_Shape& inputShape, bool applyPlacement = true) const;
    [[nodiscard]] TopoDS_Shape evaluateTool(const document::ModelDocument& document,
                                            const TopoDS_Shape& inputShape,
                                            bool applyPlacement = true) const;
    [[nodiscard]] TopoDS_Shape evaluateTool(const document::DocumentSnapshot& document,
                                            const TopoDS_Shape& inputShape,
                                            bool applyPlacement = true) const;
    [[nodiscard]] static gp_Pnt attachmentOrigin(const TopoDS_Shape& inputShape,
                                                 double cylinderHeight);

  private:
    document::FeatureId featureId_;
};

} // namespace voelund::features
