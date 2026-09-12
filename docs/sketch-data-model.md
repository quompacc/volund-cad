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
- `SketchArc` stores a planar center, radius and counter-clockwise start/end angles;
- `SketchEllipse` stores its center, major/minor radii and major-axis rotation;
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
of the retained direct rectangle/Push-Pull interaction. Line, rectangle, circle and
center-arc tools share the viewport's linear snapping, may be switched without discarding
confirmed elements, and allocate deterministic sketch-local IDs as each element is
confirmed. The viewport reports only snapped planar point intents and picking results;
the application session owns temporary geometry, selection, profile recomputation and
OCCT evaluation through `SketchEvaluation` and `SketchProfiles`.

The drawing set additionally contains continuous polylines, three-point arcs,
ellipses, obround slots and regular polygons. Compound tools decompose into ordinary
lines and arcs with stable IDs and persistent internal relations. Trim, extend,
split, fillet, offset, mirror and linear/circular patterns operate on those ordinary
elements. Local Undo/Redo, stepwise Escape and visible box selection keep these edits
inside the temporary session.

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

The center-arc tool uses three clicks: center, radius point and end point. PlaneGCS
owns the center, radius, endpoint and angle degrees of freedom and couples them with
its native arc rules. Within an active edit session, clicking a line or arc chooses
its nearest endpoint (or the arc center), while clicking a circle chooses its center.
Ctrl-click toggles additional geometry
references. Dragging one selected reference projects pointer movement back into
sketch-local coordinates and lets PlaneGCS move the remaining constrained geometry;
whole-element translation remains available through the numeric X/Y fields. Releasing
the pointer ends only the gesture; OK or Cancel still owns the document transaction
boundary.

Opening either sketch workflow aligns the camera normal to the selected support face
and uses the sketch plane's Y direction as view-up. The editor exposes a distinct
Select mode so selection and dragging cannot steal clicks from an active drawing tool.
Confirmed line endpoints, circle centers and arc centers/endpoints are rendered as
zoom-independent point markers. Drawing within 12 screen pixels snaps to such a marker
and confirmation adds a persistent PlaneGCS coincidence relation automatically; grid
snapping remains the fallback away from reference points. The sketch origin and local
axes are explicit snap targets. Boundary edges of a newly selected planar support
face are projected into the editor, displayed in violet and provide fixed endpoint
and midpoint references for new geometry.

## Parametric constraint foundation

A selected line exposes its exact length in the generic tool panel. Line length,
circle or arc radius, horizontal and vertical are independent driving constraint records
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

Two selected points can carry a driving point-to-point distance, including the two
endpoints of one line. Two selected lines can carry a signed driving angle in degrees.
Both dimensions are editable directly from their `D … mm` and `A …°` viewport labels;
the PlaneGCS adapter converts stored degrees to radians only at the solver boundary.

Directional dimensions store the signed differences `x₂−x₁` and `y₂−y₁`, including
zero, and appear as `DX … mm` and `DY … mm`. Any sketch element can be toggled to
construction geometry. It remains selectable, editable and usable by constraints,
but is rendered in blue and excluded from closed-profile detection and extrusion.
An ordered point and line can carry a midpoint relation (`[M]`). Two ordered lines
can form a perpendicular bisector (`[PB]`): the first line is kept perpendicular to
the second and its first endpoint remains equidistant from the second line's endpoints.
Both relations are expanded into native PlaneGCS equations and also work with
construction geometry.
Two circles can carry an explicit concentric relation (`[CC]`) that couples their
centers while retaining independent radii. A circle can also carry an editable
diameter dimension (`Ø … mm`); PlaneGCS stores and solves the diameter directly
rather than converting the UI operation into a one-time radius edit.
Two lines can be made collinear (`[CL]`) while retaining independent endpoints and
lengths. Two selected points can be aligned horizontally (`[HP]`) or vertically
(`[VP]`), including the two endpoints of one line. These are relational constraints,
not zero-valued dimensions, so design intent remains explicit in the model.
A line or circle can be fixed as one logical `[F]` constraint. Its complete pose and,
for circles, radius are stored as auxiliary driving values so dragging cannot silently
redefine the fixed target. An ordered point and line can carry an editable perpendicular
distance (`PL … mm`) solved directly by PlaneGCS.

Rectangles and circles each form a closed profile. Connected line-and-arc chains form
a profile when at least two non-degenerate elements close within the modeling
tolerance; this includes a semicircle plus its diameter. Open elements remain valid
sketch geometry but are ignored by extrusion.
Every detected profile receives a deterministic stable identity equal to the smallest
stable element ID in its boundary. Clicking any boundary element selects the complete
profile; Ctrl-click toggles additional profiles. A new profile extrusion stores the
explicit profile IDs and consumes only those faces. An empty selection remains the
legacy meaning of "all closed profiles", so older direct Push/Pull operations retain
their behavior.

## File format version 16

New saves use `VOLUND 16`. A sketch feature line stores its support, plane, element
count, next element ID, constraint count and next constraint ID. `ELEMENT` records
store geometry and stable IDs; following `CONSTRAINT` records store their stable ID,
kind, one or two geometry references and an optional driving value.

An extrusion record appends a profile count and its stable profile IDs. The reader
continues to accept `VOLUND 1` through `VOLUND 15`; older extrusion records migrate to
an empty profile list, which preserves their previous all-profiles behavior. A version-1 inline rectangle
is migrated in memory to a `RECTANGLE` element with ID 1 and next ID 2; version-2 lines
are migrated with a free direction. Version-3 horizontal and vertical line fields
are migrated to independent constraint records. Version 6 adds coordinate and
point-on-object constraint kinds. Version 7 adds equal and tangent relations. Version
8 appends an optional third reference for symmetric point-point-axis relations. Version
9 adds point-distance and signed angle dimensions. Version 10 appends a construction
flag to every element and adds signed horizontal and vertical point dimensions.
Version 11 adds midpoint and perpendicular-bisector relation kinds. Version 12 adds
concentric relations and driving circle diameters.
Version 13 adds collinear lines and horizontal/vertical point alignment relations.
Version 14 appends auxiliary values to constraint records and adds fixed geometry plus
point-to-line distances. Version 15 adds persistent center arcs with radius and
start/end angles. Version 16 adds persistent rotated ellipses. Saving any loaded
document writes version 16.
Invalid, duplicate or zero element IDs, non-finite coordinates, degenerate elements,
inconsistent next IDs and unsatisfied line constraints are rejected while loading.
