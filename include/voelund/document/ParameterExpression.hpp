#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// N01 (user decision 2026-10-21): expressions in parameters and measure fields, like
// "A*2", "B/2 + 3 mm", "sqrt(A^2 + B^2)" or "min(A; 40)". Numbers take a decimal point or a comma
// between digits; arguments are separated by ';' or by a comma that is not between digits.
// Units: mm, cm, m, in (lengths, kept in mm) and °, deg, rad (angles, kept in degrees).
// Functions: sqrt, abs, round, min, max, sin, cos, tan (an angle, or a plain number read as
// degrees); the constant pi. Every value carries its dimension, so "A*B" for two lengths is an
// area and refused in a length field.
namespace voelund::document {

// Powers of length and angle; a plain number is {0, 0}.
struct ExpressionDimension final {
    int length{0};
    int angle{0};
    [[nodiscard]] bool operator==(const ExpressionDimension&) const = default;
};

inline constexpr ExpressionDimension lengthDimension{1, 0};
inline constexpr ExpressionDimension angleDimension{0, 1};
inline constexpr ExpressionDimension numberDimension{0, 0};

struct ExpressionValue final {
    double value{0}; // mm and degrees
    ExpressionDimension dimension;
};

// A refused expression: the reason in German and the character position it refers to.
class ExpressionError final : public std::invalid_argument {
  public:
    ExpressionError(const std::string& reason, std::size_t position)
        : std::invalid_argument(reason), position_(position) {}
    [[nodiscard]] std::size_t position() const noexcept { return position_; }

  private:
    std::size_t position_;
};

// The value of a parameter by its name (case-insensitive), none when it is unknown.
using ExpressionLookup = std::function<std::optional<ExpressionValue>(std::string_view name)>;

// Evaluates `text` (without the leading '='). Throws ExpressionError.
[[nodiscard]] ExpressionValue evaluateExpression(std::string_view text,
                                                 const ExpressionLookup& lookup);

// The parameter names `text` uses, each once, in order of appearance. Throws ExpressionError
// for text that does not parse.
[[nodiscard]] std::vector<std::string> expressionNames(std::string_view text);

// `text` with every use of the name `from` (case-insensitive) written as `to`.
[[nodiscard]] std::string renameInExpression(std::string_view text, std::string_view from,
                                             std::string_view to);

// The value in a field of `dimension`: a plain number counts as mm or degrees there. Throws
// ExpressionError when the dimensions differ.
[[nodiscard]] double fieldValue(const ExpressionValue& value, ExpressionDimension dimension,
                                std::size_t position);

} // namespace voelund::document
