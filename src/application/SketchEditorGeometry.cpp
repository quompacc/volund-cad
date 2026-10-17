#include <voelund/application/SketchEditorSession.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace voelund::application {
namespace {

using document::SketchGeometry;
using document::SketchPoint;

// `reversed`: the picks run clockwise, so the stored (counterclockwise) arc starts at `last`.
document::SketchArc threePointArc(SketchPoint first, SketchPoint middle, SketchPoint last,
                                  bool& reversed) {
    const double determinant =
        2.0 * (first.x * (middle.y - last.y) + middle.x * (last.y - first.y) +
               last.x * (first.y - middle.y));
    if (std::abs(determinant) <= 1.0e-9)
        throw std::invalid_argument("Three-point arc needs non-collinear points");
    const double firstSquared = first.x * first.x + first.y * first.y;
    const double middleSquared = middle.x * middle.x + middle.y * middle.y;
    const double lastSquared = last.x * last.x + last.y * last.y;
    const SketchPoint center{
        (firstSquared * (middle.y - last.y) + middleSquared * (last.y - first.y) +
         lastSquared * (first.y - middle.y)) /
            determinant,
        (firstSquared * (last.x - middle.x) + middleSquared * (first.x - last.x) +
         lastSquared * (middle.x - first.x)) /
            determinant};
    const auto angle = [center](SketchPoint point) {
        return std::atan2(point.y - center.y, point.x - center.x);
    };
    const double firstAngle = angle(first);
    const double middleAngle = angle(middle);
    const double lastAngle = angle(last);
    const auto positiveSweep = [](double from, double to) {
        double result = std::fmod(to - from, 2.0 * std::numbers::pi);
        if (result < 0.0)
            result += 2.0 * std::numbers::pi;
        return result;
    };
    double start = firstAngle;
    double end = firstAngle + positiveSweep(firstAngle, lastAngle);
    reversed = positiveSweep(firstAngle, middleAngle) > positiveSweep(firstAngle, lastAngle);
    if (reversed) {
        start = lastAngle;
        end = lastAngle + positiveSweep(lastAngle, firstAngle);
    }
    return {center, std::hypot(first.x - center.x, first.y - center.y), start, end};
}

std::vector<SketchGeometry> slotGeometry(SketchPoint first, SketchPoint second,
                                         SketchPoint widthPoint) {
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    const double length = std::hypot(dx, dy);
    const double ux = dx / length;
    const double uy = dy / length;
    const double nx = -uy;
    const double ny = ux;
    const double radius = std::abs((widthPoint.x - first.x) * nx + (widthPoint.y - first.y) * ny);
    const SketchPoint firstTop{first.x + nx * radius, first.y + ny * radius};
    const SketchPoint secondTop{second.x + nx * radius, second.y + ny * radius};
    const SketchPoint secondBottom{second.x - nx * radius, second.y - ny * radius};
    const SketchPoint firstBottom{first.x - nx * radius, first.y - ny * radius};
    const double axisAngle = std::atan2(dy, dx);
    return {document::SketchLine{firstTop, secondTop},
            document::SketchArc{second, radius, axisAngle - std::numbers::pi / 2.0,
                                axisAngle + std::numbers::pi / 2.0},
            document::SketchLine{secondBottom, firstBottom},
            document::SketchArc{first, radius, axisAngle + std::numbers::pi / 2.0,
                                axisAngle + 3.0 * std::numbers::pi / 2.0}};
}

std::vector<SketchGeometry> polygonGeometry(SketchPoint center, SketchPoint vertex, int sides) {
    const double radius = std::hypot(vertex.x - center.x, vertex.y - center.y);
    const double start = std::atan2(vertex.y - center.y, vertex.x - center.x);
    std::vector<SketchGeometry> result;
    result.reserve(static_cast<std::size_t>(sides));
    for (int index = 0; index < sides; ++index) {
        const double firstAngle = start + 2.0 * std::numbers::pi * index / sides;
        const double secondAngle = start + 2.0 * std::numbers::pi * (index + 1) / sides;
        result.push_back(document::SketchLine{
            {center.x + radius * std::cos(firstAngle), center.y + radius * std::sin(firstAngle)},
            {center.x + radius * std::cos(secondAngle),
             center.y + radius * std::sin(secondAngle)}});
    }
    return result;
}

} // namespace

std::vector<document::SketchGeometry>
SketchEditorSession::geometry(document::SketchPoint first, document::SketchPoint second,
                              std::optional<document::SketchPoint> third,
                              PreviewShape& shape) const {
    using enum SketchCreationPoint;
    using Position = document::SketchPointPosition;
    shape = {};
    const auto single = [&shape](std::vector<PreviewPoint> points) {
        shape.roles = {SketchCreationRole::element};
        shape.points = std::move(points);
    };
    if (tool_ == SketchDrawingTool::line || tool_ == SketchDrawingTool::polyline) {
        single({{lineStart, 0, Position::start}, {lineEnd, 0, Position::end}});
        return {document::SketchLine{first, second}};
    }
    if (tool_ == SketchDrawingTool::point) { // S15: one pick
        single({{center, 0, Position::center}});
        return {document::SketchPointGeometry{first}};
    }
    if (tool_ == SketchDrawingTool::rectangle) // confirmRectangle names its own items
        return {document::SketchRectangle{first.x, first.y, second.x, second.y}};
    const double radius = std::hypot(second.x - first.x, second.y - first.y);
    if (tool_ == SketchDrawingTool::circle) {
        single({{center, 0, Position::center}});
        return {document::SketchCircle{first, radius}};
    }
    if (tool_ == SketchDrawingTool::polygon) {
        auto sides = polygonGeometry(first, second, polygonSides_);
        shape.roles.assign(sides.size(), SketchCreationRole::polygonSide);
        shape.roles.push_back(SketchCreationRole::polygonCircle);
        shape.points = {{polygonVertex, 0, Position::start},
                        {polygonCenter, sides.size(), Position::center}};
        sides.push_back(document::SketchCircle{first, radius});
        return sides;
    }
    if (!third.has_value()) {
        if (tool_ == SketchDrawingTool::arc)
            return {document::SketchCircle{first, radius}};
        return {document::SketchLine{first, second}};
    }
    if (tool_ == SketchDrawingTool::slot) {
        // Order of slotGeometry: side, cap around `second`, side back, cap around `first`.
        // The construction axis follows the contour.
        shape.roles = {SketchCreationRole::slotFirstSide, SketchCreationRole::slotSecondCap,
                       SketchCreationRole::slotSecondSide, SketchCreationRole::slotFirstCap,
                       SketchCreationRole::slotAxis};
        shape.points = {{slotFirstCenter, 3, Position::center},
                        {slotSecondCenter, 1, Position::center}};
        auto slot = slotGeometry(first, second, *third);
        slot.push_back(document::SketchLine{first, second});
        return slot;
    }
    if (tool_ == SketchDrawingTool::threePointArc) {
        // Picks as in other CAD tools: start, end, then a point on the arc, so the arc bulges
        // towards the pointer.
        bool reversed = false;
        auto arc = threePointArc(first, *third, second, reversed);
        single({{center, 0, Position::center},
                {arcStart, 0, reversed ? Position::end : Position::start},
                {arcEnd, 0, reversed ? Position::start : Position::end}});
        shape.arcDirection =
            reversed ? SketchArcDirection::clockwise : SketchArcDirection::counterclockwise;
        return {arc};
    }
    if (tool_ == SketchDrawingTool::ellipse) {
        const double rotation = std::atan2(second.y - first.y, second.x - first.x);
        const double minor = std::min(radius, std::abs(-(third->x - first.x) * std::sin(rotation) +
                                                       (third->y - first.y) * std::cos(rotation)));
        single({{center, 0, Position::center}});
        return {document::SketchEllipse{first, radius, minor, rotation}};
    }
    // Center arc: counterclockwise from the start pick to the end pick.
    double startAngle = std::atan2(second.y - first.y, second.x - first.x);
    double endAngle = std::atan2(third->y - first.y, third->x - first.x);
    while (endAngle <= startAngle)
        endAngle += 2.0 * std::numbers::pi;
    single({{center, 0, Position::center},
            {arcStart, 0, Position::start},
            {arcEnd, 0, Position::end}});
    shape.arcDirection = SketchArcDirection::counterclockwise;
    return {document::SketchArc{first, radius, startAngle, endAngle}};
}

bool SketchEditorSession::setPreview(document::SketchPoint first, document::SketchPoint second,
                                     std::optional<document::SketchPoint> third) {
    if (!active_ || tool_ == SketchDrawingTool::select)
        return false;
    const bool needsThird = tool_ == SketchDrawingTool::arc ||
                            tool_ == SketchDrawingTool::threePointArc ||
                            tool_ == SketchDrawingTool::ellipse || tool_ == SketchDrawingTool::slot;
    try {
        PreviewShape shape;
        std::vector<SketchGeometry> candidate = geometry(first, second, third, shape);
        if (!canAllocatePreviewIds(candidate))
            return false;
        document::SketchElements validated = elements_;
        for (const SketchGeometry& item : candidate)
            validated.values.push_back({document::SketchElementId{validated.nextId++}, item});
        document::validateSketchElements(validated);
        preview_ = std::move(candidate);
        previewShape_ = std::move(shape);
        previewConfirmable_ = !needsThird || third.has_value();
        return true;
    } catch (const std::invalid_argument&) {
        preview_.clear();
        previewShape_ = {};
        previewConfirmable_ = false;
        return false;
    }
}

void SketchEditorSession::clearPreview() noexcept {
    preview_.clear();
    previewShape_ = {};
    previewConfirmable_ = false;
}

std::optional<document::SketchElementId> SketchEditorSession::confirmPreview() {
    const auto created = confirmCreation();
    return created ? std::optional{created->primary()} : std::nullopt;
}

std::optional<SketchCreationResult> SketchEditorSession::confirmCreation() {
    if (!active_ || preview_.empty() || !previewConfirmable_ || !canAllocatePreviewIds(preview_))
        return std::nullopt;
    if (const auto* rectangle = std::get_if<document::SketchRectangle>(&preview_.front()))
        return confirmRectangle(*rectangle);
    const document::SketchElementId firstId{elements_.nextId};
    const std::uint64_t firstConstraint = constraints_.nextId;
    std::vector<document::SketchElementId> ids;
    for (std::size_t index = 0; index < preview_.size(); ++index) {
        const auto role = index < previewShape_.roles.size() ? previewShape_.roles[index]
                                                             : SketchCreationRole::element;
        ids.push_back(document::SketchElementId{elements_.nextId++});
        // S06: the polygon's circle and the slot's axis are construction geometry.
        elements_.values.push_back(
            {ids.back(), std::move(preview_[index]),
             role == SketchCreationRole::polygonCircle || role == SketchCreationRole::slotAxis});
    }
    if ((tool_ == SketchDrawingTool::slot || tool_ == SketchDrawingTool::polygon) &&
        ids.size() > 1) {
        // The contour: every item but the trailing construction circle or axis.
        const std::size_t contour = ids.size() - 1;
        for (std::size_t index = 0; index < contour; ++index) {
            // Slot caps run counterclockwise, opposite to the contour traversal.
            const bool currentIsReversed = tool_ == SketchDrawingTool::slot && index % 2 == 1;
            const bool nextIsReversed = tool_ == SketchDrawingTool::slot && index % 2 == 0;
            const auto outgoing = currentIsReversed ? document::SketchPointPosition::start
                                                    : document::SketchPointPosition::end;
            const auto incoming = nextIsReversed ? document::SketchPointPosition::end
                                                 : document::SketchPointPosition::start;
            constraints_.values.push_back(
                {document::SketchConstraintId{constraints_.nextId++},
                 document::SketchConstraintKind::coincident,
                 {ids[index], outgoing},
                 document::SketchGeometryReference{ids[(index + 1) % contour], incoming}});
        }
        using Position = document::SketchPointPosition;
        const auto addRelation = [this, &ids](document::SketchConstraintKind kind,
                                              std::size_t first, std::size_t second,
                                              Position firstPoint = Position::start,
                                              Position secondPoint = Position::start) {
            constraints_.values.push_back(
                {document::SketchConstraintId{constraints_.nextId++},
                 kind,
                 {ids[first], firstPoint},
                 document::SketchGeometryReference{ids[second], secondPoint}});
        };
        if (tool_ == SketchDrawingTool::slot && ids.size() == 5) {
            // With equal caps the tangent sides are parallel already (S06: no redundant
            // parallel relation).
            addRelation(document::SketchConstraintKind::equal, 1, 3);
            addRelation(document::SketchConstraintKind::tangent, 0, 1);
            addRelation(document::SketchConstraintKind::tangent, 1, 2);
            addRelation(document::SketchConstraintKind::tangent, 2, 3);
            addRelation(document::SketchConstraintKind::tangent, 3, 0);
            addRelation(document::SketchConstraintKind::coincident, 4, 3, Position::start,
                        Position::center);
            addRelation(document::SketchConstraintKind::coincident, 4, 1, Position::end,
                        Position::center);
        } else if (tool_ == SketchDrawingTool::polygon) {
            for (std::size_t index = 1; index < contour; ++index)
                addRelation(document::SketchConstraintKind::equal, 0, index);
            for (std::size_t index = 0; index < contour; ++index)
                addRelation(document::SketchConstraintKind::pointOnObject, index, contour);
        }
    }
    preview_.clear();
    previewConfirmable_ = false;
    selectedElement_ = firstId;
    selectedReferences_.clear();
    static_cast<void>(solveConstraints());
    SketchCreationResult result;
    std::uint32_t side = 0;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const auto role = index < previewShape_.roles.size() ? previewShape_.roles[index]
                                                             : SketchCreationRole::element;
        result.elements.push_back(
            {ids[index], role, role == SketchCreationRole::polygonSide ? side++ : 0U});
    }
    for (const PreviewPoint& point : previewShape_.points)
        result.points.push_back({point.role, {ids.at(point.item), point.position}});
    result.arcDirection = previewShape_.arcDirection;
    for (const auto& constraint : constraints_.values)
        if (constraint.id.value() >= firstConstraint)
            result.constraints.push_back({constraint.id, SketchConstraintOrigin::shape});
    previewShape_ = {};
    return result;
}

bool SketchEditorSession::setPolygonSides(int sides) noexcept {
    // S06: a side count outside 3..64 is refused, not silently clamped.
    if (sides < 3 || sides > 64)
        return false;
    polygonSides_ = sides;
    clearPreview();
    return true;
}

} // namespace voelund::application
