// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "FeatureEvaluationDetail.hpp"

#include <voelund/document/DocumentSnapshot.hpp>

#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

#include <span>
#include <vector>

// N11b: patterns and mirrors of steps ("Identisch" in Fusion). A hole or a join or cut
// extrusion is repeated by its effect on its body, the material it cut away or joined, moved
// to each copy; the copies' new faces stay unnamed.
namespace voelund::features::detail {

struct StepEffect final {
    TopoDS_Shape removed;
    TopoDS_Shape added;
};

// A body's shape with its named faces and edges.
struct NamedBody final {
    TopoDS_Shape shape;
    std::vector<SupportProvenance> faces;
    std::vector<EdgeProvenance> edges;
};

// What `steps` did to `body`, from the current (or the unplaced reference) results.
[[nodiscard]] std::vector<StepEffect>
stepEffects(const document::DocumentSnapshot& document, std::span<const document::FeatureId> steps,
            document::BodyId body, const std::vector<EvaluatedFeature>& outputs, bool reference);

// `start` with every effect moved by every transform: all joined material first, then all
// removed material, each in one Boolean.
[[nodiscard]] NamedBody repeatEffects(const NamedBody& start, std::span<const StepEffect> effects,
                                      std::span<const gp_Trsf> transforms);

} // namespace voelund::features::detail
