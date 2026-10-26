// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SketchOverview.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <map>

namespace voelund::desktop {
namespace {

using Kind = document::SketchConstraintKind;

QString relationName(Kind kind) {
    static const std::map<Kind, QString> names{
        {Kind::horizontal, QObject::tr("Horizontal")},
        {Kind::vertical, QObject::tr("Vertikal")},
        {Kind::coincident, QObject::tr("Koinzident")},
        {Kind::parallel, QObject::tr("Parallel")},
        {Kind::perpendicular, QObject::tr("Senkrecht")},
        {Kind::pointOnObject, QObject::tr("Punkt auf Objekt")},
        {Kind::equal, QObject::tr("Gleich")},
        {Kind::tangent, QObject::tr("Tangential")},
        {Kind::symmetric, QObject::tr("Symmetrisch")},
        {Kind::midpoint, QObject::tr("Mittelpunkt")},
        {Kind::perpendicularBisector, QObject::tr("Mittelsenkrechte")},
        {Kind::concentric, QObject::tr("Konzentrisch")},
        {Kind::collinear, QObject::tr("Kollinear")},
        {Kind::horizontalPoints, QObject::tr("Horizontal (Punkte)")},
        {Kind::verticalPoints, QObject::tr("Vertikal (Punkte)")},
        {Kind::fixed, QObject::tr("Fixiert")},
        {Kind::distance, QObject::tr("Länge")},
        {Kind::radius, QObject::tr("Radius")},
        {Kind::diameter, QObject::tr("Durchmesser")},
        {Kind::pointDistance, QObject::tr("Abstand")},
        {Kind::pointLineDistance, QObject::tr("Abstand zur Linie")},
        {Kind::pointDistanceX, QObject::tr("Abstand X")},
        {Kind::pointDistanceY, QObject::tr("Abstand Y")},
        {Kind::coordinateX, QObject::tr("Lage X")},
        {Kind::coordinateY, QObject::tr("Lage Y")},
        {Kind::angle, QObject::tr("Winkel")}};
    const auto found = names.find(kind);
    return found != names.end() ? found->second : QObject::tr("Bedingung");
}

// "Länge 60 mm", "Horizontal"; a reference keeps its parentheses.
QString relationText(const features::SketchConstraintAnnotation& relation) {
    const QString name = relationName(relation.kind);
    if (relation.category != features::SketchAnnotationCategory::dimension)
        return name;
    const std::string value = relation.dimension ? relation.dimension->text : relation.label;
    return name + QStringLiteral("  ") + QString::fromStdString(value);
}

QToolButton* foldButton(QWidget* parent, const char* name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QLatin1String(name));
    button->setCheckable(true);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setArrowType(Qt::RightArrow);
    button->setAutoRaise(true);
    button->setFixedHeight(24); // compact rows, no gap between the folded lists
    QObject::connect(button, &QToolButton::toggled, button, [button](bool open) {
        button->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    });
    return button;
}

QListWidget* list(QWidget* parent, const char* name, QToolButton* toggle) {
    auto* widget = new QListWidget(parent);
    widget->setObjectName(QLatin1String(name));
    widget->setMaximumHeight(160);
    widget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    widget->hide();
    QObject::connect(toggle, &QToolButton::toggled, widget, &QWidget::setVisible);
    return widget;
}

} // namespace

QString sketchElementName(const document::SketchElement& element) {
    // In the order of document::SketchGeometry's alternatives.
    static const std::array<QString, 7> kinds{
        QObject::tr("Linie"),   QObject::tr("Rechteck"), QObject::tr("Kreis"), QObject::tr("Bogen"),
        QObject::tr("Ellipse"), QObject::tr("Punkt"),    QObject::tr("Spline")};
    const QString name =
        kinds.at(element.geometry.index()) + QLatin1Char(' ') + QString::number(element.id.value());
    const bool point = std::holds_alternative<document::SketchPointGeometry>(element.geometry);
    return element.construction && !point ? name + QObject::tr(" (Hilfsgeometrie)") : name;
}

SketchOverview::SketchOverview(Handlers handlers, QWidget* parent)
    : QWidget(parent), handlers_(std::move(handlers)) {
    setObjectName(QStringLiteral("sketchOverview"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    progress_ = new QProgressBar(this);
    progress_->setObjectName(QStringLiteral("sketchProgress"));
    progress_->setRange(0, 100);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(6);
    endsRow_ = new QWidget(this);
    auto* endsRow = new QHBoxLayout(endsRow_);
    endsRow->setContentsMargins(0, 0, 0, 0);
    ends_ = new QLabel(endsRow_);
    ends_->setObjectName(QStringLiteral("sketchLooseEnds"));
    ends_->setWordWrap(true);
    tie_ = new QPushButton(tr("Verbinden"), endsRow_);
    tie_->setObjectName(QStringLiteral("sketchTieButton"));
    tie_->setToolTip(tr("Enden, die nur aufeinander liegen, koinzident verbinden"));
    endsRow->addWidget(ends_, 1);
    endsRow->addWidget(tie_);
    relationsToggle_ = foldButton(this, "sketchRelationsToggle");
    relations_ = list(this, "sketchRelationList", relationsToggle_);
    elementsToggle_ = foldButton(this, "sketchElementsToggle");
    elements_ = list(this, "sketchElementList", elementsToggle_);
    layout->addWidget(progress_);
    layout->addWidget(endsRow_);
    layout->addWidget(relationsToggle_);
    layout->addWidget(relations_);
    layout->addWidget(elementsToggle_);
    layout->addWidget(elements_);
    connect(tie_, &QPushButton::clicked, this, [this] {
        if (handlers_.tie)
            handlers_.tie();
    });
    // The pick runs after the click: it refills the list the clicked item belongs to.
    connect(relations_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const document::SketchConstraintId id{item->data(Qt::UserRole).toULongLong()};
        if (!filling_ && handlers_.relationPicked)
            QTimer::singleShot(0, this, [this, id] { handlers_.relationPicked(id); });
    });
    connect(elements_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const document::SketchElementId id{item->data(Qt::UserRole).toULongLong()};
        if (!filling_ && handlers_.elementPicked)
            QTimer::singleShot(0, this, [this, id] { handlers_.elementPicked(id); });
    });
}

void SketchOverview::display(const State& state) {
    // As SolidCraft: the determined part of the parameters the elements have without relations.
    const int total = features::sketchFreedomWithoutRelations(state.elements);
    const int free = std::max(0, state.degreesOfFreedom);
    progress_->setValue(total > 0 ? 100 * (total - std::min(free, total)) / total : 0);
    progress_->setVisible(total > 0 && state.degreesOfFreedom >= 0);
    const auto open = state.looseEnds.open.size();
    const auto untied = state.looseEnds.untied.size();
    QStringList parts;
    if (open > 0)
        parts << tr("%n offene(s) Ende(n)", nullptr, static_cast<int>(open));
    if (untied > 0)
        parts << tr("%n Ecke(n) nur aufeinandergelegt", nullptr, static_cast<int>(untied));
    ends_->setText(parts.join(QStringLiteral(" · ")));
    ends_->setVisible(!parts.isEmpty());
    endsRow_->setVisible(!parts.isEmpty()); // no empty gap above the lists
    tie_->setVisible(untied > 0);
    filling_ = true;
    fillRelations(state);
    fillElements(state);
    filling_ = false;
}

void SketchOverview::fillRelations(const State& state) {
    relations_->clear();
    for (const auto& relation : state.relations) {
        auto* item = new QListWidgetItem(relationText(relation), relations_);
        item->setData(Qt::UserRole, QVariant::fromValue(relation.id.value()));
        item->setSelected(state.selectedRelation == relation.id);
    }
    relationsToggle_->setText(tr("Beziehungen (%1)").arg(state.relations.size()));
}

void SketchOverview::fillElements(const State& state) {
    elements_->clear();
    for (const auto& element : state.elements) {
        auto* item = new QListWidgetItem(sketchElementName(element), elements_);
        item->setData(Qt::UserRole, QVariant::fromValue(element.id.value()));
        item->setSelected(std::ranges::find(state.selectedElements, element.id) !=
                          state.selectedElements.end());
    }
    elementsToggle_->setText(tr("Elemente (%1)").arg(state.elements.size()));
}

} // namespace voelund::desktop
