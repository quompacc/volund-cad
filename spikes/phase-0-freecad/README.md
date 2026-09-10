# Völund CAD Phase 0

> **Architekturhinweis:** Die Python-/FreeCAD-Integration in diesem Verzeichnis ist
> ein abgeschlossener historischer Spike und nicht der künftige Produktunterbau.
> Verbindlich für die weitere Entwicklung ist der
> [Architektur- und Codevertrag](docs/architecture-and-code-contract.md): natives
> C++20, Qt 6 und OCCT/OCAF, ohne Python- oder FreeCAD-Runtime.

This repository contains an intentionally small feasibility spike. Its job is to prove that FreeCAD can provide document, sketch, B-Rep, persistence, export, and native viewport capabilities behind Völund-owned boundaries.

It is not the product shell. The viewport proof still uses the hidden classic FreeCAD main window as a bootstrap factory, which is why the Phase-0 result is a conditional rather than unconditional GO.

## Run the backend proof

Install FreeCAD 1.1.3 or bootstrap the pinned portable Windows build:

```powershell
.\scripts\bootstrap_freecad.ps1
```

Then run:

```powershell
.\scripts\run_backend_proof.ps1
```

The repository path may contain non-ASCII characters. On Windows the runner resolves an ASCII-safe short path before launching FreeCAD because the 1.1.3 command-line launcher does not reliably preserve such paths.

If FreeCAD is in a non-standard location:

```powershell
$env:VOELUND_FREECAD_CMD = 'C:\path\to\FreeCADCmd.exe'
.\scripts\run_backend_proof.ps1
```

Successful execution creates `artifacts/backend-proof/` containing:

- `voelund_backend_proof.FCStd`
- `voelund_backend_proof.step`
- `proof-report.json`

The report records each assertion separately. A geometry-only success is not a complete Phase-0 GO; see [docs/phase-0.md](docs/phase-0.md).

## Run the GUI probes

```powershell
.\scripts\run_gui_probe.ps1
.\scripts\run_shell_proof.ps1
.\scripts\run_shell_stress.ps1
.\scripts\analyze_host_boundary.ps1
```

The first command inventories the FreeCAD/Qt/Coin boundary. The second creates a disposable Völund-owned Qt window, moves the native FreeCAD view into it, and exercises preselection, picking, zoom, orbit, and pan through Qt window events. The stress command repeats that proof five times and records latency. The final command inventories the portable package and records the remaining host-boundary limitation. This is an integration proof, not the final product UI.

## Run the core-only host proof

```powershell
.\scripts\run_core_shell_proof.ps1
.\scripts\run_core_shell_stress.ps1
```

This is the target-architecture proof. It starts the bundled Python runtime rather than `FreeCAD.exe`, loads `FreeCAD`/`Part` for modeling, converts the final B-Rep to a renderer-neutral triangle DTO, and displays it in a Völund-owned Coin3D/Quarter viewport. It neither imports `FreeCADGui` nor creates the classic FreeCAD main window. Völund owns selection, preselection, navigation, and the surrounding Qt UI.

## Run the source boundary test

```powershell
.\scripts\run_source_tests.ps1
```
