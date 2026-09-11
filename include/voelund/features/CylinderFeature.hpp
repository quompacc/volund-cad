#pragma once

#include <voelund/document/FeatureId.hpp>

#include <TopoDS_Shape.hxx>
#include <gp_Pnt.hxx>

namespace voelund::document {
class ModelDocument;
}

namespace voelund::features {

class CylinderFeature final {
public:
    explicit CylinderFeature(document::FeatureId featureId) noexcept;
    [[nodiscard]] TopoDS_Shape evaluateTool(
        const document::ModelDocument& document,
        const TopoDS_Shape& inputShape,
        bool applyPlacement = true) const;
    [[nodiscard]] static gp_Pnt attachmentOrigin(
        const TopoDS_Shape& inputShape,
        double cylinderHeight);

private:
    document::FeatureId featureId_;
};

} // namespace voelund::features
