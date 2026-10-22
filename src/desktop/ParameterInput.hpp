// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QDoubleSpinBox>
#include <QStringList>

#include <functional>
#include <optional>

class QCompleter;
class QStringListModel;

namespace voelund::desktop {

// PAR02 (docs/parameter-table.md, user decision 2026-10-16): a number field that also takes
// "=Name" for a document parameter of its unit, offering the names while typing. A bound field
// shows "Name = value unit"; a number typed into it unbinds it.
class ParameterSpinBox final : public QDoubleSpinBox {
  public:
    explicit ParameterSpinBox(QWidget* parent = nullptr);

    // The names offered after "=", asked again whenever the field gains focus.
    void setNamesProvider(std::function<QStringList()> provider);
    // nullopt: not bound.
    void setBound(std::optional<QString> name);
    [[nodiscard]] const std::optional<QString>& bound() const noexcept { return bound_; }
    // Editing finished with "=Name" (the name) or, while bound, with a number (nullopt).
    void setBindingRequested(std::function<void(std::optional<QString>)> requested);

  protected:
    QValidator::State validate(QString& text, int& position) const override;
    double valueFromText(const QString& text) const override;
    QString textFromValue(double value) const override;
    void focusInEvent(QFocusEvent* event) override;

  private:
    void finished();
    [[nodiscard]] QString plainText() const;

    std::optional<QString> bound_;
    std::function<QStringList()> names_;
    std::function<void(std::optional<QString>)> requested_;
    QStringListModel* model_{nullptr};
    QCompleter* completer_{nullptr};
};

// A length (mm), angle (degrees) or (N01b) count field that takes parameters, set up like the
// plain ones.
[[nodiscard]] ParameterSpinBox* makeParameterInput(QWidget* parent, bool angle, bool count = false);

} // namespace voelund::desktop
