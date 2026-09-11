#pragma once

#include <voelund/document/FeatureId.hpp>

#include <TopoDS_Shape.hxx>

#include <stdexcept>
#include <string>
#include <vector>

namespace voelund::document {
class ModelDocument;
}

namespace voelund::features {

struct EvaluatedBody final {
    document::FeatureId sourceFeature;
    TopoDS_Shape shape;
};

struct EvaluatedFeature final {
    document::FeatureId id;
    TopoDS_Shape shape;
    TopoDS_Shape referenceShape;
};

struct FeatureGraphResult final {
    TopoDS_Shape resultShape;
    std::vector<EvaluatedBody> bodies;
    std::vector<EvaluatedFeature> features;
};

class FeatureEvaluationError final : public std::runtime_error {
public:
    FeatureEvaluationError(document::FeatureId featureId, std::string message);
    [[nodiscard]] document::FeatureId featureId() const noexcept;

private:
    document::FeatureId featureId_;
};

class FeatureGraph final {
public:
    [[nodiscard]] FeatureGraphResult evaluate(const document::ModelDocument& document) const;
};

} // namespace voelund::features
