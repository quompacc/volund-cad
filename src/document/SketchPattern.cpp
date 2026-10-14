#include <voelund/document/SketchPattern.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>

namespace voelund::document {
namespace {

[[noreturn]] void invalid(const char* reason) {
    throw std::invalid_argument(std::string("Sketch pattern: ") + reason);
}

const SketchElement* find(const SketchElements& elements, SketchElementId id) {
    const auto match = std::ranges::find(elements.values, id, &SketchElement::id);
    return match == elements.values.end() ? nullptr : &*match;
}

// A rigid motion of the sketch plane: x' = (a x + b y + tx, c x + d y + ty).
struct Motion final {
    double a{1}, b{0}, c{0}, d{1}, tx{0}, ty{0};
    bool reflects{false};
    double turn{0}; // rotation angle, or twice the axis angle for a reflection

    [[nodiscard]] SketchPoint apply(SketchPoint p) const {
        return {a * p.x + b * p.y + tx, c * p.x + d * p.y + ty};
    }
    static Motion translation(SketchPoint offset) { return {1, 0, 0, 1, offset.x, offset.y}; }
    static Motion rotation(SketchPoint center, double angle) {
        Motion m{std::cos(angle), -std::sin(angle), std::sin(angle), std::cos(angle), 0, 0};
        m.tx = center.x - (m.a * center.x + m.b * center.y);
        m.ty = center.y - (m.c * center.x + m.d * center.y);
        m.turn = angle;
        return m;
    }
    static Motion reflection(const SketchLine& axis) {
        const double phi = std::atan2(axis.end.y - axis.start.y, axis.end.x - axis.start.x);
        Motion m{std::cos(2 * phi), std::sin(2 * phi), std::sin(2 * phi), -std::cos(2 * phi), 0, 0};
        m.tx = axis.start.x - (m.a * axis.start.x + m.b * axis.start.y);
        m.ty = axis.start.y - (m.c * axis.start.x + m.d * axis.start.y);
        m.reflects = true;
        m.turn = 2 * phi;
        return m;
    }
};

SketchGeometry moved(const SketchGeometry& geometry, const Motion& motion, bool translationOnly) {
    if (const auto* line = std::get_if<SketchLine>(&geometry))
        return SketchLine{motion.apply(line->start), motion.apply(line->end),
                          translationOnly ? line->constraint : SketchLineConstraint::none};
    if (const auto* circle = std::get_if<SketchCircle>(&geometry))
        return SketchCircle{motion.apply(circle->center), circle->radius};
    if (const auto* arc = std::get_if<SketchArc>(&geometry)) {
        const SketchPoint center = motion.apply(arc->center);
        const double sweep = arc->endAngle - arc->startAngle;
        if (!motion.reflects)
            return SketchArc{center, arc->radius, arc->startAngle + motion.turn,
                             arc->startAngle + motion.turn + sweep};
        // A reflected arc runs the other way: it starts at the image of the old end.
        const double start = motion.turn - arc->endAngle;
        return SketchArc{center, arc->radius, start, start + sweep};
    }
    if (const auto* point = std::get_if<SketchPointGeometry>(&geometry))
        return SketchPointGeometry{motion.apply(point->position)};
    const auto& ellipse = std::get<SketchEllipse>(geometry);
    return SketchEllipse{motion.apply(ellipse.center), ellipse.majorRadius, ellipse.minorRadius,
                         motion.reflects ? motion.turn - ellipse.rotation
                                         : ellipse.rotation + motion.turn};
}

} // namespace

void validateSketchPatterns(const SketchElements& elements, const SketchConstraints& constraints,
                            const SketchPatterns& patterns) {
    std::set<std::uint64_t> ids;
    std::set<std::uint64_t> sources, instances, axes;
    std::uint64_t maximumId = 0;
    for (const SketchPattern& pattern : patterns.values) {
        if (pattern.id == 0 || !ids.insert(pattern.id).second)
            invalid("pattern IDs must be unique and non-zero");
        maximumId = std::max(maximumId, pattern.id);
        const bool mirror = pattern.kind == SketchPatternKind::mirror;
        if (pattern.kind != SketchPatternKind::linear &&
            pattern.kind != SketchPatternKind::circular && !mirror)
            invalid("unknown kind");
        if (mirror ? pattern.count != 2
                   : pattern.count < minimumSketchPatternCount ||
                         pattern.count > maximumSketchPatternCount)
            invalid("count outside the limits");
        if (pattern.sources.empty() ||
            pattern.instances.size() != (pattern.count - 1) * pattern.sources.size())
            invalid("instances do not match sources and count");
        std::set<std::uint64_t> own; // a source may serve several patterns, once in each
        for (const SketchElementId source : pattern.sources) {
            const auto* element = find(elements, source);
            if (element == nullptr || !own.insert(source.value()).second)
                invalid("sources must be distinct existing elements");
            sources.insert(source.value());
            if (std::holds_alternative<SketchRectangle>(element->geometry))
                invalid("a legacy rectangle cannot be a source");
        }
        for (const SketchElementId instance : pattern.instances)
            if (find(elements, instance) == nullptr || !instances.insert(instance.value()).second)
                invalid("instances must be distinct existing elements of one pattern");
        if (mirror != pattern.axis.has_value())
            invalid("exactly a mirror has an axis");
        if (pattern.axis) {
            const auto* axis = find(elements, *pattern.axis);
            if (axis == nullptr || !std::holds_alternative<SketchLine>(axis->geometry) ||
                std::ranges::find(pattern.sources, *pattern.axis) != pattern.sources.end())
                invalid("the mirror axis must be another line");
            axes.insert(pattern.axis->value());
        }
        const auto finite = [](SketchPoint p) { return std::isfinite(p.x) && std::isfinite(p.y); };
        if (!finite(pattern.step) || !finite(pattern.center) || !std::isfinite(pattern.angle))
            invalid("values must be finite");
        if (pattern.kind == SketchPatternKind::linear &&
            std::hypot(pattern.step.x, pattern.step.y) <= 1.0e-9)
            invalid("a linear pattern needs a step");
        if (pattern.kind == SketchPatternKind::circular &&
            (std::abs(pattern.angle) <= 1.0e-9 || std::abs(pattern.angle) >= 2 * std::numbers::pi))
            invalid("a circular pattern needs an angle below a full turn");
    }
    // No pattern may build on another one's instances: no dependency, so no cycle.
    for (const std::uint64_t instance : instances)
        if (sources.contains(instance) || axes.contains(instance))
            invalid("an instance cannot be a source or an axis (dependent patterns)");
    if (!patterns.values.empty() && patterns.nextId <= maximumId)
        invalid("the next pattern ID must exceed existing IDs");
    for (const SketchConstraint& constraint : constraints.values) {
        const auto onInstance = [&](const std::optional<SketchGeometryReference>& reference) {
            return reference && instances.contains(reference->element.value());
        };
        if (instances.contains(constraint.first.element.value()) || onInstance(constraint.second) ||
            onInstance(constraint.third))
            invalid("instances carry no relations");
    }
}

std::vector<SketchElement> sketchPatternInstances(const SketchElements& elements,
                                                  const SketchPattern& pattern) {
    std::vector<SketchElement> result;
    result.reserve(pattern.instances.size());
    std::size_t next = 0;
    for (std::uint32_t position = 1; position < pattern.count; ++position) {
        Motion motion;
        if (pattern.kind == SketchPatternKind::linear)
            motion = Motion::translation({pattern.step.x * position, pattern.step.y * position});
        else if (pattern.kind == SketchPatternKind::circular)
            motion = Motion::rotation(pattern.center, pattern.angle * position);
        else
            motion = Motion::reflection(std::get<SketchLine>(find(elements, *pattern.axis)->geometry));
        for (const SketchElementId source : pattern.sources) {
            const auto* element = find(elements, source);
            result.push_back({pattern.instances[next++],
                              moved(element->geometry, motion,
                                    pattern.kind == SketchPatternKind::linear),
                              element->construction});
        }
    }
    return result;
}

void regenerateSketchPatterns(SketchElements& elements, const SketchPatterns& patterns) {
    for (const SketchPattern& pattern : patterns.values)
        for (SketchElement& instance : sketchPatternInstances(elements, pattern)) {
            auto match = std::ranges::find(elements.values, instance.id, &SketchElement::id);
            match->geometry = std::move(instance.geometry);
            match->construction = instance.construction;
        }
}

bool isSketchPatternInstance(const SketchPatterns& patterns, SketchElementId element) noexcept {
    return std::ranges::any_of(patterns.values, [element](const SketchPattern& pattern) {
        return std::ranges::find(pattern.instances, element) != pattern.instances.end();
    });
}

} // namespace voelund::document
