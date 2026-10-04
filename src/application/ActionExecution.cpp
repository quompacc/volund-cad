#include "ActionDetail.hpp"

#include <voelund/features/SketchRegionIdentity.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace voelund::application {
namespace {
using document::DocumentSnapshot;
using document::FeatureId;
using document::ParameterId;

template <class... Visitors> struct Overloaded : Visitors... {
    using Visitors::operator()...;
};

QuantityUnit parameterUnit(document::ParameterUnit unit) noexcept {
    return unit == document::ParameterUnit::length ? QuantityUnit::millimetre
                                                   : QuantityUnit::degree;
}

// First finding of every check category; the AI01 order decides between categories.
struct Findings final {
    std::optional<std::string> unknownTarget;
    std::optional<std::string> unsupportedTarget;
    std::optional<std::string> invalidUnit;
    std::optional<std::string> nonFinite;
    std::optional<std::string> unresolvedReference;

    static void note(std::optional<std::string>& category, std::string message) {
        if (!category)
            category = std::move(message);
    }
    [[nodiscard]] std::optional<std::pair<ActionRejection, std::string>> first() const {
        const std::pair<ActionRejection, const std::optional<std::string>*> ordered[] = {
            {ActionRejection::unknownTarget, &unknownTarget},
            {ActionRejection::unsupportedTarget, &unsupportedTarget},
            {ActionRejection::invalidUnit, &invalidUnit},
            {ActionRejection::nonFinite, &nonFinite},
            {ActionRejection::unresolvedReference, &unresolvedReference}};
        for (const auto& [rejection, finding] : ordered)
            if (*finding)
                return std::pair{rejection, **finding};
        return std::nullopt;
    }
};

class Validation final {
  public:
    Validation(const CadSession& session, const DocumentSnapshot& snapshot)
        : session_(session), snapshot_(snapshot) {}

    // Existence and capability of the target for this action.
    bool target(const ActionTarget& target, ActionKind kind) {
        if (!exists(target)) {
            Findings::note(findings_.unknownTarget, "The target does not exist");
            return false;
        }
        const auto actions = detail::availableActions(session_, snapshot_, target);
        if (std::ranges::find(actions, kind) == actions.end()) {
            Findings::note(findings_.unsupportedTarget, "The target does not offer this action");
            return false;
        }
        return true;
    }
    const document::DocumentParameter* parameter(std::optional<ParameterId> id) {
        if (!id)
            return nullptr;
        const auto* found = document::findParameter(snapshot_.parameterTable(), *id);
        if (found == nullptr)
            Findings::note(findings_.unknownTarget, "The parameter does not exist");
        return found;
    }
    void unit(QuantityUnit actual, QuantityUnit expected) {
        if (actual != expected)
            Findings::note(findings_.invalidUnit, "The value has the wrong unit");
    }
    void quantity(const Quantity& value, QuantityUnit expected) {
        unit(value.unit, expected);
        if (!std::isfinite(value.value))
            Findings::note(findings_.nonFinite, "The value is not finite");
    }
    void lengths(std::initializer_list<Quantity> values) {
        for (const auto& value : values)
            quantity(value, QuantityUnit::millimetre);
    }

    void operator()(const CreateParameterAction& action) {
        static_cast<void>(target(std::monostate{}, ActionKind::createParameter));
        quantity(action.value, parameterUnit(action.unit));
    }
    void operator()(const SetParameterValueAction& action) {
        if (target(action.parameter, ActionKind::setParameterValue))
            quantity(action.value, parameterUnit(parameter(action.parameter)->unit));
    }
    void operator()(const RenameParameterAction& action) {
        static_cast<void>(target(action.parameter, ActionKind::renameParameter));
    }
    void operator()(const BindSketchDimensionAction& action) {
        const auto* bound = parameter(action.parameter);
        if (!target(action.sketch, ActionKind::bindSketchDimension))
            return;
        const auto& constraints = snapshot_.sketchParameters(action.sketch).constraints.values;
        const auto constraint =
            std::ranges::find(constraints, action.constraint, &document::SketchConstraint::id);
        if (constraint == constraints.end())
            return Findings::note(findings_.unknownTarget, "The sketch dimension does not exist");
        const auto expected = document::dimensionUnit(constraint->kind);
        if (!expected)
            return Findings::note(findings_.unsupportedTarget,
                                  "The sketch constraint is not a dimension");
        if (bound != nullptr && bound->unit != *expected)
            Findings::note(findings_.invalidUnit, "The parameter has the wrong unit");
    }
    void operator()(const BindExtrusionDistanceAction& action) {
        const auto* bound = parameter(action.parameter);
        if (target(action.extrusion, ActionKind::bindExtrusionDistance) && bound != nullptr &&
            bound->unit != document::ParameterUnit::length)
            Findings::note(findings_.invalidUnit, "The parameter has the wrong unit");
    }
    void operator()(const CreateDatumSketchAction&) {
        static_cast<void>(target(std::monostate{}, ActionKind::createDatumSketch));
    }
    void operator()(const EditSketchAction& action) {
        static_cast<void>(target(action.sketch, ActionKind::editSketch));
    }
    void operator()(const CreateProfileExtrusionAction& action) {
        const bool sketch = target(action.sketch, ActionKind::createProfileExtrusion);
        quantity(action.distance, QuantityUnit::millimetre);
        if (!sketch)
            return;
        const auto& elements = snapshot_.sketchParameters(action.sketch).elements.values;
        const auto identities = features::identifySketchRegions(elements);
        for (const auto& region : action.regions)
            if (features::resolveSketchRegion(identities, elements, region).status !=
                features::SketchRegionReferenceStatus::resolved)
                Findings::note(findings_.unresolvedReference,
                               "The sketch region does not resolve: " + region.text());
    }
    void operator()(const SetExtrusionDistanceAction& action) {
        static_cast<void>(target(action.extrusion, ActionKind::setExtrusionDistance));
        quantity(action.distance, QuantityUnit::millimetre);
    }
    void operator()(const CreateBoxAction& action) {
        static_cast<void>(target(std::monostate{}, ActionKind::createBox));
        lengths({action.width, action.depth, action.height});
    }
    void operator()(const SetBoxDimensionsAction& action) {
        static_cast<void>(target(action.box, ActionKind::setBoxDimensions));
        lengths({action.width, action.depth, action.height});
    }
    void operator()(const CreateCylinderAction& action) {
        static_cast<void>(target(action.input, ActionKind::createCylinder));
    }
    void operator()(const SetCylinderSizeAction& action) {
        static_cast<void>(target(action.cylinder, ActionKind::setCylinderSize));
        lengths({action.radius, action.height});
    }
    void operator()(const SetPlacementAction& action) {
        static_cast<void>(target(action.feature, ActionKind::setPlacement));
        lengths({action.x, action.y, action.z});
        for (const auto& angle : {action.rotationX, action.rotationY, action.rotationZ})
            quantity(angle, QuantityUnit::radian);
    }

    [[nodiscard]] const Findings& findings() const noexcept { return findings_; }

  private:
    bool exists(const ActionTarget& target) const {
        if (const auto* parameter = std::get_if<ParameterId>(&target))
            return document::findParameter(snapshot_.parameterTable(), *parameter) != nullptr;
        if (const auto* feature = std::get_if<FeatureId>(&target))
            return std::ranges::any_of(snapshot_.features(), [&](const auto& descriptor) {
                return descriptor.id == *feature;
            });
        return true;
    }

    const CadSession& session_;
    const DocumentSnapshot& snapshot_;
    Findings findings_;
};

CommandResult run(CadSession& session, const DocumentSnapshot& snapshot, const Action& action) {
    return std::visit(
        Overloaded{
            [&](const CreateParameterAction& a) {
                return session.createParameter(a.name, a.unit, a.value.value, a.comment);
            },
            [&](const SetParameterValueAction& a) {
                return session.setParameterValue(a.parameter, a.value.value);
            },
            [&](const RenameParameterAction& a) {
                return session.renameParameter(a.parameter, a.name);
            },
            [&](const BindSketchDimensionAction& a) {
                return session.bindSketchDimension(a.sketch, a.constraint, a.parameter);
            },
            [&](const BindExtrusionDistanceAction& a) {
                return session.bindExtrusionDistance(a.extrusion, a.parameter);
            },
            [&](const CreateDatumSketchAction& a) {
                return session.createDatumSketchFeature(a.sketch);
            },
            [&](const EditSketchAction& a) {
                return session.editSketchParameters(a.sketch, a.parameters);
            },
            [&](const CreateProfileExtrusionAction& a) {
                return session.createSketchProfileExtrusion(a.sketch, a.regions,
                                                            a.distance.value);
            },
            [&](const SetExtrusionDistanceAction& a) {
                auto parameters = snapshot.extrusionParameters(a.extrusion);
                parameters.distance = a.distance.value;
                return session.editExtrusionParameters(a.extrusion, parameters);
            },
            [&](const CreateBoxAction& a) {
                return session.createBoxFeature({a.width.value, a.depth.value, a.height.value});
            },
            [&](const SetBoxDimensionsAction& a) {
                return session.editBoxDimensions(a.box,
                                                 {a.width.value, a.depth.value, a.height.value});
            },
            [&](const CreateCylinderAction& a) {
                return session.createCylinderFeature(a.input, a.mode);
            },
            [&](const SetCylinderSizeAction& a) {
                auto parameters = snapshot.cylinderParameters(a.cylinder);
                parameters.radius = a.radius.value;
                parameters.height = a.height.value;
                return session.editCylinderParameters(a.cylinder, parameters);
            },
            [&](const SetPlacementAction& a) {
                return session.editFeaturePlacement(
                    a.feature, {a.x.value, a.y.value, a.z.value, kernel::Radians(a.rotationX.value),
                                kernel::Radians(a.rotationY.value),
                                kernel::Radians(a.rotationZ.value)});
            }},
        action);
}

void recordChanges(ActionResult& result, const DocumentSnapshot& before,
                   const DocumentSnapshot& after) {
    const auto features = before.features();
    for (const auto& feature : after.features()) {
        const auto old = std::ranges::find(features, feature.id, &document::FeatureDescriptor::id);
        if (old == features.end())
            result.createdFeatures.push_back(feature.id);
        else if (*old != feature ||
                 before.featureParameters(feature.id) != after.featureParameters(feature.id))
            result.changedFeatures.push_back(feature.id);
    }
    const auto& parameters = before.parameterTable().values;
    for (const auto& parameter : after.parameterTable().values) {
        const auto old = std::ranges::find(parameters, parameter.id, &document::DocumentParameter::id);
        if (old == parameters.end())
            result.createdParameters.push_back(parameter.id);
        else if (*old != parameter)
            result.changedParameters.push_back(parameter.id);
    }
}

ActionResult rejected(const CadSession& session, ActionRejection rejection,
                      std::string diagnostic) {
    return {CommandResult::invalidInput(std::move(diagnostic)),
            rejection,
            session.sceneStatus().document,
            {},
            {},
            {},
            {}};
}
} // namespace

ActionResult executeAction(CadSession& session, const ActionRequest& request) {
    if (!(request.document == session.documentId()))
        return rejected(session, ActionRejection::foreignDocument,
                        "The request belongs to another document");
    if (!(request.expected == session.sceneStatus().document))
        return rejected(session, ActionRejection::staleStamp,
                        "The document changed since the request was prepared");
    const auto before = session.identifiedSnapshot();
    Validation validation(session, before.snapshot());
    std::visit(validation, request.action);
    if (const auto finding = validation.findings().first())
        return rejected(session, finding->first, finding->second);

    ActionResult result{run(session, before.snapshot(), request.action),
                        ActionRejection::none,
                        session.sceneStatus().document,
                        {},
                        {},
                        {},
                        {}};
    if (result.command.status() != CommandStatus::rejected)
        recordChanges(result, before.snapshot(), session.identifiedSnapshot().snapshot());
    return result;
}

} // namespace voelund::application
