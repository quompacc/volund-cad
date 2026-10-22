// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/document/FeatureFields.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <variant>

namespace voelund::document {
namespace {

// One field: where its value lives in the feature's parameters.
struct Slot final {
    std::string_view name;
    ParameterUnit unit;
    double* value{nullptr};
    kernel::Radians* angle{nullptr};
    std::uint32_t* count{nullptr};
};

void addPlacement(std::vector<Slot>& slots, kernel::RigidTransform& placement) {
    slots.push_back({"x", ParameterUnit::length, &placement.translationX});
    slots.push_back({"y", ParameterUnit::length, &placement.translationY});
    slots.push_back({"z", ParameterUnit::length, &placement.translationZ});
}

std::size_t usedSizes(kernel::PrimitiveShape shape) noexcept {
    switch (shape) {
    case kernel::PrimitiveShape::sphere:
        return 1;
    case kernel::PrimitiveShape::torus:
        return 2;
    case kernel::PrimitiveShape::cone:
        return 3;
    case kernel::PrimitiveShape::wedge:
        return 4;
    }
    return 0;
}

struct SlotCollector final {
    std::vector<Slot>& slots;

    void operator()(BoxParameters& box) const {
        slots.push_back({"width", ParameterUnit::length, &box.dimensions.width});
        slots.push_back({"depth", ParameterUnit::length, &box.dimensions.depth});
        slots.push_back({"height", ParameterUnit::length, &box.dimensions.height});
        addPlacement(slots, box.placement);
    }
    void operator()(CylinderParameters& cylinder) const {
        slots.push_back({"radius", ParameterUnit::length, &cylinder.radius});
        slots.push_back({"height", ParameterUnit::length, &cylinder.height});
        addPlacement(slots, cylinder.placement);
    }
    void operator()(PrimitiveParameters& primitive) const {
        static constexpr std::string_view names[] = {"size1", "size2", "size3", "size4"};
        for (std::size_t index = 0; index < usedSizes(primitive.shape); ++index)
            slots.push_back({names[index], ParameterUnit::length, &primitive.sizes[index]});
        addPlacement(slots, primitive.placement);
    }
    void operator()(ExtrusionParameters& extrusion) const {
        if (!extrusion.revolve)
            return;
        slots.push_back({"angle", ParameterUnit::angle, nullptr, &extrusion.revolve->angle});
        if (extrusion.revolve->pitch)
            slots.push_back({"pitch", ParameterUnit::length, &*extrusion.revolve->pitch});
    }
    void operator()(FilletParameters& fillet) const {
        slots.push_back({"radius", ParameterUnit::length, &fillet.radius});
    }
    void operator()(ChamferParameters& chamfer) const {
        slots.push_back({"distance", ParameterUnit::length, &chamfer.distance});
    }
    void operator()(HoleParameters& hole) const {
        slots.push_back({"x", ParameterUnit::length, &hole.x});
        slots.push_back({"y", ParameterUnit::length, &hole.y});
        slots.push_back({"diameter", ParameterUnit::length, &hole.diameter});
        if (hole.depth)
            slots.push_back({"depth", ParameterUnit::length, &*hole.depth});
    }
    void operator()(DatumPlaneParameters& plane) const {
        slots.push_back({"offset", ParameterUnit::length, &plane.offset});
        if (plane.rotationAxis != DatumRotationAxis::none)
            slots.push_back({"angle", ParameterUnit::angle, nullptr, &plane.angle});
    }
    void operator()(ShellParameters& shell) const {
        slots.push_back({"thickness", ParameterUnit::length, &shell.thickness});
    }
    void operator()(DraftParameters& draft) const {
        slots.push_back({"angle", ParameterUnit::angle, nullptr, &draft.angle});
    }
    void operator()(PatternParameters& pattern) const {
        slots.push_back({"count", ParameterUnit::number, nullptr, nullptr, &pattern.count});
        if (pattern.kind == PatternKind::linear)
            slots.push_back({"spacing", ParameterUnit::length, &pattern.spacing});
        else
            slots.push_back({"angle", ParameterUnit::angle, nullptr, &pattern.angle});
    }
    void operator()(MoveParameters& move) const {
        addPlacement(slots, move.transform);
        slots.push_back({"rx", ParameterUnit::angle, nullptr, &move.transform.rotationX});
        slots.push_back({"ry", ParameterUnit::angle, nullptr, &move.transform.rotationY});
        slots.push_back({"rz", ParameterUnit::angle, nullptr, &move.transform.rotationZ});
    }
    template <class Other> void operator()(Other&) const {}
};

std::vector<Slot> slotsOf(FeatureParameters& parameters) {
    std::vector<Slot> slots;
    std::visit(SlotCollector{slots}, parameters);
    return slots;
}

std::optional<Slot> slotOf(FeatureParameters& parameters, std::string_view field) {
    for (const auto& slot : slotsOf(parameters))
        if (slot.name == field)
            return slot;
    return std::nullopt;
}

double valueOf(const Slot& slot) {
    if (slot.angle != nullptr)
        return kernel::toDegrees(*slot.angle).value();
    if (slot.count != nullptr)
        return static_cast<double>(*slot.count);
    return *slot.value;
}

// The checks the document makes when the feature is set (validate*Parameters).
struct Validator final {
    void operator()(const BoxParameters& box) const {
        if (!kernel::modelLength(box.dimensions.width) ||
            !kernel::modelLength(box.dimensions.depth) ||
            !kernel::modelLength(box.dimensions.height))
            throw std::invalid_argument(kernel::modelRangeDiagnostic);
        validateCylinderParameters({1.0, 1.0, box.placement, CylinderResultMode::separate});
    }
    void operator()(const CylinderParameters& value) const { validateCylinderParameters(value); }
    void operator()(const PrimitiveParameters& value) const { validatePrimitiveParameters(value); }
    void operator()(const ExtrusionParameters& value) const {
        if (value.revolve)
            validateRevolveParameters(*value.revolve);
    }
    void operator()(const FilletParameters& value) const { validateFilletParameters(value); }
    void operator()(const ChamferParameters& value) const { validateChamferParameters(value); }
    void operator()(const HoleParameters& value) const { validateHoleParameters(value); }
    void operator()(const DatumPlaneParameters& value) const {
        validateDatumPlaneParameters(value);
    }
    void operator()(const ShellParameters& value) const { validateShellParameters(value); }
    void operator()(const DraftParameters& value) const { validateDraftParameters(value); }
    void operator()(const PatternParameters& value) const { validatePatternParameters(value); }
    void operator()(const MoveParameters& value) const { validateMoveParameters(value); }
    template <class Other> void operator()(const Other&) const {}
};

} // namespace

std::optional<FeatureField> featureField(const FeatureParameters& parameters,
                                         std::string_view field) {
    auto copy = parameters;
    const auto slot = slotOf(copy, field);
    if (!slot)
        return std::nullopt;
    return FeatureField{slot->unit, valueOf(*slot)};
}

std::vector<std::string> featureFieldNames(const FeatureParameters& parameters) {
    auto copy = parameters;
    std::vector<std::string> names;
    for (const auto& slot : slotsOf(copy))
        names.emplace_back(slot.name);
    return names;
}

bool setFeatureField(FeatureParameters& parameters, std::string_view field, double value) {
    if (!std::isfinite(value))
        throw std::invalid_argument("A field value must be finite");
    auto changed = parameters;
    const auto slot = slotOf(changed, field);
    if (!slot)
        throw std::invalid_argument("The feature has no field " + std::string(field));
    if (slot->count != nullptr) {
        if (value != std::round(value) || value < 0 || value > 1'000'000)
            throw std::invalid_argument("A count must be a whole number: " + std::string(field));
        *slot->count = static_cast<std::uint32_t>(value);
    } else if (slot->angle != nullptr) {
        *slot->angle = kernel::toRadians(kernel::Degrees(value));
    } else {
        *slot->value = value;
    }
    if (changed == parameters)
        return false;
    std::visit(Validator{}, changed);
    parameters = std::move(changed);
    return true;
}

namespace {
bool same(double left, double right) {
    return std::abs(left - right) <= 1e-9 * std::max(1.0, std::abs(right));
}

// The binding stood before with its parameter at the same value.
bool unchangedBinding(const FieldBinding& binding, const DocumentParameter& parameter,
                      const ParameterTable* before) {
    if (before == nullptr || std::ranges::find(before->bindings, binding) == before->bindings.end())
        return false;
    const auto* prior = findParameter(*before, binding.parameter);
    return prior != nullptr && same(prior->value, parameter.value);
}
} // namespace

void applyFieldBindings(ParameterTable& table, std::span<const FeatureDescriptor> features,
                        std::span<FeatureParameters> parameters, const ParameterTable* before) {
    const auto indexOf = [&](FeatureId id) -> std::optional<std::size_t> {
        for (std::size_t index = 0; index < features.size(); ++index)
            if (features[index].id == id)
                return index;
        return std::nullopt;
    };
    std::erase_if(table.bindings, [&](const FieldBinding& binding) {
        const auto index = indexOf(binding.feature);
        return !index || !featureField(parameters[*index], binding.field);
    });
    std::vector<FieldBinding> kept;
    for (const auto& binding : table.bindings) {
        const auto* parameter = findParameter(table, binding.parameter);
        auto& feature = parameters[*indexOf(binding.feature)];
        const auto field = featureField(feature, binding.field);
        if (parameter == nullptr || parameter->unit != field->unit)
            throw std::invalid_argument("Parameter unit does not match the bound field: " +
                                        binding.field);
        if (unchangedBinding(binding, *parameter, before) && !same(field->value, parameter->value))
            continue; // the field was changed by itself
        static_cast<void>(setFeatureField(feature, binding.field, parameter->value));
        kept.push_back(binding);
    }
    table.bindings = std::move(kept);
}

} // namespace voelund::document
