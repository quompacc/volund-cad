#include <voelund/features/SketchProfiles.hpp>

#include <cmath>
#include <optional>

namespace voelund::features {
namespace {

constexpr double connectionTolerance = 1.0e-8;

bool connected(const document::SketchPoint& first, const document::SketchPoint& second) {
    return std::hypot(second.x - first.x, second.y - first.y) <= connectionTolerance;
}

struct OrientedLine final {
    document::SketchElementId id;
    document::SketchPoint start;
    document::SketchPoint end;
};

std::optional<OrientedLine> connectingLine(const document::SketchPoint& point,
                                           std::span<const document::SketchElement> elements,
                                           const std::vector<bool>& used) {
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (used[index])
            continue;
        const auto* line = std::get_if<document::SketchLine>(&elements[index].geometry);
        if (line == nullptr)
            continue;
        if (connected(point, line->start)) {
            return OrientedLine{elements[index].id, line->start, line->end};
        }
        if (connected(point, line->end)) {
            return OrientedLine{elements[index].id, line->end, line->start};
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

std::optional<SketchProfile> lineProfile(std::size_t firstIndex,
                                         std::span<const document::SketchElement> elements,
                                         std::vector<bool>& used) {
    std::vector<bool> candidateUsed = used;
    const auto& first = std::get<document::SketchLine>(elements[firstIndex].geometry);
    SketchProfile profile{document::SketchProfileId{elements[firstIndex].id.value()},
                          {elements[firstIndex].id}};
    document::SketchPoint current = first.end;
    while (!connected(current, first.start)) {
        const std::optional<OrientedLine> next = connectingLine(current, elements, candidateUsed);
        if (!next.has_value())
            return std::nullopt;
        const std::size_t index = indexOf(elements, next->id);
        profile.elementIds.push_back(next->id);
        if (next->id.value() < profile.id.value())
            profile.id = document::SketchProfileId{next->id.value()};
        candidateUsed[index] = true;
        current = next->end;
    }
    if (profile.elementIds.size() < 3)
        return std::nullopt;
    used = std::move(candidateUsed);
    return profile;
}

} // namespace

std::vector<SketchProfile> detectClosedProfiles(std::span<const document::SketchElement> elements) {
    std::vector<SketchProfile> profiles;
    std::vector<bool> used(elements.size(), false);
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (std::holds_alternative<document::SketchRectangle>(elements[index].geometry) ||
            std::holds_alternative<document::SketchCircle>(elements[index].geometry)) {
            profiles.push_back(SketchProfile{document::SketchProfileId{elements[index].id.value()},
                                             {elements[index].id}});
            used[index] = true;
        }
    }
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (used[index] ||
            !std::holds_alternative<document::SketchLine>(elements[index].geometry)) {
            continue;
        }
        used[index] = true;
        if (auto profile = lineProfile(index, elements, used)) {
            profiles.push_back(std::move(*profile));
        } else {
            used[index] = false;
        }
    }
    return profiles;
}

} // namespace voelund::features
