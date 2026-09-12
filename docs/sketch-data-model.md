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

Within an active edit session, clicking a line chooses its nearest endpoint and
clicking a circle chooses its center. Ctrl-click toggles additional geometry
references. Dragging one selected reference projects pointer movement back into
sketch-local coordinates and lets PlaneGCS move the remaining constrained geometry;
whole-element translation remains available through the numeric X/Y fields. Releasing
the pointer ends only the gesture; OK or Cancel still owns the document transaction
boundary.

## Parametric constraint foundation

A selected line exposes its exact length in the generic tool panel. Line length,
circle radius, horizontal and vertical are independent driving constraint records
with stable sketch-local IDs. Editing one of these values runs the PlaneGCS adapter
immediately, updates the temporary geometry and retains the constraint for later
recomputation. Whole-element translation is solved against the same records, and OK
commits geometry and constraints together in one transaction. The legacy direction
field on `SketchLine` is temporarily mirrored for version-3 compatibility; it is no
longer the parametric source of truth.

The rectangle tool is a construction command rather than a persistent macro element:
confirmation creates four line elements, alternating horizontal and vertical driving
constraints and four endpoint-coincidence constraints. Each edge and endpoint can
therefore participate in later relations. Legacy `SketchRectangle` elements remain in
the document model so older files and the direct Push/Pull convenience path continue
to work unchanged.

The editor exposes endpoint coincidence plus line parallel and perpendicular
relations for two Ctrl-selected references. The PlaneGCS result reports remaining
degrees of freedom and stable conflicting constraint IDs. Conflicting elements are
presented in red, OK stays disabled until the conflict is resolved, and the most
recent constraint can be removed from the tool panel.

Every constraint is evaluated into a presentation annotation in sketch-local space
and transformed onto the supporting plane. The viewport renders direction and
relation constraints as compact text glyphs and dimensions as millimetre values.
Conflicting annotations are red. Distance and radius annotations are selectable;
double-clicking one opens direct numeric input, updates its stable constraint record
and immediately re-solves the temporary sketch.

A selected line endpoint or circle center can be locked through independent driving
X and Y coordinate constraints. Unlock removes exactly that coordinate pair without
disturbing unrelated dimensions. With two ordered Ctrl-selected references, a
point-on-object relation constrains the first point to the second line or circle.
PlaneGCS therefore retains the remaining tangential degree of freedom instead of
collapsing the relation into a one-time geometric move.

Two selected lines can share one driving length, and two selected circles can share
one radius through an equal relation. Tangency is available for a line and circle or
for two circles. Both relations remain in the sketch, are solved by PlaneGCS after
every edit and appear as `[=]` and `[T]` glyphs in the viewport.

A symmetric relation uses three ordered references: two points followed by a line
as the symmetry axis. PlaneGCS keeps their midpoint on the axis and their connecting
segment perpendicular to it. The viewport identifies the relation with `[S]`.

Rectangles and circles each form a closed profile. Connected line chains form a
profile when at least three non-degenerate elements close within the modeling
tolerance. Open elements remain valid sketch geometry but are ignored by extrusion.
Every detected profile receives a deterministic stable identity equal to the smallest
stable element ID in its boundary. Clicking any boundary element selects the complete
profile; Ctrl-click toggles additional profiles. A new profile extrusion stores the
explicit profile IDs and consumes only those faces. An empty selection remains the
legacy meaning of "all closed profiles", so older direct Push/Pull operations retain
their behavior.

## File format version 8

New saves use `VOLUND 8`. A sketch feature line stores its support, plane, element
count, next element ID, constraint count and next constraint ID. `ELEMENT` records
store geometry and stable IDs; following `CONSTRAINT` records store their stable ID,
kind, one or two geometry references and an optional driving value.

An extrusion record appends a profile count and its stable profile IDs. The reader
continues to accept `VOLUND 1` through `VOLUND 7`; older extrusion records migrate to
an empty profile list, which preserves their previous all-profiles behavior. A version-1 inline rectangle
is migrated in memory to a `RECTANGLE` element with ID 1 and next ID 2; version-2 lines
are migrated with a free direction. Version-3 horizontal and vertical line fields
are migrated to independent constraint records. Version 6 adds coordinate and
point-on-object constraint kinds. Version 7 adds equal and tangent relations. Version
8 appends an optional third reference for symmetric point-point-axis relations. Saving
any loaded document writes version 8.
Invalid, duplicate or zero element IDs, non-finite coordinates, degenerate elements,
inconsistent next IDs and unsatisfied line constraints are rejected while loading.
