#include <voelund/document/SketchGeometry.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace voelund::document {
namespace {

constexpr double minimumLength = 1.0e-9;

bool finite(const SketchPoint& point) { return std::isfinite(point.x) && std::isfinite(point.y); }

double squaredDistance(const SketchPoint& first, const SketchPoint& second) {
    const double x = second.x - first.x;
    const double y = second.y - first.y;
    return x * x + y * y;
}

void validateGeometry(const SketchGeometry& geometry) {
    if (const auto* line = std::get_if<SketchLine>(&geometry)) {
        if (!finite(line->start) || !finite(line->end) ||
            squaredDistance(line->start, line->end) <= minimumLength * minimumLength) {
            throw std::invalid_argument("Sketch line must have finite, distinct endpoints");
        }
        return;
    }
    if (const auto* rectangle = std::get_if<SketchRectangle>(&geometry)) {
        if (!finite(rectangle->first) || !finite(rectangle->second) ||
            std::abs(rectangle->second.x - rectangle->first.x) <= minimumLength ||
            std::abs(rectangle->second.y - rectangle->first.y) <= minimumLength) {
            throw std::invalid_argument("Sketch rectangle must have a finite positive area");
        }
        return;
    }
    const SketchCircle& circle = std::get<SketchCircle>(geometry);
    if (!finite(circle.center) || !std::isfinite(circle.radius) || circle.radius <= minimumLength) {
        throw std::invalid_argument("Sketch circle must have a finite positive radius");
    }
}

} // namespace

void validateSketchElements(const SketchElements& elements) {
    if (elements.values.empty()) {
        throw std::invalid_argument("Sketch must contain at least one element");
    }
    std::uint64_t maximumId = 0;
    std::vector<std::uint64_t> ids;
    ids.reserve(elements.values.size());
    for (const SketchElement& element : elements.values) {
        if (element.id.value() == 0) {
            throw std::invalid_argument("Sketch element ID must not be zero");
        }
        validateGeometry(element.geometry);
        maximumId = std::max(maximumId, element.id.value());
        ids.push_back(element.id.value());
    }
    std::ranges::sort(ids);
    if (std::ranges::adjacent_find(ids) != ids.end()) {
        throw std::invalid_argument("Sketch element IDs must be unique");
    }
    if (elements.nextId <= maximumId) {
        throw std::invalid_argument("Sketch next element ID must exceed existing IDs");
    }
}

} // namespace voelund::document
