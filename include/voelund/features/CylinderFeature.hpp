#pragma once

#include <voelund/document/FeatureId.hpp>

#include <TopoDS_Shape.hxx>

namespace voelund::document {
class ModelDocument;
}

namespace voelund::features {

class CylinderFeature final {
public:
    explicit CylinderFeature(document::FeatureId featureId) noexcept;
    [[nodiscard]] TopoDS_Shape evaluateTool(
        const document::ModelDocument& document,
        const TopoDS_Shape& inputShape) const;

private:
    document::FeatureId featureId_;
};

} // namespace voelund::features
