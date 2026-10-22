// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/DocumentParameters.hpp>

#include <QWidget>

#include <cstddef>
#include <functional>
#include <vector>

class QTableWidget;
class QTableWidgetItem;
class QToolButton;

namespace voelund::desktop {

// PAR02 (docs/parameter-table.md, user decision 2026-10-16): the document's parameters in a
// card under the model tree. Name and value are edited in the row; "+ Länge" and "+ Winkel" add
// one, "Entfernen" deletes the selected one (the shell asks when it is in use). The panel
// shows the document and reports edits; it changes nothing itself.
class ParameterPanel final : public QWidget {
  public:
    struct Handlers final {
        std::function<void(document::ParameterId, const QString&)> renamed;
        std::function<void(document::ParameterId, double)> valueChanged;
        std::function<void(document::ParameterUnit)> added;
        std::function<void(document::ParameterId)> removed;
        std::function<void(const QString&)> refused; // a value that is no number
    };
    struct Row final {
        document::DocumentParameter parameter;
        std::size_t uses;
        bool operator==(const Row&) const = default;
    };

    explicit ParameterPanel(QWidget* parent = nullptr);

    void setHandlers(Handlers handlers);
    // N01: "=A*2" typed into a value cell.
    void setExpressionHandler(std::function<void(document::ParameterId, const QString&)> handler) {
        expressionChanged_ = std::move(handler);
    }
    // Shows the rows; `editable` is off while a tool is open. Unchanged contents keep an edit
    // in progress.
    void show(std::vector<Row> rows, bool editable);
    // Opens the name of a parameter for editing (after adding it).
    void editName(document::ParameterId id);
    [[nodiscard]] int contentHeight() const;
    [[nodiscard]] QTableWidget& table() const noexcept { return *table_; }

  private:
    std::function<void(document::ParameterId, const QString&)> expressionChanged_;
    void changed(QTableWidgetItem* item);
    void removeSelected();

    QTableWidget* table_{nullptr};
    QToolButton* addLength_{nullptr};
    QToolButton* addAngle_{nullptr};
    QToolButton* remove_{nullptr};
    Handlers handlers_;
    std::vector<Row> rows_;
    bool editable_{true};
    bool filling_{false};
    bool shown_{false};
};

} // namespace voelund::desktop
