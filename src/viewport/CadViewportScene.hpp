// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include "BasePlanePresentation.hpp"
#include "BodyStyle.hpp"
#include "DatumPresentation.hpp"
#include "PlanarFaceSelection.hpp"
#include "PushPullHandle.hpp"
#include "SectionView.hpp"
#include "SketchBoxSelection.hpp"
#include "SketchConstraintPresentation.hpp"
#include "SketchEditorInteraction.hpp"
#include "SketchGlyphOverlay.hpp"
#include "SketchPresentation.hpp"
#include "SketchRegionPresentation.hpp"
#include "SketchSnapOverlay.hpp"
#include "TransformManipulator.hpp"
#include "ViewportHighlightStyle.hpp"

#include "SelectionBand.hpp"
#include <AIS_DisplayMode.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_SelectionScheme.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_GradientFillMethod.hxx>
#include <Aspect_Grid.hxx>
#include <Aspect_GridDrawMode.hxx>
#include <Aspect_GridType.hxx>
#include <Aspect_PolygonOffsetMode.hxx>
#include <Aspect_TypeOfTriedronPosition.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <NCollection_Vec2.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <QDebug>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QWheelEvent>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <SelectMgr_EntityOwner.hxx>
#include <Standard_Failure.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <StdSelect_ViewerSelector3d.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <gp_Pnt.hxx>
#include <optional>
#include <utility>
#include <vector>

namespace voelund::viewport {

class CadViewportScene {
  public:
    // cancelled: a body drag ended with Escape; the button's release is no click (S14).
    enum class Gesture { none, pan, rotate, moveAxis, pushPull, section, selectBox, cancelled };

    struct BodyPresentation final {
        std::uint64_t id;
        occ::handle<AIS_Shape> shape;
        bool transformable;
        std::optional<std::uint32_t> color; // N06
    };

    void configureScene() {
        viewer = new V3d_Viewer(graphicDriver);
        viewer->SetDefaultLights();
        viewer->SetLightOn();
        context = new AIS_InteractiveContext(viewer);
        context->SetDisplayMode(AIS_Shaded, false);
        view = viewer->CreateView();
        view->SetImmediateUpdate(false);
        viewer->SetRectangularGridValues(0.0, 0.0, 10.0, 10.0, 0.0);
        viewer->SetRectangularGridGraphicValues(500.0, 500.0, -0.05);
        applyGrid();
        view->ZBufferTriedronSetup(Quantity_NOC_RED, Quantity_NOC_GREEN, Quantity_NOC_BLUE1, 0.8,
                                   0.05, 12);
        applyTheme();
        view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight);
    }

    // Thin edges keep faces apart on the light theme (ADR 0012 prototype).
    // N06: edges and display mode after the display style.
    void showEdges(const occ::handle<AIS_Shape>& shape) const {
        styleBody(shape, displayStyle, occtColor(theme.edge));
    }
    [[nodiscard]] Quantity_Color bodyColor(const std::optional<std::uint32_t>& color) const {
        return color ? rgbColor(*color) : occtColor(theme.body);
    }
    // N06: every body after the display style; hidden edges need the view's hidden-line mode.
    void applyDisplayStyle() {
        view->SetComputedMode(displayStyle == DisplayStyle::hiddenEdges);
        for (const BodyPresentation& presentation : presentations) {
            showEdges(presentation.shape);
            context->SetDisplayMode(presentation.shape, bodyDisplayMode(displayStyle), false);
            context->Redisplay(presentation.shape, false);
        }
    }

    // User report 2026-10-25: on a scaled screen (200 %) OCCT drew text, markers and lines at
    // half their size. The view's resolution scales them; the sketch labels' own pixel offsets
    // follow the same ratio.
    void applyPixelScale(double ratio) {
        const auto resolution = static_cast<unsigned int>(std::lround(72.0 * ratio));
        if (view->RenderingParams().Resolution == resolution &&
            sketchConstraintPresentation.pixelScale() == ratio)
            return;
        view->ChangeRenderingParams().Resolution = resolution;
        sketchConstraintPresentation.setPixelScale(ratio);
        sketchConstraintPresentation.setConstraints(context, pendingSketchConstraints);
    }

    // Colors of the current theme for background, grid, bodies, highlights and planes.
    void applyTheme() {
        configureHighlightStyles(context, theme);
        view->SetBgGradientStyle(Aspect_GradientFillMethod_None, false);
        view->SetBackgroundColor(occtColor(theme.background));
        view->SetComputedMode(displayStyle == DisplayStyle::hiddenEdges); // N06
        viewer->Grid()->SetColors(occtColor(theme.gridMinor), occtColor(theme.gridMajor));
        view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, occtColor(theme.axisLabel), 0.075,
                              V3d_ZBUFFER);
        for (const BodyPresentation& presentation : presentations) {
            presentation.shape->SetColor(bodyColor(presentation.color));
            showEdges(presentation.shape);
            context->Redisplay(presentation.shape, false);
        }
        refreshBasePlanes();
        sketchPresentation.setColors(occtColor(theme.sketchFree),
                                     occtColor(theme.sketchDetermined));
        sketchPresentation.select(context, selectedSketchElements);
        sketchConstraintPresentation.setLabelColor(occtColor(theme.sketchLabel));
        sketchConstraintPresentation.setConstraints(context, pendingSketchConstraints);
        sketchRegions.show(context, pendingSketchRegions, theme); // W15
    }

    // S14: while the sketch editor is open only sketch geometry is picked; a later feature's
    // body over the sketch (edited from the timeline) must not hide its elements.
    void setBodiesPickable(bool pickable) {
        if (context.IsNull())
            return;
        for (const BodyPresentation& presentation : presentations) {
            context->Deactivate(presentation.shape);
            if (pickable && edgePicking) {
                // M06: only edges while a fillet is open.
                context->Activate(presentation.shape, AIS_Shape::SelectionMode(TopAbs_EDGE));
            } else if (pickable) {
                activateBodyModes(presentation.shape); // W07: as the selection filter allows
            }
        }
    }
    void activateBodyModes(const occ::handle<AIS_Shape>& shape) {
        if (selectionFilter.bodies)
            context->Activate(shape, shape->GlobalSelectionMode());
        if (selectionFilter.faces)
            context->Activate(shape, AIS_Shape::SelectionMode(TopAbs_FACE));
        if (selectionFilter.edges)
            context->Activate(shape, AIS_Shape::SelectionMode(TopAbs_EDGE));
        if (vertexPicking) // N08
            context->Activate(shape, AIS_Shape::SelectionMode(TopAbs_VERTEX));
    }
    // W07: the selection filter on what is shown now.
    void applySelectionFilter() {
        if (context.IsNull())
            return;
        setBodiesPickable(!sketchEditor.isActive() || edgePicking);
        basePlanes.setPickable(context, selectionFilter.planes);
        datums.setPickable(context, selectionFilter.planes);
    }
    // W07: a box from empty space - edges for a tool that takes them, else bodies if allowed.
    [[nodiscard]] bool boxAllowed() const {
        return edgePicking ? static_cast<bool>(edgeBoxHandler)
                           : bodyBoxSelectionHandler && selectionFilter.bodies;
    }
    // N05: the box around the shown bodies sizes the section patch.
    [[nodiscard]] Bnd_Box bodyExtent() const {
        Bnd_Box extent;
        for (const BodyPresentation& presentation : presentations)
            BRepBndLib::Add(presentation.shape->Shape(), extent);
        return extent;
    }
    void showSection(const std::optional<SectionPlane>& plane) {
        section.set(context, plane, bodyExtent(), occtColor(theme.selection));
    }
    // After a new GL context the section patch is shown again.
    void restoreSection() {
        if (section.isShown() && !section.grip().isShown())
            showSection(std::optional<SectionPlane>(section.plane()));
    }
    // T04c, N05: a press on the push/pull or the section arrow starts dragging it.
    bool startGrip(const occ::handle<AIS_InteractiveObject>& detected, int x, int y) {
        for (const auto& [grip, kind] : {std::pair{&pushPull, Gesture::pushPull},
                                         std::pair{&section.grip(), Gesture::section}}) {
            if (grip->matches(detected) && grip->hasActiveMode()) {
                gesture = kind;
                grip->start(x, y, view);
                return true;
            }
        }
        return false;
    }
    [[nodiscard]] PushPullHandle* activeGrip() noexcept {
        if (gesture == Gesture::pushPull)
            return &pushPull;
        return gesture == Gesture::section ? &section.grip() : nullptr;
    }

    void displayPendingBodies() {
        if (!ready) {
            return;
        }
        manipulator.detach();
        sketchPresentation.clear(context);
        sketchConstraintPresentation.clear(context);
        const bool isFirstShape = presentations.empty();
        for (const BodyPresentation& presentation : presentations) {
            context->Remove(presentation.shape, false);
        }
        presentations.clear();
        section.forgetBodies();

        for (const SceneBody& body : pendingBodies) {
            occ::handle<AIS_Shape> presentation = new AIS_Shape(body.shape);
            // VP05: no shine, else a flat face looking at the camera burns out white.
            Graphic3d_MaterialAspect material(Graphic3d_NameOfMaterial_Brass); // the default
            material.SetSpecularColor(Quantity_Color(0.04, 0.04, 0.04, Quantity_TOC_RGB));
            presentation->SetMaterial(material);
            presentation->SetColor(bodyColor(body.color));
            if (body.stale)
                presentation->SetTransparency(0.6);
            // S14: the shaded faces lie a little behind what is drawn exactly on them (the
            // lines and regions of a face sketch, the hover highlight); without it the face
            // hid a sketch on itself.
            if (!presentation->Attributes()->HasOwnShadingAspect())
                presentation->Attributes()->SetupOwnShadingAspect();
            presentation->Attributes()->ShadingAspect()->Aspect()->SetPolygonOffsets(
                Aspect_POM_Fill, 1.0F, 2.0F);
            showEdges(presentation);
            section.clip(presentation, bodyColor(body.color)); // N05
            context->Display(presentation, false);
            presentations.push_back(
                BodyPresentation{body.id, presentation, body.transformable, body.color});
        }
        restoreSection();
        setBodiesPickable(!sketchEditor.isActive() || edgePicking);
        sketchPresentation.setElements(context, pendingSketchElements, selectedSketchElements,
                                       sketchEditor.isActive());
        sketchConstraintPresentation.setConstraints(context, pendingSketchConstraints);
        sketchRegions.show(context, pendingSketchRegions, theme); // W15
        if (activeSketchSupport)
            sketchPresentation.setReferences(context, *activeSketchSupport,
                                             sketchEditor.isActive());
        attachTransformTarget();
        refreshBasePlanes();
        if (isFirstShape && (!presentations.empty() || !basePlanesFitted)) {
            view->FitAll(firstFitMargin(), false);
            view->ZFitAll();
            basePlanesFitted = true;
        }
    }

    // Prominent in an empty scene, faint beside bodies, hidden while sketching.
    void refreshBasePlanes() {
        if (context.IsNull())
            return; // Not initialized yet; the first rebuild shows the planes.
        if (sketchEditor.isActive() || activeSketchSupport) {
            basePlanes.hide(context);
            datums.hide(context);
            return;
        }
        double halfSize = 50.0;
        if (!presentations.empty()) {
            Bnd_Box bounds;
            for (const BodyPresentation& presentation : presentations)
                BRepBndLib::Add(presentation.shape->Shape(), bounds);
            if (!bounds.IsVoid()) {
                double x0, y0, z0, x1, y1, z1;
                bounds.Get(x0, y0, z0, x1, y1, z1);
                halfSize = std::max({25.0, 0.6 * (x1 - x0), 0.6 * (y1 - y0), 0.6 * (z1 - z0)});
            }
        }
        basePlanes.show(context, presentations.empty(), halfSize, theme, planeVisibility);
        datums.show(context, pendingDatums, halfSize, theme);    // M19
        basePlanes.setPickable(context, selectionFilter.planes); // W07
        datums.setPickable(context, selectionFilter.planes);
        showSketchCurves();
    }

    // User reports 2026-10-24/25: the base planes alone fill only part of the view, at the
    // start and on every fit (view cube, Home, F6).
    [[nodiscard]] double firstFitMargin() const {
        return presentations.empty() && basePlanes.displayedCount() > 0 ? 0.45 : 0.08;
    }

    // Sketches shown besides the selected one: plain lines, no clicks.
    std::vector<TopoDS_Shape> pendingShownSketches;
    std::vector<occ::handle<AIS_Shape>> shownSketches;
    void showSketchCurves() {
        if (context.IsNull())
            return;
        for (const auto& curves : shownSketches)
            context->Remove(curves, false);
        shownSketches.clear();
        for (const TopoDS_Shape& shape : pendingShownSketches) {
            occ::handle<AIS_Shape> curves = new AIS_Shape(shape);
            curves->SetColor(occtColor(theme.sketchDetermined));
            curves->SetWidth(1.5);
            context->Display(curves, AIS_WireFrame, -1, false);
            shownSketches.push_back(curves);
        }
    }

    void setTransformToolEnabled(bool enabled) {
        manipulator.setEnabled(enabled);
        attachTransformTarget();
    }

    void attachTransformTarget() {
        manipulator.detach();
        if (!manipulator.isEnabled() || !transformTarget.has_value()) {
            return;
        }
        // N02b: the target first, then the bodies chosen with it.
        std::vector<occ::handle<AIS_Shape>> group;
        for (const BodyPresentation& presentation : presentations)
            if (presentation.transformable && presentation.id == *transformTarget)
                group.insert(group.begin(), presentation.shape);
            else if (presentation.transformable &&
                     std::ranges::find(transformCompanions, presentation.id) !=
                         transformCompanions.end())
                group.push_back(presentation.shape);
        if (!group.empty())
            manipulator.attach(group, *transformTarget);
    }

    void applyGrid() {
        if (gridVisible)
            viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
        else
            viewer->DeactivateGrid();
    }

    // U06: the first detected owner that belongs to a body, even behind a base plane.
    [[nodiscard]] occ::handle<SelectMgr_EntityOwner> detectedBodyOwner() const {
        for (context->InitDetected(); context->MoreDetected(); context->NextDetected()) {
            const auto owner = context->DetectedCurrentOwner();
            if (owner.IsNull())
                continue;
            const auto object = occ::handle<AIS_InteractiveObject>::DownCast(owner->Selectable());
            for (const BodyPresentation& presentation : presentations)
                if (presentation.shape == object)
                    return owner;
        }
        return {};
    }

    // VP02 (CadViewportInput.cpp): the corner or center within 12 logical pixels of a framebuffer
    // point, with its logical pixel; lighting it up; the element a press takes; its pick.
    [[nodiscard]] std::optional<std::pair<SceneSketchPoint, QPointF>>
    sketchPointNear(QPoint framebuffer, double ratio) const;
    void hoverSketchPoint(QPoint framebuffer, double ratio);
    [[nodiscard]] std::optional<std::uint64_t>
    pressedSketchElement(std::optional<std::uint64_t> detected, QPoint framebuffer, double ratio);
    void notifySketchPick(std::uint64_t element, QPoint framebuffer, bool additive, double ratio);

    // V01: the topmost sketch element, or one behind a base plane (planes never steal a
    // click from the sketch); a body in front keeps the click.
    [[nodiscard]] std::optional<std::uint64_t> detectedSketchElement() const {
        if (!context->HasDetected()) // DetectedInteractive() needs a detected owner
            return std::nullopt;
        const auto topmost = context->DetectedInteractive();
        if (const auto id = sketchPresentation.detectedElement(topmost))
            return id;
        if (!basePlanes.plane(topmost).has_value())
            return std::nullopt;
        for (context->InitDetected(); context->MoreDetected(); context->NextDetected()) {
            const auto owner = context->DetectedCurrentOwner();
            if (owner.IsNull())
                continue;
            const auto object = occ::handle<AIS_InteractiveObject>::DownCast(owner->Selectable());
            if (const auto id = sketchPresentation.detectedElement(object))
                return id;
        }
        return std::nullopt;
    }

    // W07: the body owners under the pointer, front to back (an edge before its face).
    [[nodiscard]] std::vector<occ::handle<SelectMgr_EntityOwner>> detectedBodyOwners() const {
        std::vector<occ::handle<SelectMgr_EntityOwner>> owners;
        for (context->InitDetected(); context->MoreDetected(); context->NextDetected()) {
            const auto owner = context->DetectedCurrentOwner();
            if (owner.IsNull())
                continue;
            const auto object = occ::handle<AIS_InteractiveObject>::DownCast(owner->Selectable());
            if (std::ranges::any_of(presentations, [&](const BodyPresentation& presentation) {
                    return presentation.shape == object;
                }))
                owners.push_back(owner);
        }
        return owners;
    }

    // A body edge picked without a tool (user request 2026-10-13), as in any CAD.
    bool pickBodyEdge(const occ::handle<SelectMgr_EntityOwner>& detected, bool additive) {
        const auto edge = occ::handle<StdSelect_BRepOwner>::DownCast(detected);
        if (edge.IsNull() || !edgeSelectionHandler ||
            (edge->Shape().ShapeType() != TopAbs_EDGE &&
             edge->Shape().ShapeType() != TopAbs_VERTEX)) // N08: a corner while measuring
            return false;
        for (const auto& presentation : presentations)
            if (presentation.shape == edge->Selectable()) {
                context->ClearSelected(false);
                const auto handler = edgeSelectionHandler;
                handler(presentation.id, edge->Shape(), additive);
                return true;
            }
        return false;
    }

    void notifyDetectedBody(bool includeFace) {
        notifyBodyOwner(context->DetectedOwner(), includeFace);
    }

    struct FacePick final {
        std::optional<PlanarFaceSelection> planar{};
        std::optional<CurvedFaceSelection> curved{};
    };
    // The face under the mouse: a plane for sketches and tools, or (I04) any other surface.
    [[nodiscard]] FacePick facePick(const occ::handle<SelectMgr_EntityOwner>& detectedOwner,
                                    std::uint64_t bodyId) const {
        FacePick pick;
        const auto owner = occ::handle<StdSelect_BRepOwner>::DownCast(detectedOwner);
        if (owner.IsNull())
            return pick;
        pick.planar = planarFaceSelection(bodyId, owner->Shape());
        if (!pick.planar && owner->Shape().ShapeType() == TopAbs_FACE)
            pick.curved = CurvedFaceSelection{bodyId, owner->Shape()};
        if (pick.planar && context->MainSelector()->NbPicked() > 0) {
            const gp_Pnt point = context->MainSelector()->PickedPoint(1);
            pick.planar->pickedPoint = std::array<double, 3>{point.X(), point.Y(), point.Z()};
        }
        return pick;
    }

    void notifyBodyOwner(const occ::handle<SelectMgr_EntityOwner>& detectedOwner,
                         bool includeFace) {
        if (detectedOwner.IsNull())
            return;
        const auto detected =
            occ::handle<AIS_InteractiveObject>::DownCast(detectedOwner->Selectable());
        for (const BodyPresentation& presentation : presentations) {
            if (presentation.shape != detected || !bodySelectionHandler)
                continue;
            const auto bodyId = presentation.id;
            const auto bodyHandler = bodySelectionHandler;
            const auto faceHandler = planarFaceSelectionHandler;
            const auto curvedHandler = curvedFaceSelectionHandler;
            const auto clickHandler = includeFace ? bodyClickHandler : nullptr;
            const auto [selection, curved] =
                includeFace && faceHandler ? facePick(detectedOwner, bodyId) : FacePick{};
            // The application callback may rebuild presentations; retain no vector reference.
            bodyHandler(bodyId);
            if (selection && faceHandler)
                faceHandler(*selection);
            if (curved && curvedHandler)
                curvedHandler(*curved);
            if (clickHandler)
                clickHandler(bodyId);
            return;
        }
    }

    occ::handle<Graphic3d_GraphicDriver> graphicDriver;
    occ::handle<V3d_Viewer> viewer;
    occ::handle<AIS_InteractiveContext> context;
    occ::handle<V3d_View> view;
    TransformManipulator manipulator;
    PushPullHandle pushPull;                                  // T04c
    DisplayStyle displayStyle{DisplayStyle::shadedWithEdges}; // N06
    SectionView section;                                      // N05
    SketchEditorInteraction sketchEditor;
    SketchPresentation sketchPresentation;
    SketchConstraintPresentation sketchConstraintPresentation;
    std::vector<BodyPresentation> presentations;
    std::vector<SceneBody> pendingBodies;
    std::vector<SceneSketchElement> pendingSketchElements;
    SketchRegionPresentation sketchRegions; // W15
    std::vector<SceneSketchRegion> pendingSketchRegions;
    std::vector<SceneSketchConstraint> pendingSketchConstraints;
    std::vector<std::uint64_t> selectedSketchElements;
    std::function<void(std::uint64_t)> bodySelectionHandler;
    std::function<void(std::uint64_t)> bodyClickHandler; // M21
    std::function<void(const PlanarFaceSelection&)> planarFaceSelectionHandler;
    std::function<void(const CurvedFaceSelection&)> curvedFaceSelectionHandler;
    std::function<void(BasePlane)> basePlaneSelectionHandler;
    std::function<void()> emptySelectionHandler;
    std::function<bool(const ViewRay&)> areaClickHandler;
    BasePlanePresentation basePlanes;
    bool basePlanesFitted{false};
    std::function<void(const std::optional<SketchElementIntent>&)> sketchEditorPreviewHandler;
    std::function<void(const SketchElementIntent&)> sketchEditorElementHandler;
    std::function<void(double, double)> sketchEditorTranslationHandler;
    std::function<void()> sketchEditorDragEndHandler;
    std::function<void(const SketchSelectionIntent&)> sketchElementSelectionHandler;
    std::function<void(std::uint64_t)> sketchConstraintEditHandler;
    std::function<void(std::uint64_t)> sketchConstraintSelectionHandler;
    std::function<void(QPoint)> sketchContextHandler;                                    // W20
    std::function<void(std::uint64_t, double, double, bool)> sketchDimensionDragHandler; // W21
    struct DimensionDrag final {
        std::uint64_t id{0};
        QPoint origin{};
        bool moving{false};
    } dimensionDrag; // W21
    std::function<void(const std::vector<std::uint64_t>&, bool)> sketchBoxSelectionHandler;
    // W07: a box over bodies outside a sketch.
    bool bodyBox{false};
    std::function<void(const std::vector<std::uint64_t>&, bool)> bodyBoxSelectionHandler;
    // M06: edge picking of an open fillet and the highlighted edges.
    bool edgePicking{false};
    std::function<void(std::uint64_t, const TopoDS_Shape&, bool)> edgeSelectionHandler;
    std::function<void(std::uint64_t, const TopoDS_Shape&)> edgeChainHandler;            // W07
    std::function<void(std::uint64_t, const std::vector<TopoDS_Shape>&)> edgeBoxHandler; // W07
    std::pair<std::uint64_t, TopoDS_Shape> lastPickedEdge{0, {}};                        // W07
    bool swallowRelease{false}; // W07: the release after a chaining double click
    // W07: the left releases so far and where the last click chose a body candidate.
    std::uint64_t releases{0};
    struct ClickCycle final {
        QPoint at{};
        gp_Pnt eye{};
        std::size_t index{0};
        std::uint64_t release{0};
        std::uint64_t scene{0}; // the scene the click left (bodies, sketches, planes, picking)
    } clickCycle;
    std::uint64_t sceneChanges{0};
    SelectionFilter selectionFilter; // W07
    bool vertexPicking{false};       // N08
    std::function<bool()> clickCycleAllowed;
    // The click right after the last one, at its place, with the view unchanged.
    [[nodiscard]] bool repeatsClick(QPoint position, const gp_Pnt& eye) const {
        return clickCycle.release != 0 && clickCycle.release + 1 == releases &&
               clickCycle.scene == sceneChanges &&
               (position - clickCycle.at).manhattanLength() <= 3 &&
               eye.Distance(clickCycle.eye) < 1e-9 && (!clickCycleAllowed || clickCycleAllowed());
    }
    std::array<occ::handle<AIS_Shape>, 2> edgeHighlights;
    std::optional<std::uint64_t> transformTarget;
    std::vector<std::uint64_t> transformCompanions; // N02b
    Qt::KeyboardModifiers pressModifiers;           // N02b
    QPoint gestureOrigin;
    Gesture gesture{Gesture::none};
    bool sketchElementPressed{false};
    SelectionBand* selectionBand{nullptr};
    bool ready{false};
    ViewportTheme theme;
    SketchSnapOverlay* snapOverlay{nullptr};   // S04a, child widget of the viewport
    SketchGlyphOverlay* glyphOverlay{nullptr}; // W18, child widget of the viewport
    std::array<bool, 3> planeVisibility{true, true, true};
    DatumPresentation datums;              // M19
    std::vector<SceneDatum> pendingDatums; // M19
    std::function<void(std::uint64_t)> datumSelectionHandler;
    bool gridVisible{true};
    occ::handle<Graphic3d_Camera> savedCamera;
    std::optional<PlanarFaceSelection> activeSketchSupport;
    // D01: where the pointer was while drawing (framebuffer pixels) and its logical position.
    QPoint sketchPointer;
    void showDynamicFields(QPointF logical) {
        if (snapOverlay == nullptr)
            return;
        snapOverlay->showFields(sketchEditor.dynamicInput(), logical,
                                sketchEditor.fieldLines(sketchPointer.x(), sketchPointer.y()));
        snapOverlay->showOutlines(sketchEditor.stageOutline(sketchPointer.x(), sketchPointer.y()));
    }
    // S03: the view before the sketch editor oriented it; restored when the editor ends.
    occ::handle<Graphic3d_Camera> sketchReturnCamera;
};

} // namespace voelund::viewport
