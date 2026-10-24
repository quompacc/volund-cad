// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchSnapOverlay.hpp"

#include <QEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <utility>

namespace voelund::viewport {

QString snapLabel(const SketchEditorInteraction::SnapFeedback& feedback) {
    if (feedback.relation.has_value()) {
        switch (*feedback.relation) {
        case SketchAutomaticConstraint::horizontal:
            return QObject::tr("Waagrecht");
        case SketchAutomaticConstraint::vertical:
            return QObject::tr("Senkrecht");
        case SketchAutomaticConstraint::parallel:
            return QObject::tr("Parallel");
        case SketchAutomaticConstraint::perpendicular:
            return QObject::tr("Rechtwinklig");
        case SketchAutomaticConstraint::pointOnObject:
            return QObject::tr("Auf Objekt");
        case SketchAutomaticConstraint::tangent:
            return QObject::tr("Tangential");
        case SketchAutomaticConstraint::equal:
            return QObject::tr("Gleich");
        case SketchAutomaticConstraint::concentric:
            return QObject::tr("Konzentrisch");
        }
    }
    if (!feedback.activeKind.has_value())
        return {};
    switch (*feedback.activeKind) {
    case SketchPointKind::origin:
        return QObject::tr("Ursprung");
    case SketchPointKind::midpoint:
        return QObject::tr("Mittelpunkt");
    case SketchPointKind::center:
        return QObject::tr("Zentrum");
    case SketchPointKind::xAxis:
    case SketchPointKind::yAxis:
        return QObject::tr("Auf Achse");
    case SketchPointKind::onObject:
        return QObject::tr("Auf Objekt");
    default:
        return QObject::tr("Deckungsgleich");
    }
}

SketchSnapOverlay::SketchSnapOverlay(QWidget& viewport) : QWidget(&viewport), viewport_(viewport) {
    setObjectName(QStringLiteral("sketchSnapOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setGeometry(viewport.rect());
    viewport.installEventFilter(this);
    hide();
}

void SketchSnapOverlay::setColors(const QColor& accent, const QColor& muted, const QColor& panel) {
    accent_ = accent;
    muted_ = muted;
    panel_ = panel;
    update();
}

void SketchSnapOverlay::display(const SketchEditorInteraction::SnapFeedback& feedback,
                                QPointF cursor, double devicePixelRatio) {
    const auto logical = [devicePixelRatio](std::pair<int, int> point) {
        return QPointF(point.first / devicePixelRatio, point.second / devicePixelRatio);
    };
    candidates_.clear();
    for (const auto& candidate : feedback.candidates)
        candidates_.push_back(logical(candidate));
    active_ = feedback.active.has_value() ? std::optional{logical(*feedback.active)} : std::nullopt;
    label_ = snapLabel(feedback);
    cursor_ = cursor;
    // Test hooks: what the user sees, without reading pixels.
    setProperty("activeSnap", active_.has_value());
    setProperty("snapLabel", label_);
    setVisible(!candidates_.empty() || active_.has_value() || !label_.isEmpty() ||
               !fields_.empty());
    raise();
    update();
}

namespace {
QString fieldLabel(DynamicField field) {
    switch (field) {
    case DynamicField::length:
        return QObject::tr("Länge");
    case DynamicField::angle:
        return QObject::tr("Winkel");
    case DynamicField::width:
        return QObject::tr("Breite");
    case DynamicField::height:
        return QObject::tr("Höhe");
    case DynamicField::diameter:
        return QObject::tr("Durchmesser");
    case DynamicField::radius:
        return QObject::tr("Radius");
    case DynamicField::sweep:
        return QObject::tr("Öffnungswinkel");
    case DynamicField::thickness:
        return QObject::tr("Dicke");
    case DynamicField::minorRadius:
        return QObject::tr("Kleiner Radius");
    }
    return {};
}

QString fieldValue(const SketchDynamicInput::Field& field, bool current) {
    const bool angle = field.kind == DynamicField::angle || field.kind == DynamicField::sweep;
    if (!field.text.empty())
        return QString::fromStdString(field.text) + (current ? QStringLiteral("|") : QString{});
    return QLocale().toString(field.live, 'f', angle ? 1 : 2) +
           (angle ? QStringLiteral("°") : QStringLiteral(" mm"));
}
} // namespace

void SketchSnapOverlay::showFields(
    const SketchDynamicInput& input, QPointF cursor,
    const std::vector<std::optional<SketchEditorInteraction::FieldLine>>& lines) {
    fields_.clear();
    const double ratio = viewport_.devicePixelRatioF();
    for (std::size_t index = 0; index < input.fields().size(); ++index) {
        const auto& field = input.fields()[index];
        const bool current = index == input.current();
        std::optional<SketchEditorInteraction::FieldLine> line;
        if (index < lines.size() && lines[index]) {
            line = *lines[index];
            line->from = {line->from.first / ratio, line->from.second / ratio};
            line->to = {line->to.first / ratio, line->to.second / ratio};
        }
        fields_.push_back({fieldLabel(field.kind), fieldValue(field, current), current,
                           !field.text.empty(), line});
    }
    fieldCursor_ = cursor;
    // Test hook: the field texts as shown, "Länge: 25|" and so on.
    QStringList shown;
    for (const auto& field : fields_)
        shown << field.label + QStringLiteral(": ") + field.value;
    setProperty("dynamicFields", shown);
    // W12b test hook: how many fields sit on dimension lines.
    setProperty("fieldsOnLines",
                static_cast<int>(std::ranges::count_if(
                    fields_, [](const auto& field) { return field.line.has_value(); })));
    if (!fields_.empty())
        show();
    raise();
    update();
}

void SketchSnapOverlay::showOutlines(
    const std::vector<std::vector<std::pair<double, double>>>& outlines) {
    outlines_.clear();
    const double ratio = viewport_.devicePixelRatioF();
    for (const auto& outline : outlines) {
        QPolygonF polygon;
        for (const auto& [x, y] : outline)
            polygon << QPointF(x / ratio, y / ratio);
        outlines_.push_back(polygon);
    }
    setProperty("stageOutlines", static_cast<int>(outlines_.size())); // test hook
    if (!outlines_.empty())
        show();
    update();
}

void SketchSnapOverlay::paintFields(QPainter& painter) const {
    painter.setPen(QPen(accent_, 1.5));
    painter.setBrush(Qt::NoBrush);
    for (const auto& outline : outlines_)
        painter.drawPolyline(outline);
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    QPointF corner = fieldCursor_ + QPointF(18, 34);
    for (const auto& field : fields_) {
        if (field.line) {
            paintFieldBox(painter, field, paintFieldLine(painter, *field.line));
            continue;
        }
        const QString text = field.label + QStringLiteral("  ") + field.value;
        const QRectF size = painter.fontMetrics().boundingRect(text).adjusted(-9, -5, 9, 5);
        paintFieldBox(painter, field, corner + QPointF(size.width() / 2, size.height() / 2));
        corner += QPointF(size.width() + 6, 0);
    }
}

// A field's box centred on `centre`: the current one framed in the accent colour, a typed
// value in the accent colour. On a dimension line only the value shows, as on a drawing.
void SketchSnapOverlay::paintFieldBox(QPainter& painter, const FieldView& field,
                                      QPointF centre) const {
    const QString text =
        field.line ? field.value : field.label + QStringLiteral("  ") + field.value;
    const QRectF size = painter.fontMetrics().boundingRect(text).adjusted(-9, -5, 9, 5);
    const QRectF box(centre - QPointF(size.width() / 2, size.height() / 2), size.size());
    QPainterPath path;
    path.addRoundedRect(box, 6, 6);
    painter.setPen(QPen(field.current ? accent_ : muted_, field.current ? 2.0 : 1.0));
    painter.setBrush(panel_);
    painter.drawPath(path);
    painter.setPen(field.typed ? accent_ : muted_.darker(160));
    painter.drawText(box, Qt::AlignCenter, text);
}

QPointF SketchSnapOverlay::paintFieldLine(QPainter& painter,
                                          const SketchEditorInteraction::FieldLine& line) const {
    constexpr double standOff = 28.0, gap = 4.0, beyond = 6.0, arrow = 9.0;
    const QPointF out(line.outward.first, line.outward.second);
    const QPointF from(line.from.first, line.from.second);
    const QPointF to(line.to.first, line.to.second);
    const QPointF a = from + out * standOff;
    const QPointF b = to + out * standOff;
    painter.setPen(QPen(muted_, 1.0));
    if (!out.isNull()) {
        painter.drawLine(from + out * gap, a + out * beyond);
        painter.drawLine(to + out * gap, b + out * beyond);
    }
    painter.drawLine(a, b);
    const QPointF span = b - a;
    const double length = std::hypot(span.x(), span.y());
    if (length > 2.5 * arrow) {
        const QPointF unit = span / length;
        const QPointF side(-unit.y(), unit.x());
        painter.setBrush(muted_);
        for (const auto& [tip, direction] : {std::pair{a, -unit}, std::pair{b, unit}}) {
            const QPointF base = tip - direction * arrow;
            painter.drawPolygon(
                QPolygonF{tip, base + side * (arrow * 0.35), base - side * (arrow * 0.35)});
        }
    }
    return (a + b) / 2;
}

void SketchSnapOverlay::clear() {
    candidates_.clear();
    active_.reset();
    label_.clear();
    fields_.clear();
    outlines_.clear();
    setProperty("dynamicFields", QStringList{});
    setProperty("activeSnap", false);
    setProperty("snapLabel", QString{});
    hide();
}

bool SketchSnapOverlay::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        setGeometry(viewport_.rect());
    return QWidget::eventFilter(watched, event);
}

void SketchSnapOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(muted_, 1.4));
    for (const QPointF& point : candidates_)
        painter.drawEllipse(point, 4.0, 4.0);
    if (active_.has_value()) {
        painter.setPen(QPen(panel_, 2.0));
        painter.setBrush(accent_);
        painter.drawEllipse(*active_, 6.0, 6.0);
    }
    paintFields(painter);
    if (label_.isEmpty())
        return;
    QFont font = painter.font();
    font.setPixelSize(12);
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    const QRectF text = painter.fontMetrics().boundingRect(label_).adjusted(-8, -4, 8, 4);
    const QRectF box(cursor_ + QPointF(16, 14), text.size());
    QPainterPath path;
    path.addRoundedRect(box, 8, 8);
    painter.setPen(Qt::NoPen);
    painter.setBrush(accent_);
    painter.drawPath(path);
    painter.setPen(panel_);
    painter.drawText(box, Qt::AlignCenter, label_);
}

} // namespace voelund::viewport
