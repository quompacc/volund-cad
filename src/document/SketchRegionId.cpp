// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/document/SketchRegionId.hpp>

#include <algorithm>
#include <charconv>
#include <stdexcept>

namespace voelund::document {
namespace {

// Bounds untrusted text before it is parsed; real signatures are far shorter.
constexpr std::size_t maximumTextLength = 65536;

std::string boundaryText(std::vector<SketchRegionBoundaryUse> uses) {
    if (uses.empty())
        throw std::invalid_argument("A region boundary needs at least one piece");
    // A set, not a multiset: a circle seam or a moving cut must not change identity.
    std::ranges::sort(uses);
    const auto duplicates = std::ranges::unique(uses);
    uses.erase(duplicates.begin(), duplicates.end());
    std::string text;
    for (const SketchRegionBoundaryUse& use : uses) {
        if (use.element.value() == 0 || use.part > 3)
            throw std::invalid_argument("Invalid region boundary reference");
        if (!text.empty())
            text += ',';
        text += std::to_string(use.element.value()) + '.' + std::to_string(use.part);
        if (use.reversed)
            text += *use.reversed ? '-' : '+';
    }
    return text;
}

std::optional<SketchRegionBoundaryUse> parseUse(std::string_view text) {
    const auto dot = text.find('.');
    if (dot == std::string_view::npos || dot + 2 > text.size())
        return std::nullopt;
    std::uint64_t element = 0;
    const auto digits = text.substr(0, dot);
    const auto [end, error] =
        std::from_chars(digits.data(), digits.data() + digits.size(), element);
    if (error != std::errc{} || end != digits.data() + digits.size())
        return std::nullopt;
    const char part = text[dot + 1];
    if (part < '0' || part > '3')
        return std::nullopt;
    std::optional<bool> reversed;
    const auto rest = text.substr(dot + 2);
    if (rest == "+")
        reversed = false;
    else if (rest == "-")
        reversed = true;
    else if (!rest.empty())
        return std::nullopt;
    return SketchRegionBoundaryUse{SketchElementId{element}, static_cast<unsigned>(part - '0'),
                                   reversed};
}

std::optional<std::vector<SketchRegionBoundaryUse>> parseBoundary(std::string_view text) {
    std::vector<SketchRegionBoundaryUse> uses;
    while (true) {
        const auto comma = text.find(',');
        const auto use = parseUse(text.substr(0, comma));
        if (!use)
            return std::nullopt;
        uses.push_back(*use);
        if (comma == std::string_view::npos)
            return uses;
        text.remove_prefix(comma + 1);
    }
}

// Splits "o:<outer>;h:<hole>..." into boundaries; the outer boundary comes first.
std::optional<std::vector<std::vector<SketchRegionBoundaryUse>>> parseText(std::string_view text) {
    if (text.size() > maximumTextLength || !text.starts_with("o:"))
        return std::nullopt;
    std::vector<std::vector<SketchRegionBoundaryUse>> boundaries;
    for (std::string_view rest = text.substr(2);;) {
        const auto separator = rest.find(";h:");
        auto boundary = parseBoundary(rest.substr(0, separator));
        if (!boundary)
            return std::nullopt;
        boundaries.push_back(std::move(*boundary));
        if (separator == std::string_view::npos)
            return boundaries;
        rest.remove_prefix(separator + 3);
    }
}

} // namespace

SketchRegionId
SketchRegionId::fromBoundaries(std::vector<SketchRegionBoundaryUse> outer,
                               std::vector<std::vector<SketchRegionBoundaryUse>> holes) {
    std::vector<std::string> holeTexts;
    for (auto& hole : holes)
        holeTexts.push_back(boundaryText(std::move(hole)));
    std::ranges::sort(holeTexts);
    std::string text = "o:" + boundaryText(std::move(outer));
    for (const std::string& hole : holeTexts)
        text += ";h:" + hole;
    return SketchRegionId{std::move(text)};
}

std::optional<SketchRegionId> SketchRegionId::parse(std::string_view text) {
    auto boundaries = parseText(text);
    if (!boundaries)
        return std::nullopt;
    auto outer = std::move(boundaries->front());
    boundaries->erase(boundaries->begin());
    try {
        auto id = fromBoundaries(std::move(outer), std::move(*boundaries));
        if (id.text_ != text)
            return std::nullopt; // Only the canonical spelling identifies a region.
        return id;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

std::vector<SketchElementId> SketchRegionId::elements() const {
    std::vector<std::uint64_t> values;
    const auto boundaries = parseText(text_); // Canonical by construction.
    for (const auto& boundary : *boundaries)
        for (const SketchRegionBoundaryUse& use : boundary)
            values.push_back(use.element.value());
    std::ranges::sort(values);
    const auto duplicates = std::ranges::unique(values);
    values.erase(duplicates.begin(), duplicates.end());
    std::vector<SketchElementId> result;
    for (std::uint64_t value : values)
        result.emplace_back(value);
    return result;
}

} // namespace voelund::document
