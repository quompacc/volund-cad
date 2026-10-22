// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/ModelDocument.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace voelund::document {

// N01b (user decision 2026-10-21): the measure fields of a feature that a parameter can drive,
// named as the tool panels name them. Lengths in mm, angles in degrees, counts as numbers.
//   box: width, depth, height, x, y, z      cylinder: radius, height, x, y, z
//   primitive: size1..size4 (as its shape uses them), x, y, z
//   revolve: angle, pitch (a helix)         fillet: radius        chamfer: distance
//   hole: x, y, diameter, depth (blind)     construction plane: offset, angle
//   shell: thickness    draft: angle        pattern: count, spacing (linear), angle (circular)
//   move: x, y, z, rx, ry, rz
// Extrusion distances and sketch dimensions keep their own bindings (PAR01).
struct FeatureField final {
    ParameterUnit unit;
    double value;
};
[[nodiscard]] std::optional<FeatureField> featureField(const FeatureParameters& parameters,
                                                       std::string_view field);
[[nodiscard]] std::vector<std::string> featureFieldNames(const FeatureParameters& parameters);
// Sets the field; false when it already had the value. Throws std::invalid_argument for a field
// the feature does not have or a value it cannot take (a count that is not whole, a radius of 0).
bool setFeatureField(FeatureParameters& parameters, std::string_view field, double value);

// Drops the bindings whose feature or field is gone. A new binding or a changed parameter gives
// the field the parameter's value; a field changed by itself, its parameter not, leaves the
// binding and keeps the new value (a typed number unbinds, PAR02). Without `before` (the state
// the change starts from) every binding counts as new. Throws std::invalid_argument when the
// units differ or a value does not fit.
void applyFieldBindings(ParameterTable& table, std::span<const FeatureDescriptor> features,
                        std::span<FeatureParameters> parameters,
                        const ParameterTable* before = nullptr);

} // namespace voelund::document
