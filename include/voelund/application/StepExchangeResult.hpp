#pragma once

#include <voelund/document/FeatureId.hpp>

#include <cstddef>
#include <optional>
#include <string>

namespace voelund::application {

// M17 (user decision 2026-10-16): a STEP import becomes one feature "Import N" whose solids are
// bodies named after their products; nothing changes unless it is imported.
enum class StepImportStatus { imported, unreadable, noSolids, rejected };
struct StepImportResult final {
    StepImportStatus status;
    std::string message{};
    std::optional<document::FeatureId> feature{};
    std::size_t solids{0};
    std::size_t skippedShapes{0}; // shells, faces and curves that are no solid
    std::size_t invalidSolids{0}; // solids still invalid after healing
    std::string sourceUnit{};     // the file's length unit as it names it
};
// M17: an export writes the bodies of the current revision in mm, or is refused.
enum class StepExportStatus {
    written,
    invalidRevision, // the current revision has errors (even if an older display is valid)
    noBodies,        // nothing visible, or nothing given
    unknownBody,     // a body that is no current valid body
    notASolid,
    writeFailed
};
struct StepExportResult final {
    StepExportStatus status;
    std::string message{};
    std::size_t solids{0};
};

} // namespace voelund::application
