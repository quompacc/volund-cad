#include <voelund/document/ParameterExpression.hpp>

#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

// N01: arithmetic, units, functions, decimal commas, dimensions, refusals with a position, the
// names an expression uses and renaming inside expressions.
namespace {
using namespace voelund::document;

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9 * std::max(1.0, std::abs(b)); }

const std::map<std::string, ExpressionValue> parameters{
    {"a", {10, lengthDimension}},     {"b", {20, lengthDimension}}, {"c", {3, lengthDimension}},
    {"d", {4, lengthDimension}},      {"w", {30, angleDimension}},  {"n", {6, numberDimension}},
    {"breite", {50, lengthDimension}}};

ExpressionValue eval(const std::string& text) {
    return evaluateExpression(text, [](std::string_view name) -> std::optional<ExpressionValue> {
        std::string lower(name);
        for (auto& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const auto found = parameters.find(lower);
        if (found == parameters.end())
            return std::nullopt;
        return found->second;
    });
}

void value(const std::string& text, double expected, ExpressionDimension dimension) {
    ExpressionValue result;
    try {
        result = eval(text);
    } catch (const ExpressionError& error) {
        throw std::runtime_error("\"" + text + "\": " + error.what());
    }
    require(near(result.value, expected) && result.dimension == dimension,
            "\"" + text + "\" gives " + std::to_string(result.value));
}

void refused(const std::string& text, const std::string& reason) {
    try {
        static_cast<void>(eval(text));
    } catch (const ExpressionError& error) {
        require(std::string(error.what()).find(reason) != std::string::npos,
                "\"" + text + "\" refused as: " + error.what());
        return;
    }
    throw std::runtime_error("\"" + text + "\" was not refused");
}

void arithmetic() {
    value("2+3*4", 14, numberDimension);
    value("(2+3)*4", 20, numberDimension);
    value("A*2", 20, lengthDimension);
    value("B/2 + 3 mm", 13, lengthDimension);
    value("a + 3", 13, lengthDimension); // a bare number takes the length
    value("-A + 2*B", 30, lengthDimension);
    value("3 \xC3\x97 4 \xC3\xB7 2", 6, numberDimension);
    value("2^3", 8, numberDimension);
    value("2^3^2", 512, numberDimension); // right-associative
    value("N*A", 60, lengthDimension);
}

void unitsAndFunctions() {
    value("2,5 cm", 25, lengthDimension);
    value("1 in", 25.4, lengthDimension);
    value("0.5m", 500, lengthDimension);
    value("90\xC2\xB0", 90, angleDimension);
    value("pi rad", 180, angleDimension);
    value("sqrt(C^2 + D^2)", 5, lengthDimension);
    value("min(Breite; 40)", 40, lengthDimension);
    value("max(A, 40)", 40, lengthDimension);
    value("abs(-A)", 10, lengthDimension);
    value("round(2,6)", 3, numberDimension);
    value("sin(30)", 0.5, numberDimension);
    value("cos(60 deg)", 0.5, numberDimension);
    value("2*W", 60, angleDimension);
    value("pi", 3.141592653589793, numberDimension);
}

void refusals() {
    refused("A + 90\xC2\xB0", "Einheiten");
    refused("Q*2", "Unbekannter Parameter");
    refused("foo(2)", "Unbekannte Funktion");
    refused("2/0", "Division durch null");
    refused("sqrt(-1)", "negativen");
    refused("sqrt(A)", "Wurzel");
    refused("(2+3", "Erwartet");
    refused("", "leer");
    refused("2 $", "Unerwartetes Zeichen");
    refused("2*", "zu früh");
    refused("min(2)", "2 Werte");
    refused("sin(A)", "Winkel");
    try {
        static_cast<void>(fieldValue(eval("A*B"), lengthDimension, 0));
        throw std::runtime_error("An area was taken for a length");
    } catch (const ExpressionError& error) {
        require(std::string(error.what()).find("Länge") != std::string::npos, error.what());
    }
    require(near(fieldValue(eval("12"), lengthDimension, 0), 12), "A plain number counts as mm");
    try {
        static_cast<void>(eval("A + Q"));
    } catch (const ExpressionError& error) {
        require(error.position() == 4, "The position points at Q");
    }
}

void namesAndRenaming() {
    const auto names = expressionNames("A*b + sqrt(a) + pi + 2 mm");
    require(names == std::vector<std::string>{"A", "b"}, "Names once each, pi and units are none");
    require(renameInExpression("a*2 + ab + A", "a", "Breite") == "Breite*2 + ab + Breite",
            "Only whole names are renamed");
    require(renameInExpression("2mm + m", "m", "Mass") == "2mm + Mass",
            "A unit after a number stays");
}

} // namespace

int main() {
    try {
        arithmetic();
        unitsAndFunctions();
        refusals();
        namesAndRenaming();
        std::cout << "PASS N01 expressions: arithmetic, units, functions, dimensions, refusals, "
                     "names, renaming\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
