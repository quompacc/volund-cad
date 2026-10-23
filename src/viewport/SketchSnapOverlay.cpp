// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchSnapOverlay.hpp"

#include <QEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>

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
    }
    return {};
}

QString fieldValue(const SketchDynamicInput::Field& field, bool current) {
    const bool angle = field.kind == DynamicField::angle;
    if (!field.text.empty())
        return QString::fromStdString(field.text) + (current ? QStringLiteral("|") : QString{});
    return QLocale().toString(field.live, 'f', angle ? 1 : 2) +
           (angle ? QStringLiteral("°") : QStringLiteral(" mm"));
}
} // namespace

void SketchSnapOverlay::showFields(const SketchDynamicInput& input, QPointF cursor) {
    fields_.clear();
    for (std::size_t index = 0; index < input.fields().size(); ++index) {
        const auto& field = input.fields()[index];
        const bool current = index == input.current();
        fields_.push_back(
            {fieldLabel(field.kind), fieldValue(field, current), current, !field.text.empty()});
    }
    fieldCursor_ = cursor;
    // Test hook: the field texts as shown, "Länge: 25|" and so on.
    QStringList shown;
    for (const auto& field : fields_)
        shown << field.label + QStringLiteral(": ") + field.value;
    setProperty("dynamicFields", shown);
    if (!fields_.empty())
        show();
    raise();
    update();
}

void SketchSnapOverlay::paintFields(QPainter& painter) const {
    QFont font = painter.font();
    font.setPixelSize(12);
    painter.setFont(font);
    QPointF corner = fieldCursor_ + QPointF(18, 34);
    for (const auto& field : fields_) {
        const QString text = field.label + QStringLiteral("  ") + field.value;
        const QRectF size = painter.fontMetrics().boundingRect(text).adjusted(-9, -5, 9, 5);
        const QRectF box(corner, size.size());
        QPainterPath path;
        path.addRoundedRect(box, 6, 6);
        painter.setPen(QPen(field.current ? accent_ : muted_, field.current ? 2.0 : 1.0));
        painter.setBrush(panel_);
        painter.drawPath(path);
        painter.setPen(field.typed ? accent_ : muted_.darker(160));
        painter.drawText(box, Qt::AlignCenter, text);
        corner += QPointF(box.width() + 6, 0);
    }
}

void SketchSnapOverlay::clear() {
    candidates_.clear();
    active_.reset();
    label_.clear();
    fields_.clear();
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
