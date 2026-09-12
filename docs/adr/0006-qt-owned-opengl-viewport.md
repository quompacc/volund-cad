# ADR 0006: Qt-owned OpenGL viewport on all desktop targets

**Status:** accepted  
**Date:** 12 September 2026  
**Owner:** Völund project

## Context

The first Windows viewport used a native OCCT child window embedded in the Qt shell.
This path renders the CAD scene correctly, but Win32 native-child window stacking
prevents sibling Qt widgets from being alpha-composited over the scene. The camera
cube therefore acquired an opaque black or solid-colored rectangular backing even
when the widget itself painted transparently.

Völund's Fusion-oriented interface requires contextual tool panels, camera controls,
selection hints and later sketch dimensions to appear as lightweight overlays over
the actual CAD scene. Simulating transparency with a sampled background color would
break as soon as the gradient, grid or model moves.

## Decision

Völund uses the Qt-owned OpenGL context backend as the default desktop viewport on
Windows as well as FreeBSD/Wayland. OCCT continues to own AIS/V3d scene rendering,
selection and camera behavior; Qt owns the composited surface and combines it with
the interface overlays.

The native Win32 adapter remains in the source tree as a diagnostic and fallback
implementation. It is not the default for the redesigned product shell.

Windows deployment of every executable using this backend must include both
`Qt6OpenGL` and `Qt6OpenGLWidgets` in addition to the normal Qt runtime.

## Consequences

- Transparent overlays show the live grid and model through unused widget regions.
- Windows and FreeBSD exercise the same viewport composition architecture.
- Overlay styling no longer depends on matching the current viewport background.
- Qt context lifetime, framebuffer binding and buffer swapping remain explicit
  responsibilities of the Völund OCCT adapter.
- Both desktop startup tests and the full regression suite must pass after changes to
  the shared-context path.

## Reconsideration criterion

Reconsider this decision only if the native OCCT window can participate in reliable
per-pixel Qt composition on all supported Windows versions without a separate
top-level overlay window or if the Qt-owned context proves a measurable rendering or
stability regression that cannot be corrected in the adapter.
