# ADR 0005: PlaneGCS as the sketch constraint solver

- Status: accepted
- Date: 12 September 2026
- Owner: Völund CAD

## Context

A production-quality parametric sketcher needs nonlinear solving, subsystem
partitioning, degrees-of-freedom analysis and conflict and redundancy diagnosis.
Reimplementing those numerical algorithms would consume substantial time while
creating avoidable correctness risk. FreeCAD is already the project's permitted
behavioural reference, and its PlaneGCS subsystem provides this capability in C++.

## Decision

Völund vendors PlaneGCS from FreeCAD 1.1.1 commit
`0108fd4b4850cc46e625b60e53cea7a7bbe69f8d` as an isolated LGPL-2.1-or-later
component under `third_party/planegcs`. The upstream solver files remain unchanged.
Small Völund-owned compatibility headers replace FreeCAD logging, timing and export
macros; no FreeCAD application, document, Part, GUI, Coin3D or Python component is
linked or loaded.

The Völund features layer owns a narrow adapter between stable document identities
and PlaneGCS parameters. Document persistence, transactions, feature evaluation,
viewport presentation and Qt interaction remain Völund responsibilities.

Eigen 3.4 or newer and Boost.Graph 1.75 or newer are admitted as solver-only build
dependencies. Eigen is header-only. Only the Boost modules selected by the package
manager for Boost.Graph are admitted; no Boost.Python component is used.

## Alternatives considered

- Continue a solver from scratch: rejected because numerical diagnosis and robust
  nonlinear dragging are not product differentiators and carry high regression risk.
- Link the complete FreeCAD Sketcher: rejected because it depends on FreeCADApp,
  Part, FreeCADGui, PartGui, generated Python bindings and Coin3D interaction code.
- Use SolveSpace's solver: viable, but would discard the closer behavioural match and
  existing FreeCAD reference workflow without a demonstrated technical advantage.

## Consequences

- PlaneGCS notices and the LGPL text ship with every distribution containing it.
- Every upstream update is pinned, provenance-reviewed and compared against the
  unmodified vendor tree.
- Release packaging must satisfy LGPL relinking and corresponding-source obligations.
  Static development builds are not a release-distribution decision.
- The dependency and binary-size impact is measured on Windows and FreeBSD before the
  first release; an excessive impact reopens this ADR.

## Release linking (L04, user decision 18 October 2026)

PlaneGCS ships as its own shared library `voelund_planegcs` (Windows DLL, Linux `.so`) in every
build, so that a recipient can replace it. Its source (`third_party/planegcs`) and LGPL text are
part of every package (`source/planegcs/`, `licenses/planegcs/`). Static linking with a relink
kit was not chosen. This closes the release-linking question above; see
[L04](../implementation-plan/evidence/L04.md).
