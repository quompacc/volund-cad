# Völund CAD

Völund CAD is a native C++20 parametric CAD application built on Qt 6 and OpenCASCADE Technology (OCCT/OCAF). Its primary desktop target is FreeBSD with Meridian. FreeCAD is a reference implementation and test oracle, not a runtime dependency.

The binding engineering rules are defined in the [architecture and code contract](docs/architecture-and-code-contract.md).

## Current state

The repository contains a working native CAD vertical slice:

- native C++20/Qt 6 shell with an OCCT AIS/V3d viewport;
- face preselection and selection, orbit, pan and zoom;
- parametric box, cylinder, sketch and extrusion features;
- Join, Cut, Intersect and Separate cylinder results;
- an always-visible connection mode that controls the current cylinder operation and
  supplies the mode for the next cylinder;
- combined Move/Rotate manipulator with snapping and live properties;
- feature tree, exact dirty-state tracking, Undo/Redo and `.volund` persistence;
- planar-face rectangle sketching followed by direct Push/Pull extrusion;
- reusable tool panel with preview, parameters, OK and Cancel;
- modular workspace and command registry with `Solid` as the first product module;
- native C++ tests and an enforced 600-line source-file limit.

The detailed hand-off state and next development slice are recorded in
[`docs/development-status.md`](docs/development-status.md).

The approved dependency budget and the distinction between build-time and shipped components are recorded in [`docs/dependencies.md`](docs/dependencies.md).

The earlier FreeCAD/Python feasibility work is frozen under [`spikes/phase-0-freecad/`](spikes/phase-0-freecad/). It is not part of the product build.

## Prerequisites

- Windows: Visual Studio with the Desktop development with C++ workload
- FreeBSD desktop: Clang and Qt 6 with native Wayland integration; the project builds
  its pinned, Xlib-free OCCT 7.9.3 profile locally
- Linux core compatibility: GCC or Clang
- Git

CMake, Ninja, Qt, and OCCT are resolved by the project setup. No Python runtime is used.

## Configure, build, and test

On Windows, run from a Developer PowerShell:

```powershell
./tools/setup-windows.ps1
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

The Qt-owned OpenGL context backend is the Windows default. It keeps the OCCT scene
and Qt overlays in one composition path, which is required for transparent camera and
tool overlays. The explicit preset remains available as an isolated build directory:

```powershell
cmake --preset windows-msvc-qt-opengl
cmake --build --preset windows-msvc-qt-opengl
ctest --preset windows-msvc-qt-opengl
```

On FreeBSD:

```sh
sh ./tools/setup-freebsd.sh
sh ./tools/build-freebsd-occt.sh
cmake --preset freebsd-clang-lean-occt
cmake --build --preset freebsd-clang-lean-occt
ctest --preset freebsd-clang-lean-occt
```

Run the Wayland-labelled desktop gate from an active Wayland session:

```sh
ctest --preset freebsd-clang-lean-occt -L wayland
```

See [`docs/freebsd-meridian.md`](docs/freebsd-meridian.md) for the two-stage
control-desktop and Meridian validation procedure.

For the Linux core compatibility build:

```bash
./tools/setup-linux.sh
cmake --build --preset linux-gcc
ctest --preset linux-gcc
```

The Linux preset currently builds and tests the CAD core without the desktop target.
The product desktop target is native Wayland on FreeBSD under Meridian. X11 and
XWayland do not satisfy its viewport acceptance gate. Desktop enablement is validated
first on a mature FreeBSD Wayland desktop as a control and then under Meridian.

Local dependency builds are stored below `.tools/` and are intentionally excluded from version control.

## Viewport controls

- move the pointer over the body to preselect faces;
- left click to select a face;
- drag with the middle mouse button to pan;
- drag with the right mouse button to orbit;
- use the mouse wheel to zoom.
- choose Join, Cut, Intersect or Separate from the persistent Connection toolbar;
- enable Move/Rotate and drag an axis or rotation ring; a Separate result moves only
  that body, while a joined result moves as one body from either contributing feature;
- select a planar face, choose Sketch Rectangle, set two corners, pull the extrusion
  preview and confirm it in the temporary tool panel.

## Product structure

```text
apps/                executable composition roots
include/voelund/     public module interfaces
src/kernel/          OCCT geometry and topology boundary
src/document/        document, transactions, persistence, migration
src/features/        parametric feature graph
src/application/     commands and use cases
src/workspaces/      workspace and command contracts
src/modules/         independently registered product workspaces
src/viewport/        AIS/V3d scene and interaction
src/desktop/         Qt Widgets shell
src/platform/        operating-system adapters
packaging/           desktop, MIME, and platform package metadata
tests/               unit, integration, regression, differential
tools/               project-owned build and quality tooling
docs/                architecture, ADRs, and provenance
spikes/              frozen feasibility evidence outside the product build
```
