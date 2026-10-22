// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <voelund/features/SketchRegionIdentity.hpp>
using namespace voelund;
using namespace document;
using namespace features;
namespace {
void require(bool ok, const char* why) {
    if (!ok)
        throw std::runtime_error(why);
}
SketchElement line(std::uint64_t id, double x, double y, double u, double v) {
    return {SketchElementId{id}, SketchLine{{x, y}, {u, v}}};
}
SketchElement circle(std::uint64_t id, double x, double y, double r) {
    return {SketchElementId{id}, SketchCircle{{x, y}, r}};
}
SketchElement rectangle(std::uint64_t id, double x, double y, double u, double v) {
    return {SketchElementId{id}, SketchRectangle{x, y, u, v}};
}
SketchRegionIdentities identify(const std::vector<SketchElement>& elements) {
    std::uint64_t next = 1;
    for (const auto& e : elements)
        next = std::max(next, e.id.value() + 1);
    validateSketchElements({elements, next});
    return identifySketchRegions(elements);
}
std::vector<std::string> texts(const SketchRegionIdentities& identities) {
    std::vector<std::string> result;
    for (const auto& id : identities.ids)
        result.push_back(id.text());
    std::ranges::sort(result);
    return result;
}
SketchRegionReferenceStatus status(const std::vector<SketchElement>& elements,
                                   const SketchRegionId& id) {
    return resolveSketchRegion(identify(elements), elements, id).status;
}
SketchRegionId parsed(std::string_view text) {
    const auto id = SketchRegionId::parse(text);
    require(id.has_value(), "Canonical region text rejected");
    return *id;
}

void canonicalText() {
    const std::vector<SketchElement> ring{rectangle(1, 0, 0, 8, 6), circle(2, 4, 3, 1)};
    require(texts(identify(ring)) == std::vector<std::string>{"o:1.0,1.1,1.2,1.3;h:2.0+", "o:2.0+"},
            "Ring and inner disc signatures");
    for (const auto* text :
         {"o:1.0,1.1,1.2,1.3;h:2.0+", "o:2.0+", "o:3.0;h:1.0+;h:2.0+", "o:18446744073709551615.0-"})
        require(SketchRegionId::parse(text)->text() == text, "Canonical round trip");
    for (const auto* text :
         {"", "x", "o:", "o:1.1,1.0", "o:01.0", "o:1.4", "o:0.0", "o:1.0;h:", "o:1.0;h:2.0+;h:1.0+",
          "o:1.0*", "o:1", "o:1.0,", " o:1.0", "o:18446744073709551616.0"})
        require(!SketchRegionId::parse(text), "Non-canonical region text accepted");
    require(parsed("o:3.0;h:1.0+;h:2.0+").elements() ==
                std::vector{SketchElementId{1}, SketchElementId{2}, SketchElementId{3}},
            "Referenced elements");
    std::cout << "canonical signature text, parser rejection and element extraction passed\n";
}

void stableUnderEdits() {
    const std::vector<SketchElement> base{rectangle(1, 0, 0, 8, 6), circle(2, 4, 3, 1)};
    const auto ring = parsed("o:1.0,1.1,1.2,1.3;h:2.0+");
    auto reordered = base;
    std::ranges::reverse(reordered);
    require(status(reordered, ring) == SketchRegionReferenceStatus::resolved, "Reordering");
    auto resized = base;
    std::get<SketchCircle>(resized[1].geometry).radius = 2.5;
    std::get<SketchRectangle>(resized[0].geometry).second = {10, 7};
    const auto identities = identify(resized);
    const auto resolved = resolveSketchRegion(identities, resized, ring);
    require(resolved.status == SketchRegionReferenceStatus::resolved &&
                std::abs(identities.regions.regions[*resolved.region].area -
                         (70 - 6.25 * std::numbers::pi)) < 1e-7,
            "Pure dimension change keeps the ring");
    auto extended = base;
    extended.push_back(line(3, 0, 0, 8, 6));
    extended.back().construction = true;
    extended.push_back(rectangle(4, 20, 0, 22, 2));
    require(status(extended, ring) == SketchRegionReferenceStatus::resolved,
            "Irrelevant construction or island");
    // Lines carry no direction: drawing a side the other way round keeps the identity.
    std::vector<SketchElement> lines{line(7, 0, 0, 4, 0), line(3, 4, 0, 4, 3), line(9, 4, 3, 0, 3),
                                     line(5, 0, 3, 0, 0)};
    const auto square = identify(lines).ids.at(0);
    std::swap(std::get<SketchLine>(lines[2].geometry).start,
              std::get<SketchLine>(lines[2].geometry).end);
    require(status(lines, square) == SketchRegionReferenceStatus::resolved, "Line direction");
    std::cout << "reordering, dimension change, construction/island and line direction passed\n";
}

void neverSilentlyRedirected() {
    // Two squares; the left one owns the smallest element ID.
    std::vector<SketchElement> e{line(1, 0, 0, 2, 0),  line(2, 2, 0, 2, 2),  line(3, 2, 2, 0, 2),
                                 line(4, 0, 2, 0, 0),  line(11, 5, 0, 7, 0), line(12, 7, 0, 7, 2),
                                 line(13, 7, 2, 5, 2), line(14, 5, 2, 5, 0)};
    const auto left = parsed("o:1.0,2.0,3.0,4.0");
    require(status(e, left) == SketchRegionReferenceStatus::resolved, "Left square");
    e.erase(e.begin()); // Delete the smallest ID and close the gap with a new line.
    e.push_back(line(20, 0, 0, 2, 0));
    require(status(e, left) == SketchRegionReferenceStatus::elementsRemoved,
            "Deleted smallest ID must not select another region");
    e.back().construction = true;
    require(status(e, left) == SketchRegionReferenceStatus::elementsRemoved, "Construction");
    std::vector<SketchElement> disc{circle(1, 0, 0, 2)};
    const auto whole = parsed("o:1.0+");
    require(status(disc, whole) == SketchRegionReferenceStatus::resolved, "Disc");
    disc.push_back(line(2, -2, 0, 2, 0));
    require(status(disc, whole) == SketchRegionReferenceStatus::topologyChanged,
            "Split requires repair");
    disc.push_back(line(3, -1, 0, 1, 0)); // Overlapping the chord invalidates the arrangement.
    require(status(disc, whole) == SketchRegionReferenceStatus::invalidSketch, "Invalid sketch");
    std::cout << "deleted smallest ID, split and invalid sketch never resolve silently passed\n";
}

void cellsOfSameElements() {
    const std::vector<SketchElement> crossing{circle(1, 0, 0, 2), circle(2, 2, 0, 2)};
    const auto identities = identify(crossing);
    require(identities.ids.size() == 3 &&
                texts(identities) ==
                    std::vector<std::string>{"o:1.0+,2.0+", "o:1.0+,2.0-", "o:1.0-,2.0+"},
            "Lens and crescents are distinct");
    for (std::size_t i = 0; i < 3; ++i)
        require(!identities.ambiguous(i), "Crossing circle cell ambiguous");
    const std::vector<SketchElement> chord{circle(1, 0, 0, 2), line(2, -2, 0, 2, 0)};
    const auto halves = identify(chord);
    require(halves.ids.size() == 2 && halves.ids[0] == halves.ids[1] && halves.ambiguous(0),
            "Mirror cells at a chord are reported ambiguous");
    require(resolveSketchRegion(halves, chord, halves.ids[0]).status ==
                SketchRegionReferenceStatus::ambiguous,
            "Ambiguous cell resolved");
    std::cout << "crossing-circle cells distinct, chord halves explicitly ambiguous passed\n";
}

void legacyMigration() {
    const std::vector<SketchElement> e{circle(2, 4, 3, 1), rectangle(1, 0, 0, 8, 6),
                                       circle(5, 4, 3, 0.5), rectangle(9, 20, 0, 22, 2)};
    const auto identities = identify(e);
    const auto names = [](const auto& ids) {
        std::vector<std::string> result;
        for (const auto& id : *ids)
            result.push_back(id.text());
        return result;
    };
    const auto outer = legacyProfileRegions(identities, e, SketchProfileId{1});
    require(outer && names(outer) == std::vector<std::string>{"o:1.0,1.1,1.2,1.3;h:2.0+",
                                                              "o:2.0+;h:5.0+", "o:5.0+"},
            "Legacy rectangle maps to its filled interior");
    const auto inner = legacyProfileRegions(identities, e, SketchProfileId{2});
    require(inner && names(inner) == std::vector<std::string>{"o:2.0+;h:5.0+", "o:5.0+"},
            "Legacy circle maps to its filled interior");
    require(legacyProfileRegions(identities, e, SketchProfileId{9}) &&
                legacyProfileRegions(identities, e, SketchProfileId{9})->size() == 1,
            "Legacy island");
    require(!legacyProfileRegions(identities, e, SketchProfileId{3}), "Unknown legacy profile");
    std::cout << "legacy minimum-ID profiles map to their filled interiors passed\n";
}
} // namespace

int main() {
    try {
        canonicalText();
        stableUnderEdits();
        neverSilentlyRedirected();
        cellsOfSameElements();
        legacyMigration();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
