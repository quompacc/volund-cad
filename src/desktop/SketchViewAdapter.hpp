// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/features/SketchEvaluation.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QString>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <vector>

namespace voelund::application {
class CadSession;
struct SketchSupportProjection;
} // namespace voelund::application

namespace voelund::desktop {
void setSupportProjection(viewport::PlanarFaceSelection& support,
                          const application::SketchSupportProjection& projection);

[[nodiscard]] viewport::SceneSketchElement
makeSceneSketchElement(const features::EvaluatedSketchElement& element, bool conflicting = false,
                       bool determined = false);
[[nodiscard]] std::vector<viewport::SceneSketchElement>
makeSceneSketchElements(const application::CadSession& session);
// The curves of a sketch without its construction geometry, as one shape (shown sketches).
[[nodiscard]] TopoDS_Shape sketchCurves(const document::SketchParameters& sketch);
[[nodiscard]] QString sketchSelectionSummary(const application::CadSession& session);

// Where a view ray crosses a sketch's plane: the distance along the ray and the point; none
// when the ray runs along the plane.
struct SketchPlaneCrossing final {
    double distance;
    std::array<double, 3> point;
};
[[nodiscard]] std::optional<SketchPlaneCrossing>
sketchPlaneCrossing(const application::CadSession& session, document::FeatureId sketch,
                    const viewport::ViewRay& ray);
// A click into the area of a visible sketch region picks it (user report 2026-10-05); a
// sketch behind the clicked face stays hidden (S14).
[[nodiscard]] bool pickSketchArea(application::CadSession& session, const viewport::ViewRay& ray,
                                  const std::set<std::uint64_t>& hiddenBodies);

} // namespace voelund::desktop
