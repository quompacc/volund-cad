#pragma once
#include <stdexcept>
#include <voelund/application/CommandResult.hpp>

namespace voelund::test {
// Fixture setup must succeed. Rejection tests inspect the result directly.
inline document::FeatureId requireCommittedFeature(const application::CommandResult& result) {
    if (result.status() != application::CommandStatus::committed || !result.feature())
        throw std::runtime_error("Fixture command did not commit a valid feature: " +
                                 result.diagnostic());
    return *result.feature();
}
} // namespace voelund::test
