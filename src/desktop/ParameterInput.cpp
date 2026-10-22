// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ParameterInput.hpp"

#include <QCompleter>
#include <QLineEdit>

#include <QStringListModel>

#include <utility>

namespace voelund::desktop {
namespace {
bool isReference(const QString& text) { return text.trimmed().startsWith(QLatin1Char('=')); }
} // namespace

ParameterSpinBox::ParameterSpinBox(QWidget* parent)
    : QDoubleSpinBox(parent), model_(new QStringListModel(this)),
      completer_(new QCompleter(model_, this)) {
    completer_->setCaseSensitivity(Qt::CaseInsensitive);
    completer_->setCompletionMode(QCompleter::PopupCompletion);
    lineEdit()->setCompleter(completer_);
    setKeyboardTracking(false);
    connect(this, &QDoubleSpinBox::editingFinished, this, [this] { finished(); });
}

void ParameterSpinBox::setNamesProvider(std::function<QStringList()> provider) {
    names_ = std::move(provider);
}

void ParameterSpinBox::setBound(std::optional<QString> name) {
    if (bound_ == name)
        return;
    bound_ = std::move(name);
    // The shown text follows at once, also while the value stays the same.
    lineEdit()->setText(textFromValue(value()) + suffix());
    setToolTip(bound_
                   ? tr("Gebunden an den Parameter „%1“; eine Zahl löst die Bindung").arg(*bound_)
                   : tr("Zahl, =Name eines Parameters oder =Ausdruck wie =A*2"));
}

void ParameterSpinBox::setBindingRequested(std::function<void(std::optional<QString>)> requested) {
    requested_ = std::move(requested);
}

QString ParameterSpinBox::plainText() const {
    QString text = lineEdit()->text().trimmed();
    if (!suffix().isEmpty() && text.endsWith(suffix().trimmed()))
        text.chop(suffix().trimmed().size());
    return text.trimmed();
}

QValidator::State ParameterSpinBox::validate(QString& text, int& position) const {
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1Char('='))) {
        // N01: a name binds, any other text is an expression the shell checks.
        return trimmed.size() > 1 ? QValidator::Acceptable : QValidator::Intermediate;
    }
    if (bound_ && trimmed.startsWith(*bound_ + QStringLiteral(" =")))
        return QValidator::Acceptable;
    return QDoubleSpinBox::validate(text, position);
}

double ParameterSpinBox::valueFromText(const QString& text) const {
    if (isReference(text) || (bound_ && text.trimmed().startsWith(*bound_ + QStringLiteral(" ="))))
        return value();
    return QDoubleSpinBox::valueFromText(text);
}

QString ParameterSpinBox::textFromValue(double value) const {
    const QString number = QDoubleSpinBox::textFromValue(value);
    return bound_ ? *bound_ + QStringLiteral(" = ") + number : number;
}

void ParameterSpinBox::focusInEvent(QFocusEvent* event) {
    if (names_) {
        QStringList offered;
        for (const QString& name : names_())
            offered.append(QStringLiteral("=") + name);
        model_->setStringList(offered);
    }
    QDoubleSpinBox::focusInEvent(event);
}

void ParameterSpinBox::finished() {
    const QString text = plainText();
    if (text.startsWith(QLatin1Char('='))) {
        const QString name = text.mid(1).trimmed();
        // Back to the field's own text until the binding is confirmed.
        lineEdit()->setText(textFromValue(value()) + suffix());
        if (requested_ && !name.isEmpty())
            requested_(name);
        return;
    }
    // A number typed into a bound field: the same value still unbinds (rule 7).
    if (bound_ && !text.startsWith(*bound_ + QStringLiteral(" =")) && requested_)
        requested_(std::nullopt);
}

ParameterSpinBox* makeParameterInput(QWidget* parent, bool angle, bool count) {
    auto* input = new ParameterSpinBox(parent);
    input->setDecimals(count ? 0 : 3);
    if (count) {
        input->setRange(0, 1'000'000.0);
        input->setSingleStep(1.0);
    } else if (angle) {
        input->setRange(-360'000.0, 360'000.0);
        input->setSingleStep(5.0);
        input->setSuffix(QStringLiteral("°"));
    } else {
        input->setRange(0.01, 1'000'000.0);
        input->setSingleStep(1.0);
        input->setSuffix(QStringLiteral(" mm"));
    }
    input->setToolTip(QObject::tr("Zahl, =Name eines Parameters oder =Ausdruck wie =A*2"));
    return input;
}

} // namespace voelund::desktop
