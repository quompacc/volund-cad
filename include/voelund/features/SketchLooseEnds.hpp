// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <span>
#include <utility>
#include <vector>

namespace voelund::features {

// W19 (as SolidCraft's sketch palette): the ends of open curves (lines, arcs) that no relation
// ties to anything. `open` ends touch nothing; `untied` ends lie on another end by position
// only, in pairs a coincident relation would tie. Construction geometry is left out.
struct SketchLooseEnds final {
    std::vector<document::SketchGeometryReference> open;
    std::vector<std::pair<document::SketchGeometryReference, document::SketchGeometryReference>>
        untied;
};

[[nodiscard]] SketchLooseEnds
sketchLooseEnds(std::span<const document::SketchElement> elements,
                std::span<const document::SketchConstraint> constraints);

// W19: the degrees of freedom the sketch's elements have without any relation (a line 4, a
// circle 3, an arc 5 ...), the whole a progress of determination is measured against.
[[nodiscard]] int sketchFreedomWithoutRelations(std::span<const document::SketchElement> elements);

} // namespace voelund::features
