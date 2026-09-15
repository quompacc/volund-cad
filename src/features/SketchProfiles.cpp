#include "SketchConnection.hpp"
#include <voelund/features/SketchProfiles.hpp>

#include <cmath>
#include <optional>

namespace voelund::features {
namespace {

using detail::connected;



struct OrientedElement final {
    document::SketchElementId id;
    document::SketchPoint start;
    document::SketchPoint end;
};

std::optional<std::pair<document::SketchPoint, document::SketchPoint>>
endpoints(const document::SketchGeometry& geometry) {
    if (const auto* line = std::get_if<document::SketchLine>(&geometry))
        return std::pair{line->start, line->end};
    if (const auto* arc = std::get_if<document::SketchArc>(&geometry)) {
        return std::pair{
            document::SketchPoint{arc->center.x + arc->radius * std::cos(arc->startAngle),
                                  arc->center.y + arc->radius * std::sin(arc->startAngle)},
            document::SketchPoint{arc->center.x + arc->radius * std::cos(arc->endAngle),
                                  arc->center.y + arc->radius * std::sin(arc->endAngle)}};
    }
    return std::nullopt;
}

std::optional<OrientedElement> connectingElement(const document::SketchPoint& point,
                                                 std::span<const document::SketchElement> elements,
                                                 const std::vector<bool>& used) {
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (used[index] || elements[index].construction)
            continue;
        const auto points = endpoints(elements[index].geometry);
        if (!points.has_value())
            continue;
        if (connected(point, points->first)) {
            return OrientedElement{elements[index].id, points->first, points->second};
        }
        if (connected(point, points->second)) {
            return OrientedElement{elements[index].id, points->second, points->first};
        }
    }
    return std::nullopt;
}

std::size_t indexOf(std::span<const document::SketchElement> elements,
                    document::SketchElementId id) {
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (elements[index].id == id)
            return index;
    }
    return elements.size();
}

std::optional<SketchProfile> curveProfile(std::size_t firstIndex,
                                          std::span<const document::SketchElement> elements,
                                          std::vector<bool>& used) {
    std::vector<bool> candidateUsed = used;
    const auto first = endpoints(elements[firstIndex].geometry);
    SketchProfile profile{document::SketchProfileId{elements[firstIndex].id.value()},
                          {elements[firstIndex].id}};
    document::SketchPoint current = first->second;
    while (!connected(current, first->first)) {
        const std::optional<OrientedElement> next =
            connectingElement(current, elements, candidateUsed);
        if (!next.has_value())
            return std::nullopt;
        const std::size_t index = indexOf(elements, next->id);
        profile.elementIds.push_back(next->id);
        if (next->id.value() < profile.id.value())
            profile.id = document::SketchProfileId{next->id.value()};
        candidateUsed[index] = true;
        current = next->end;
    }
    if (profile.elementIds.size() < 2)
        return std::nullopt;
    used = std::move(candidateUsed);
    return profile;
}

} // namespace

std::vector<SketchProfile> detectClosedProfiles(std::span<const document::SketchElement> elements) {
    std::vector<SketchProfile> profiles;
    std::vector<bool> used(elements.size(), false);
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (!elements[index].construction &&
            (std::holds_alternative<document::SketchRectangle>(elements[index].geometry) ||
             std::holds_alternative<document::SketchCircle>(elements[index].geometry) ||
             std::holds_alternative<document::SketchEllipse>(elements[index].geometry))) {
            profiles.push_back(SketchProfile{document::SketchProfileId{elements[index].id.value()},
                                             {elements[index].id}});
            used[index] = true;
        }
    }
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (used[index] || elements[index].construction ||
            !endpoints(elements[index].geometry).has_value()) {
            continue;
        }
        used[index] = true;
        if (auto profile = curveProfile(index, elements, used)) {
            profiles.push_back(std::move(*profile));
        } else {
            used[index] = false;
        }
    }
    return profiles;
}

} // namespace voelund::features
