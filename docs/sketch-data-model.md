# Parametric sketch data model

## First vertical slice

The original direct Push/Pull path projected two viewport clicks onto a selected
planar face, previewed one rectangle and one extrusion, and committed a dedicated
sketch feature followed by an extrusion feature. The sketch feature nevertheless
stored exactly one anonymous rectangle. Geometry creation lived in `features`, while
the Qt controller only translated the accepted viewport result into document values.

The first general sketch slice retains that interaction and replaces the anonymous
rectangle with an ordered collection of independently identified elements:

- `SketchLine` stores two planar endpoints;
- `SketchRectangle` stores two opposite planar corners;
- `SketchCircle` stores a planar center and radius;
- every `SketchElement` owns a non-zero, sketch-local `SketchElementId`;
- `SketchElements::nextId` preserves deterministic ID allocation through edits,
  Undo/Redo and Save/Load.

The document layer owns element identity, numeric validation, history and
persistence. The features layer detects closed profiles and converts them to OCCT
faces. The application layer offers both the retained rectangle convenience command
and the general multi-element command. Viewport and desktop continue to implement the
tested two-click rectangle workflow without duplicating profile semantics.

Selecting a sketch feature evaluates each element into a separate presentation shape.
The viewport receives only the shape and stable element ID, owns the AIS presentation
and picking lifecycle, and reports selection intent back to the application. A selected
element is highlighted independently of the supporting body and identified in the
properties area. Both Windows viewport backends and the FreeBSD Wayland backend use
the same presentation component.

## Multi-element editor slice

The general Sketch command opens a dedicated editor on the selected planar face. Its
temporary `SketchEditorSession` belongs to the application layer and is independent
of the retained direct rectangle/Push-Pull interaction. Line, rectangle and circle
tools share the viewport's linear snapping, may be switched without discarding
confirmed elements, and allocate deterministic sketch-local IDs as each element is
confirmed. The viewport reports only snapped planar point intents and picking results;
the application session owns temporary geometry, selection, profile recomputation and
OCCT evaluation through `SketchEvaluation` and `SketchProfiles`.

OK writes the complete element collection as one sketch-feature transaction. Cancel
or Escape discards the editor session without adding a feature or history entry.
Sketches containing only open geometry are valid document features and survive
Save/Load. The feature graph evaluates their element geometry without requiring a
face; only an extrusion evaluates profile faces and reports an error when no closed
profile exists.

The editor can also reopen a selected sketch. Existing elements retain their IDs and
may be selected, translated in sketch-local X/Y coordinates or deleted; newly added
elements continue from the persisted `nextId`. OK replaces the sketch parameters as
one document transaction and recomputes downstream profiles and extrusions. Cancel
discards every temporary edit. Undo/Redo and Save/Load therefore preserve both the
edited geometry and the monotonic ID allocation. Direct handle editing, dimensions,
constraints and persistent profile selection remain later slices.

Rectangles and circles each form a closed profile. Connected line chains form a
profile when at least three non-degenerate elements close within the modeling
tolerance. Open elements remain valid sketch geometry but are ignored by extrusion.
An extrusion requires at least one closed profile and currently consumes all closed
profiles in the sketch. Profile selection and persistent profile IDs are intentionally
reserved for a later editor slice; the stable element IDs and profile-to-element
mapping are already suitable for those boundaries.

## File format version 2

New saves use `VOLUND 2`. A sketch feature line stores its support, plane, element
count and next element ID. Exactly that many following `ELEMENT` records store
`LINE`, `RECTANGLE` or `CIRCLE` geometry and its stable ID.

The reader continues to accept `VOLUND 1`. Its single inline rectangle is migrated in
memory to a `RECTANGLE` element with ID 1 and next ID 2. Saving the loaded document
writes version 2. Invalid, duplicate or zero element IDs, non-finite coordinates,
degenerate elements and inconsistent next IDs are rejected while loading.
