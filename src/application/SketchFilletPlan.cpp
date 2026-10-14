#include "SketchFilletPlan.hpp"

#include <voelund/kernel/NumericPolicy.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <sstream>

namespace voelund::application::detail {
namespace {

using document::SketchElementId;
using document::SketchLine;
using document::SketchPoint;
using Position = document::SketchPointPosition;

constexpr double nullSegment = kernel::minimumModelLength;

double distance(SketchPoint a, SketchPoint b) { return std::hypot(a.x - b.x, a.y - b.y); }
SketchPoint point(const SketchLine& line, Position position) {
    return position == Position::start ? line.start : line.end;
}
Position other(Position position) {
    return position == Position::start ? Position::end : Position::start;
}
std::string millimetres(double value) {
    std::ostringstream text;
    text << std::fixed << std::setprecision(3) << value << " mm";
    return text.str();
}

// The end each line keeps: away from the corner, or the selected one where the lines cross.
Position keptEnd(const SketchLine& line, SketchPoint corner, Position selected) {
    const double toStart = distance(corner, line.start);
    const double toEnd = distance(corner, line.end);
    if (toStart < nullSegment)
        return Position::end;
    if (toEnd < nullSegment)
        return Position::start;
    const double dx = line.end.x - line.start.x;
    const double dy = line.end.y - line.start.y;
    const double t =
        ((corner.x - line.start.x) * dx + (corner.y - line.start.y) * dy) / (dx * dx + dy * dy);
    if (t < 0.0 || t > 1.0)
        return toStart > toEnd ? Position::start : Position::end;
    return selected == Position::center ? Position::end : selected;
}

} // namespace

SketchFilletPlan planCorner(SketchCornerKind kind, SketchElementId firstId, const SketchLine& first,
                            Position firstSelected, SketchElementId secondId,
                            const SketchLine& second, Position secondSelected, double size) {
    const bool chamfer = kind == SketchCornerKind::chamfer;
    const std::string prefix = chamfer ? "Fasen: " : "Verrunden: ";
    const auto refuse = [&prefix](const std::string& reason,
                                  std::vector<SketchElementId> elements) {
        throw SketchEditRefusal(prefix + reason, std::move(elements));
    };
    const std::string sizeName = chamfer ? "Der Abstand " : "Der Radius ";
    const double radius = size;
    if (!std::isfinite(size) || size <= nullSegment)
        refuse(sizeName + "muss größer als 0 sein", {});
    const double ax = first.end.x - first.start.x, ay = first.end.y - first.start.y;
    const double bx = second.end.x - second.start.x, by = second.end.y - second.start.y;
    const double cross = ax * by - ay * bx;
    if (std::abs(cross) <= 1.0e-10 * std::hypot(ax, ay) * std::hypot(bx, by)) {
        const double offset = std::abs((second.start.x - first.start.x) * ay -
                                       (second.start.y - first.start.y) * ax) /
                              std::hypot(ax, ay);
        refuse(offset < nullSegment ? "Die Linien liegen auf einer Geraden; es gibt keine Ecke"
                                    : "Die Linien sind parallel; es gibt keine Ecke",
               {firstId, secondId});
    }
    const double t = ((second.start.x - first.start.x) * by - (second.start.y - first.start.y) * bx) /
                     cross;
    const SketchPoint corner{first.start.x + ax * t, first.start.y + ay * t};

    const Position firstKept = keptEnd(first, corner, firstSelected);
    const Position secondKept = keptEnd(second, corner, secondSelected);
    const SketchPoint firstFar = point(first, firstKept);
    const SketchPoint secondFar = point(second, secondKept);
    const double firstLength = distance(corner, firstFar);
    const double secondLength = distance(corner, secondFar);
    const SketchPoint u{(firstFar.x - corner.x) / firstLength, (firstFar.y - corner.y) / firstLength};
    const SketchPoint v{(secondFar.x - corner.x) / secondLength,
                        (secondFar.y - corner.y) / secondLength};
    const double theta = std::acos(std::clamp(u.x * v.x + u.y * v.y, -1.0, 1.0));
    if (theta >= std::numbers::pi - 1.0e-6)
        refuse("Die Linien setzen sich gerade fort; es gibt keine Ecke", {firstId, secondId});
    if (theta <= 1.0e-6)
        refuse("Die Linien liegen übereinander", {firstId, secondId});

    // The arc touches each line `reach` from the corner; each kept part must be longer. A
    // chamfer cuts at its distance.
    const double reach = chamfer ? size : radius / std::tan(theta / 2.0);
    std::vector<SketchElementId> tooShort;
    if (reach > firstLength - nullSegment)
        tooShort.push_back(firstId);
    if (reach > secondLength - nullSegment)
        tooShort.push_back(secondId);
    if (!tooShort.empty()) {
        const double largest = std::max(0.0, std::min(firstLength, secondLength) - nullSegment) *
                               (chamfer ? 1.0 : std::tan(theta / 2.0));
        // The reason first: argument order is unspecified, and the list is moved.
        const std::string reason =
            sizeName + millimetres(size) + " ist zu groß; höchstens " +
            millimetres(largest) +
            (tooShort.size() == 2          ? " (beide Linien sind zu kurz)"
             : tooShort.front() == firstId ? " (die erste Linie ist zu kurz)"
                                           : " (die zweite Linie ist zu kurz)");
        refuse(reason, std::move(tooShort));
    }
    const SketchPoint firstTouch{corner.x + u.x * reach, corner.y + u.y * reach};
    const SketchPoint secondTouch{corner.x + v.x * reach, corner.y + v.y * reach};
    const double bisector = std::hypot(u.x + v.x, u.y + v.y);
    const double centerDistance = radius / std::sin(theta / 2.0);
    const SketchPoint center{corner.x + (u.x + v.x) / bisector * centerDistance,
                             corner.y + (u.y + v.y) / bisector * centerDistance};
    double startAngle = std::atan2(firstTouch.y - center.y, firstTouch.x - center.x);
    double endAngle = std::atan2(secondTouch.y - center.y, secondTouch.x - center.x);
    double sweep = std::fmod(endAngle - startAngle + 2.0 * std::numbers::pi, 2.0 * std::numbers::pi);
    bool arcStartOnFirst = true;
    if (sweep > std::numbers::pi) {
        std::swap(startAngle, endAngle);
        sweep = 2.0 * std::numbers::pi - sweep;
        arcStartOnFirst = false;
    }

    const auto shortened = [&](const SketchLine& line, Position kept, SketchPoint touch) {
        SketchLine result = line;
        (other(kept) == Position::start ? result.start : result.end) = touch;
        return result;
    };
    const Position firstMoved = other(firstKept);
    const Position secondMoved = other(secondKept);
    return {{firstId, shortened(first, firstKept, firstTouch), firstMoved,
             distance(point(first, firstMoved), corner) < nullSegment},
            {secondId, shortened(second, secondKept, secondTouch), secondMoved,
             distance(point(second, secondMoved), corner) < nullSegment},
            document::SketchArc{center, radius, startAngle, startAngle + sweep},
            arcStartOnFirst,
            SketchLine{firstTouch, secondTouch},
            corner};
}

} // namespace voelund::application::detail
