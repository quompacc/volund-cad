#include "ExportSelection.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/MeshExchange.hpp>

#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <string>

namespace voelund::application {
namespace detail {
namespace {
// The bodies given, or every visible body with a current solid.
std::vector<document::BodyId> chosenBodies(const CadSession& session, const ModelOutline& outline,
                                           std::span<const document::BodyId> bodies) {
    std::vector<document::BodyId> chosen(bodies.begin(), bodies.end());
    if (!chosen.empty())
        return chosen;
    const auto& valid = session.validBodies();
    for (const auto& body : outline.bodies)
        if (!body.hidden &&
            std::ranges::find(valid, body.id, &features::EvaluatedBody::bodyId) != valid.end())
            chosen.push_back(body.id);
    return chosen;
}
} // namespace

ExportSolids exportSolids(const CadSession& session, std::span<const document::BodyId> bodies) {
    const auto outline = session.modelOutline();
    const auto& valid = session.validBodies();
    const auto chosen = chosenBodies(session, outline, bodies);
    if (chosen.empty())
        return {{}, StepExportResult{StepExportStatus::noBodies, "No visible body to export"}};
    ExportSolids result;
    for (const auto body : chosen) {
        const auto found = std::ranges::find(valid, body, &features::EvaluatedBody::bodyId);
        const auto named = std::ranges::find(outline.bodies, body, &OutlineBody::id);
        if (found == valid.end() || named == outline.bodies.end())
            return {{},
                    StepExportResult{StepExportStatus::unknownBody,
                                     "A body to export is not in the current model"}};
        // As the browser names it: its own name, else "Körper N" by its place.
        const std::string name =
            named->name.empty() ? "Körper " + std::to_string(named - outline.bodies.begin() + 1)
                                : named->name;
        std::size_t pieces = 0;
        for (TopExp_Explorer explorer(found->shape, TopAbs_SOLID); explorer.More(); explorer.Next())
            result.solids.push_back(
                {explorer.Current(), ++pieces == 1 ? name : name + " " + std::to_string(pieces)});
        if (pieces == 0)
            return {
                {},
                StepExportResult{StepExportStatus::notASolid, "\"" + name + "\" holds no solid"}};
    }
    return result;
}

} // namespace detail

StepExportResult exportMesh(const CadSession& session, const std::filesystem::path& path,
                            kernel::MeshFormat format, kernel::MeshResolution resolution,
                            std::span<const document::BodyId> bodies) {
    const auto states = session.featureStates();
    if (!session.sceneStatus().isCurrent() ||
        std::ranges::any_of(states, [](const FeatureRuntimeState& state) {
            return state.status == FeatureRuntimeStatus::error ||
                   state.status == FeatureRuntimeStatus::blocked;
        }))
        return {StepExportStatus::invalidRevision,
                "The current model has errors; repair them before exporting"};
    const auto chosen = detail::exportSolids(session, bodies);
    if (chosen.refusal)
        return *chosen.refusal;
    const auto written = kernel::writeMesh(path, chosen.solids, format, resolution);
    if (!written.error)
        return {StepExportStatus::written, {}, chosen.solids.size()};
    return {*written.error == kernel::MeshExportError::notASolid ? StepExportStatus::notASolid
                                                                 : StepExportStatus::writeFailed,
            written.message};
}

} // namespace voelund::application
