// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchEditorToolController.hpp"
#include "ToolPanelHost.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/application/SketchEditorSession.hpp>
#include <voelund/document/DocumentParameters.hpp>
#include <voelund/document/ParameterExpression.hpp>

#include <QObject>
#include <QRegularExpression>

#include <cmath>
#include <variant>

// D01 (user report 2026-10-23, as in Fusion and SolidCraft): values typed while drawing become
// driving dimensions of the new element, in the same undo step as the element. Width and height
// of a rectangle dimension its horizontal and vertical edge; "=Name" binds the dimension to
// the parameter. A typed angle only places the line (no angle dimension yet).
namespace voelund::desktop {
namespace {

bool rectangleEdge(application::SketchCreationRole role) {
    using Role = application::SketchCreationRole;
    return role == Role::rectangleFromFirstCorner || role == Role::rectangleToSecondCorner ||
           role == Role::rectangleFromSecondCorner || role == Role::rectangleToFirstCorner;
}

// The edge of a new rectangle that runs horizontally (width) or vertically (height).
std::optional<document::SketchElementId>
rectangleSide(const application::SketchCreationResult& created,
              const application::SketchEditorSession& editor, bool horizontal) {
    for (const auto& item : created.elements) {
        if (!rectangleEdge(item.role))
            continue;
        const auto match =
            std::ranges::find(editor.elements(), item.id, &document::SketchElement::id);
        if (match == editor.elements().end())
            continue;
        const auto* line = std::get_if<document::SketchLine>(&match->geometry);
        if (line == nullptr)
            continue;
        const double dx = std::abs(line->end.x - line->start.x);
        const double dy = std::abs(line->end.y - line->start.y);
        if (horizontal ? dx > dy : dy > dx)
            return item.id;
    }
    return std::nullopt;
}

std::optional<document::SketchElementId>
dimensionTarget(const application::SketchCreationResult& created,
                const application::SketchEditorSession& editor, viewport::DynamicField field) {
    switch (field) {
    case viewport::DynamicField::length:
    case viewport::DynamicField::diameter:
        return created.primary();
    case viewport::DynamicField::width:
        return rectangleSide(created, editor, true);
    case viewport::DynamicField::height:
        return rectangleSide(created, editor, false);
    case viewport::DynamicField::angle:
    case viewport::DynamicField::radius: // W11: arc, polygon and ellipse are only placed
    case viewport::DynamicField::sweep:
    case viewport::DynamicField::minorRadius:
        break;
    }
    return std::nullopt;
}

// "=Breite" names one parameter; "=Breite*2" is an expression that only gives a value.
QString boundName(const std::string& text) {
    static const QRegularExpression name(QStringLiteral("^=\\s*([A-Za-z_][A-Za-z0-9_]*)\\s*$"));
    const auto match = name.match(QString::fromStdString(text));
    return match.hasMatch() ? match.captured(1) : QString{};
}

} // namespace

void SketchEditorToolController::applyTypedDimensions(
    const application::SketchCreationResult& created,
    const std::vector<viewport::SketchTypedDimension>& typed) {
    for (const auto& dimension : typed) {
        const auto target = dimensionTarget(created, *editor_, dimension.field);
        if (!target || !editor_->selectElement(*target))
            continue;
        const bool diameter = dimension.field == viewport::DynamicField::diameter;
        const bool set = diameter ? editor_->setSelectedCircleRadius(dimension.value / 2.0)
                                  : editor_->setSelectedLineLength(dimension.value);
        if (!set) {
            statusChanged_(QObject::tr("Der Wert %1 ließ sich nicht als Maß setzen")
                               .arg(QString::fromStdString(dimension.text)));
            continue;
        }
        const QString name = boundName(dimension.text);
        if (name.isEmpty() || diameter)
            continue;
        const auto constraint =
            editor_->selectedDimension(document::SketchConstraintKind::distance);
        const auto parameter =
            panel_.resolveParameter(name, ToolValueKind::length, dimension.value);
        if (constraint && parameter)
            static_cast<void>(editor_->bindDimension(*constraint, parameter->id));
    }
    static_cast<void>(editor_->selectElements({}, false));
}

void SketchEditorToolController::installDynamicInputEvaluator() {
    viewport_.setSketchDynamicInputEvaluator([this](
                                                 const std::string& text) -> std::optional<double> {
        const auto& table = session_.parameterTable();
        try {
            return document::evaluateExpression(
                       std::string_view(text).substr(1),
                       [&table](std::string_view name) -> std::optional<document::ExpressionValue> {
                           const auto* parameter = document::findParameter(table, name);
                           if (parameter == nullptr)
                               return std::nullopt;
                           return document::ExpressionValue{
                               parameter->value, document::parameterDimension(parameter->unit)};
                       })
                .value;
        } catch (const std::invalid_argument&) {
            return std::nullopt;
        }
    });
}

} // namespace voelund::desktop
