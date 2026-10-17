#include "DocumentSerializationDetail.hpp"
#include <voelund/kernel/NumericPolicy.hpp>
#include <voelund/kernel/NumericValidation.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <ranges>
#include <sstream>
#include <stdexcept>

namespace voelund::document::detail {

UnsignedValue unsignedValue(std::uint64_t& value) { return {value}; }

std::istream& operator>>(std::istream& input, UnsignedValue value) {
    std::string token;
    if (!(input >> token))
        return input;
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
        input.setstate(std::ios::failbit);
    else
        value.value = parsed;
    return input;
}

[[noreturn]] void invalidDocument(const std::string& reason) {
    throw std::runtime_error("Invalid Völund document: " + reason);
}

std::string requiredLine(std::istream& input, const char* description) {
    std::string line;
    if (!std::getline(input, line))
        invalidDocument(std::string("missing ") + description);
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    return line;
}

std::uint64_t headerValue(std::istream& input, const char* expectedName) {
    std::istringstream line(requiredLine(input, expectedName));
    line.imbue(std::locale::classic());
    std::string name;
    std::uint64_t value = 0;
    std::string trailing;
    if (!(line >> name >> unsignedValue(value)) || name != expectedName || (line >> trailing))
        invalidDocument(std::string("malformed ") + expectedName + " header");
    return value;
}

kernel::RigidTransform readTransform(std::istringstream& line) {
    kernel::RigidTransform transform{};
    double xDegrees = 0, yDegrees = 0, zDegrees = 0;
    if (!(line >> transform.translationX >> transform.translationY >> transform.translationZ >>
          xDegrees >> yDegrees >> zDegrees))
        invalidDocument("malformed feature placement");
    transform.rotationX = kernel::toRadians(kernel::Degrees(xDegrees));
    transform.rotationY = kernel::toRadians(kernel::Degrees(yDegrees));
    transform.rotationZ = kernel::toRadians(kernel::Degrees(zDegrees));
    return transform;
}

void validateTransform(const kernel::RigidTransform& transform) {
    if (!kernel::modelCoordinate(transform.translationX) ||
        !kernel::modelCoordinate(transform.translationY) ||
        !kernel::modelCoordinate(transform.translationZ))
        invalidDocument(kernel::modelRangeDiagnostic);
    const double values[] = {transform.translationX,      transform.translationY,
                             transform.translationZ,      transform.rotationX.value(),
                             transform.rotationY.value(), transform.rotationZ.value()};
    for (double value : values)
        if (!std::isfinite(value))
            invalidDocument("placement contains a non-finite value");
}

bool positiveFinite(double value) { return kernel::modelLength(value); }

void validateConstraintVersion(SketchConstraintKind kind, std::uint64_t version) {
    using Kind = SketchConstraintKind;
    // Upper endpoints of the append-only enum groups introduced by each format.
    constexpr std::pair<Kind, std::uint64_t> introduced[] = {{Kind::perpendicular, 4},
                                                             {Kind::pointOnObject, 6},
                                                             {Kind::tangent, 7},
                                                             {Kind::symmetric, 8},
                                                             {Kind::angle, 9},
                                                             {Kind::pointDistanceY, 10},
                                                             {Kind::perpendicularBisector, 11},
                                                             {Kind::diameter, 12},
                                                             {Kind::verticalPoints, 13},
                                                             {Kind::pointLineDistance, 14}};
    const auto match =
        std::ranges::find_if(introduced, [kind](const auto& item) { return kind <= item.first; });
    if (match == std::end(introduced) || version < match->second)
        invalidDocument("constraint kind is newer than file version");
}

CylinderResultMode readResultMode(std::istringstream& line) {
    std::string mode;
    if (!(line >> mode))
        invalidDocument("missing cylinder result mode");
    if (mode == "JOIN")
        return CylinderResultMode::join;
    if (mode == "CUT")
        return CylinderResultMode::cut;
    if (mode == "INTERSECT")
        return CylinderResultMode::intersect;
    if (mode == "SEPARATE")
        return CylinderResultMode::separate;
    invalidDocument("unknown cylinder result mode");
}

const char* resultModeName(CylinderResultMode mode) {
    switch (mode) {
    case CylinderResultMode::join:
        return "JOIN";
    case CylinderResultMode::cut:
        return "CUT";
    case CylinderResultMode::intersect:
        return "INTERSECT";
    case CylinderResultMode::separate:
        return "SEPARATE";
    }
    return "JOIN";
}

SketchLineConstraint readLineConstraint(std::istringstream& line) {
    std::string constraint;
    if (!(line >> constraint))
        invalidDocument("missing sketch line constraint");
    if (constraint == "FREE")
        return SketchLineConstraint::none;
    if (constraint == "HORIZONTAL")
        return SketchLineConstraint::horizontal;
    if (constraint == "VERTICAL")
        return SketchLineConstraint::vertical;
    invalidDocument("unknown sketch line constraint");
}

const char* lineConstraintName(SketchLineConstraint constraint) {
    if (constraint == SketchLineConstraint::horizontal)
        return "HORIZONTAL";
    if (constraint == SketchLineConstraint::vertical)
        return "VERTICAL";
    return "FREE";
}

SketchPointPosition readPointPosition(std::istringstream& line) {
    std::string value;
    if (!(line >> value))
        invalidDocument("missing sketch point position");
    if (value == "START")
        return SketchPointPosition::start;
    if (value == "END")
        return SketchPointPosition::end;
    if (value == "CENTER")
        return SketchPointPosition::center;
    invalidDocument("unknown sketch point position");
}

const char* pointPositionName(SketchPointPosition position) {
    if (position == SketchPointPosition::end)
        return "END";
    if (position == SketchPointPosition::center)
        return "CENTER";
    return "START";
}

void writeTransform(std::ostream& output, const kernel::RigidTransform& transform) {
    output << ' ' << transform.translationX << ' ' << transform.translationY << ' '
           << transform.translationZ << ' ' << kernel::toDegrees(transform.rotationX).value() << ' '
           << kernel::toDegrees(transform.rotationY).value() << ' '
           << kernel::toDegrees(transform.rotationZ).value();
}

bool containsFeature(const std::vector<FeatureDescriptor>& features, FeatureId featureId) {
    return std::ranges::find(features, featureId, &FeatureDescriptor::id) != features.end();
}

bool containsFeatureKind(const std::vector<FeatureDescriptor>& features, FeatureId featureId,
                         FeatureKind kind) {
    const auto feature = std::ranges::find(features, featureId, &FeatureDescriptor::id);
    return feature != features.end() && feature->kind == kind;
}

void ensureLineComplete(std::istringstream& line) {
    std::string trailing;
    if (line >> trailing)
        invalidDocument("unexpected data after feature");
}

} // namespace voelund::document::detail
