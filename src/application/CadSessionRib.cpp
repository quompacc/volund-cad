// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CadSessionGeometryDetail.hpp"
#include "CommandInput.hpp"

#include <string>

// N12b: a rib ("Rippe N"), an extrusion that joins its target body from its sketch's line.
namespace voelund::application {
using namespace detail;

CommandResult CadSession::createRib(document::FeatureId sketch, document::BodyId target,
                                    const document::RibParameters& rib) {
    const auto features = document_.features();
    const auto found = std::ranges::find(features, sketch, &document::FeatureDescriptor::id);
    if (found == features.end() || found->kind != document::FeatureKind::sketch)
        return CommandResult::invalidInput("A rib needs a sketch");
    document::ExtrusionParameters parameters{10.0, {}, {}};
    parameters.operation = document::ExtrusionOperation::join;
    parameters.target = target;
    parameters.rib = rib;
    std::size_t sequence = 1;
    const auto snapshot = document::DocumentSnapshot::capture(document_);
    for (const auto& feature : features)
        if (feature.kind == document::FeatureKind::extrusion &&
            snapshot.extrusionParameters(feature.id).rib)
            ++sequence;
    document::DocumentTransaction transaction(document_);
    auto& candidate = transaction.edit();
    const auto input = detail::prepareInput([&] {
        return candidate.addExtrusionFeature("Rippe " + std::to_string(sequence), sketch,
                                             parameters);
    });
    if (!input.value)
        return CommandResult::invalidInput(input.diagnostic);
    return finishCandidate(transaction, *input.value, CommandKind::newFeature,
                           ErrorDisposition::reject, true);
}

} // namespace voelund::application
