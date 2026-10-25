// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/document/EdgeReference.hpp>

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/SketchDynamicInput.hpp>

#include <QColor>

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include <QOpenGLWidget>
#else
#include <QWidget>
#endif

#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

class QMouseEvent;
class QPaintEngine;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

namespace voelund::viewport {

struct SceneBody final {
    std::uint64_t id;
    TopoDS_Shape shape;
    bool transformable;
    bool stale{false}; // U10: last valid geometry of a model with errors, drawn faded
    std::optional<std::uint32_t> color{}; // N06: 0xRRGGBB; none: the theme's body colour
};

// M19: a construction plane (origin, normal, x direction) or axis (a point, its direction as
// normal) to show; `id` is its feature.
struct SceneDatum final {
    std::uint64_t id;
    bool axis;
    std::array<double, 3> origin;
    std::array<double, 3> normal;
    std::array<double, 3> xDirection;
};

/** Read-only lifecycle counters for diagnostics and native desktop tests. */
struct ViewportLifecycleDiagnostics final {
    std::uint64_t contextGeneration;
    std::uint64_t cleanupGeneration;
};

enum class SketchPointKind {
    start,
    end,
    center,
    midpoint,
    origin,
    xAxis,
    yAxis,
    projected,
    onObject
};

struct SceneSketchPoint final {
    std::uint64_t elementId;
    SketchPointKind kind;
    double localX;
    double localY;
    double worldX;
    double worldY;
    double worldZ;
    std::uint64_t supportReferenceId{0};
    double supportReferenceParameter{0.0};
    double radialRadius{0.0};
    std::optional<document::ProjectedEdgeReference> projectedEdge{};
};

// W15: a closed area of the selected sketch, shown lightly filled so it can be seen and picked
// (a click into it goes the usual way, areaClickHandler). `key` names the region.
struct SceneSketchRegion final {
    std::string key;
    TopoDS_Face face;
    bool selected{false};
};

struct SceneSketchElement final {
    std::uint64_t id;
    TopoDS_Shape shape;
    bool conflicting;
    bool construction;
    std::vector<SceneSketchPoint> points;
    bool determined{false}; // No degree of freedom left.

    SceneSketchElement(std::uint64_t elementId, TopoDS_Shape elementShape, bool hasConflict = false,
                       bool isConstruction = false, std::vector<SceneSketchPoint> snapPoints = {},
                       bool isDetermined = false)
        : id(elementId), shape(std::move(elementShape)), conflicting(hasConflict),
          construction(isConstruction), points(std::move(snapPoints)), determined(isDetermined) {}
};

struct SketchPointReferenceIntent final {
    std::uint64_t elementId;
    SketchPointKind point;
    std::uint64_t supportReferenceId{0};
    double supportReferenceParameter{0.0};
    std::optional<document::ProjectedEdgeReference> projectedEdge{};
};

struct SketchSelectionIntent final {
    std::uint64_t elementId;
    double localX;
    double localY;
    bool additive;
};

// W12 (as in Fusion and FreeCAD): a dimension drawn as in a technical drawing, in world
// coordinates (features::SketchDimensionShape describes the parts).
enum class SceneDimensionStyle { linear, radius, diameter, angle };
struct SceneDimension final {
    SceneDimensionStyle style;
    gp_Pnt first;
    gp_Pnt second;
    gp_Pnt vertex;
    gp_Dir along;
    gp_Dir outward;
    gp_Dir normal;
    double offset;
    std::string text;
    int textBeside{0}; // +1 right of an upright dimension line, -1 left, 0 above
};

struct SceneSketchConstraint final {
    std::uint64_t id;
    std::string label;
    double x;
    double y;
    double z;
    bool conflicting;
    bool editable;
    bool redundant{false};                     // S07: implied by other relations
    bool selected{false};                      // S13: picked at its label
    std::optional<SceneDimension> dimension{}; // W12: lines and arrows; the label sits on it
};

struct PlanarReferenceSegment final {
    double firstX;
    double firstY;
    double secondX;
    double secondY;
    double firstWorldX;
    double firstWorldY;
    double firstWorldZ;
    double secondWorldX;
    double secondWorldY;
    double secondWorldZ;
    std::uint64_t referenceId{0};
};

struct PlanarFaceSelection final {
    std::uint64_t bodyId;
    double originX;
    double originY;
    double originZ;
    double normalX;
    double normalY;
    double normalZ;
    double xDirectionX;
    double xDirectionY;
    double xDirectionZ;
    std::vector<PlanarReferenceSegment> referenceSegments{};
    // Transient picked topology, checked against current application provenance.
    TopoDS_Shape selectedFace{};
    bool stableReferences{false};
    std::vector<SceneSketchPoint> referencePoints{};
    // M08: where the click hit the face (model coordinates), when picked with the mouse.
    std::optional<std::array<double, 3>> pickedPoint{};
};

[[nodiscard]] std::optional<PlanarFaceSelection>
resolvePlanarFaceSelection(const PlanarFaceSelection& previous, const TopoDS_Shape& supportShape);

enum class SketchElementTool {
    select,
    line,
    polyline,
    rectangle,
    circle,
    arc,
    threePointArc,
    ellipse,
    slot,
    polygon,
    centerRectangle, // S04a: first click is the center, second a corner
    point            // S15: one click
};

enum class SketchAutomaticConstraint {
    horizontal,
    vertical,
    parallel,
    perpendicular,
    pointOnObject,
    tangent,
    equal,
    concentric
};

enum class CameraView { isometric, top, front, right, left, back, bottom };

// Camera orientation in model coordinates: `toEye` points from the viewed center towards the
// eye (OCCT's projection vector), `up` is the screen's up direction.
struct CameraFrame final {
    double toEye[3];
    double up[3];
};

// A click as a ray in model coordinates (from the eye into the scene).
struct ViewRay final {
    double origin[3];
    double direction[3];
    bool additive; // Ctrl held
    // S14: distance along the ray to the body face the click hit; a sketch region behind it
    // is hidden, one on it (a face sketch) or in front of it wins.
    double nearestSurface{std::numeric_limits<double>::infinity()};
};

// U03: colors of the 3D view, supplied by the desktop theme (ADR 0012). Defaults are the
// light theme.
struct ViewportTheme final {
    QColor background{0xE9, 0xEC, 0xEF};
    QColor gridMinor{0xD6, 0xDB, 0xE0};
    QColor gridMajor{0xBA, 0xC2, 0xCB};
    QColor body{0xB4, 0xBF, 0xCB};
    QColor edge{0x6E, 0x79, 0x86};
    QColor hover{0xF0, 0xA0, 0x6E};
    QColor selection{0xD9, 0x62, 0x2B};
    QColor axisLabel{0x5F, 0x68, 0x73};
    // Sketch geometry by determination (user decision 2026-10-05): still free, fully
    // determined. Conflicts stay red, selection orange; construction geometry is dashed.
    QColor sketchFree{0x1F, 0x6F, 0xEB};
    QColor sketchDetermined{0x1B, 0x20, 0x26};
    // U11: constraint labels, readable on the background.
    QColor sketchLabel{0x0B, 0x4F, 0x9C};
};
// Base planes offered in the viewport (V01b); values match document::DatumPlane.
enum class BasePlane { xy = 1, xz = 2, yz = 3 };
// N06 (user decision 2026-10-23): how bodies are drawn; a view setting, not in the document.
enum class DisplayStyle { shaded, shadedWithEdges, wireframe, hiddenEdges };
// N05: a section plane; the side its normal points to stays visible.
struct SectionPlane final {
    std::array<double, 3> origin;
    std::array<double, 3> normal;
    bool operator==(const SectionPlane&) const = default;
};

struct SketchElementIntent final {
    double firstX;
    double firstY;
    double secondX;
    double secondY;
    std::optional<std::pair<double, double>> third;
    std::optional<SketchPointReferenceIntent> firstReference;
    std::optional<SketchPointReferenceIntent> secondReference;
    std::optional<SketchPointReferenceIntent> thirdReference;
    std::optional<SketchAutomaticConstraint> automaticConstraint;
    std::optional<std::uint64_t> automaticTargetId;
    std::vector<SketchTypedDimension> typed{}; // D01: values typed while drawing
    // W13b: a rectangle's corner radius and frame thickness from their steps (thickness > 0
    // inwards, < 0 outwards); none when that option is off.
    std::optional<double> cornerRadius{};
    std::optional<double> frameWidth{};
};

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
using CadViewportBase = QOpenGLWidget;
#else
using CadViewportBase = QWidget;
#endif

class CadViewport final : public CadViewportBase {
  public:
    explicit CadViewport(QWidget* parent = nullptr);
    ~CadViewport() override;

    void setShape(const TopoDS_Shape& shape);
    void setBodies(std::span<const SceneBody> bodies);
    void setSketchRegions(std::span<const SceneSketchRegion> regions); // W15
    void setSketchElements(std::span<const SceneSketchElement> elements,
                           std::optional<std::uint64_t> selectedId = std::nullopt);
    void
    setSketchElementSelectionHandler(std::function<void(const SketchSelectionIntent&)> handler);
    void setSelectedSketchElement(std::optional<std::uint64_t> elementId);
    void setSelectedSketchElements(std::span<const std::uint64_t> elementIds);
    void setSketchConstraints(std::span<const SceneSketchConstraint> constraints);
    // S13: projected geometry of the support (drawn and snapped) on or off; view state.
    void setSketchProjectedGeometryVisible(bool visible);
    // U11: fits the open sketch into the part of the view rail and panel leave free.
    void fitSketchIntoFreeArea();
    void setSketchConstraintEditHandler(std::function<void(std::uint64_t)> handler);
    // S13: a single click on a constraint label where no element lies.
    void setSketchConstraintSelectionHandler(std::function<void(std::uint64_t)> handler);
    // S13: the whole elements in a selection box (window or crossing); additive with Ctrl.
    void setSketchBoxSelectionHandler(
        std::function<void(const std::vector<std::uint64_t>&, bool additive)> handler);
    void setBodySelectionHandler(std::function<void(std::uint64_t)> handler);
    // W07 (as in Fusion): a drag from empty space outside a sketch selects bodies; left to
    // right those wholly inside, right to left those it touches. Reports the bodies' ids (as
    // SceneBody::id) and whether Ctrl was held.
    void setBodyBoxSelectionHandler(
        std::function<void(const std::vector<std::uint64_t>&, bool additive)> handler);
    // M21: a completed click on a body (on release, once per click); the selection handler
    // above also runs on press so that a drag can start.
    void setBodyClickHandler(std::function<void(std::uint64_t)> handler);
    void setPlanarFaceSelectionHandler(std::function<void(const PlanarFaceSelection&)> handler);
    void setBasePlaneSelectionHandler(std::function<void(BasePlane)> handler);
    // M19: construction planes and axes; a click reports the feature.
    void setDatums(std::span<const SceneDatum> datums);
    // User report 2026-10-24 (as in Fusion): sketches shown besides the selected one, as plain
    // lines that take no clicks (one shape per sketch).
    void setShownSketches(std::span<const TopoDS_Shape> sketches);
    // W13b: the rectangle tool's options; with them a rectangle is drawn in further steps.
    void setSketchRectangleOptions(bool rounded, bool frame);
    // User report 2026-10-25: where a tool's arrow stands on screen (logical pixels), for the
    // value field beside it; none without an arrow.
    [[nodiscard]] std::optional<QPoint> toolGripPoint() const;
    [[nodiscard]] std::size_t shownSketchCount() const noexcept;
    void setDatumSelectionHandler(std::function<void(std::uint64_t)> handler);
    [[nodiscard]] std::size_t displayedDatumCount() const noexcept;
    // A click outside the sketch editor that hits nothing.
    void setEmptySelectionHandler(std::function<void()> handler);
    // A click that hits nothing or only a base plane; returning true consumes it (e.g. the
    // area of a sketch region was picked, user report 2026-10-05).
    void setAreaClickHandler(std::function<bool(const ViewRay&)> handler);
    void setSketchEditorPreviewHandler(
        std::function<void(const std::optional<SketchElementIntent>&)> handler);
    void setSketchEditorElementHandler(std::function<void(const SketchElementIntent&)> handler);
    void setSketchEditorTranslationHandler(std::function<void(double, double)> handler);
    // U09: pointer-up after dragging sketch elements; cancelSketchElementDrag ends a running
    // drag without that notification (Escape).
    void setSketchEditorDragEndHandler(std::function<void()> handler);
    // D01: resolves "=Name*2" typed into a field at the pointer (N01 expressions).
    void setSketchDynamicInputEvaluator(SketchDynamicInput::Evaluator evaluator);
    [[nodiscard]] bool cancelSketchElementDrag();
    void beginSketchEditor(const PlanarFaceSelection& support, SketchElementTool tool);
    void setSketchEditorTool(SketchElementTool tool);
    // S04: whether drawing binds snapped points and adds suggested relations.
    void setSketchAutomaticRelations(bool enabled);
    void continueSketchPolyline(double x, double y, std::uint64_t elementId);
    [[nodiscard]] bool cancelSketchEditorStep();
    void cancelSketch();
    void setBodyMoveHandler(std::function<void(std::uint64_t, double, double, double)> handler);
    void
    setBodyMovePreviewHandler(std::function<void(std::uint64_t, double, double, double)> handler);
    void setBodyRotationHandler(std::function<void(std::uint64_t, int, double)> handler);
    void setBodyRotationPreviewHandler(std::function<void(std::uint64_t, int, double)> handler);
    void setTransformToolEnabled(bool enabled);
    void setTransformTarget(std::optional<std::uint64_t> bodyId);
    // N02b: bodies that move with the transform target (a group only moves).
    void setTransformCompanions(std::vector<std::uint64_t> bodyIds);
    // N02b: the keyboard modifiers of the last mouse press in the view.
    [[nodiscard]] Qt::KeyboardModifiers pressModifiers() const noexcept;
    void setTransformSnapping(bool enabled, double linearStep, double angularStep);
    // T04c: push/pull on a picked planar face: a highlighted copy of the face with an arrow
    // along its outward normal. The handler gets the distance while the arrow is dragged and
    // once more when the drag ends; a typed distance moves copy and arrow.
    void beginPushPull(const PlanarFaceSelection& face);
    void setPushPullDistance(double distance);
    void endPushPull();
    void setPushPullHandler(std::function<void(double, bool)> handler);
    // User request 2026-10-13: the same grip on edges (a fillet radius or chamfer distance):
    // a copy of `edges` with an arrow along `direction`, dragged like the push/pull arrow;
    // setPushPullDistance moves it, endPushPull removes it, the push/pull handler reports.
    void beginEdgeGrip(const TopoDS_Shape& edges, const std::array<double, 3>& direction);
    // M06: while a fillet is open, bodies offer only their edges; otherwise edges and faces
    // (user request 2026-10-13: edges are picked before a tool, as in any CAD; near an edge
    // the edge wins). A click on an edge reports the body (its source feature), the edge and
    // whether Ctrl was held. Highlighted edges are drawn over the body: chosen ones in the
    // selection color, failing ones red. While a fillet is open a chosen edge stays clickable
    // over the preview and is reported with body 0.
    void setEdgePicking(bool enabled);
    void setEdgeSelectionHandler(
        std::function<void(std::uint64_t, const TopoDS_Shape&, bool additive)> handler);
    void setHighlightedEdges(std::span<const TopoDS_Shape> chosen,
                             std::span<const TopoDS_Shape> failing = {});
    void setTheme(const ViewportTheme& theme);
    // U06: view state from the model tree; no document change. Index 0 = XY, 1 = XZ, 2 = YZ.
    void setBasePlaneVisibility(std::array<bool, 3> visible);
    void setGridVisible(bool visible);
    void setDisplayStyle(DisplayStyle style); // N06
    [[nodiscard]] DisplayStyle displayStyle() const noexcept;
    [[nodiscard]] bool isGridVisible() const noexcept;
    // N05: a section through the bodies, a pure view state; nullopt shows the whole model.
    // Dragging its arrow moves the plane and reports it, `finished` on release.
    void setSection(const std::optional<SectionPlane>& plane);
    void setSectionHandler(std::function<void(const SectionPlane&, bool finished)> handler);
    void setCameraView(CameraView view);
    // View cube: read and set any direction (center and zoom stay), orbit and roll.
    [[nodiscard]] CameraFrame cameraFrame() const;
    void setCameraFrame(const CameraFrame& frame);
    void orbitBy(double horizontalRadians, double verticalRadians);
    void rollView(double radians);
    void fitView();
    [[nodiscard]] bool isRenderingReady() const noexcept;
    [[nodiscard]] std::size_t displayedSketchElementCount() const noexcept;
    [[nodiscard]] std::size_t displayedSketchRegionCount() const noexcept;
    [[nodiscard]] std::optional<std::string> hoveredSketchRegion() const;
    [[nodiscard]] std::size_t displayedSketchConstraintCount() const noexcept;
    [[nodiscard]] std::size_t displayedBasePlaneCount() const noexcept;
    [[nodiscard]] std::pair<int, int> renderingPixelSize() const noexcept;
    [[nodiscard]] ViewportLifecycleDiagnostics lifecycleDiagnostics() const noexcept;

  protected:
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;
#else
    [[nodiscard]] QPaintEngine* paintEngine() const override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
#endif
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    // D01: Tab and the keys of the fields at the pointer go to the fields while drawing.
    bool event(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

  private:
    friend class ViewportTestAccess;
    [[nodiscard]] bool takesDynamicKey(const QKeyEvent& event) const;
    bool handleDynamicKey(const QKeyEvent& event);
    void startBodyBox(const QMouseEvent& event);
    bool finishBodyBox(const QMouseEvent& event);
#if defined(VOELUND_VIEWPORT_QT_OPENGL)
    void cleanupGlResources() noexcept;
#endif
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace voelund::viewport
