# ADR 0001: Wayland-first Linux viewport

**Status:** superseded in platform priority by ADR 0004
**Date:** 10 September 2026
**Owner:** Völund project

## Context

OCCT 8.0.1 provides native window wrappers for Win32 and Xlib, but no released native
Wayland wrapper. Its traditional Linux viewer path creates a GLX context for an X11
window. Selecting `Xw_Window` merely because the build runs on Unix would therefore
make X11 or XWayland an accidental architectural dependency.

Völund intends to be a long-lived Linux desktop application. Wayland is consequently
the product target, while the only currently available validation host is Windows.

This original platform priority is no longer current. ADR 0004 makes FreeBSD with
Meridian the primary desktop target. The Qt-owned native Wayland/OpenGL context
decision remains valid and is carried forward unchanged.

## Considered options

1. Use `Xw_Window` through XWayland. This is immediately familiar to OCCT, but makes a
   compatibility server part of the runtime path and does not prove native Wayland.
2. Maintain a Völund-specific native Wayland window and EGL implementation. This gives
   maximum control, but duplicates responsibilities already owned by Qt and creates a
   substantial platform-specific maintenance burden.
3. Let Qt own the Wayland surface and OpenGL/EGL context, then attach OCCT through its
   external-context and platform-neutral window facilities.

## Decision

Völund chooses option 3. The Linux viewport will use a Qt-owned OpenGL surface and
context. OCCT remains responsible for AIS/V3d scene rendering, selection, and camera
behavior, but not for creating an X11 window or GLX context.

The automatic Unix-to-`Xw_Window` adapter is removed. The Linux preset builds the CAD
core without the desktop until the Wayland adapter is implemented and exercised on a
real Wayland compositor. XWayland may be used only for explicitly labelled diagnostics;
it cannot satisfy Linux desktop acceptance.

## Consequences

- No direct X11 runtime dependency is introduced by Völund.
- Qt's Wayland platform plug-in remains an existing part of the approved Qt dependency,
  not a new UI framework.
- The adapter must handle Qt context lifetime, framebuffer binding, resize, redraw, and
  device-pixel scaling explicitly.
- Windows continues to use the already verified native OCCT adapter while the shared
  context design is developed.
- Linux desktop support is not declared complete until rendering, picking, resize,
  multiple monitors, suspend/resume, and compositor restart behavior pass on Wayland.

## Removal criterion for the temporary limitation

Enable `VOELUND_BUILD_DESKTOP` in the Linux preset only after the native Wayland smoke
test and interaction tests pass on a Linux Wayland host without `DISPLAY` and without
XWayland libraries loaded by the Völund process.
