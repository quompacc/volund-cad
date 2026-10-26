// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/application/SketchEditorSession.hpp>

#include <array>
#include <cmath>
#include <optional>
#include <stdexcept>

namespace voelund::application {

SketchCreationResult
SketchEditorSession::confirmRectangle(const document::SketchRectangle& rectangle) {
    const std::array points{
        rectangle.first,
        document::SketchPoint{rectangle.second.x, rectangle.first.y},
        rectangle.second,
        document::SketchPoint{rectangle.first.x, rectangle.second.y},
    };
    const std::uint64_t firstConstraint = constraints_.nextId;
    std::array<document::SketchElementId, 4> ids{
        document::SketchElementId{elements_.nextId++},
        document::SketchElementId{elements_.nextId++},
        document::SketchElementId{elements_.nextId++},
        document::SketchElementId{elements_.nextId++},
    };

    for (std::size_t index = 0; index < ids.size(); ++index) {
        const auto lineConstraint = index % 2 == 0 ? document::SketchLineConstraint::horizontal
                                                   : document::SketchLineConstraint::vertical;
        elements_.values.push_back(
            {ids[index], document::SketchLine{points[index], points[(index + 1) % points.size()],
                                              lineConstraint}});
        constraints_.values.push_back({document::SketchConstraintId{constraints_.nextId++},
                                       index % 2 == 0 ? document::SketchConstraintKind::horizontal
                                                      : document::SketchConstraintKind::vertical,
                                       {ids[index], document::SketchPointPosition::start},
                                       std::nullopt,
                                       0.0});
        constraints_.values.push_back(
            {document::SketchConstraintId{constraints_.nextId++},
             document::SketchConstraintKind::coincident,
             {ids[index], document::SketchPointPosition::end},
             document::SketchGeometryReference{ids[(index + 1) % ids.size()],
                                               document::SketchPointPosition::start},
             0.0});
    }

    clearPreview();
    selectedElement_ = ids.front();
    selectedReferences_.clear();
    static_cast<void>(solveConstraints());
    SketchCreationResult result{{{ids[0], SketchCreationRole::rectangleFromFirstCorner},
                                 {ids[1], SketchCreationRole::rectangleToSecondCorner},
                                 {ids[2], SketchCreationRole::rectangleFromSecondCorner},
                                 {ids[3], SketchCreationRole::rectangleToFirstCorner}},
                                {{SketchCreationPoint::rectangleFirstCorner,
                                  {ids[0], document::SketchPointPosition::start}},
                                 {SketchCreationPoint::rectangleSecondCorner,
                                  {ids[2], document::SketchPointPosition::start}}}};
    for (const auto& constraint : constraints_.values)
        if (constraint.id.value() >= firstConstraint)
            result.constraints.push_back({constraint.id, SketchConstraintOrigin::shape});
    return result;
}

namespace {
// The corners of the rectangle on the edge first-second, as wide as `third` lies off it.
std::optional<std::array<document::SketchPoint, 4>> rectangleCorners(document::SketchPoint first,
                                                                     document::SketchPoint second,
                                                                     document::SketchPoint third) {
    const double ux = second.x - first.x, uy = second.y - first.y;
    const double length = std::hypot(ux, uy);
    if (length < 1e-9)
        return std::nullopt;
    const double nx = -uy / length, ny = ux / length;
    const double width = (third.x - first.x) * nx + (third.y - first.y) * ny;
    if (std::abs(width) < 1e-9)
        return std::nullopt;
    return std::array{first, second,
                      document::SketchPoint{second.x + nx * width, second.y + ny * width},
                      document::SketchPoint{first.x + nx * width, first.y + ny * width}};
}
} // namespace

SketchCreationResult SketchEditorSession::createThreePointRectangle(document::SketchPoint first,
                                                                    document::SketchPoint second,
                                                                    document::SketchPoint third) {
    const auto corners = rectangleCorners(first, second, third);
    if (!active_ || !corners)
        throw std::invalid_argument("Die drei Punkte spannen kein Rechteck auf");
    Rollback rollback(*this);
    const std::uint64_t firstConstraint = constraints_.nextId;
    const std::uint64_t firstId = elements_.nextId;
    elements_.nextId += 4;
    const std::array ids{document::SketchElementId{firstId}, document::SketchElementId{firstId + 1},
                         document::SketchElementId{firstId + 2},
                         document::SketchElementId{firstId + 3}};
    const auto relate = [this](document::SketchConstraintKind kind,
                               document::SketchGeometryReference a,
                               std::optional<document::SketchGeometryReference> b) {
        constraints_.values.push_back(
            {document::SketchConstraintId{constraints_.nextId++}, kind, a, b, 0.0});
    };
    using Kind = document::SketchConstraintKind;
    using Position = document::SketchPointPosition;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        elements_.values.push_back(
            {ids[index], document::SketchLine{(*corners)[index], (*corners)[(index + 1) % 4]}});
        relate(Kind::coincident, {ids[index], Position::end},
               document::SketchGeometryReference{ids[(index + 1) % 4], Position::start});
    }
    relate(Kind::perpendicular, {ids[0]}, document::SketchGeometryReference{ids[1]});
    relate(Kind::parallel, {ids[0]}, document::SketchGeometryReference{ids[2]});
    relate(Kind::parallel, {ids[1]}, document::SketchGeometryReference{ids[3]});
    clearPreview();
    selectedElement_ = ids.front();
    selectedReferences_.clear();
    static_cast<void>(solveConstraints());
    SketchCreationResult result{{{ids[0], SketchCreationRole::rectangleFromFirstCorner},
                                 {ids[1], SketchCreationRole::rectangleToSecondCorner},
                                 {ids[2], SketchCreationRole::rectangleFromSecondCorner},
                                 {ids[3], SketchCreationRole::rectangleToFirstCorner}},
                                {}};
    for (const auto& constraint : constraints_.values)
        if (constraint.id.value() >= firstConstraint)
            result.constraints.push_back({constraint.id, SketchConstraintOrigin::shape});
    rollback.keep();
    commitHistory();
    return result;
}

} // namespace voelund::application
