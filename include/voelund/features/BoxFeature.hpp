#pragma once

#include <voelund/document/FeatureId.hpp>

#include <TopoDS_Shape.hxx>

namespace voelund::document {
class ModelDocument;
}

namespace voelund::features {

class BoxFeature final {
public:
    explicit BoxFeature(document::FeatureId featureId) noexcept;
    [[nodiscard]] TopoDS_Shape evaluate(const document::ModelDocument& document) const;

private:
    document::FeatureId featureId_;
};

} // namespace voelund::features
