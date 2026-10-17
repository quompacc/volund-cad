#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/IdCapacity.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

// S12: associative patterns in the editor. Creating, changing and dissolving a pattern are
// single steps; the instances are derived from the sources by the solver, so they follow every
// change of a source. Instances are read-only: they are moved, deleted or changed through
// their pattern only.
namespace voelund::application {
namespace {

using document::SketchElementId;
using document::SketchPattern;
using document::SketchPatternKind;

[[noreturn]] void refuse(const std::string& reason, std::vector<SketchElementId> elements = {}) {
    throw SketchEditRefusal("Muster: " + reason, std::move(elements));
}

const char* kindName(SketchPatternKind kind) {
    return kind == SketchPatternKind::linear     ? "lineares Muster"
           : kind == SketchPatternKind::circular ? "Kreismuster"
                                                 : "Spiegelung";
}

// The rule values of a pattern from the panel values; refuses what no pattern can hold.
void applyRule(SketchPattern& pattern, int count, double spacing, double totalAngleDegrees) {
    if (pattern.kind == SketchPatternKind::mirror)
        return;
    if (count < static_cast<int>(document::minimumSketchPatternCount) ||
        count > static_cast<int>(document::maximumSketchPatternCount))
        refuse("Die Anzahl liegt zwischen 2 und 1000");
    pattern.count = static_cast<std::uint32_t>(count);
    if (pattern.kind == SketchPatternKind::linear) {
        if (!std::isfinite(spacing) || std::abs(spacing) <= 1.0e-9)
            refuse("Der Abstand darf nicht 0 sein");
        // Keeps a changed pattern's direction; a new one runs along +x.
        const double length = std::hypot(pattern.step.x, pattern.step.y);
        pattern.step = length > 0.0 ? document::SketchPoint{pattern.step.x / length * spacing,
                                                            pattern.step.y / length * spacing}
                                    : document::SketchPoint{spacing, 0.0};
        return;
    }
    const double step = kernel::toRadians(kernel::Degrees(totalAngleDegrees)).value() / count;
    if (!std::isfinite(step) || std::abs(step) <= 1.0e-9 || std::abs(step) >= 2 * std::numbers::pi)
        refuse("Der Winkel je Schritt liegt zwischen 0° und 360°");
    pattern.angle = step;
}

} // namespace

std::vector<SketchElementId> SketchEditorSession::selectedElementIds() const {
    std::vector<SketchElementId> result;
    for (const auto& reference : selectedReferences_)
        if (std::ranges::find(result, reference.element) == result.end())
            result.push_back(reference.element);
    if (result.empty() && selectedElement_.has_value())
        result.push_back(*selectedElement_);
    return result;
}

std::span<const SketchPattern> SketchEditorSession::patterns() const noexcept {
    return patterns_.values;
}

bool SketchEditorSession::isPatternInstance(SketchElementId element) const noexcept {
    return document::isSketchPatternInstance(patterns_, element);
}

std::optional<std::string> SketchEditorSession::patternLock(bool deleting) const {
    for (const SketchElementId id : selectedElementIds()) {
        if (isPatternInstance(id))
            return "Muster: Eine Kopie folgt ihrem Original; das Original oder das Muster ändern "
                   "oder das Muster auflösen";
        if (deleting)
            for (const auto& pattern : patterns_.values)
                if (std::ranges::find(pattern.sources, id) != pattern.sources.end() ||
                    pattern.axis == id)
                    return "Muster: Das Element trägt ein Muster; zuerst das Muster auflösen";
    }
    return std::nullopt;
}

std::optional<SketchPattern> SketchEditorSession::selectedPattern() const {
    if (!selectedElement_.has_value())
        return std::nullopt;
    for (const auto& pattern : patterns_.values)
        if (std::ranges::find(pattern.instances, *selectedElement_) != pattern.instances.end() ||
            std::ranges::find(pattern.sources, *selectedElement_) != pattern.sources.end())
            return pattern;
    return std::nullopt;
}

std::optional<std::uint64_t> SketchEditorSession::createPattern(SketchPatternKind kind, int count,
                                                                double spacing,
                                                                double totalAngleDegrees) {
    if (!active_)
        refuse("Der Skizzeneditor ist nicht aktiv");
    auto ids = selectedElementIds();
    const bool mirror = kind == SketchPatternKind::mirror;
    if (ids.size() < (mirror ? 2U : 1U))
        refuse(mirror ? "Elemente und zuletzt die Spiegelachse wählen" : "Elemente wählen");
    std::optional<SketchElementId> axis;
    if (mirror) {
        axis = ids.back();
        ids.pop_back();
        const auto match = std::ranges::find(elements_.values, *axis, &document::SketchElement::id);
        if (match == elements_.values.end() ||
            !std::holds_alternative<document::SketchLine>(match->geometry))
            refuse("Die zuletzt gewählte Achse muss eine Linie sein", {*axis});
    }
    std::vector<SketchElementId> copies;
    bool legacyRectangle = false;
    for (const SketchElementId id : ids) {
        if (isPatternInstance(id))
            copies.push_back(id);
        const auto match = std::ranges::find(elements_.values, id, &document::SketchElement::id);
        legacyRectangle =
            legacyRectangle || (match != elements_.values.end() &&
                                std::holds_alternative<document::SketchRectangle>(match->geometry));
    }
    if (axis && isPatternInstance(*axis))
        copies.push_back(*axis);
    if (!copies.empty())
        refuse("Eine Kopie kann kein Original und keine Achse sein; zuerst ihr Muster auflösen",
               copies);
    if (legacyRectangle) {
        // A legacy rectangle cannot be a pattern source: copied once, said so by nullopt.
        const bool copied = mirror ? copyMirrored()
                            : kind == SketchPatternKind::linear
                                ? copyLinear(count, spacing)
                                : copyCircular(count, totalAngleDegrees);
        if (!copied)
            refuse("Die Kopie des Alt-Rechtecks ist nicht möglich");
        return std::nullopt;
    }
    SketchPattern pattern{patterns_.nextId, kind, ids, 2, {}, {}, 0.0, axis, {}};
    applyRule(pattern, count, spacing, totalAngleDegrees);
    if (!document::hasRepeatedIdCapacity(elements_.nextId, ids.size(), pattern.count - 1) ||
        !document::hasIdCapacity(patterns_.nextId, 1))
        refuse("Die Skizze hat keine freien IDs mehr");
    Rollback rollback(*this);
    clearPreview();
    for (std::uint32_t position = 1; position < pattern.count; ++position)
        for (const SketchElementId source : ids) {
            const auto match =
                std::ranges::find(elements_.values, source, &document::SketchElement::id);
            pattern.instances.emplace_back(elements_.nextId++);
            elements_.values.push_back(
                {pattern.instances.back(), match->geometry, match->construction});
        }
    ++patterns_.nextId;
    patterns_.values.push_back(pattern);
    document::regenerateSketchPatterns(elements_, patterns_);
    if (!keepIfValid(rollback))
        refuse(std::string("Das ") + kindName(kind) + " lässt sich nicht lösen", ids);
    return pattern.id;
}

bool SketchEditorSession::editPattern(std::uint64_t id, int count, double spacing,
                                      double totalAngleDegrees) {
    const auto match = std::ranges::find(patterns_.values, id, &SketchPattern::id);
    if (!active_ || match == patterns_.values.end())
        refuse("Das Muster gibt es nicht mehr");
    SketchPattern changed = *match;
    applyRule(changed, count, spacing, totalAngleDegrees);
    if (changed == *match)
        return false;
    const std::size_t wanted = (changed.count - 1) * changed.sources.size();
    if (wanted > changed.instances.size() &&
        !document::hasIdCapacity(elements_.nextId, wanted - changed.instances.size()))
        refuse("Die Skizze hat keine freien IDs mehr");
    Rollback rollback(*this);
    // Positions are added or removed at the end; instances carry no relations.
    while (changed.instances.size() > wanted) {
        const SketchElementId removed = changed.instances.back();
        changed.instances.pop_back();
        std::erase_if(elements_.values,
                      [removed](const auto& element) { return element.id == removed; });
        if (selectedElement_ == removed)
            selectedElement_ = changed.sources.front();
    }
    while (changed.instances.size() < wanted) {
        const SketchElementId source =
            changed.sources[changed.instances.size() % changed.sources.size()];
        const auto from = std::ranges::find(elements_.values, source, &document::SketchElement::id);
        changed.instances.emplace_back(elements_.nextId++);
        elements_.values.push_back({changed.instances.back(), from->geometry, from->construction});
    }
    std::erase_if(selectedReferences_, [&](const auto& reference) {
        return std::ranges::find(elements_.values, reference.element,
                                 &document::SketchElement::id) == elements_.values.end();
    });
    *std::ranges::find(patterns_.values, id, &SketchPattern::id) = changed;
    document::regenerateSketchPatterns(elements_, patterns_);
    if (!keepIfValid(rollback))
        refuse(std::string("Das geänderte ") + kindName(changed.kind) + " lässt sich nicht lösen",
               changed.sources);
    return true;
}

bool SketchEditorSession::dissolvePattern(std::uint64_t id) {
    if (!active_ ||
        std::ranges::find(patterns_.values, id, &SketchPattern::id) == patterns_.values.end())
        return false;
    Rollback rollback(*this);
    std::erase_if(patterns_.values,
                  [id](const SketchPattern& pattern) { return pattern.id == id; });
    return keepIfValid(rollback);
}

bool SketchEditorSession::mirrorSelected() {
    try {
        static_cast<void>(createPattern(SketchPatternKind::mirror, 2, 0.0, 0.0));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

bool SketchEditorSession::linearPatternSelected(int count, double spacing) {
    try {
        static_cast<void>(createPattern(SketchPatternKind::linear, count, spacing, 0.0));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

bool SketchEditorSession::circularPatternSelected(int count, double angleDegrees) {
    try {
        static_cast<void>(createPattern(SketchPatternKind::circular, count, 0.0, angleDegrees));
        return true;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

} // namespace voelund::application
