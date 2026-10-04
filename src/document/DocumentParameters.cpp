#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/IdCapacity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace voelund::document {
namespace {
bool letter(char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}
bool digit(char value) noexcept { return value >= '0' && value <= '9'; }
char lower(char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}
// Units and words later expressions (v2) will need.
constexpr std::array<std::string_view, 12> reservedNames{
    "mm", "cm", "m", "um", "in", "ft", "deg", "rad", "pi", "e", "inf", "nan"};
} // namespace

bool sameParameterName(std::string_view left, std::string_view right) noexcept {
    return std::ranges::equal(left, right, [](char a, char b) { return lower(a) == lower(b); });
}

bool isValidParameterName(std::string_view name) noexcept {
    if (name.empty() || name.size() > maximumParameterNameBytes || !letter(name.front()))
        return false;
    if (!std::ranges::all_of(name, [](char c) { return letter(c) || digit(c) || c == '_'; }))
        return false;
    return std::ranges::none_of(reservedNames,
                                [&](std::string_view word) { return sameParameterName(word, name); });
}

void validateParameterTable(const ParameterTable& table) {
    if (table.values.size() > maximumParameterCount || !hasIdCapacity(table.nextId, 0))
        throw std::invalid_argument("Parameter table exceeds its limits");
    for (std::size_t index = 0; index < table.values.size(); ++index) {
        const auto& parameter = table.values[index];
        if (parameter.id.value() == 0 || parameter.id.value() >= table.nextId)
            throw std::invalid_argument("Parameter ID is outside the allocated range");
        if (!isValidParameterName(parameter.name))
            throw std::invalid_argument("Invalid parameter name: " + parameter.name);
        if (parameter.unit != ParameterUnit::length && parameter.unit != ParameterUnit::angle)
            throw std::invalid_argument("Unknown parameter unit");
        if (parameter.unit == ParameterUnit::length ? !kernel::modelCoordinate(parameter.value)
                                                    : !std::isfinite(parameter.value) ||
                                                          std::abs(parameter.value) > 360.0)
            throw std::invalid_argument("Parameter value is out of range: " + parameter.name);
        if (parameter.comment.size() > 4096 ||
            parameter.comment.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid parameter comment");
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (table.values[prior].id == parameter.id)
                throw std::invalid_argument("Parameter IDs must be unique");
            if (sameParameterName(table.values[prior].name, parameter.name))
                throw std::invalid_argument("Parameter name already exists: " + parameter.name);
        }
    }
}

const DocumentParameter* findParameter(const ParameterTable& table, ParameterId id) noexcept {
    const auto found = std::ranges::find(table.values, id, &DocumentParameter::id);
    return found == table.values.end() ? nullptr : &*found;
}

const DocumentParameter* findParameter(const ParameterTable& table,
                                       std::string_view name) noexcept {
    const auto found = std::ranges::find_if(
        table.values, [&](const DocumentParameter& p) { return sameParameterName(p.name, name); });
    return found == table.values.end() ? nullptr : &*found;
}

std::optional<ParameterUnit> dimensionUnit(SketchConstraintKind kind) noexcept {
    switch (kind) {
    case SketchConstraintKind::distance:
    case SketchConstraintKind::radius:
    case SketchConstraintKind::diameter:
    case SketchConstraintKind::pointDistance:
    case SketchConstraintKind::pointLineDistance:
    case SketchConstraintKind::pointDistanceX:
    case SketchConstraintKind::pointDistanceY:
        return ParameterUnit::length;
    case SketchConstraintKind::angle:
        return ParameterUnit::angle;
    default:
        return std::nullopt;
    }
}

} // namespace voelund::document
