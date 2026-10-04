#pragma once

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
enum class ParameterUnit { length = 1, angle = 2 };

struct DocumentParameter final {
    ParameterId id;
    std::string name;
    ParameterUnit unit;
    double value;
    std::string comment{};

    [[nodiscard]] bool operator==(const DocumentParameter&) const = default;
};

struct ParameterTable final {
    std::vector<DocumentParameter> values;
    std::uint64_t nextId{1};

    [[nodiscard]] bool operator==(const ParameterTable&) const = default;
};

inline constexpr std::size_t maximumParameterNameBytes = 64;
inline constexpr std::size_t maximumParameterCount = 10'000;

// A letter, then letters, digits or '_' (ASCII); not a unit or reserved word.
[[nodiscard]] bool isValidParameterName(std::string_view name) noexcept;
// Names are unique without regard to case.
[[nodiscard]] bool sameParameterName(std::string_view left, std::string_view right) noexcept;
// Throws std::invalid_argument for invalid names, duplicates, values, IDs or comments.
void validateParameterTable(const ParameterTable& table);
[[nodiscard]] const DocumentParameter* findParameter(const ParameterTable& table,
                                                     ParameterId id) noexcept;
[[nodiscard]] const DocumentParameter* findParameter(const ParameterTable& table,
                                                     std::string_view name) noexcept;
// The unit a dimensional constraint kind accepts; nullopt for kinds without a value.
[[nodiscard]] std::optional<ParameterUnit> dimensionUnit(SketchConstraintKind kind) noexcept;

} // namespace voelund::document
