# Parametric sketch data model

## Current document contract

R04b adds an optional durable `SketchSupportBinding`: document/body namespace,
primitive face role or extrusion end-cap boundary element IDs, and a relative
sketch frame. The evaluated source frame supplies current placement. Deleted,
ambiguous or unproven ancestry fails the sketch; geometry proximity never replaces
the reference. End-cap keys cover the current supported profiles, not the general
region identity promised by R09. Duplication remaps internal document namespaces.
R04c connects picked faces to new bindings; the editor retains existing bindings
and resolves their current frame. Overlays and extrusion use evaluated sketch
parameters. Pending drafts check the document revision before confirmation.
Unbound legacy sketches remain on the legacy resolver until R06;
bound sketches reject legacy projected segment references pending R05.

As of D14, private OCAF state is the sole mutable document authority. Public
snapshots contain immutable parameter values and identities. Transactions publish
prepared states atomically; one compound sketch/extrusion command is one Undo step.
Separate confirmed tools retain separate steps. Undo/Redo recomputes geometry and
does not create another history entry. Returning to saved content clears Dirty;
the feature-ID allocation watermark does not move backwards after Undo.

New saves use BinOcaf 12 / application schema 4 (schemas 1/2/3 remain readable; see R06b). Native Open and Save-As preserve
DocumentId; every successful legacy import gets a fresh DocumentId. New and
Duplicate also create new identities. The import report remains available while
editing or saving the imported session, but is not stored as native provenance.
Undo history and evaluated shapes are not persistent parameter data.

Structurally invalid files fail before publication. Structurally valid parameters
with solver, geometry or missing-profile errors remain loadable and editable;
errors are recomputed after Open. A failed Open or Save preserves the previous
document and save state. Original legacy files remain unchanged when migrating
with Save-As. In-place Save explicitly replaces the legacy file with native bytes.

Legacy support planes and profile IDs do not provide stable topological identity.
Import reports preserve unresolved references without guessing replacements.
Stable reference migration and repair UI remain R02–R06; independent error branches
are implemented and accepted in R01 on Windows and FreeBSD/Sway. Whole-scene current geometry remains unavailable on errors, while valid branch bodies are exposed separately. See [R01 status](implementation-plan/evidence/R01.md) and [D14 acceptance](implementation-plan/evidence/D14.md).

## Historical first vertical slice

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
and midpoint references for new geometry. A confirmed projected reference stores the
support-edge segment identity and its normalized position, not merely the current X/Y
coordinates. Recompute resolves the supporting planar face again, moves the sketch
plane along its retained normal and updates those driving coordinates from the current
edge geometry before PlaneGCS solves the sketch.

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

While drawing, the same relations are inferred within a screen-space tolerance.
Endpoints, midpoints and points on existing lines create coincidence, midpoint or
point-on-object relations; nearly horizontal, vertical, parallel or perpendicular
lines are snapped exactly and constrained. A circle or center arc placed tangentially
on a referenced line receives a persistent tangency relation. Preview glyphs expose
the pending inference before confirmation.

Circle and center-arc creation also compares the live radius and center placement
with existing radial geometry. Matching radii snap to an equal-radius relation,
matching centers create a concentric relation, and external or internal contact
creates circle-circle or arc-circle tangency. Ambiguous equal-and-tangent placement
prefers tangency, while `[=?]`, `[CC?]` and `[T?]` identify the pending relation.

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
endpoints of one line. Two selected lines can carry a signed driving angle.
Both dimensions are editable directly from their `D … mm` and `A …°` viewport labels.
The document and native store use radians; degrees are converted at the legacy
text-import and UI boundaries. PlaneGCS receives radians directly.

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
Since R09 a selected profile is a region identified by its canonical boundary
signature (`SketchRegionId`, see `implementation-plan/R09-plan.md`), not by the smallest
element ID. Clicking a boundary element selects the one region whose outer boundary
uses it; Ctrl-click toggles additional regions. A new profile extrusion stores the
explicit RegionIds. Legacy minimum-element profile IDs are migrated on Open to the
filled loop interior; unmappable ones keep the old meaning and need repair. An empty
selection remains the legacy meaning of "all closed profiles", so older direct
Push/Pull operations retain their behavior.

## Current persistence

New saves use native BinOcaf container version 12 and application schema 4 (R06b),
retaining document identity. Native schemas 1/2/3 and VOLUND 1–16 remain readable.
The following describes the historical text format; saving an imported document
writes the native format. See [D13c](implementation-plan/evidence/D13c.md).

## Legacy file format version 16

In `VOLUND 16`, a sketch feature line stores its support, plane, element
count, next element ID, constraint count and next constraint ID. `ELEMENT` records
store geometry and stable IDs; following `CONSTRAINT` records store their stable ID,
kind, one or two geometry references and an optional driving value.

An extrusion record appends a profile count and its stable profile IDs. The reader
continues to accept `VOLUND 1` through `VOLUND 15`; older extrusion records migrate to
an empty profile list for versions 1–4, which preserves their previous all-profiles behavior. A version-1 inline rectangle
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
start/end angles. Version 16 adds persistent rotated ellipses.
Invalid, duplicate or zero element IDs, non-finite coordinates, degenerate elements,
inconsistent next IDs and structurally invalid constraint references are rejected
while loading. Solver satisfaction and valid final geometry are separate checks;
a failed solve or missing selected profile does not make a structurally valid file
unreadable.

## Typed projected edge references (R05b-P)

A coordinateX/coordinateY constraint may carry `ProjectedEdgeReference`: DocumentId,
BodyId, primitive face-pair or contour origin, and start/end/normalized analytic
parameter selection. Endpoint selections store an unused parameter of zero.
Contour origin includes sketch/element IDs, geometry kind or rectangle-side role,
and for extrusion contours the extrusion feature and start/end stage.
Only explicitly bound support sketches accept this field. It cannot coexist with
legacy numeric auxiliary attachments. Duplication remaps internal document IDs.
Evaluation resolves the current analytic curve, verifies membership in the named
support face and updates derived coordinates before solving. Missing, ambiguous,
unavailable and clipped-away parameters fail; stored coordinates are not fallback.

Schema 3 adds optional constraint child 8. Its root stores the document hex ID;
children: 1 body, 2 origin tag (1 primitive / 2 contour), 3 producer/sketch ID,
4 first primitive face role / contour element ID, 5 second face role / contour role,
6 contour stage and 7 optional extrusion ID encoded as zero for a sketch contour,
8 point kind and 9 normalized parameter. Primitive payload has seven children,
contour payload nine. IDs use canonical decimal strings, role codes correspond to
schema-3 enum values and must not be reordered. All payloads are fixed-size and
share the existing constraint/document count and bounded container limits.
The point-reference UI and both platforms were accepted in R05b-U.
## Persistent reference repair (R06b)

Application Open marks sketches without support binding as `legacySupport`, or
bound sketches with legacy coordinate attachments as `legacyEdges`. It preserves
original plane, element and constraint data, does not write the source file and
does not create an undo entry or dirty state. A persisted repair reason produces
`MigrationNeedsRepair`; descendants are blocked while independent branches remain
valid. The low-level codec remains a lossless structural reader.

Sketches on base planes (V01a, schema 6) are exempt: they have no support feature
and no binding by design, carry `datum` (1 = XY, 2 = XZ, 3 = YZ) in optional sketch
child 13 and lie exactly on that plane. Such a sketch has no input reference (feature
child 5), opens its own body group with BodyId equal to its FeatureId, and its
extrusion produces that body's first solid without a fuse.

Schema 4 adds optional sketch child 12 containing one integer attribute:
1 = legacySupport (requires absent binding), 2 = legacyEdges (requires binding
and at least one legacy coordinate attachment). Unknown values, inconsistent
states, extra payload and this child under schemas 1–3 are rejected.

Support and edge repairs require current document identity, generation/revision,
original slot contents and a verified current candidate. A geometric match is
only a proposal. Repair is a single document transaction with the existing
reject/keepEditableErrors policy. Paired coordinate slots can be repaired together;
remaining legacy attachments keep the error state. Undo/Redo and native Save/Open
preserve pending and repaired states. The editor refuses pending migration states.
Candidate selection in the product UI and A-REF-03 were accepted in R06c. The repair dialog requires explicit selection, previews face bounds and edge points, and retains pending errors only after explicit confirmation.
## Konturgraph (R07)

Die Profilerkennung nutzt einen abgeleiteten Graphen orientierter Randstücke.
Konstruktion wird ausgeschlossen; Verzweigungen und überlappende Dubletten
liefern keinen Materialrand. Eine Hilfslinie am Rand muss deshalb ausdrücklich
Konstruktion sein, wenn sie nicht zum Materialrand gehören soll. Die alte
greedy Suche konnte solche Zweige still übergehen.

Toleranzverbindungen ändern keine gespeicherten Kurven. Mehrdeutige transitive
Verbindungen werden abgewiesen. Diagnosen tragen Element-IDs; die Flächenauswertung
übernimmt die berechnete Laufrichtung. Profil-IDs bleiben vorläufig kompatibel,
werden aber nicht als dauerhafte Regionsidentität ausgegeben (R09).
Schnittbildung, Innenkonturen und allgemeine Regionsbildung folgen in R08.

## Analytische Schnittintervalle (R08a)

SketchIntersectionResult enthält Quellstücke, Schnittparameter/Kontaktart und
Teilintervalle auf unveränderten analytischen Kurven. Konstruktion ist ausgeschlossen.
Bei Diagnosen sind keine Intervalle zur Weiterverarbeitung vorhanden. Periodische
Naht und Originaldomäne bleiben explizit. Weder dauerhafte Region-ID noch Face
sind Bestandteil dieses Ergebnisses; R08b/R09/R10 ergänzen diese Schritte.

## Abgeleitete Regionen (R08b)

SketchRegionResult enthält positive Randläufe und Regionen mit Außenrand und
unmittelbaren Innenrändern. Außenläufe sind gegen den Uhrzeigersinn orientiert;
Innenränder verwenden die referenzierten Läufe in umgekehrter Richtung.
Region.area subtrahiert ausschließlich die unmittelbaren Löcher. Eine innere
Fläche ist eine eigene, nicht automatisch ausgewählte Region. Vektorindizes
sind keine dauerhaften IDs. R09 führt Identität/Auswahl und Migration ein.
R10 übernimmt den produktiven Face-Weg. Bei ungültiger Topologie liefert der
Regionsvertrag Diagnosen mit Element-IDs, aber keine verwendbaren Regionen.
