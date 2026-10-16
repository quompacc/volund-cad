#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <voelund/kernel/StepExchange.hpp>

#include <TopExp_Explorer.hxx>

#include <algorithm>
#include <string>
#include <utility>

// M17 (user decision 2026-10-16): STEP in and out at the edge of the session. An import is one
// feature whose solids are bodies named after their products; an export writes current bodies
// in mm, and never a revision with errors.
namespace voelund::application {
using namespace detail;

StepImportResult CadSession::importStep(const std::filesystem::path& path) {
    auto read = kernel::readStep(path);
    StepImportResult result{StepImportStatus::imported, read.message, std::nullopt,
                            read.solids.size(), read.skippedShapes, read.invalidSolids,
                            read.sourceUnit};
    if (read.error) {
        result.status = *read.error == kernel::StepImportError::noSolids
                            ? StepImportStatus::noSolids
                            : StepImportStatus::unreadable;
        return result;
    }
    document::ImportParameters parameters;
    const auto fileName = path.filename().u8string();
    parameters.fileName.assign(fileName.begin(), fileName.end());
    parameters.sourceUnit = std::move(read.sourceUnit);
    try {
        for (auto& solid : read.solids)
            parameters.solids.push_back(
                document::importedSolid(solid.solid, std::move(solid.name)));
    } catch (const std::invalid_argument& error) {
        result.status = StepImportStatus::rejected;
        result.message = error.what();
        return result;
    }
    const std::size_t sequence =
        featureCount(document_.features(), document::FeatureKind::stepImport) + 1;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput([&] {
        return candidate.addImportFeature("Import " + std::to_string(sequence),
                                          std::move(parameters));
    });
    if (!idInput.value) {
        result.status = StepImportStatus::rejected;
        result.message = idInput.diagnostic;
        return result;
    }
    const auto command = finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                                         ErrorDisposition::reject, true);
    if (command.status() != CommandStatus::committed) {
        result.status = StepImportStatus::rejected;
        result.message = command.diagnostic().empty() ? "The imported solids do not evaluate"
                                                      : command.diagnostic();
        return result;
    }
    result.feature = *idInput.value;
    return result;
}

StepExportResult CadSession::exportStep(const std::filesystem::path& path,
                                        std::span<const document::BodyId> bodies) {
    if (!evaluation_ || !evaluation_->valid())
        return {StepExportStatus::invalidRevision,
                "The current model has errors; repair them before exporting"};
    const auto outline = modelOutline();
    std::vector<document::BodyId> chosen(bodies.begin(), bodies.end());
    if (chosen.empty())
        for (const auto& body : outline.bodies)
            if (!body.hidden &&
                std::ranges::find(validBodies(), body.id, &features::EvaluatedBody::bodyId) !=
                    validBodies().end())
                chosen.push_back(body.id);
    if (chosen.empty())
        return {StepExportStatus::noBodies, "No visible body to export"};
    std::vector<kernel::StepSolid> solids;
    for (const auto body : chosen) {
        const auto found = std::ranges::find(validBodies(), body, &features::EvaluatedBody::bodyId);
        const auto named = std::ranges::find(outline.bodies, body, &OutlineBody::id);
        if (found == validBodies().end() || named == outline.bodies.end())
            return {StepExportStatus::unknownBody, "A body to export is not in the current model"};
        // As the browser names it: its own name, else "Körper N" by its place.
        const std::string name =
            named->name.empty()
                ? "Körper " + std::to_string(named - outline.bodies.begin() + 1)
                : named->name;
        std::size_t pieces = 0;
        for (TopExp_Explorer explorer(found->shape, TopAbs_SOLID); explorer.More();
             explorer.Next()) {
            ++pieces;
            solids.push_back(
                {explorer.Current(), pieces == 1 ? name : name + " " + std::to_string(pieces)});
        }
        if (pieces == 0)
            return {StepExportStatus::notASolid, "\"" + name + "\" holds no solid"};
    }
    const auto written = kernel::writeStep(path, solids);
    if (!written.error)
        return {StepExportStatus::written, {}, solids.size()};
    return {*written.error == kernel::StepExportError::notASolid ? StepExportStatus::notASolid
                                                                 : StepExportStatus::writeFailed,
            written.message};
}

} // namespace voelund::application
