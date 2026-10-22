// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <cstdint>
#include <optional>
#include <vector>

// S12: an associative sketch pattern. Its instances are ordinary elements whose geometry is
// always derived from the sources by the pattern rule; they follow every change of a source
// (and of a mirror axis). Instances carry no relations and are no source or axis of any
// pattern, so patterns cannot depend on each other. Dissolving a pattern leaves its instances
// as independent elements. Persisted from native schema 11.
namespace voelund::document {

enum class SketchPatternKind { linear = 1, circular = 2, mirror = 3 };

// Positions of a linear or circular pattern, the source's position included.
inline constexpr std::uint32_t minimumSketchPatternCount = 2;
inline constexpr std::uint32_t maximumSketchPatternCount = 1000;

struct SketchPattern final {
    std::uint64_t id;
    SketchPatternKind kind;
    std::vector<SketchElementId> sources;
    // Linear and circular: positions including the source's; mirror: always 2.
    std::uint32_t count{2};
    SketchPoint step{};   // linear: offset from one position to the next
    SketchPoint center{}; // circular: rotation center
    double angle{0.0};    // circular: radians between neighbouring positions, |angle| < 2 pi
    std::optional<SketchElementId> axis{}; // mirror: the axis line
    // By position (2nd, 3rd, ...), then in source order: (count - 1) x sources.
    std::vector<SketchElementId> instances{};

    [[nodiscard]] bool operator==(const SketchPattern&) const = default;
};

struct SketchPatterns final {
    std::vector<SketchPattern> values;
    std::uint64_t nextId{1};

    [[nodiscard]] bool operator==(const SketchPatterns&) const = default;
};

// Throws std::invalid_argument when a pattern is not consistent with the elements and
// relations: unknown or duplicate IDs, counts outside the limits, a legacy rectangle as source,
// an instance as source or axis (a dependency between patterns), relations on instances.
void validateSketchPatterns(const SketchElements& elements, const SketchConstraints& constraints,
                            const SketchPatterns& patterns);

// The geometry the pattern rule gives its instances from the current sources, in instance
// order. Construction flags follow the sources.
[[nodiscard]] std::vector<SketchElement> sketchPatternInstances(const SketchElements& elements,
                                                                const SketchPattern& pattern);

// Sets every instance to the geometry its pattern derives from the current sources.
void regenerateSketchPatterns(SketchElements& elements, const SketchPatterns& patterns);

[[nodiscard]] bool isSketchPatternInstance(const SketchPatterns& patterns,
                                           SketchElementId element) noexcept;

} // namespace voelund::document
