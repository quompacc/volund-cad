# Dependency inventory

Völund keeps its direct dependency surface deliberately small. A new direct or runtime dependency requires an ADR under `docs/adr/`.

## Product runtime

| Dependency | Purpose | Enabled scope |
|---|---|---|
| OpenCASCADE Technology 8.0.1 | geometry, topology, OCAF, AIS/V3d | core only; FreeType, FreeImage, RapidJSON, TBB, VTK disabled |
| Qt Base 6.11.1 | desktop window, widgets, event loop, OpenGL integration | official prebuilt Qt Base archive; only required runtime DLLs are packaged |

The C++ standard library is used for general containers, strings, algorithms, errors, filesystem access, and concurrency. No separate convenience library is approved.

## Build-only tools

| Tool | Purpose | Runtime component |
|---|---|---|
| CMake | configure and generate builds | no |
| Ninja | execute local builds | no |
| vcpkg | pin and build external sources | no |
| MSVC / GCC / Clang | compile and link | no |
| CTest | discover and run native tests | no |

On Windows, Qt Base is taken from Qt's official prebuilt archive instead of being built through vcpkg. This avoids vcpkg's large Qt host-tool dependency graph. On Linux, the distribution's Qt Base development package is preferred. Release packaging must derive its library list from the final executable dependency graph, never from a complete SDK directory.

On Linux, Qt's native Wayland platform integration owns the surface and the OpenGL/EGL
context. Völund does not add a direct X11 dependency, and OCCT's `Xw_Window` is not a
product adapter. Direct Wayland protocol code is avoided unless the Qt-owned integration
proves insufficient, in which case it requires a separate ADR.

## Not currently required

- Python or a Python interpreter;
- FreeCAD binaries, modules, or libraries;
- Boost als pauschales Gesamtpaket; einzelne Module dürfen bei belegtem Nutzen geprüft werden;
- TBB, VTK, FreeImage, or an additional rendering engine;
- zusätzliche Test-, Logging-, Formatierungs-, Serialisierungs- oder Containerframeworks ohne konkreten Bedarf;
- WebView, browser runtime, Electron, or Tauri.

Diese Liste ist kein Verbot technisch sinnvoller Standardbibliotheken. Sie verhindert
nur vorsorgliche Aufnahme. Sobald Eigenentwicklung unverhältnismäßig, riskant oder
schlechter wartbar wäre, wird die passende Bibliothek nach dem ADR-Verfahren bewertet
und bei positivem Ergebnis aufgenommen.
