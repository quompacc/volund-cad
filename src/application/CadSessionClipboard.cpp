// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <voelund/application/CadSession.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

// N02 (user decision 2026-10-21): copied bodies are pasted as fixed copies. A paste is one
// import feature "Kopie N" holding the solids as they were when copied; changes to the
// originals do not reach it, and it works across documents (the solids travel as data).
namespace voelund::application {

std::vector<document::ImportedSolid>
CadSession::copyBodies(std::span<const document::BodyId> bodies) const {
    std::vector<document::ImportedSolid> solids;
    for (const document::BodyId body : bodies) {
        const auto valid = validBodies();
        const auto found = std::ranges::find(valid, body, &features::EvaluatedBody::bodyId);
        if (found == valid.end())
            throw std::invalid_argument("Only bodies with a valid current solid can be copied");
        solids.push_back(document::importedSolid(found->shape, {}));
    }
    return solids;
}

CommandResult CadSession::pasteBodies(std::vector<document::ImportedSolid> solids) {
    if (solids.empty())
        return CommandResult::invalidInput("Nothing to paste");
    const std::size_t sequence = pasteCount() + 1;
    const std::string name = "Kopie " + std::to_string(sequence);
    for (std::size_t index = 0; index < solids.size(); ++index)
        solids[index].name = solids.size() == 1 ? name : name + "." + std::to_string(index + 1);
    document::ImportParameters parameters;
    parameters.fileName = pasteSource;
    parameters.sourceUnit = "mm";
    parameters.solids = std::move(solids);
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto idInput = detail::prepareInput(
        [&] { return candidate.addImportFeature(name, std::move(parameters)); });
    if (!idInput.value)
        return CommandResult::invalidInput(idInput.diagnostic);
    return finishCandidate(transaction, *idInput.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

std::size_t CadSession::pasteCount() const {
    std::size_t count = 0;
    for (const auto& feature : document_.features())
        if (feature.kind == document::FeatureKind::stepImport &&
            document_.importParameters(feature.id).fileName == pasteSource)
            ++count;
    return count;
}

} // namespace voelund::application
