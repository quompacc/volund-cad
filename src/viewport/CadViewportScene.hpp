#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include "BasePlanePresentation.hpp"
#include "SketchBoxSelection.hpp"
#include "SketchConstraintPresentation.hpp"
#include "SketchEditorInteraction.hpp"
#include "PlanarFaceSelection.hpp"
#include "SketchPresentation.hpp"
#include "SketchSnapOverlay.hpp"
#include "PushPullHandle.hpp"
#include "TransformManipulator.hpp"
#include "ViewportHighlightStyle.hpp"

#include <AIS_DisplayMode.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_SelectionScheme.hxx>
#include <AIS_Shape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_GradientFillMethod.hxx>
#include <Aspect_Grid.hxx>
#include <Aspect_GridDrawMode.hxx>
#include <Aspect_GridType.hxx>
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
#include <Aspect_PolygonOffsetMode.hxx>
#include <QDebug>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include "SelectionBand.hpp"
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
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace voelund::viewport {

class CadViewportScene {
  public:
    // cancelled: a body drag ended with Escape; the button's release is no click (S14).
    enum class Gesture { none, pan, rotate, moveAxis, pushPull, selectBox, cancelled };

    struct BodyPresentation final {
        std::uint64_t id;
        occ::handle<AIS_Shape> shape;
        bool transformable;
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
    void showEdges(const occ::handle<AIS_Shape>& shape) const {
        shape->Attributes()->SetFaceBoundaryDraw(true);
        shape->Attributes()->SetFaceBoundaryAspect(
            new Prs3d_LineAspect(occtColor(theme.edge), Aspect_TOL_SOLID, 1.0));
    }

    // Colors of the current theme for background, grid, bodies, highlights and planes.
    void applyTheme() {
        configureHighlightStyles(context, theme);
        view->SetBgGradientStyle(Aspect_GradientFillMethod_None, false);
        view->SetBackgroundColor(occtColor(theme.background));
        viewer->Grid()->SetColors(occtColor(theme.gridMinor), occtColor(theme.gridMajor));
        view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, occtColor(theme.axisLabel), 0.075,
                              V3d_ZBUFFER);
        for (const BodyPresentation& presentation : presentations) {
            presentation.shape->SetColor(occtColor(theme.body));
            showEdges(presentation.shape);
            context->Redisplay(presentation.shape, false);
        }
        refreshBasePlanes();
        sketchPresentation.setColors(occtColor(theme.sketchFree), occtColor(theme.sketchDetermined));
        sketchPresentation.select(context, selectedSketchElements);
        sketchConstraintPresentation.setLabelColor(occtColor(theme.sketchLabel));
        sketchConstraintPresentation.setConstraints(context, pendingSketchConstraints);
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
                context->Activate(presentation.shape, presentation.shape->GlobalSelectionMode());
                context->Activate(presentation.shape, AIS_Shape::SelectionMode(TopAbs_FACE));
                context->Activate(presentation.shape, AIS_Shape::SelectionMode(TopAbs_EDGE));
            }
        }
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

        for (const SceneBody& body : pendingBodies) {
            occ::handle<AIS_Shape> presentation = new AIS_Shape(body.shape);
            presentation->SetColor(occtColor(theme.body));
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
            context->Display(presentation, false);
            presentations.push_back(BodyPresentation{body.id, presentation, body.transformable});
        }
        setBodiesPickable(!sketchEditor.isActive());
        sketchPresentation.setElements(context, pendingSketchElements, selectedSketchElements,
                                       sketchEditor.isActive());
        sketchConstraintPresentation.setConstraints(context, pendingSketchConstraints);
        if (activeSketchSupport)
            sketchPresentation.setReferences(context, *activeSketchSupport,
                                             sketchEditor.isActive());
        attachTransformTarget();
        refreshBasePlanes();
        if (isFirstShape && (!presentations.empty() || !basePlanesFitted)) {
            view->FitAll(0.08, false);
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
        for (const BodyPresentation& presentation : presentations) {
            if (presentation.transformable && presentation.id == *transformTarget) {
                manipulator.attach(presentation.shape, presentation.id);
                break;
            }
        }
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

    // V01: the topmost sketch element, or one behind a base plane (planes never steal a
    // click from the sketch); a body in front keeps the click.
    [[nodiscard]] std::optional<std::uint64_t> detectedSketchElement() const {
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

    void notifyDetectedBody(bool includeFace) {
        notifyBodyOwner(context->DetectedOwner(), includeFace);
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
            const auto clickHandler = includeFace ? bodyClickHandler : nullptr;
            std::optional<PlanarFaceSelection> selection;
            if (includeFace && faceHandler) {
                const auto owner = occ::handle<StdSelect_BRepOwner>::DownCast(detectedOwner);
                if (!owner.IsNull())
                    selection = planarFaceSelection(bodyId, owner->Shape());
                if (selection && context->MainSelector()->NbPicked() > 0) {
                    const gp_Pnt point = context->MainSelector()->PickedPoint(1);
                    selection->pickedPoint = std::array<double, 3>{point.X(), point.Y(), point.Z()};
                }
            }
            // The application callback may rebuild presentations; retain no vector reference.
            bodyHandler(bodyId);
            if (selection && faceHandler)
                faceHandler(*selection);
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
    PushPullHandle pushPull; // T04c
    SketchEditorInteraction sketchEditor;
    SketchPresentation sketchPresentation;
    SketchConstraintPresentation sketchConstraintPresentation;
    std::vector<BodyPresentation> presentations;
    std::vector<SceneBody> pendingBodies;
    std::vector<SceneSketchElement> pendingSketchElements;
    std::vector<SceneSketchConstraint> pendingSketchConstraints;
    std::vector<std::uint64_t> selectedSketchElements;
    std::function<void(std::uint64_t)> bodySelectionHandler;
    std::function<void(std::uint64_t)> bodyClickHandler; // M21
    std::function<void(const PlanarFaceSelection&)> planarFaceSelectionHandler;
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
    std::function<void(const std::vector<std::uint64_t>&, bool)> sketchBoxSelectionHandler;
    // M06: edge picking of an open fillet and the highlighted edges.
    bool edgePicking{false};
    std::function<void(std::uint64_t, const TopoDS_Shape&, bool)> edgeSelectionHandler;
    std::array<occ::handle<AIS_Shape>, 2> edgeHighlights;
    std::optional<std::uint64_t> transformTarget;
    QPoint gestureOrigin;
    Gesture gesture{Gesture::none};
    bool sketchElementPressed{false};
    SelectionBand* selectionBand{nullptr};
    bool ready{false};
    ViewportTheme theme;
    SketchSnapOverlay* snapOverlay{nullptr}; // S04a, child widget of the viewport
    std::array<bool, 3> planeVisibility{true, true, true};
    bool gridVisible{true};
    occ::handle<Graphic3d_Camera> savedCamera;
    std::optional<PlanarFaceSelection> activeSketchSupport;
    // S03: the view before the sketch editor oriented it; restored when the editor ends.
    occ::handle<Graphic3d_Camera> sketchReturnCamera;
};

} // namespace voelund::viewport
