# Phase 0 acceptance plan

> **Historischer Machbarkeitsplan.** Die Versuche bleiben als technische Evidenz
> erhalten, legen aber keine Produktarchitektur fest. Die aktuelle Grundlage ist der
> [Architektur- und Codevertrag](architecture-and-code-contract.md).

Phase 0 is split into two independently reported gates. This prevents a successful geometry script from being mistaken for proof of the complete architecture.

## Gate A Backend

The automated backend proof must demonstrate all of the following on the pinned FreeCAD version:

- create a FreeCAD document and constrained sketch through the Völund adapter;
- generate an extrusion and B-Rep fillet;
- change a model parameter and recompute the result;
- undo and redo the parameter change;
- save and reopen an FCStd document without changing the resulting volume;
- export a non-empty STEP file;
- keep FreeCAD objects and imports out of the calling script.

Passing Gate A means the FreeCAD application layer is usable behind a narrow adapter. It does not prove viewport independence.

## Gate B Native viewport

Gate B must demonstrate in an isolated application:

- a Völund-owned Qt window without the classic FreeCAD main window;
- an embedded Coin3D/FreeCAD view;
- orbit, pan, zoom, selection, and preselection;
- model updates visible after adapter commands;
- acceptable startup and interaction latency;
- a written inventory of required GUI modules and global FreeCAD state.

## Decision rule

- `GO`: both gates pass without broad core patches or dependence on the classic main GUI.
- `CONDITIONAL GO`: the gates pass with narrow, documented compatibility code and an acceptable maintenance estimate.
- `NO-GO`: the classic GUI or fragile internal patches are required for stable operation.

The product repository structure, design system, Vault integration, and manufacturing workflow remain out of scope until this decision is recorded.

## Recorded result

Gate A passed. The initial Gate B experiment passed functionally but depended on a hidden FreeCAD main window. It has been superseded by a core-only host: FreeCAD performs modeling and tessellation, while Völund owns the Qt shell, Coin3D scene, navigation, preselection, and picking. The core-only process does not load FreeCADGui or create any classic FreeCAD window.

Phase 0 therefore records a `GO` for the core-only architecture. This deliberately accepts responsibility for Völund's own CAD visualization layer. Linux validation is deferred until a Linux test environment exists and is not a current blocker.
