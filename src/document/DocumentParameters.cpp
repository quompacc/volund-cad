#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/IdCapacity.hpp>
#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace voelund::document {
namespace {
bool letter(char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}
bool digit(char value) noexcept { return value >= '0' && value <= '9'; }
char lower(char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}
bool validValue(const DocumentParameter& parameter) {
    switch (parameter.unit) {
    case ParameterUnit::length:
        return kernel::modelCoordinate(parameter.value);
    case ParameterUnit::angle:
        return std::isfinite(parameter.value) && std::abs(parameter.value) <= 360.0;
    case ParameterUnit::number:
        return std::isfinite(parameter.value) && std::abs(parameter.value) <= 1e9;
    }
    return false; // an unknown unit
}
// Units and words of expressions (N01).
constexpr std::array<std::string_view, 12> reservedNames{"mm",  "cm",  "m",  "um", "in",  "ft",
                                                         "deg", "rad", "pi", "e",  "inf", "nan"};
} // namespace

bool sameParameterName(std::string_view left, std::string_view right) noexcept {
    return std::ranges::equal(left, right, [](char a, char b) { return lower(a) == lower(b); });
}

bool isValidParameterName(std::string_view name) noexcept {
    if (name.empty() || name.size() > maximumParameterNameBytes || !letter(name.front()))
        return false;
    if (!std::ranges::all_of(name, [](char c) { return letter(c) || digit(c) || c == '_'; }))
        return false;
    return std::ranges::none_of(
        reservedNames, [&](std::string_view word) { return sameParameterName(word, name); });
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
        if (!validValue(parameter))
            throw std::invalid_argument("Parameter value is out of range: " + parameter.name);
        if (parameter.expression.size() > maximumParameterExpressionBytes ||
            parameter.expression.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid parameter expression: " + parameter.name);
        if (parameter.comment.size() > 4096 || parameter.comment.find('\0') != std::string::npos)
            throw std::invalid_argument("Invalid parameter comment");
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (table.values[prior].id == parameter.id)
                throw std::invalid_argument("Parameter IDs must be unique");
            if (sameParameterName(table.values[prior].name, parameter.name))
                throw std::invalid_argument("Parameter name already exists: " + parameter.name);
        }
    }
    // N01: stored values are what the expressions give.
    ParameterTable resolved = table;
    resolveParameterTable(resolved);
    for (std::size_t index = 0; index < table.values.size(); ++index)
        if (std::abs(resolved.values[index].value - table.values[index].value) >
            1e-9 * std::max(1.0, std::abs(table.values[index].value)))
            throw std::invalid_argument("Parameter value differs from its expression: " +
                                        table.values[index].name);
}

ExpressionDimension parameterDimension(ParameterUnit unit) noexcept {
    return unit == ParameterUnit::length  ? lengthDimension
           : unit == ParameterUnit::angle ? angleDimension
                                          : numberDimension;
}

namespace {

// Resolves one parameter, its inputs first; `path` holds the parameters being resolved.
class Resolver final {
  public:
    explicit Resolver(ParameterTable& table)
        : table_(table), state_(table.values.size(), State::open) {}

    void resolve(std::size_t index) {
        if (state_[index] == State::done)
            return;
        auto& parameter = table_.values[index];
        if (state_[index] == State::busy)
            throw std::invalid_argument("Zyklus: " + cycle(index));
        state_[index] = State::busy;
        path_.push_back(index);
        if (!parameter.expression.empty())
            parameter.value = evaluate(parameter);
        path_.pop_back();
        state_[index] = State::done;
    }

  private:
    enum class State { open, busy, done };

    double evaluate(const DocumentParameter& parameter) {
        try {
            const auto result =
                evaluateExpression(parameter.expression,
                                   [this](std::string_view name) -> std::optional<ExpressionValue> {
                                       const auto* used = findParameter(table_, name);
                                       if (used == nullptr)
                                           return std::nullopt;
                                       const auto index =
                                           static_cast<std::size_t>(used - table_.values.data());
                                       resolve(index);
                                       return ExpressionValue{table_.values[index].value,
                                                              parameterDimension(used->unit)};
                                   });
            return fieldValue(result, parameterDimension(parameter.unit), 0);
        } catch (const ExpressionError& error) {
            throw std::invalid_argument(parameter.name + ": " + error.what());
        }
    }

    std::string cycle(std::size_t index) const {
        std::string text;
        const auto start = std::ranges::find(path_, index);
        for (auto it = start; it != path_.end(); ++it)
            text += table_.values[*it].name + " → ";
        return text + table_.values[index].name;
    }

    ParameterTable& table_;
    std::vector<State> state_;
    std::vector<std::size_t> path_;
};

} // namespace

void resolveParameterTable(ParameterTable& table) {
    Resolver resolver(table);
    for (std::size_t index = 0; index < table.values.size(); ++index)
        resolver.resolve(index);
}

std::vector<ParameterId> parameterDependents(const ParameterTable& table, std::string_view name) {
    std::vector<ParameterId> dependents;
    for (const auto& parameter : table.values) {
        if (parameter.expression.empty())
            continue;
        try {
            const auto names = expressionNames(parameter.expression);
            if (std::ranges::any_of(
                    names, [&](const std::string& used) { return sameParameterName(used, name); }))
                dependents.push_back(parameter.id);
        } catch (const ExpressionError&) {
            continue; // an expression that does not parse uses nothing
        }
    }
    return dependents;
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
