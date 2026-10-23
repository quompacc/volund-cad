// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "DisplayStyleMenu.hpp"

#include "MainWindowSupport.hpp"
#include "OverlayCard.hpp"

#include <QAction>
#include <QActionGroup>
#include <QEvent>
#include <QMenu>
#include <QSettings>
#include <QToolButton>

namespace voelund::desktop {
namespace {

using viewport::DisplayStyle;

struct StyleEntry final {
    DisplayStyle style;
    const char* id;
    const char* key; // the stored setting
    const char* text;
};
// In the order of DisplayStyle.
constexpr std::array<StyleEntry, 4> entries{{
    {DisplayStyle::shaded, "view.style.shaded", "shaded", QT_TRANSLATE_NOOP("Style", "Schattiert")},
    {DisplayStyle::shadedWithEdges, "view.style.shaded_edges", "shadedEdges",
     QT_TRANSLATE_NOOP("Style", "Schattiert mit Kanten")},
    {DisplayStyle::wireframe, "view.style.wireframe", "wireframe",
     QT_TRANSLATE_NOOP("Style", "Drahtgitter")},
    {DisplayStyle::hiddenEdges, "view.style.hidden_edges", "hiddenEdges",
     QT_TRANSLATE_NOOP("Style", "Verdeckte Kanten")},
}};

constexpr auto settingsKey = "view/displayStyle";
constexpr int cubeWidth = 144; // ViewCube

DisplayStyle storedStyle() {
    const auto stored = QSettings().value(settingsKey).toString();
    for (const auto& entry : entries)
        if (stored == QLatin1String(entry.key))
            return entry.style;
    return DisplayStyle::shadedWithEdges;
}

} // namespace

DisplayStyleMenu::DisplayStyleMenu(QWidget& window, viewport::CadViewport& viewport)
    : QObject(&window), viewport_(viewport) {
    auto* group = new QActionGroup(this);
    auto* menu = new QMenu(&window);
    for (const auto& entry : entries) {
        auto* action = new QAction(QObject::tr(entry.text), &window);
        action->setCheckable(true);
        setCommandId(*action, entry.id);
        window.addAction(action);
        group->addAction(action);
        menu->addAction(action);
        connect(action, &QAction::triggered, this, [this, style = entry.style] { choose(style); });
        actions_[static_cast<std::size_t>(entry.style)] = action;
    }
    button_ = new QToolButton(&viewport);
    button_->setObjectName(QStringLiteral("displayStyleButton"));
    button_->setText(QStringLiteral("◐"));
    button_->setToolTip(tr("Darstellung der Körper"));
    button_->setAccessibleName(tr("Darstellung"));
    button_->setPopupMode(QToolButton::InstantPopup);
    button_->setMenu(menu);
    button_->setFixedSize(32, 32);
    viewport.installEventFilter(this);
    choose(storedStyle());
    place();
}

QAction& DisplayStyleMenu::action(viewport::DisplayStyle style) const {
    return *actions_[static_cast<std::size_t>(style)];
}

void DisplayStyleMenu::choose(viewport::DisplayStyle style) {
    action(style).setChecked(true);
    viewport_.setDisplayStyle(style);
    QSettings().setValue(settingsKey, QLatin1String(entries[static_cast<std::size_t>(style)].key));
}

bool DisplayStyleMenu::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        place();
    return QObject::eventFilter(watched, event);
}

void DisplayStyleMenu::place() {
    // Beside the lower left corner of the view cube (U14: below the header card).
    button_->move(viewport_.width() - 12 - cubeWidth - button_->width() - 4,
                  overlay::cubeTop + overlay::cubeHeight - button_->height() - 8);
    button_->raise();
}

} // namespace voelund::desktop
