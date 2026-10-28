// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QColor>
#include <QIcon>

namespace voelund::desktop {

// U03: one line-icon set for the shell (24-unit grid, round caps, 1.8 stroke), drawn in
// the current ink color so it follows the light and dark themes.
enum class ShellIcon {
    brand,
    menu,
    undo,
    redo,
    sun,
    moon,
    search,
    rail,
    box,
    cylinder,
    sketch,
    extrude,
    transform,
    editSketch,
    pushPull,
    fillet,     // M05
    chamfer,    // M07
    hole,       // M08
    combine,    // M21
    datumPlane, // M19
    datumAxis,  // M19
    revolve,    // M20
    shell,      // M22
    sphere,     // M27
    cone,
    torus,
    wedge,
    sweep,         // M23
    loft,          // M24
    helix,         // M26
    draft,         // M25
    mirror,        // M09
    patternLinear, // M10
    patternCircular,
    measure,    // M12
    importFile, // M18: a STEP import in the timeline
    parameters, // PAR02: the parameter table
    repair,
    eye,
    eyeOff,
    split, // N12a
    rib,   // N12b
};

[[nodiscard]] QIcon shellIcon(ShellIcon icon, const QColor& ink, const QColor& accent);
// N05: the section view switch.
[[nodiscard]] QIcon sectionIcon(const QColor& ink, const QColor& accent);

} // namespace voelund::desktop
