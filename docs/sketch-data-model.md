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
edited geometry and the monotonic ID allocation.

Within an active edit session, the selected element itself acts as a viewport grip.
Dragging it projects pointer movement back into sketch-local coordinates, applies the
configured linear snap and reports only incremental translation intent to the
application editor. The preview presentation and the numeric X/Y translation fields
stay synchronized. Releasing the pointer ends only the gesture; OK or Cancel still
owns the document transaction boundary.

## Parametric constraint foundation

A selected line exposes its exact length in the generic tool panel. Line length,
circle radius, horizontal and vertical are independent driving constraint records
with stable sketch-local IDs. Editing one of these values runs the PlaneGCS adapter
immediately, updates the temporary geometry and retains the constraint for later
recomputation. Whole-element translation is solved against the same records, and OK
commits geometry and constraints together in one transaction. The legacy direction
field on `SketchLine` is temporarily mirrored for version-3 compatibility; it is no
longer the parametric source of truth.

Rectangles expose independent positive width and height values while preserving the
sign of their original drawing directions. These remain direct macro-geometry values
for now and participate in the same single OK transaction.

The document and solver adapter already support endpoint coincidence, parallel and
perpendicular relations, degrees-of-freedom reporting and stable conflict IDs. Their
multi-selection UI remains the next Sketcher slice. Persistent profile selection also
remains later work.

Rectangles and circles each form a closed profile. Connected line chains form a
profile when at least three non-degenerate elements close within the modeling
tolerance. Open elements remain valid sketch geometry but are ignored by extrusion.
An extrusion requires at least one closed profile and currently consumes all closed
profiles in the sketch. Profile selection and persistent profile IDs are intentionally
reserved for a later editor slice; the stable element IDs and profile-to-element
mapping are already suitable for those boundaries.

## File format version 4

New saves use `VOLUND 4`. A sketch feature line stores its support, plane, element
count, next element ID, constraint count and next constraint ID. `ELEMENT` records
store geometry and stable IDs; following `CONSTRAINT` records store their stable ID,
kind, one or two geometry references and an optional driving value.

The reader continues to accept `VOLUND 1`, `VOLUND 2` and `VOLUND 3`. A version-1 inline rectangle
is migrated in memory to a `RECTANGLE` element with ID 1 and next ID 2; version-2 lines
are migrated with a free direction. Version-3 horizontal and vertical line fields
are migrated to independent constraint records. Saving any loaded document writes version 4.
Invalid, duplicate or zero element IDs, non-finite coordinates, degenerate elements,
inconsistent next IDs and unsatisfied line constraints are rejected while loading.
