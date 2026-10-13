#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include "ViewportContextGuard.hpp"
#include <BRep_Builder.hxx>
#include <Graphic3d_ZLayerId.hxx>
#include <TopoDS_Compound.hxx>

namespace voelund::viewport {
void CadViewport::setEdgePicking(bool enabled) {
    performViewportOperation(*this, impl_->ready, [&] {
        impl_->edgePicking = enabled;
        if (!impl_->ready)
            return;
        impl_->context->ClearSelected(false);
        impl_->setBodiesPickable(!impl_->sketchEditor.isActive());
        if (const auto& chosen = impl_->edgeHighlights[0]; !chosen.IsNull()) {
            impl_->context->Deactivate(chosen);
            if (enabled)
                impl_->context->Activate(chosen, AIS_Shape::SelectionMode(TopAbs_EDGE));
        }
        update();
    });
}

void CadViewport::setEdgeSelectionHandler(
    std::function<void(std::uint64_t, const TopoDS_Shape&, bool)> handler) {
    impl_->edgeSelectionHandler = std::move(handler);
}

void CadViewport::setHighlightedEdges(std::span<const TopoDS_Shape> chosen,
                                      std::span<const TopoDS_Shape> failing) {
    performViewportOperation(*this, impl_->ready, [&] {
        if (!impl_->ready)
            return;
        const std::array<std::span<const TopoDS_Shape>, 2> groups{chosen, failing};
        const std::array<Quantity_Color, 2> colors{
            occtColor(impl_->theme.selection), Quantity_Color(1.0, 0.15, 0.12, Quantity_TOC_RGB)};
        for (std::size_t group = 0; group < groups.size(); ++group) {
            auto& highlight = impl_->edgeHighlights[group];
            if (!highlight.IsNull())
                impl_->context->Remove(highlight, false);
            highlight.Nullify();
            if (groups[group].empty())
                continue;
            TopoDS_Compound edges;
            BRep_Builder builder;
            builder.MakeCompound(edges);
            for (const auto& edge : groups[group])
                builder.Add(edges, edge);
            highlight = new AIS_Shape(edges);
            highlight->SetColor(colors[group]);
            highlight->SetWidth(4.0);
            // Drawn over the body's own edge lines; never picked itself.
            highlight->SetZLayer(Graphic3d_ZLayerId_Topmost);
            impl_->context->Display(highlight, false);
            impl_->context->Deactivate(highlight);
            if (group == 0 && impl_->edgePicking)
                impl_->context->Activate(highlight, AIS_Shape::SelectionMode(TopAbs_EDGE));
        }
        update();
    });
}

void CadViewport::setSketchConstraintEditHandler(std::function<void(std::uint64_t)> handler) {
    impl_->sketchConstraintEditHandler = std::move(handler);
}

void CadViewport::setSketchConstraintSelectionHandler(std::function<void(std::uint64_t)> handler) {
    impl_->sketchConstraintSelectionHandler = std::move(handler);
}

void CadViewport::setSketchBoxSelectionHandler(
    std::function<void(const std::vector<std::uint64_t>&, bool)> handler) {
    impl_->sketchBoxSelectionHandler = std::move(handler);
}

void CadViewport::setSketchElementSelectionHandler(
    std::function<void(const SketchSelectionIntent&)> handler) {
    impl_->sketchElementSelectionHandler = std::move(handler);
}

void CadViewport::setBodySelectionHandler(std::function<void(std::uint64_t)> handler) {
    impl_->bodySelectionHandler = std::move(handler);
}

void CadViewport::setBasePlaneSelectionHandler(std::function<void(BasePlane)> handler) {
    impl_->basePlaneSelectionHandler = std::move(handler);
}

void CadViewport::setEmptySelectionHandler(std::function<void()> handler) {
    impl_->emptySelectionHandler = std::move(handler);
}

void CadViewport::setAreaClickHandler(std::function<bool(const ViewRay&)> handler) {
    impl_->areaClickHandler = std::move(handler);
}

void CadViewport::setPlanarFaceSelectionHandler(
    std::function<void(const PlanarFaceSelection&)> handler) {
    impl_->planarFaceSelectionHandler = std::move(handler);
}

void CadViewport::setSketchEditorPreviewHandler(
    std::function<void(const std::optional<SketchElementIntent>&)> handler) {
    impl_->sketchEditorPreviewHandler = std::move(handler);
}

void CadViewport::setSketchEditorElementHandler(
    std::function<void(const SketchElementIntent&)> handler) {
    impl_->sketchEditorElementHandler = std::move(handler);
}

void CadViewport::setSketchEditorDragEndHandler(std::function<void()> handler) {
    impl_->sketchEditorDragEndHandler = std::move(handler);
}

void CadViewport::setSketchEditorTranslationHandler(std::function<void(double, double)> handler) {
    impl_->sketchEditorTranslationHandler = std::move(handler);
}

void CadViewport::setBodyMoveHandler(
    std::function<void(std::uint64_t, double, double, double)> handler) {
    impl_->manipulator.setMoveHandler(std::move(handler));
}

void CadViewport::setBodyMovePreviewHandler(
    std::function<void(std::uint64_t, double, double, double)> handler) {
    impl_->manipulator.setMovePreviewHandler(std::move(handler));
}

void CadViewport::setBodyRotationHandler(std::function<void(std::uint64_t, int, double)> handler) {
    impl_->manipulator.setRotationHandler(std::move(handler));
}

void CadViewport::setBodyRotationPreviewHandler(
    std::function<void(std::uint64_t, int, double)> handler) {
    impl_->manipulator.setRotationPreviewHandler(std::move(handler));
}
} // namespace voelund::viewport
