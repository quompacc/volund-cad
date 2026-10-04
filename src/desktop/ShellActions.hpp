#pragma once

#include <voelund/application/Actions.hpp>
#include <voelund/application/CadSession.hpp>

#include <optional>
#include <utility>

// AI02d: shell commands run as action requests. The selection or a tool's stored state
// prepares the request; its explicit target and stamp then decide alone.
namespace voelund::desktop {

inline application::CommandResult
runAction(application::CadSession& session, application::Action action,
          std::optional<application::DocumentStamp> stamp = std::nullopt,
          std::optional<application::ErrorDisposition> disposition = std::nullopt) {
    return application::executeAction(
               session,
               application::prepareRequest(session, std::move(action), stamp, disposition))
        .command;
}

// Property panel edits target the feature selected when the edit is applied.
template <class Build>
application::CommandResult runOnSelected(application::CadSession& session, Build build,
                                         application::ErrorDisposition disposition) {
    const auto selected = session.selectedFeature();
    if (!selected)
        return application::CommandResult::invalidInput(
            "Select a feature before editing parameters");
    return runAction(session, build(*selected), std::nullopt, disposition);
}

inline application::Quantity millimetres(double value) {
    return {value, application::QuantityUnit::millimetre};
}

inline application::SetPlacementAction placementAction(document::FeatureId feature,
                                                       const kernel::RigidTransform& placement) {
    const auto radians = [](kernel::Radians angle) {
        return application::Quantity{angle.value(), application::QuantityUnit::radian};
    };
    return {feature,
            millimetres(placement.translationX),
            millimetres(placement.translationY),
            millimetres(placement.translationZ),
            radians(placement.rotationX),
            radians(placement.rotationY),
            radians(placement.rotationZ)};
}

} // namespace voelund::desktop
