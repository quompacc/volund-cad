#pragma once
#include <iosfwd>
#include <voelund/document/DocumentSnapshot.hpp>

namespace voelund::document {
enum class DocumentEncoding { legacyText };
struct DocumentSourceFormat final {
    DocumentEncoding encoding;
    std::uint64_t version;
    bool operator==(const DocumentSourceFormat&) const = default;
};
enum class LegacyReferenceKind { supportPlaneOnly, legacyProfileIds, implicitAllProfiles };
struct ReferenceMigrationIssue final {
    FeatureId feature;
    FeatureId sourceFeature;
    LegacyReferenceKind kind;
    std::vector<SketchProfileId> profileIds;
    std::string message;
    bool operator==(const ReferenceMigrationIssue&) const = default;
};
struct LegacyPlaneIssue final {
    FeatureId feature;
    SketchPlane original;
    std::string message;
    bool operator==(const LegacyPlaneIssue&) const = default;
};
struct MigrationReport final {
    DocumentSourceFormat source;
    std::vector<ReferenceMigrationIssue> referenceIssues;
    std::vector<LegacyPlaneIssue> planeIssues;
    bool operator==(const MigrationReport&) const = default;
};
struct ImportedDocument final {
    DocumentSnapshot snapshot;
    MigrationReport report;
};
class LegacyDocumentImporter final {
  public:
    [[nodiscard]] static ImportedDocument read(std::istream& source);

  private:
    friend class DocumentSerializer;
    [[nodiscard]] static ImportedDocument readPayload(std::istream& input,
                                                      DocumentSourceFormat format);
};
} // namespace voelund::document
