// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>

#include <QString>
#include <QStringList>

#include <vector>

// N15c (agent decision 2026-10-29): text in a sketch becomes its outline as sketch curves when
// it is placed (straight pieces as lines, the font's curves as cubic splines over their Bézier
// points), as Fusion's "Explode text" right away; it is not kept as text. Its height is the
// height of capital letters; it starts at `origin` on its baseline.
namespace voelund::desktop {

// The bundled free font (Albert Sans, OFL) first, then the system's sans and serif families.
[[nodiscard]] QStringList sketchTextFamilies();

// The outline of `text`; empty for empty text or a font without outlines.
[[nodiscard]] std::vector<document::SketchGeometry> sketchTextCurves(const QString& text,
                                                                     const QString& family,
                                                                     double height,
                                                                     document::SketchPoint origin);

} // namespace voelund::desktop
