#pragma once
#include <voelund/document/ModelDocument.hpp>

namespace voelund::document {
namespace detail { class NativePrimitiveCodec; }
class LegacyDocumentImporter;
// Validated, read-only interchange value; no OCAF labels or mutable document authority.
class DocumentSnapshot final {
  public:
    [[nodiscard]] static DocumentSnapshot capture(const ModelDocument& document);
    // Model content equality deliberately excludes the monotonic allocation watermark.
    [[nodiscard]] bool matchesContent(const ModelDocument& document) const;
    [[nodiscard]] std::span<const FeatureDescriptor> features() const noexcept { return features_; }
    [[nodiscard]] std::span<const FeatureParameters> parameters() const noexcept {
        return parameters_;
    }
    [[nodiscard]] std::uint64_t nextFeatureId() const noexcept { return nextFeatureId_; }
    [[nodiscard]] ModelDocument toDocument() const;
    [[nodiscard]] bool operator==(const DocumentSnapshot&) const = default;

  private:
    friend class LegacyDocumentImporter;
    friend class detail::NativePrimitiveCodec;
    DocumentSnapshot(std::vector<FeatureDescriptor> features,
                     std::vector<FeatureParameters> parameters, std::uint64_t nextFeatureId)
        : features_(std::move(features)), parameters_(std::move(parameters)),
          nextFeatureId_(nextFeatureId) {}
    std::vector<FeatureDescriptor> features_;
    std::vector<FeatureParameters> parameters_;
    std::uint64_t nextFeatureId_;
};
} // namespace voelund::document
