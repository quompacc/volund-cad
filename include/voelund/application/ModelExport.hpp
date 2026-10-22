#pragma once

#include <voelund/application/StepExchangeResult.hpp>
#include <voelund/document/BodyId.hpp>
#include <voelund/document/FeatureId.hpp>
#include <voelund/kernel/MeshExport.hpp>

#include <filesystem>
#include <span>

namespace voelund::application {
class CadSession;

// N03 (user decision 2026-10-22): writes `bodies` (none given: every visible body) of the
// current revision as one STL or 3MF file in mm, the bodies kept separate. Refused like a STEP
// export while the model has errors; `solids` counts what was written.
[[nodiscard]] StepExportResult
exportMesh(const CadSession& session, const std::filesystem::path& path, kernel::MeshFormat format,
           kernel::MeshResolution resolution, std::span<const document::BodyId> bodies = {});

// N04 (user decision 2026-10-22): the same bodies as IGES in mm with their names.
[[nodiscard]] StepExportResult exportIges(const CadSession& session,
                                          const std::filesystem::path& path,
                                          std::span<const document::BodyId> bodies = {});
// N04: a sketch as it currently stands as a DXF drawing in mm (document::sketchDxf); `solids`
// counts its elements. Refused for a feature that is no sketch.
[[nodiscard]] StepExportResult exportSketchDxf(const CadSession& session,
                                               const std::filesystem::path& path,
                                               document::FeatureId sketch);

} // namespace voelund::application
