#pragma once

#include <memory>
#include <voelund/document/BodyId.hpp>
#include <voelund/document/DocumentSnapshot.hpp>
#include <voelund/document/FeatureId.hpp>
#include <voelund/features/EdgeProvenance.hpp>
#include <voelund/features/EvaluationState.hpp>
#include <voelund/features/FeatureDependencies.hpp>
#include <voelund/features/SupportProvenance.hpp>

#include <TopoDS_Shape.hxx>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace voelund::document {
class ModelDocument;
class DocumentSnapshot;
} // namespace voelund::document

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
    std::vector<SupportProvenance> supportOrigins{};
    std::optional<document::SketchParameters> resolvedSketch{};
    std::vector<SupportProvenance> referenceSupportOrigins{};
    std::vector<EdgeProvenance> edgeOrigins{};
    std::vector<EdgeProvenance> referenceEdgeOrigins{};
    // M21: bodies this feature took into its own (tools of a combination); they are no
    // longer separate results after it.
    std::vector<document::BodyId> consumedBodies{};
};

struct FeatureGraphResult final {
    TopoDS_Shape resultShape;
    std::vector<EvaluatedBody> bodies;
    std::vector<EvaluatedFeature> features;
};

struct FeatureGraphEvaluation final {
    document::DocumentSnapshot snapshot;
    FeatureDependencies dependencies;
    FeatureGraphResult result;
    EvaluationReport report;
    [[nodiscard]] bool valid() const noexcept;
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
    [[nodiscard]] std::shared_ptr<const FeatureGraphEvaluation>
    evaluateDetailed(const document::DocumentSnapshot& document,
                     const std::shared_ptr<const FeatureGraphEvaluation>& previous = {}) const;
    [[nodiscard]] FeatureGraphResult evaluate(const document::ModelDocument& document) const;
    [[nodiscard]] FeatureGraphResult evaluate(const document::DocumentSnapshot& document) const;
};

} // namespace voelund::features
