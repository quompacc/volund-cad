// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ParameterPanel.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLocale>
#include <QShortcut>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>
#include <utility>

namespace voelund::desktop {
namespace {
constexpr int idRole = Qt::UserRole + 1;
enum Column { nameColumn = 0, valueColumn = 1, usesColumn = 2 };

QString unitText(document::ParameterUnit unit) {
    return unit == document::ParameterUnit::angle ? QStringLiteral("°") : QStringLiteral(" mm");
}
QString valueText(const document::DocumentParameter& parameter) {
    const QString value = QLocale().toString(parameter.value, 'f', 3) + unitText(parameter.unit);
    if (parameter.expression.empty())
        return value;
    // N01: the expression and what it gives.
    return QStringLiteral("=") + QString::fromStdString(parameter.expression) +
           QStringLiteral(" \u2192 ") + value;
}

// N01: what follows '=' in a value cell, without the shown result.
std::optional<QString> expressionIn(const QString& text) {
    if (!text.startsWith(QLatin1Char('=')))
        return std::nullopt;
    QString expression = text.mid(1);
    if (const auto arrow = expression.indexOf(QStringLiteral(" \u2192 ")); arrow >= 0)
        expression.truncate(arrow);
    return expression.trimmed();
}

// A number in a value cell, with or without its unit, in the user's or the C locale.
std::optional<double> numberIn(QString text) {
    for (const QString& unit : {QStringLiteral("mm"), QStringLiteral("°")})
        if (text.endsWith(unit))
            text = text.chopped(unit.size()).trimmed();
    bool ok = false;
    double value = QLocale().toDouble(text, &ok);
    if (!ok)
        value = QLocale::c().toDouble(text, &ok);
    return ok ? std::optional{value} : std::nullopt;
}
} // namespace

ParameterPanel::ParameterPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    table_ = new QTableWidget(0, 3, this);
    table_->setObjectName(QStringLiteral("parameterTable"));
    table_->setHorizontalHeaderLabels({tr("Name"), tr("Wert"), tr("Nutzung")});
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setSectionResizeMode(nameColumn, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(valueColumn, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(usesColumn, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                            QAbstractItemView::AnyKeyPressed);
    table_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(table_);
    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(4);
    const auto button = [this, buttons](const char* name, const QString& text, const QString& tip) {
        auto* result = new QToolButton(this);
        result->setObjectName(QLatin1String(name));
        result->setText(text);
        result->setToolTip(tip);
        result->setAutoRaise(true);
        buttons->addWidget(result);
        return result;
    };
    addLength_ = button("parameterAddLength", tr("+ Länge"), tr("Längenparameter anlegen"));
    addAngle_ = button("parameterAddAngle", tr("+ Winkel"), tr("Winkelparameter anlegen"));
    buttons->addStretch(1);
    remove_ = button("parameterRemove", tr("Entfernen"), tr("Gewählten Parameter entfernen"));
    layout->addLayout(buttons);

    connect(table_, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem* item) { changed(item); });
    connect(addLength_, &QToolButton::clicked, this, [this] {
        if (handlers_.added)
            handlers_.added(document::ParameterUnit::length);
    });
    connect(addAngle_, &QToolButton::clicked, this, [this] {
        if (handlers_.added)
            handlers_.added(document::ParameterUnit::angle);
    });
    connect(remove_, &QToolButton::clicked, this, [this] { removeSelected(); });
    auto* deleteKey = new QShortcut(QKeySequence::Delete, table_);
    deleteKey->setContext(Qt::WidgetShortcut);
    connect(deleteKey, &QShortcut::activated, this, [this] { removeSelected(); });
}

void ParameterPanel::setHandlers(Handlers handlers) { handlers_ = std::move(handlers); }

void ParameterPanel::show(std::vector<Row> rows, bool editable) {
    if (shown_ && rows == rows_ && editable == editable_)
        return; // an edit in progress stays open
    shown_ = true;
    rows_ = std::move(rows);
    editable_ = editable;
    const QSignalBlocker blocker(table_);
    filling_ = true;
    table_->setRowCount(static_cast<int>(rows_.size()));
    for (std::size_t index = 0; index < rows_.size(); ++index) {
        const auto& [parameter, uses] = rows_[index];
        const int row = static_cast<int>(index);
        const auto flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                           (editable ? Qt::ItemIsEditable : Qt::NoItemFlags);
        auto* name = new QTableWidgetItem(QString::fromStdString(parameter.name));
        name->setData(idRole, static_cast<qulonglong>(parameter.id.value()));
        name->setFlags(flags);
        if (!parameter.comment.empty())
            name->setToolTip(QString::fromStdString(parameter.comment));
        auto* value = new QTableWidgetItem(valueText(parameter));
        value->setData(idRole, static_cast<qulonglong>(parameter.id.value()));
        value->setFlags(flags);
        value->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* used =
            new QTableWidgetItem(uses == 0 ? QStringLiteral("–") : QStringLiteral("%1×").arg(uses));
        used->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        used->setTextAlignment(Qt::AlignCenter);
        used->setToolTip(
            tr("%n Verwendung(en) in Skizzen und Extrusionen", nullptr, static_cast<int>(uses)));
        table_->setItem(row, nameColumn, name);
        table_->setItem(row, valueColumn, value);
        table_->setItem(row, usesColumn, used);
    }
    filling_ = false;
    addLength_->setEnabled(editable);
    addAngle_->setEnabled(editable);
    remove_->setEnabled(editable && !rows_.empty());
    setToolTip(editable ? QString{}
                        : tr("Während ein Werkzeug offen ist, bleibt die Tabelle fest"));
}

void ParameterPanel::editName(document::ParameterId id) {
    for (int row = 0; row < table_->rowCount(); ++row)
        if (auto* item = table_->item(row, nameColumn);
            item && item->data(idRole).toULongLong() == id.value()) {
            table_->setCurrentItem(item);
            table_->editItem(item);
            return;
        }
}

int ParameterPanel::contentHeight() const {
    const int row = table_->verticalHeader()->defaultSectionSize();
    const int rows = std::max(1, table_->rowCount());
    return table_->horizontalHeader()->height() + rows * row + 2 * table_->frameWidth() + 6 +
           addLength_->sizeHint().height();
}

void ParameterPanel::changed(QTableWidgetItem* item) {
    if (filling_ || item == nullptr)
        return;
    const document::ParameterId id{item->data(idRole).toULongLong()};
    const auto row =
        std::ranges::find_if(rows_, [&](const Row& entry) { return entry.parameter.id == id; });
    if (row == rows_.end())
        return;
    shown_ = false; // the next show() redraws, also when the shell refused the edit
    if (item->column() == nameColumn) {
        if (handlers_.renamed)
            handlers_.renamed(id, item->text().trimmed());
        return;
    }
    if (item->column() != valueColumn)
        return;
    const QString text = item->text().trimmed();
    if (const auto expression = expressionIn(text); expression && expressionChanged_) {
        expressionChanged_(id, *expression);
        return;
    }
    const auto value = numberIn(text);
    if (!value) {
        // Back to the stored value; the shell says why.
        const QSignalBlocker blocker(table_);
        item->setText(valueText(row->parameter));
        if (handlers_.refused)
            handlers_.refused(tr("„%1“ ist keine Zahl").arg(item->text()));
        return;
    }
    if (handlers_.valueChanged)
        handlers_.valueChanged(id, *value);
}

void ParameterPanel::removeSelected() {
    if (!editable_ || !handlers_.removed)
        return;
    const auto* item = table_->currentItem();
    if (item == nullptr)
        return;
    handlers_.removed(
        document::ParameterId{table_->item(item->row(), nameColumn)->data(idRole).toULongLong()});
}

} // namespace voelund::desktop
