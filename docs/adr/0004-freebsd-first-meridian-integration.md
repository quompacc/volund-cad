# ADR 0004: FreeBSD-first Meridian desktop integration

**Status:** accepted; Entwicklungsreihenfolge und frühe Meridian-Sperre durch [ADR 0007](0007-sway-development-meridian-final.md) ersetzt
**Date:** 10 September 2026
**Owner:** Völund project

## Context

Völund is intended to be the native parametric CAD application for a desktop
environment built around Meridian. Meridian targets FreeBSD as its product operating
system. Earlier OpenBSD work proved compositor and platform concepts but exposed
desktop limitations and is not the product direction.

Völund must feel coherent inside Meridian without becoming part of the compositor,
shell, or Meridian's WebKit-based system UI. CAD rendering and interaction also need
an accelerated native Wayland path; an XWayland-only result is insufficient.

## Considered options

1. Embed Völund into Meridian or expose CAD behavior through Meridian-private IPC.
   This creates lifecycle, language, security, and release coupling between an
   application and the desktop.
2. Keep Völund desktop-agnostic and accept only generic Qt defaults. This preserves
   portability but leaves packaging, portals, window identity, and visual integration
   unspecified.
3. Keep Völund an independent Qt Wayland client and integrate through freedesktop
   contracts, with Meridian as the primary acceptance environment.

## Decision

Völund chooses option 3.

- FreeBSD with Meridian is the primary desktop product and acceptance target.
- Völund remains a separate C++20/Qt 6/OCCT process and ordinary Wayland client.
- Qt owns the Wayland surface and EGL/OpenGL context. OCCT renders through the
  existing external-context adapter.
- Window chrome is negotiated through `xdg-decoration`; Völund does not implement a
  custom title bar merely to resemble Meridian.
- Theme preference and native dialogs are consumed through Qt's platform integration
  and XDG portals. Völund does not copy Meridian's private design-token tables.
- Application identity uses the stable desktop ID `io.github.quompacc.Voelund`.
- The `.volund` format is registered as `application/x-voelund`.
- Private Meridian Rust crates and compositor/shell IPC are not Völund dependencies.
- XWayland is a diagnostic or compatibility path and cannot satisfy the native
  FreeBSD/Meridian viewport gate.
- Windows remains a development and regression platform. Linux/Wayland is a
  compatibility target. OpenBSD evidence is retained as historical platform research.

## Validation strategy

The same FreeBSD native-Wayland smoke test runs in two environments:

1. a mature Wayland desktop as a control baseline;
2. Meridian as the authoritative product acceptance environment.

This separates Qt/OCCT/driver failures from compositor integration failures. The gate
requires rendering, picking, resize, scale, maximize, theme, portal file access,
clipboard, suspend/resume, and clean context recreation without `DISPLAY`.

## Consequences

- `src/platform` owns operating-system and desktop-boundary adapters; CAD core layers
  remain unaware of Meridian.
- FreeBSD build and package metadata become product artifacts rather than future work.
- The stock FreeBSD OCCT package cannot satisfy the native gate because its GLES
  renderer is compiled with `HAVE_XLIB`. Völund therefore owns a reproducible,
  project-local OCCT profile with EGL/GLES enabled and Xlib, TBB, VTK, Qt 5, Tcl/Tk,
  data exchange, samples, and documentation disabled.
- FreeBSD's Qt 6 package remains a combined Wayland/X11 build. Völund forces and
  verifies the Wayland QPA, while a future distribution package may provide a
  Wayland-only Qt flavor if dormant ELF linkage must also be removed.
- Large CAD feature expansion pauses until the initial FreeBSD/Meridian viewport gate
  is measured on real hardware.
- Meridian may improve its standard portal or toolkit exports for every application;
  Völund-specific compositor policy is not the default solution.
