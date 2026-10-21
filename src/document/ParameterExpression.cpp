#include <voelund/document/ParameterExpression.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace voelund::document {
namespace {

enum class TokenKind { number, name, symbol, end };

struct Token final {
    TokenKind kind{TokenKind::end};
    double number{0};
    std::string text;         // name, unit or symbol
    ExpressionDimension unit; // of a number with a unit
    bool hasUnit{false};
    std::size_t position{0};
};

bool sameName(std::string_view a, std::string_view b) {
    return std::ranges::equal(
        a, b, [](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
}

bool isNameStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }
bool isNamePart(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }
bool isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

// A unit's factor to mm or degrees and its dimension; none for any other word.
std::optional<std::pair<double, ExpressionDimension>> unitOf(std::string_view word) {
    if (sameName(word, "mm"))
        return std::pair{1.0, lengthDimension};
    if (sameName(word, "cm"))
        return std::pair{10.0, lengthDimension};
    if (sameName(word, "m"))
        return std::pair{1000.0, lengthDimension};
    if (sameName(word, "in"))
        return std::pair{25.4, lengthDimension};
    if (sameName(word, "deg") || word == "\xC2\xB0")
        return std::pair{1.0, angleDimension};
    if (sameName(word, "rad"))
        return std::pair{180.0 / std::numbers::pi, angleDimension};
    return std::nullopt;
}

class Lexer final {
  public:
    explicit Lexer(std::string_view text) : text_(text) {}

    std::vector<Token> tokens() {
        std::vector<Token> result;
        while (true) {
            skipSpace();
            if (at_ >= text_.size()) {
                result.push_back({TokenKind::end, 0, {}, {}, false, at_});
                return result;
            }
            result.push_back(next());
        }
    }

  private:
    void skipSpace() {
        while (at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_])) != 0)
            ++at_;
    }

    Token next() {
        const std::size_t start = at_;
        if (isDigit(text_[at_]) ||
            (text_[at_] == '.' && at_ + 1 < text_.size() && isDigit(text_[at_ + 1])))
            return number(start);
        if (isNameStart(text_[at_])) {
            while (at_ < text_.size() && isNamePart(text_[at_]))
                ++at_;
            return {TokenKind::name, 0,    std::string(text_.substr(start, at_ - start)), {},
                    false,           start};
        }
        return symbol(start);
    }

    Token symbol(std::size_t start) {
        // × and ÷ (UTF-8) stand for * and /.
        if (text_.substr(at_, 2) == "\xC3\x97" || text_.substr(at_, 2) == "\xC3\xB7") {
            const char op = text_[at_ + 1] == '\x97' ? '*' : '/';
            at_ += 2;
            return {TokenKind::symbol, 0, std::string(1, op), {}, false, start};
        }
        const char c = text_[at_++];
        if (std::string_view("+-*/^();,").find(c) == std::string_view::npos)
            throw ExpressionError("Unerwartetes Zeichen „" + std::string(1, c) + "“", start);
        return {TokenKind::symbol, 0, std::string(1, c == ',' ? ';' : c), {}, false, start};
    }

    Token number(std::size_t start) {
        std::string digits;
        while (at_ < text_.size() &&
               (isDigit(text_[at_]) || text_[at_] == '.' || decimalComma(at_))) {
            digits += text_[at_] == ',' ? '.' : text_[at_];
            ++at_;
        }
        Token token{TokenKind::number, 0, {}, {}, false, start};
        try {
            std::size_t used = 0;
            token.number = std::stod(digits, &used);
            if (used != digits.size())
                throw ExpressionError("Ungültige Zahl „" + digits + "“", start);
        } catch (const std::logic_error&) {
            throw ExpressionError("Ungültige Zahl „" + digits + "“", start);
        }
        attachUnit(token);
        return token;
    }

    // A comma between two digits is a decimal comma; any other comma separates arguments.
    [[nodiscard]] bool decimalComma(std::size_t at) const {
        return text_[at] == ',' && at > 0 && isDigit(text_[at - 1]) && at + 1 < text_.size() &&
               isDigit(text_[at + 1]);
    }

    void attachUnit(Token& token) {
        std::size_t look = at_;
        while (look < text_.size() && text_[look] == ' ')
            ++look;
        std::size_t end = look;
        if (text_.substr(look, 2) == "\xC2\xB0")
            end = look + 2;
        else
            while (end < text_.size() && isNamePart(text_[end]))
                ++end;
        const auto unit = unitOf(text_.substr(look, end - look));
        if (!unit || end == look)
            return;
        token.number *= unit->first;
        token.unit = unit->second;
        token.hasUnit = true;
        at_ = end;
    }

    std::string_view text_;
    std::size_t at_{0};
};

// A value while parsing; a bare number (no unit, no parameter) takes the dimension of what it is
// added to or compared with.
struct Operand final {
    double value{0};
    ExpressionDimension dimension;
    bool bare{false};
};

class Parser final {
  public:
    Parser(std::string_view text, const ExpressionLookup* lookup)
        : tokens_(Lexer(text).tokens()), lookup_(lookup) {}

    Operand parse() {
        if (peek().kind == TokenKind::end)
            throw ExpressionError("Der Ausdruck ist leer", 0);
        Operand result = sum();
        if (peek().kind != TokenKind::end)
            throw ExpressionError("Unerwartet: „" + describe(peek()) + "“", peek().position);
        return result;
    }

    std::vector<std::string> names;

  private:
    const Token& peek() const { return tokens_[at_]; }
    const Token& take() { return tokens_[at_ < tokens_.size() - 1 ? at_++ : at_]; }
    bool symbol(char c) const { return peek().kind == TokenKind::symbol && peek().text[0] == c; }
    static std::string describe(const Token& token) {
        return token.kind == TokenKind::number ? "Zahl" : token.text;
    }
    void expect(char c) {
        if (!symbol(c))
            throw ExpressionError(std::string("Erwartet: „") + c + "“", peek().position);
        take();
    }

    static void match(Operand& a, Operand& b, std::size_t position, const char* what) {
        if (a.bare && !b.bare)
            a.dimension = b.dimension;
        else if (b.bare && !a.bare)
            b.dimension = a.dimension;
        if (a.dimension != b.dimension)
            throw ExpressionError(std::string("Verschiedene Einheiten lassen sich nicht ") + what,
                                  position);
    }

    Operand sum() {
        Operand result = product();
        while (symbol('+') || symbol('-')) {
            const Token op = take();
            Operand right = product();
            match(result, right, op.position, op.text == "+" ? "addieren" : "abziehen");
            result.value += op.text == "+" ? right.value : -right.value;
            result.bare = result.bare && right.bare;
        }
        return result;
    }

    Operand product() {
        Operand result = unary();
        while (symbol('*') || symbol('/')) {
            const Token op = take();
            const Operand right = unary();
            if (op.text == "/" && right.value == 0)
                throw ExpressionError("Division durch null", op.position);
            const int sign = op.text == "*" ? 1 : -1;
            result.value = op.text == "*" ? result.value * right.value : result.value / right.value;
            result.dimension = {result.dimension.length + sign * right.dimension.length,
                                result.dimension.angle + sign * right.dimension.angle};
            result.bare = result.bare && right.bare;
        }
        return result;
    }

    Operand unary() {
        if (symbol('-') || symbol('+')) {
            const bool negative = take().text == "-";
            Operand value = unary();
            if (negative)
                value.value = -value.value;
            return value;
        }
        return power();
    }

    Operand power() {
        Operand base = primary();
        if (!symbol('^'))
            return base;
        const Token op = take();
        const Operand exponent = unary();
        if (exponent.dimension != numberDimension)
            throw ExpressionError("Ein Exponent hat keine Einheit", op.position);
        const bool whole = exponent.value == std::round(exponent.value);
        if (base.dimension != numberDimension && !whole)
            throw ExpressionError("Größen mit Einheit nur mit ganzen Exponenten", op.position);
        const int times = whole ? static_cast<int>(exponent.value) : 0;
        base.value = std::pow(base.value, exponent.value);
        base.dimension = {base.dimension.length * times, base.dimension.angle * times};
        return base;
    }

    Operand primary() {
        const Token token = take();
        if (token.kind == TokenKind::number)
            return {token.number, token.unit, !token.hasUnit};
        if (token.kind == TokenKind::name)
            return withUnit(symbol('(') ? call(token) : name(token));
        if (token.kind == TokenKind::symbol && token.text == "(") {
            Operand inner = sum();
            expect(')');
            return withUnit(inner);
        }
        throw ExpressionError(token.kind == TokenKind::end ? "Der Ausdruck endet zu früh"
                                                           : "Unerwartet: „" + token.text + "“",
                              token.position);
    }

    // A unit after a name or a bracket, as in "pi rad" or "(A + 2) cm", scales a plain value.
    Operand withUnit(Operand value) {
        if (peek().kind != TokenKind::name || value.dimension != numberDimension)
            return value;
        const auto unit = unitOf(peek().text);
        if (!unit)
            return value;
        take();
        return {value.value * unit->first, unit->second, false};
    }

    Operand name(const Token& token) {
        // Only collecting names, every value is bare, so no unit is checked.
        if (sameName(token.text, "pi"))
            return {std::numbers::pi, numberDimension, lookup_ == nullptr};
        if (std::ranges::none_of(names,
                                 [&](const std::string& n) { return sameName(n, token.text); }))
            names.push_back(token.text);
        if (lookup_ == nullptr)
            return {1, numberDimension, true};
        const auto value = (*lookup_)(token.text);
        if (!value)
            throw ExpressionError("Unbekannter Parameter „" + token.text + "“", token.position);
        return {value->value, value->dimension, false};
    }

    std::vector<Operand> arguments() {
        expect('(');
        std::vector<Operand> values{sum()};
        while (symbol(';')) {
            take();
            values.push_back(sum());
        }
        expect(')');
        return values;
    }

    Operand call(const Token& function) {
        auto values = arguments();
        const auto count = [&](std::size_t wanted) {
            if (values.size() != wanted)
                throw ExpressionError(function.text + " braucht " + std::to_string(wanted) +
                                          (wanted == 1 ? " Wert" : " Werte"),
                                      function.position);
        };
        const std::string lower = [&] {
            std::string text = function.text;
            std::ranges::transform(text, text.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return text;
        }();
        if (lower == "min" || lower == "max")
            return extremum(values, lower == "min", function.position, count);
        count(1);
        return single(lower, values[0], function);
    }

    template <class Count>
    static Operand extremum(std::vector<Operand>& values, bool minimum, std::size_t position,
                            const Count& count) {
        count(2);
        match(values[0], values[1], position, "vergleichen");
        const bool first =
            minimum ? values[0].value <= values[1].value : values[0].value >= values[1].value;
        return first ? values[0] : values[1];
    }

    static Operand single(const std::string& function, Operand value, const Token& token) {
        if (function == "abs")
            return {std::abs(value.value), value.dimension, value.bare};
        if (function == "round")
            return {std::round(value.value), value.dimension, value.bare};
        if (function == "sqrt")
            return root(value, token.position);
        if (function == "sin" || function == "cos" || function == "tan")
            return trigonometric(function, value, token.position);
        throw ExpressionError("Unbekannte Funktion „" + token.text + "“", token.position);
    }

    static Operand root(const Operand& value, std::size_t position) {
        if (value.value < 0)
            throw ExpressionError("Wurzel aus einer negativen Zahl", position);
        if (value.dimension.length % 2 != 0 || value.dimension.angle % 2 != 0)
            throw ExpressionError("Die Wurzel dieser Einheit ist keine Länge", position);
        return {std::sqrt(value.value),
                {value.dimension.length / 2, value.dimension.angle / 2},
                value.bare};
    }

    static Operand trigonometric(const std::string& function, const Operand& value,
                                 std::size_t position) {
        if (value.dimension != angleDimension && value.dimension != numberDimension)
            throw ExpressionError(function + " braucht einen Winkel", position);
        const double radians = value.value * std::numbers::pi / 180.0;
        const double result = function == "sin"   ? std::sin(radians)
                              : function == "cos" ? std::cos(radians)
                                                  : std::tan(radians);
        return {result, numberDimension, false};
    }

    std::vector<Token> tokens_;
    std::size_t at_{0};
    const ExpressionLookup* lookup_;
};

} // namespace

ExpressionValue evaluateExpression(std::string_view text, const ExpressionLookup& lookup) {
    Parser parser(text, &lookup);
    const Operand result = parser.parse();
    if (!std::isfinite(result.value))
        throw ExpressionError("Der Ausdruck ergibt keine endliche Zahl", 0);
    return {result.value, result.dimension};
}

std::vector<std::string> expressionNames(std::string_view text) {
    Parser parser(text, nullptr);
    static_cast<void>(parser.parse());
    return parser.names;
}

std::string renameInExpression(std::string_view text, std::string_view from, std::string_view to) {
    std::string result;
    std::size_t at = 0;
    while (at < text.size()) {
        if (!isNameStart(text[at]) ||
            (at > 0 && (isNamePart(text[at - 1]) || isDigit(text[at - 1])))) {
            result += text[at++];
            continue;
        }
        std::size_t end = at;
        while (end < text.size() && isNamePart(text[end]))
            ++end;
        const std::string_view word = text.substr(at, end - at);
        result += sameName(word, from) ? std::string(to) : std::string(word);
        at = end;
    }
    return result;
}

double fieldValue(const ExpressionValue& value, ExpressionDimension dimension,
                  std::size_t position) {
    if (value.dimension == dimension || value.dimension == numberDimension)
        return value.value;
    const char* wanted = dimension == lengthDimension  ? "eine Länge"
                         : dimension == angleDimension ? "einen Winkel"
                                                       : "eine Zahl";
    throw ExpressionError(std::string("Hier wird ") + wanted + " erwartet", position);
}

} // namespace voelund::document
