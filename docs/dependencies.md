# Dependency inventory

Völund keeps its direct dependency surface deliberately small. A new direct or runtime dependency requires an ADR under `docs/adr/`.

## Product runtime

| Dependency | Purpose | Enabled scope |
|---|---|---|
| OpenCASCADE Technology 7.9.3 or 8.0.1 | geometry, topology, OCAF, AIS/V3d | 8.0.1 is pinned on Windows; the FreeBSD package is 7.9.3 and uses the compatibility header; FreeBSD port linkage must be audited |
| Qt Base 6.11.1 | desktop window, widgets, event loop, OpenGL integration | official Windows archive or FreeBSD package; ship only required runtime components |
| Eigen 3.4+ | PlaneGCS matrix decomposition and nonlinear solving | header-only, solver target only |
| Boost.Graph 1.75+ | PlaneGCS constraint-system partitioning | solver target only; no Boost.Python |

The C++ standard library is used for general containers, strings, algorithms, errors, filesystem access, and concurrency. No separate convenience library is approved.

## Build-only tools

| Tool | Purpose | Runtime component |
|---|---|---|
| CMake | configure and generate builds | no |
| Ninja | execute local builds | no |
| vcpkg | pin and build external sources | no |
| MSVC / GCC / Clang | compile and link | no |
| CTest | discover and run native tests | no |

On Windows, Qt Base is taken from Qt's official prebuilt archive instead of being built through vcpkg. This avoids vcpkg's large Qt host-tool dependency graph. On FreeBSD, system packages are the initial build input; release packaging must derive its library list from the final executable dependency graph, never from a complete SDK directory.

Linux is a non-product compatibility path and uses the same pinned vcpkg
toolchain as Windows for OCCT, Eigen and Boost; `linux-gcc` deliberately keeps
the desktop disabled. It does not claim that a distribution package mix is the
reproducible product input. FreeBSD remains the separate system-package/
project-local-OCCT path documented below.

For the FreeBSD solver build, install package `eigen` from `math/eigen3` and `boost-libs` from `devel/boost-libs`
(the latter deliberately excludes Boost.Python) from the selected package branch,
then verify the resolved versions before configuring:

```sh
pkg install eigen boost-libs
pkg info eigen boost-libs
```

The exact package version follows the selected `latest` or `quarterly` repository;
the official Ports tree identifies the Eigen package/origin in its
[Ceres dependency declaration](https://cgit.freebsd.org/ports/tree/math/ceres-solver/Makefile?id=70a626a6448663cca56f9193ab3889b771fa4d51)
and defines the Boost package in
[devel/boost-libs](https://cgit.freebsd.org/ports/tree/devel/boost-libs/Makefile?id=d5d82c5ff9a1069d6e0382029cc8f99d324c9e0b).
Check Eigen >=3.4 and Boost >=1.75 on the actual target. Installed package contents
do not authorize shipping every contained library; runtime packaging stays limited
to the dependencies actually required by Völund.

On FreeBSD, Qt's native Wayland platform integration owns the surface and the
OpenGL/EGL context. Völund does not add a direct X11 dependency, and OCCT's `Xw_Window`
is not a product adapter. The stock FreeBSD OCCT port currently enables Qt 5 and Xlib;
it is acceptable for initial investigation but not approved for release until its
runtime linkage is audited. A lean package flavor or project-owned OCCT build may be
needed. Direct Wayland protocol code is avoided unless the Qt-owned integration proves
insufficient, in which case it requires a separate ADR.

## Not currently required

- Python or a Python interpreter;
- FreeCAD binaries, modules, or libraries;
- weitere Boost-Module außerhalb des für PlaneGCS belegten Boost.Graph-Graphen;
- TBB, VTK, FreeImage, or an additional rendering engine;
- zusätzliche Test-, Logging-, Formatierungs-, Serialisierungs- oder Containerframeworks ohne konkreten Bedarf;
- WebView, browser runtime, Electron, or Tauri.

Diese Liste ist kein Verbot technisch sinnvoller Standardbibliotheken. Sie verhindert
nur vorsorgliche Aufnahme. Sobald Eigenentwicklung unverhältnismäßig, riskant oder
schlechter wartbar wäre, wird die passende Bibliothek nach dem ADR-Verfahren bewertet
und bei positivem Ergebnis aufgenommen.

## D08: isolated OCAF persistence probe

[ADR 0008](adr/0008-ocaf-binary-persistence.md) selects native BinOcaf format 12
for D12/D13. The isolated probe links TKBin, TKCAF and TKPrim and verifies actual
OCCT 8.0.1 / 7.9.3 bidirectional persistence. The OCAF modules TKCDF, TKLCAF, TKCAF,
TKBinL and TKBin are available in the full installations; the lean FreeBSD product
build still needs its own verified extension before D12. Do not mix its kernel
libraries with system OCAF libraries. [Evidence and runtime lists](implementation-plan/evidence/D08.md).
