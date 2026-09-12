# FreeCAD PlaneGCS provenance

- Upstream: <https://github.com/FreeCAD/FreeCAD>
- Release: FreeCAD 1.1.1
- Commit: `0108fd4b4850cc46e625b60e53cea7a7bbe69f8d`
- Source path: `src/Mod/Sketcher/App/planegcs/`
- License: LGPL-2.1-or-later; full text in
  `third_party/planegcs/COPYING.LIB`

The eleven files in `third_party/planegcs/upstream/src/Mod/Sketcher/App/planegcs`
are copied without source modifications. `SketcherGlobal.h` and
`boost_graph_adjacency_list.hpp` are also copied from the same commit to retain the
upstream include boundary. Völund-owned headers under `third_party/planegcs/compat`
provide only the FreeCAD export macros, diagnostic sink and elapsed-time facility
needed to compile the solver independently.

No FreeCAD GUI, application object model, Part module, Python binding or runtime
binary is included. Future upstream refreshes must update this record and verify the
vendor files against the pinned commit before integration.
