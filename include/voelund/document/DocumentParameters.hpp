#pragma once

#include <voelund/document/FeatureId.hpp>
#include <voelund/document/ParameterExpression.hpp>
#include <voelund/document/ParameterId.hpp>
#include <voelund/document/SketchGeometry.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace voelund::document {

// PAR01 (docs/parameter-table.md): one table of named values per document.
// Lengths are stored in millimetres, angles in degrees, as the user enters them.
// N01 (schema 37): plain numbers, e.g. a pattern's count.
enum class ParameterUnit { length = 1, angle = 2, number = 3 };

struct DocumentParameter final {
    ParameterId id;
    std::string name;
    ParameterUnit unit;
    double value;
    std::string comment{};
    // N01 (user decision 2026-10-21, schema 37): "A*2" etc.; empty for a plain value. `value`
    // holds what it gives (resolveParameterTable).
    std::string expression{};

    [[nodiscard]] bool operator==(const DocumentParameter&) const = default;
};

// N01b (user decision 2026-10-21, schema 37): a measure field of a feature ("radius",
// "diameter", ...; see featureField) that follows a parameter.
struct FieldBinding final {
    FeatureId feature;
    std::string field;
    ParameterId parameter;

    [[nodiscard]] bool operator==(const FieldBinding&) const = default;
};
inline constexpr std::size_t maximumFieldBindings = 10'000;

struct ParameterTable final {
    std::vector<DocumentParameter> values;
    std::uint64_t nextId{1};
    // N01b: one list for the fields of every feature; sketch dimensions and extrusion distances
    // keep their own bindings (PAR01).
    std::vector<FieldBinding> bindings{};

    [[nodiscard]] bool operator==(const ParameterTable&) const = default;
};

inline constexpr std::size_t maximumParameterNameBytes = 64;
inline constexpr std::size_t maximumParameterCount = 10'000;

// A letter, then letters, digits or '_' (ASCII); not a unit or reserved word.
[[nodiscard]] bool isValidParameterName(std::string_view name) noexcept;
// Names are unique without regard to case.
[[nodiscard]] bool sameParameterName(std::string_view left, std::string_view right) noexcept;
// Throws std::invalid_argument for invalid names, duplicates, values, IDs or comments, and for
// field bindings to unknown parameters or bound twice.
void validateParameterTable(const ParameterTable& table);
[[nodiscard]] const DocumentParameter* findParameter(const ParameterTable& table,
                                                     ParameterId id) noexcept;
[[nodiscard]] const DocumentParameter* findParameter(const ParameterTable& table,
                                                     std::string_view name) noexcept;
inline constexpr std::size_t maximumParameterExpressionBytes = 1024;
// N01: the dimension values of `unit` carry in expressions.
[[nodiscard]] ExpressionDimension parameterDimension(ParameterUnit unit) noexcept;
// N01: sets the value of every parameter with an expression, others first. Throws
// std::invalid_argument naming a cycle ("A → B → A"), an unknown name or a wrong unit.
void resolveParameterTable(ParameterTable& table);
// N01: the parameters whose expressions use the parameter `name`.
[[nodiscard]] std::vector<ParameterId> parameterDependents(const ParameterTable& table,
                                                           std::string_view name);
// The unit a dimensional constraint kind accepts; nullopt for kinds without a value.
[[nodiscard]] std::optional<ParameterUnit> dimensionUnit(SketchConstraintKind kind) noexcept;

} // namespace voelund::document
