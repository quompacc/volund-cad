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
    fillet, // M05
    chamfer, // M07
    hole,    // M08
    combine, // M21
    datumPlane, // M19
    datumAxis,  // M19
    revolve,    // M20
    shell,      // M22
    sphere,     // M27
    cone,
    torus,
    wedge,
    sweep,      // M23
    loft,       // M24
    helix,      // M26
    draft,      // M25
    repair,
    eye,
    eyeOff,
};

[[nodiscard]] QIcon shellIcon(ShellIcon icon, const QColor& ink, const QColor& accent);

} // namespace voelund::desktop
