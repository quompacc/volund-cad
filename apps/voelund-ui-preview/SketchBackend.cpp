#include "SketchBackend.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/ModelDocument.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace voelund::preview {
namespace {

application::SketchDrawingTool drawingTool(PreviewCanvas::SketchTool tool)
{
    switch (tool) {
    case PreviewCanvas::SketchTool::Line: return application::SketchDrawingTool::line;
    case PreviewCanvas::SketchTool::Polyline: return application::SketchDrawingTool::polyline;
    case PreviewCanvas::SketchTool::Rectangle: return application::SketchDrawingTool::rectangle;
    case PreviewCanvas::SketchTool::Circle: return application::SketchDrawingTool::circle;
    case PreviewCanvas::SketchTool::Arc: return application::SketchDrawingTool::arc;
    case PreviewCanvas::SketchTool::ThreePointArc:
        return application::SketchDrawingTool::threePointArc;
    case PreviewCanvas::SketchTool::Ellipse: return application::SketchDrawingTool::ellipse;
    case PreviewCanvas::SketchTool::Slot: return application::SketchDrawingTool::slot;
    case PreviewCanvas::SketchTool::Polygon: return application::SketchDrawingTool::polygon;
    default: return application::SketchDrawingTool::select;
    }
}

document::SketchPointPosition documentPoint(viewport::SketchPointKind point)
{
    if (point == viewport::SketchPointKind::end) return document::SketchPointPosition::end;
    if (point == viewport::SketchPointKind::center) return document::SketchPointPosition::center;
    return document::SketchPointPosition::start;
}

viewport::SketchPointKind scenePoint(document::SketchPointPosition point)
{
    if (point == document::SketchPointPosition::end) return viewport::SketchPointKind::end;
    if (point == document::SketchPointPosition::center) return viewport::SketchPointKind::center;
    return viewport::SketchPointKind::start;
}

void addCoincidence(application::SketchEditorSession& editor,
                    document::SketchGeometryReference created,
                    const std::optional<viewport::SketchPointReferenceIntent>& target,
                    document::SketchPoint targetPoint)
{
    if (!target.has_value()) return;
    if (target->point == viewport::SketchPointKind::midpoint) {
        static_cast<void>(editor.addMidpointReference(
            created, document::SketchElementId{target->elementId}));
    } else if (target->point == viewport::SketchPointKind::onObject) {
        static_cast<void>(editor.addPointOnObjectReference(
            created, document::SketchElementId{target->elementId}));
    } else if (target->elementId == 0) {
        if (target->point == viewport::SketchPointKind::projected) {
            static_cast<void>(editor.addSupportReference(
                created, targetPoint, target->supportReferenceId,
                target->supportReferenceParameter));
        } else {
            const auto axis = target->point == viewport::SketchPointKind::origin
                                  ? application::SketchAxisReference::origin
                              : target->point == viewport::SketchPointKind::xAxis
                                  ? application::SketchAxisReference::xAxis
                                  : application::SketchAxisReference::yAxis;
            static_cast<void>(editor.addAxisReference(created, axis));
        }
    } else {
        static_cast<void>(editor.addCoincidentReferences(
            created, {document::SketchElementId{target->elementId}, documentPoint(target->point)}));
    }
}

std::string constraintLabel(const features::SketchConstraintAnnotation& annotation)
{
    using document::SketchConstraintKind;
    if (annotation.kind == SketchConstraintKind::horizontal) return "[H]";
    if (annotation.kind == SketchConstraintKind::vertical) return "[V]";
    if (annotation.kind == SketchConstraintKind::coincident) return "[C]";
    if (annotation.kind == SketchConstraintKind::parallel) return "[||]";
    if (annotation.kind == SketchConstraintKind::perpendicular) return "[90]";
    if (annotation.kind == SketchConstraintKind::equal) return "[=]";
    if (annotation.kind == SketchConstraintKind::tangent) return "[T]";
    if (annotation.kind == SketchConstraintKind::concentric) return "[CC]";
    if (annotation.kind == SketchConstraintKind::fixed) return "[F]";
    std::ostringstream label;
    if (annotation.kind == SketchConstraintKind::angle) label << "A ";
    else if (annotation.kind == SketchConstraintKind::radius) label << "R ";
    else if (annotation.kind == SketchConstraintKind::diameter) label << "DIA ";
    else label << "D ";
    label << std::fixed << std::setprecision(3) << annotation.value;
    if (annotation.kind != SketchConstraintKind::angle) label << " mm";
    return label.str();
}

} // namespace

SketchBackend::SketchBackend(application::CadSession& session)
    : session_(session)
{
}

SketchBackend::~SketchBackend() = default;

bool SketchBackend::begin()
{
    const viewport::PlanarFaceSelection top{document::ModelDocument::initialBoxId().value(),
        0.0, 0.0, 20.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0};
    return begin(top);
}

bool SketchBackend::begin(const viewport::PlanarFaceSelection& support)
{
    if (editor_) return true;
    const document::SketchPlane plane{support.originX, support.originY, support.originZ,
        support.normalX, support.normalY, support.normalZ,
        support.xDirectionX, support.xDirectionY, support.xDirectionZ};
    editor_ = std::make_unique<application::SketchEditorSession>(
        session_, document::FeatureId{support.bodyId}, plane);
    lastMessage_ = QStringLiteral("Parametric sketch session started on top face");
    return true;
}

bool SketchBackend::add(PreviewCanvas::SketchTool tool, const QPointF& first, const QPointF& second)
{
    if (!editor_ || drawingTool(tool) == application::SketchDrawingTool::select) {
        lastMessage_ = QStringLiteral("No active creation tool");
        return false;
    }
    editor_->setTool(drawingTool(tool));
    std::optional<document::SketchPoint> third;
    if (tool == PreviewCanvas::SketchTool::Arc) {
        const QPointF radial = second - first;
        third = document::SketchPoint{first.x() - radial.y(), first.y() + radial.x()};
    }
    if (!editor_->setPreview({first.x(), first.y()}, {second.x(), second.y()}, third)) {
        lastMessage_ = QStringLiteral("Geometry is degenerate and was not added");
        return false;
    }
    const auto id = editor_->confirmPreview();
    if (!id.has_value()) {
        lastMessage_ = QStringLiteral("Sketch solver rejected the geometry");
        return false;
    }
    if (tool == PreviewCanvas::SketchTool::Line) {
        if (std::abs(first.y() - second.y()) < 1.0e-9)
            static_cast<void>(editor_->setSelectedLineConstraint(
                document::SketchLineConstraint::horizontal));
        else if (std::abs(first.x() - second.x()) < 1.0e-9)
            static_cast<void>(editor_->setSelectedLineConstraint(
                document::SketchLineConstraint::vertical));
    }
    editor_->commitHistory();
    lastMessage_ = QStringLiteral("Element #%1 stored in the parametric sketch").arg(id->value());
    return true;
}

void SketchBackend::setTool(PreviewCanvas::SketchTool tool)
{
    activeTool_ = tool;
    if (editor_) editor_->setTool(drawingTool(tool));
}

void SketchBackend::updatePreview(const std::optional<viewport::SketchElementIntent>& intent)
{
    if (!editor_) return;
    if (!intent.has_value()) {
        editor_->clearPreview();
        return;
    }
    const auto third = intent->third.has_value()
                           ? std::optional{document::SketchPoint{intent->third->first,
                                                                 intent->third->second}}
                           : std::nullopt;
    static_cast<void>(editor_->setPreview({intent->firstX, intent->firstY},
                                          {intent->secondX, intent->secondY}, third));
}

std::optional<std::uint64_t> SketchBackend::confirmElement(
    const viewport::SketchElementIntent& intent)
{
    if (!editor_) return std::nullopt;
    const auto third = intent.third.has_value()
                           ? std::optional{document::SketchPoint{intent.third->first,
                                                                 intent.third->second}}
                           : std::nullopt;
    if (!editor_->setPreview({intent.firstX, intent.firstY},
                             {intent.secondX, intent.secondY}, third)) return std::nullopt;
    const auto id = editor_->confirmPreview();
    if (!id.has_value()) return std::nullopt;
    const auto tool = editor_->tool();
    if (tool == application::SketchDrawingTool::line ||
        tool == application::SketchDrawingTool::polyline) {
        addCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                       intent.firstReference, {intent.firstX, intent.firstY});
        addCoincidence(*editor_, {*id, document::SketchPointPosition::end},
                       intent.secondReference, {intent.secondX, intent.secondY});
        if (intent.automaticConstraint == viewport::SketchAutomaticConstraint::horizontal ||
            intent.automaticConstraint == viewport::SketchAutomaticConstraint::vertical) {
            static_cast<void>(editor_->setSelectedLineConstraint(
                *intent.automaticConstraint == viewport::SketchAutomaticConstraint::horizontal
                    ? document::SketchLineConstraint::horizontal
                    : document::SketchLineConstraint::vertical));
        } else {
            static_cast<void>(editor_->addAutomaticLineRelation(*id));
        }
    } else if (tool == application::SketchDrawingTool::circle) {
        addCoincidence(*editor_, {*id, document::SketchPointPosition::center},
                       intent.firstReference, {intent.firstX, intent.firstY});
        if (intent.automaticTargetId.has_value() && intent.automaticConstraint.has_value()) {
            const auto kind = *intent.automaticConstraint == viewport::SketchAutomaticConstraint::equal
                                  ? document::SketchConstraintKind::equal
                              : *intent.automaticConstraint == viewport::SketchAutomaticConstraint::tangent
                                  ? document::SketchConstraintKind::tangent
                                  : document::SketchConstraintKind::concentric;
            static_cast<void>(editor_->addAutomaticRadialRelation(
                *id, document::SketchElementId{*intent.automaticTargetId}, kind));
        }
    } else if (tool == application::SketchDrawingTool::arc && intent.third.has_value()) {
        addCoincidence(*editor_, {*id, document::SketchPointPosition::center},
                       intent.firstReference, {intent.firstX, intent.firstY});
        addCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                       intent.secondReference, {intent.secondX, intent.secondY});
        addCoincidence(*editor_, {*id, document::SketchPointPosition::end},
                       intent.thirdReference, {intent.third->first, intent.third->second});
    } else {
        addCoincidence(*editor_, {*id, document::SketchPointPosition::start},
                       intent.firstReference, {intent.firstX, intent.firstY});
    }
    editor_->commitHistory();
    lastMessage_ = QStringLiteral("Sketch element #%1 added").arg(id->value());
    return id->value();
}

PreviewCanvas::SketchTool SketchBackend::activeTool() const noexcept { return activeTool_; }

bool SketchBackend::selectElement(const viewport::SketchSelectionIntent& intent)
{
    if (!editor_) return false;
    const bool selected = editor_->selectNearestReference(
        document::SketchElementId{intent.elementId}, {intent.localX, intent.localY}, intent.additive);
    if (selected) lastMessage_ = QStringLiteral("Sketch reference selected");
    return selected;
}

bool SketchBackend::translateSelected(double deltaX, double deltaY)
{
    if (!editor_ || !editor_->translateSelected(deltaX, deltaY)) return false;
    editor_->commitHistory();
    lastMessage_ = QStringLiteral("Sketch geometry moved");
    return true;
}

std::vector<viewport::SceneSketchElement> SketchBackend::sceneElements() const
{
    std::vector<viewport::SceneSketchElement> result;
    if (!editor_) return result;
    const auto evaluated = editor_->evaluatedElements();
    result.reserve(evaluated.size());
    for (const auto& element : evaluated) {
        std::vector<viewport::SceneSketchPoint> points;
        for (const auto& point : element.points) {
            points.push_back({element.id.value(), scenePoint(point.kind), point.local.x, point.local.y,
                              point.world.X(), point.world.Y(), point.world.Z()});
            if (point.kind == document::SketchPointPosition::center)
                points.back().radialRadius = element.radialRadius;
        }
        if (element.points.size() == 2) {
            const auto& first = element.points[0];
            const auto& second = element.points[1];
            points.push_back({element.id.value(), viewport::SketchPointKind::midpoint,
                              (first.local.x + second.local.x) * 0.5,
                              (first.local.y + second.local.y) * 0.5,
                              (first.world.X() + second.world.X()) * 0.5,
                              (first.world.Y() + second.world.Y()) * 0.5,
                              (first.world.Z() + second.world.Z()) * 0.5});
        }
        if (std::ranges::find(editor_->elements(), element.id, &document::SketchElement::id) ==
            editor_->elements().end()) points.clear();
        result.emplace_back(element.id.value(), element.shape,
                            editor_->isElementConflicting(element.id), element.construction,
                            std::move(points));
    }
    return result;
}

std::vector<viewport::SceneSketchConstraint> SketchBackend::sceneConstraints() const
{
    std::vector<viewport::SceneSketchConstraint> result;
    if (!editor_) return result;
    for (const auto& annotation : editor_->evaluatedConstraints()) {
        const bool editable = annotation.kind == document::SketchConstraintKind::distance ||
                              annotation.kind == document::SketchConstraintKind::radius ||
                              annotation.kind == document::SketchConstraintKind::diameter ||
                              annotation.kind == document::SketchConstraintKind::angle;
        result.push_back({annotation.id.value(), constraintLabel(annotation), annotation.position.X(),
                          annotation.position.Y(), annotation.position.Z(),
                          editor_->isConstraintConflicting(annotation.id), editable});
    }
    return result;
}

std::vector<std::uint64_t> SketchBackend::selectedElementIds() const
{
    std::vector<std::uint64_t> result;
    if (!editor_) return result;
    for (const auto& reference : editor_->selectedReferences()) {
        if (std::ranges::find(result, reference.element.value()) == result.end())
            result.push_back(reference.element.value());
    }
    if (result.empty() && editor_->selectedElement().has_value())
        result.push_back(editor_->selectedElement()->value());
    return result;
}

bool SketchBackend::finish()
{
    if (!editor_ || !editor_->canAccept()) {
        lastMessage_ = QStringLiteral("Add valid geometry before finishing the sketch");
        return false;
    }
    const auto feature = editor_->accept();
    lastMessage_ = QStringLiteral("Sketch feature #%1 committed to document history").arg(feature.value());
    editor_.reset();
    return true;
}

void SketchBackend::cancel()
{
    if (editor_) editor_->cancel();
    editor_.reset();
    lastMessage_ = QStringLiteral("Sketch cancelled");
}

bool SketchBackend::active() const noexcept
{
    return editor_ != nullptr && editor_->isActive();
}

int SketchBackend::degreesOfFreedom() const noexcept
{
    return editor_ ? editor_->lastSolveResult().degreesOfFreedom : 0;
}

QString SketchBackend::lastMessage() const
{
    return lastMessage_;
}

} // namespace voelund::preview
