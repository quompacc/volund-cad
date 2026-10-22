#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"
#include "ExportSelection.hpp"

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
    return addImport(path, kernel::readStep(path));
}

StepImportResult CadSession::openStep(const std::filesystem::path& path) {
    auto read = kernel::readStep(path);
    if (!read.error)
        newDocument();
    return addImport(path, std::move(read));
}

StepImportResult CadSession::addImport(const std::filesystem::path& path, kernel::StepImport read) {
    StepImportResult result{
        StepImportStatus::imported, read.message,       std::nullopt,   read.solids.size(),
        read.skippedShapes,         read.invalidSolids, read.sourceUnit};
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
    const auto chosen = exportSolids(*this, bodies);
    if (chosen.refusal)
        return *chosen.refusal;
    const auto written = kernel::writeStep(path, chosen.solids);
    if (!written.error)
        return {StepExportStatus::written, {}, chosen.solids.size()};
    return {*written.error == kernel::StepExportError::notASolid ? StepExportStatus::notASolid
                                                                 : StepExportStatus::writeFailed,
            written.message};
}

} // namespace voelund::application
