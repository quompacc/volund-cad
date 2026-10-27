// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SelectionFilterMenu.hpp"

#include "MainWindowSupport.hpp"
#include "OverlayCard.hpp"

#include <QAction>
#include <QEvent>
#include <QMenu>
#include <QSettings>
#include <QStringList>
#include <QToolButton>

namespace voelund::desktop {
namespace {

struct FilterEntry final {
    const char* id;
    const char* key; // in the stored list of what is off
    const char* text;
};
// In the order of the fields of viewport::SelectionFilter.
constexpr std::array<FilterEntry, 5> entries{{
    {"select.filter.bodies", "bodies", QT_TRANSLATE_NOOP("Select", "Körper")},
    {"select.filter.faces", "faces", QT_TRANSLATE_NOOP("Select", "Flächen")},
    {"select.filter.edges", "edges", QT_TRANSLATE_NOOP("Select", "Kanten")},
    {"select.filter.planes", "planes", QT_TRANSLATE_NOOP("Select", "Ebenen und Achsen")},
    {"select.filter.sketches", "sketches", QT_TRANSLATE_NOOP("Select", "Skizzenflächen")},
}};

constexpr auto settingsKey = "view/selectionFilter"; // the kinds switched off
constexpr int cubeWidth = 144;                       // ViewCube

} // namespace

SelectionFilterMenu::SelectionFilterMenu(QWidget& window, viewport::CadViewport& viewport)
    : QObject(&window), viewport_(viewport) {
    const QStringList off = QSettings().value(settingsKey).toStringList();
    auto* menu = new QMenu(&window);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        auto* action = new QAction(QObject::tr(entry.text), &window);
        action->setCheckable(true);
        action->setChecked(!off.contains(QLatin1String(entry.key)));
        setCommandId(*action, entry.id);
        window.addAction(action);
        menu->addAction(action);
        connect(action, &QAction::toggled, this, [this] { changed(); });
        actions_[index] = action;
    }
    button_ = new QToolButton(&viewport);
    button_->setObjectName(QStringLiteral("selectionFilterButton"));
    button_->setText(QStringLiteral("⊡"));
    button_->setToolTip(tr("Auswahlfilter: was ein Klick oder eine Box wählt"));
    button_->setAccessibleName(tr("Auswahlfilter"));
    button_->setPopupMode(QToolButton::InstantPopup);
    button_->setMenu(menu);
    button_->setFixedSize(32, 32);
    viewport.installEventFilter(this);
    viewport_.setSelectionFilter(chosen());
    showFiltered(!off.isEmpty());
    place();
}

// A filter that takes something away shows a dot on the button.
void SelectionFilterMenu::showFiltered(bool filtered) {
    button_->setText(filtered ? QStringLiteral("⊡•") : QStringLiteral("⊡"));
}

viewport::SelectionFilter SelectionFilterMenu::chosen() const {
    return {actions_[0]->isChecked(), actions_[1]->isChecked(), actions_[2]->isChecked(),
            actions_[3]->isChecked(), actions_[4]->isChecked()};
}

void SelectionFilterMenu::sync(bool toolOpen) {
    toolOpen_ = toolOpen;
    viewport_.setSelectionFilter(toolOpen ? viewport::SelectionFilter{} : chosen());
}

void SelectionFilterMenu::changed() {
    QStringList off;
    for (std::size_t index = 0; index < entries.size(); ++index)
        if (!actions_[index]->isChecked())
            off << QLatin1String(entries[index].key);
    QSettings().setValue(settingsKey, off);
    showFiltered(!off.isEmpty());
    sync(toolOpen_);
}

bool SelectionFilterMenu::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        place();
    return QObject::eventFilter(watched, event);
}

void SelectionFilterMenu::place() {
    // Left of the display style button, beside the lower left corner of the view cube.
    button_->move(viewport_.width() - 12 - cubeWidth - 2 * (button_->width() + 4),
                  overlay::cubeTop + overlay::cubeHeight - button_->height() - 8);
    button_->raise();
}

} // namespace voelund::desktop
