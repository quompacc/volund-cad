#pragma once

#include <voelund/document/FeatureId.hpp>
#include <voelund/document/BodyId.hpp>

#include <TopoDS_Shape.hxx>

#include <stdexcept>
#include <optional>
#include <string>
#include <vector>

namespace voelund::document {
class ModelDocument;
class DocumentSnapshot;
}

namespace voelund::features {

struct EvaluatedBody final {
    document::BodyId bodyId;
    // Latest producer of this body's current shape, not its identity.
    document::FeatureId sourceFeature;
    TopoDS_Shape shape;
};

struct EvaluatedFeature final {
    document::FeatureId id;
    TopoDS_Shape shape;
    TopoDS_Shape referenceShape;
    std::optional<document::BodyId> bodyId;
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
    [[nodiscard]] FeatureGraphResult evaluate(const document::DocumentSnapshot& document) const;
};

} // namespace voelund::features
