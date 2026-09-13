#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include "SketchBoxSelection.hpp"
#include "SketchConstraintPresentation.hpp"
#include "SketchEditorInteraction.hpp"
#include "SketchInteraction.hpp"
#include "SketchPresentation.hpp"
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
#include <Graphic3d_Camera.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <NCollection_Vec2.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <QDebug>
#include <QMetaObject>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QRubberBand>
#include <QWheelEvent>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>
#include <Standard_Failure.hxx>
#include <StdSelect_BRepOwner.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_TypeOfOrientation.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace voelund::viewport {

class CadViewportScene {
  public:
    enum class Gesture { none, pan, rotate, moveAxis, selectBox };

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
        configureHighlightStyles(context);
        view = viewer->CreateView();
        view->SetImmediateUpdate(false);
        view->SetBgGradientColors(Quantity_Color(0.12, 0.15, 0.13, Quantity_TOC_RGB),
                                  Quantity_Color(0.035, 0.05, 0.04, Quantity_TOC_RGB),
                                  Aspect_GradientFillMethod_Vertical, false);
        viewer->SetRectangularGridValues(0.0, 0.0, 10.0, 10.0, 0.0);
        viewer->SetRectangularGridGraphicValues(500.0, 500.0, -0.05);
        viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
        viewer->Grid()->SetColors(Quantity_Color(0.19, 0.23, 0.19, Quantity_TOC_RGB),
                                  Quantity_Color(0.34, 0.31, 0.16, Quantity_TOC_RGB));
        view->ZBufferTriedronSetup(Quantity_NOC_RED, Quantity_NOC_GREEN, Quantity_NOC_BLUE1, 0.8,
                                   0.05, 12);
        view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER,
                              Quantity_Color(0.82, 0.69, 0.30, Quantity_TOC_RGB), 0.075,
                              V3d_ZBUFFER);
        view->SetProj(V3d_TypeOfOrientation_Zup_AxoRight);
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
            presentation->SetColor(Quantity_Color(0.72, 0.78, 0.88, Quantity_TOC_RGB));
            context->Display(presentation, false);
            context->Activate(presentation, AIS_Shape::SelectionMode(TopAbs_FACE));
            presentations.push_back(BodyPresentation{body.id, presentation, body.transformable});
        }
        sketchPresentation.setElements(context, pendingSketchElements, selectedSketchElements,
                                       sketchEditor.isActive());
        sketchConstraintPresentation.setConstraints(context, pendingSketchConstraints);
        if (activeSketchSupport)
            sketchPresentation.setReferences(context, *activeSketchSupport,
                                             sketchEditor.isActive());
        attachTransformTarget();
        if (isFirstShape) {
            view->FitAll(0.08, false);
            view->ZFitAll();
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
        for (const BodyPresentation& presentation : presentations) {
            if (presentation.transformable && presentation.id == *transformTarget) {
                manipulator.attach(presentation.shape, presentation.id);
                break;
            }
        }
    }

    void notifyDetectedBody(bool includeFace) {
        const auto detected = context->DetectedInteractive();
        for (const BodyPresentation& presentation : presentations) {
            if (presentation.shape != detected || !bodySelectionHandler)
                continue;
            const auto bodyId = presentation.id;
            const auto bodyHandler = bodySelectionHandler;
            const auto faceHandler = planarFaceSelectionHandler;
            std::optional<PlanarFaceSelection> selection;
            if (includeFace && faceHandler) {
                const auto owner =
                    occ::handle<StdSelect_BRepOwner>::DownCast(context->DetectedOwner());
                if (!owner.IsNull())
                    selection = planarFaceSelection(bodyId, owner->Shape());
            }
            // The application callback may rebuild presentations; retain no vector reference.
            bodyHandler(bodyId);
            if (selection && faceHandler)
                faceHandler(*selection);
            return;
        }
    }

    occ::handle<Graphic3d_GraphicDriver> graphicDriver;
    occ::handle<V3d_Viewer> viewer;
    occ::handle<AIS_InteractiveContext> context;
    occ::handle<V3d_View> view;
    TransformManipulator manipulator;
    SketchInteraction sketch;
    SketchEditorInteraction sketchEditor;
    SketchPresentation sketchPresentation;
    SketchConstraintPresentation sketchConstraintPresentation;
    std::vector<BodyPresentation> presentations;
    std::vector<SceneBody> pendingBodies;
    std::vector<SceneSketchElement> pendingSketchElements;
    std::vector<SceneSketchConstraint> pendingSketchConstraints;
    std::vector<std::uint64_t> selectedSketchElements;
    std::function<void(std::uint64_t)> bodySelectionHandler;
    std::function<void(const PlanarFaceSelection&)> planarFaceSelectionHandler;
    std::function<void(const SketchRectangleResult&)> sketchRectangleHandler;
    std::function<void(const std::optional<SketchElementIntent>&)> sketchEditorPreviewHandler;
    std::function<void(const SketchElementIntent&)> sketchEditorElementHandler;
    std::function<void(double, double)> sketchEditorTranslationHandler;
    std::function<void(const SketchSelectionIntent&)> sketchElementSelectionHandler;
    std::function<void(std::uint64_t)> sketchConstraintEditHandler;
    std::optional<std::uint64_t> transformTarget;
    QPoint gestureOrigin;
    Gesture gesture{Gesture::none};
    bool sketchElementPressed{false};
    QRubberBand* selectionBand{nullptr};
    bool ready{false};
    occ::handle<Graphic3d_Camera> savedCamera;
    std::optional<PlanarFaceSelection> activeSketchSupport;
};

} // namespace voelund::viewport
