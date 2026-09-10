# Phase 0 decision record

> **Historischer Nachweis, keine aktuelle Produktarchitektur.** Die hier beschriebene
> Python-/FreeCAD-Runtime wurde verworfen. Verbindlich ist seit dem 10. September 2026
> der [Architektur- und Codevertrag](architecture-and-code-contract.md).

Status: Gate A passed, core-only Gate B passed

Pinned FreeCAD version: 1.1.3

## Gate A Backend

Passed on 10 September 2026 with the official portable FreeCAD 1.1.3 Windows x86_64 py311 build.

Verified:

- fully constrained rectangle sketch;
- parametric extrusion and B-Rep fillet;
- parameter update and recompute;
- undo and redo;
- FCStd save, close, and reopen with identical volume;
- STEP export and import round-trip with one solid and identical volume;
- FreeCAD access isolated in the Völund adapter.

Compatibility finding: the FreeCAD 1.1.3 Windows launcher does not reliably preserve the non-ASCII project path `Völund` in command-line file arguments. The runner therefore resolves and uses the Windows short path. This workaround is acceptable for the spike but must not become a product requirement.

## Gate B Native viewport

Integration proof passed:

- FreeCAD provides PySide6 with Qt 6.8.3 and Coin 4.0.3;
- `FreeCADGui.createViewer()` is available;
- the native Coin scene graph is accessible after initializing Pivy;
- a Völund-owned `QMainWindow` can display the reparented native viewport while the classic FreeCAD main window is hidden;
- the model renders correctly;
- pointer-driven preselection, picking, wheel zoom, orbit, and pan pass through Qt window events;
- the probe records startup, picking, zoom, orbit, and pan latency;
- the proof exits automatically and produces a reproducible screenshot and JSON report.

Repeatability result on the Windows test host: 5/5 runs passed all interactions. Median launcher-to-report time was 4098.470 ms (maximum 4148.960 ms); median picking latency was 5.735 ms, wheel-event latency 16.759 ms, orbit sequence 83.282 ms, and pan sequence 66.595 ms. These are Phase-0 regression baselines, not final product performance budgets.

Remaining limitation:

- FreeCAD's hidden main window and MDI view are still used to create the native viewport before it is reparented;
- the official portable Windows package contains the runtime DLLs but no FreeCAD headers, CMake package, or import libraries, so it cannot be used to compile and test a supported main-window-free C++ host;
- the proof must still be repeated against the intended Linux build before product-shell implementation.

Current assessment: the native viewport is usable and interactive behind a Völund-owned Qt shell. The hidden main-window/MDI bootstrap remains the central coupling risk and is acceptable only for the spike.

## Core-only host supersession

The reparented FreeCADGui viewport is no longer the proposed product boundary. A second host proof now starts the bundled `python.exe` directly, imports `FreeCAD` and `Part` for modeling, and transfers a renderer-neutral triangle mesh DTO to a Völund-owned Coin3D/Quarter viewport.

The core-only host proves:

- `FreeCAD.exe` is not started;
- neither the `FreeCADGui` Python module nor `FreeCADGui.dll` is loaded;
- no classic FreeCAD main window is created;
- the Völund layer owns the Qt shell, rendering scene, preselection, picking, zoom, orbit, and pan;
- a backend parameter change is recomputed and displayed through a fresh mesh DTO;
- the old main-window/MDI bootstrap dependency is eliminated.

Repeatability result on the Windows reference host: 5/5 core-only runs passed. No run loaded FreeCADGui or created a classic main window. Median startup-to-report time was 1452.123 ms.

This changes the implementation tradeoff: Völund must own display-mesh lifecycle, stable topology-to-pick identifiers, edge/sketch overlays, clipping, and other CAD visualization behavior instead of inheriting FreeCAD ViewProviders. The Phase-0 implementation proves the boundary with one filleted solid; those richer capabilities belong in subsequent product slices.

## Final decision

`GO` for FreeCAD as the modeling kernel behind a Völund-owned UI and viewport.

The former reparented FreeCADGui route remains only as historical spike evidence and must not become the product shell. Product structure may now grow around the `CadBackend` and `DisplayMesh` boundaries. Linux remains expected but unverified; it is not a blocker while Windows is the only available test host.

Evidence is written to `artifacts/backend-proof/proof-report.json`, `artifacts/core-shell-probe/core-shell-report.json`, and `artifacts/core-shell-stress/summary.json`. The older GUI artifacts document the superseded reparenting experiment.
