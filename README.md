# Völund CAD

Völund CAD is a native C++20 parametric CAD application built on Qt 6 and OpenCASCADE Technology (OCCT/OCAF). Its desktop targets are Linux (native Wayland, reference: Fedora), FreeBSD with Meridian, and Windows; see [ADR 0011](docs/adr/0011-linux-desktop-and-ci-runner.md). FreeCAD is a reference implementation and test oracle, not a runtime dependency.

The binding engineering rules are defined in the [architecture and code contract](docs/architecture-and-code-contract.md).

The [audited completion plan (German, 13 September 2026)](docs/implementation-plan/README.md)
records the implementation gaps and defines 110 bounded work packages, dependencies,
architecture decisions, acceptance scenarios, and an execution template for smaller models.
The current package states and acceptance evidence are tracked in
[`packets.json`](docs/implementation-plan/packets.json). D10 is complete;
D11 identity acceptance is complete; see the [acceptance matrix](docs/implementation-plan/evidence/D11-review.md).
D12d integrates OCAF as the product model authority, including transactions, history and document identity; see [evidence](docs/implementation-plan/evidence/D12d.md). D12 is complete; see the [acceptance matrix](docs/implementation-plan/evidence/D12e.md). Native product persistence is complete; see [D13 acceptance](docs/implementation-plan/evidence/D13d.md).

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

D13c switches product Save/Open to native BinOcaf persistence and retains document
identity. The baseline passes **76/76 Windows** and **73/73 FreeBSD/Sway** tests;
318 source hashes match. See [D13c evidence](docs/implementation-plan/evidence/D13c.md).
D13d completes bidirectional product-file exchange and D13 acceptance: 77/77 Windows, 74/74 FreeBSD/Sway, 319 matching source hashes. See [D13d evidence](docs/implementation-plan/evidence/D13d.md). D14 migration acceptance is complete: 79/79 Windows and 76/76 FreeBSD/Sway tests, including all legacy versions and all 26 constraint kinds. See [D14 evidence](docs/implementation-plan/evidence/D14.md). R01 implements dependency ordering, incremental parameter evaluation and independent error branches (80/80 Windows and 77/77 FreeBSD/Sway tests; 325 matching source/fixture hashes). See [R01 status](docs/implementation-plan/evidence/R01.md). R02 defines the [SubshapeRef contract](docs/subshape-reference-contract.md), with all 26 symbolic fixtures passing on Windows and FreeBSD; the three prototype source hashes match. R03 exposes real primitive/Boolean/transform history: 81/81 Windows and 78/78 FreeBSD tests pass, with matching history outcomes and 328 identical source/fixture hashes. See [R03 status](docs/implementation-plan/evidence/R03.md). R04 is complete: persistent support binding and product integration pass 84/84 Windows and 81/81 FreeBSD tests, with 341 matching source/fixture hashes and reviewed UI captures on both platforms. See [R04 acceptance](docs/implementation-plan/evidence/R04.md). R05 is complete, including persistent projected edges and product integration: 88/88 Windows and 85/85 FreeBSD tests. See [R05 acceptance](docs/implementation-plan/evidence/R05.md). R06 is complete: persistent repair state, explicit transactions and a product repair dialog pass A-REF-03. 91/91 Windows and 88/88 FreeBSD tests, 371 matching source hashes, bidirectional file exchange and reviewed UI captures. See [R06 acceptance](docs/implementation-plan/evidence/R06.md). R06d adds document-wide multi-selection and immediate Auto repair of unique support matches, with one Undo for the batch and unresolved cases left for review. See [R06d evidence](docs/implementation-plan/evidence/R06d.md). R07 adds deterministic contour traversal and localized boundary diagnostics: 93/93 Windows, 90/90 FreeBSD tests and 380 matching source hashes. See [R07 acceptance](docs/implementation-plan/evidence/R07.md). R08a adds analytic intersection splitting (94/94 Windows, 91/91 FreeBSD tests; 385 matching hashes). [R08a evidence](docs/implementation-plan/evidence/R08a.md). R08b completes derived region/containment results with separate holes and inner regions: 95/95 Windows, 92/92 FreeBSD tests and 390 matching hashes. [R08 acceptance](docs/implementation-plan/evidence/R08.md). R09–R10 and Meridian remain open. Project file transfers and tests on the dedicated FreeBSD host have standing user authorization.

## Versions and changes

The current base version is `0.1.0`; no versioned release is tagged yet.
Changes are collected under `Unreleased` in the [Changelog](CHANGELOG.md).
[Versioning and release rules](docs/versioning.md) distinguish product releases,
Git revisions and document formats. New saves use BinOcaf container version 12
with native application schema 2 (schema 1 remains readable); VOLUND 1–16 remain readable legacy formats.

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
./tools/test-windows.ps1
```

The test starter verifies the ASCII mapping and invokes CTest in the same execution
context. It also works across separate agent tool calls and rejects empty test filters.
For an existing build, use `-BuildDirectory build/<name>`; optional `-Label`,
`-TestRegex` and `-ReportPath` select tests and capture JUnit output. A fresh configure
directory can be requested with `setup-windows.ps1 -BuildName <name>`.

The Qt-owned OpenGL context backend is the Windows product default. The separate
native viewport is a diagnostic path:

```powershell
./tools/setup-windows.ps1 -Preset windows-msvc-native-diagnostic
cmake --build --preset windows-msvc-native-diagnostic
./tools/test-windows.ps1 -BuildDirectory build/windows-msvc-native-diagnostic
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

On Linux (reference: Fedora), the native Wayland desktop build uses system Qt 6,
Boost and Eigen plus the same project-local OCCT 7.9.3 as FreeBSD:

```bash
./tools/setup-linux.sh            # checks packages, builds lean OCCT, configures linux-desktop
cmake --build --preset linux-desktop
sh ./tools/run-linux-desktop.sh   # inside the Wayland session: full CTest, DISPLAY unset
```

`./tools/setup-linux.sh core` keeps the vcpkg-based `linux-gcc` core build without
the desktop target. Packet P05 accepts the Linux desktop path. Product desktop targets
are native Wayland on Linux and on FreeBSD under Meridian. X11 and
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
