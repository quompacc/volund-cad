#pragma once

#include <voelund/application/StepExchangeResult.hpp>
#include <voelund/document/BodyId.hpp>
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

} // namespace voelund::application
