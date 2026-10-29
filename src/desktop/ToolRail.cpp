// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ToolRail.hpp"

#include "CommandActions.hpp"
#include "MainWindowSupport.hpp"
#include "OverlayCard.hpp"
#include "PillButton.hpp"
#include "ShellIcons.hpp"
#include "SketchIcons.hpp"
#include "ToolRailRow.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QRegion>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace voelund::desktop {
namespace {

std::optional<ShellIcon> iconFor(const QString& id) {
    static const std::map<QString, ShellIcon> icons{
        {QStringLiteral("solid.create.box"), ShellIcon::box},
        {QStringLiteral("solid.create.cylinder"), ShellIcon::cylinder},
        {QStringLiteral("solid.create.sphere"), ShellIcon::sphere}, // M27
        {QStringLiteral("solid.create.cone"), ShellIcon::cone},
        {QStringLiteral("solid.create.torus"), ShellIcon::torus},
        {QStringLiteral("solid.create.wedge"), ShellIcon::wedge},
        {QStringLiteral("solid.create.sketch"), ShellIcon::sketch},
        {QStringLiteral("solid.create.extrude_profiles"), ShellIcon::extrude},
        {QStringLiteral("solid.create.revolve"), ShellIcon::revolve},
        {QStringLiteral("solid.create.sweep"), ShellIcon::sweep}, // M23
        {QStringLiteral("solid.create.loft"), ShellIcon::loft},   // M24
        {QStringLiteral("solid.create.helix"), ShellIcon::helix}, // M26
        {QStringLiteral("solid.create.rib"), ShellIcon::rib},     // N12b
        {QStringLiteral("solid.modify.transform"), ShellIcon::transform},
        {QStringLiteral("solid.modify.edit_sketch"), ShellIcon::editSketch},
        {QStringLiteral("solid.modify.push_pull"), ShellIcon::pushPull},
        {QStringLiteral("solid.modify.fillet"), ShellIcon::fillet},
        {QStringLiteral("solid.modify.chamfer"), ShellIcon::chamfer},
        {QStringLiteral("solid.modify.hole"), ShellIcon::hole},
        {QStringLiteral("solid.modify.combine"), ShellIcon::combine},
        {QStringLiteral("solid.modify.shell"), ShellIcon::shell},
        {QStringLiteral("solid.modify.offset_face"), ShellIcon::pushPull},         // N13a
        {QStringLiteral("solid.modify.delete_face"), ShellIcon::repair},           // N13a
        {QStringLiteral("solid.modify.draft"), ShellIcon::draft},                  // M25
        {QStringLiteral("solid.modify.mirror"), ShellIcon::mirror},                // M09
        {QStringLiteral("solid.modify.split"), ShellIcon::split},                  // N12a
        {QStringLiteral("solid.modify.pattern_linear"), ShellIcon::patternLinear}, // M10
        {QStringLiteral("solid.modify.pattern_circular"), ShellIcon::patternCircular},
        {QStringLiteral("solid.inspect.measure"), ShellIcon::measure},      // M12
        {QStringLiteral("solid.inspect.interference"), ShellIcon::combine}, // N08
        {QStringLiteral("solid.create.datum_plane"), ShellIcon::datumPlane},
        {QStringLiteral("solid.create.datum_axis"), ShellIcon::datumAxis},
        {QStringLiteral("solid.create.midplane"), ShellIcon::datumPlane},     // N13b
        {QStringLiteral("solid.create.plane_points"), ShellIcon::datumPlane}, // N13b
        {QStringLiteral("solid.create.datum_point"), ShellIcon::datumPoint},  // N13b
        {QStringLiteral("repair.references"), ShellIcon::repair},
        {QStringLiteral("repair.all"), ShellIcon::repair},
        {QStringLiteral("view.command_search"), ShellIcon::search},
        {QStringLiteral("view.tool_rail"), ShellIcon::rail},
        {QStringLiteral("history.undo"), ShellIcon::undo},
        {QStringLiteral("history.redo"), ShellIcon::redo}};
    const auto match = icons.find(id);
    return match == icons.end() ? std::nullopt : std::optional{match->second};
}

struct RailGroup {
    const char* title;
    ShellIcon icon;
    std::vector<QString> commands;
};

// U14: the groups of the slim rail; a command missing here lands in "Mehr".
const std::vector<RailGroup>& railGroups() {
    static const std::vector<RailGroup> groups{
        {QT_TRANSLATE_NOOP("ToolRail", "Erstellen"),
         ShellIcon::box,
         {QStringLiteral("solid.create.box"), QStringLiteral("solid.create.cylinder"),
          QStringLiteral("solid.create.sphere"), QStringLiteral("solid.create.cone"),
          QStringLiteral("solid.create.torus"), QStringLiteral("solid.create.wedge")}},
        {QT_TRANSLATE_NOOP("ToolRail", "Skizze"),
         ShellIcon::sketch,
         {QStringLiteral("solid.create.sketch"), QStringLiteral("solid.modify.edit_sketch"),
          QStringLiteral("solid.create.extrude_profiles"), QStringLiteral("solid.create.revolve"),
          QStringLiteral("solid.create.sweep"), QStringLiteral("solid.create.loft"),
          QStringLiteral("solid.create.helix"), QStringLiteral("solid.create.rib")}},
        {QT_TRANSLATE_NOOP("ToolRail", "Bearbeiten"),
         ShellIcon::fillet,
         {QStringLiteral("solid.modify.push_pull"), QStringLiteral("solid.modify.fillet"),
          QStringLiteral("solid.modify.chamfer"), QStringLiteral("solid.modify.hole"),
          QStringLiteral("solid.modify.shell"), QStringLiteral("solid.modify.draft"),
          QStringLiteral("solid.modify.offset_face"), QStringLiteral("solid.modify.delete_face"),
          QStringLiteral("solid.modify.combine"), QStringLiteral("solid.modify.mirror"),
          QStringLiteral("solid.modify.split"), QStringLiteral("solid.modify.pattern_linear"),
          QStringLiteral("solid.modify.pattern_circular")}},
        {QT_TRANSLATE_NOOP("ToolRail", "Bewegen"),
         ShellIcon::transform,
         {QStringLiteral("solid.modify.transform")}},
        {QT_TRANSLATE_NOOP("ToolRail", "Mehr"),
         ShellIcon::datumPlane,
         {QStringLiteral("solid.create.datum_plane"), QStringLiteral("solid.create.datum_axis"),
          QStringLiteral("solid.create.midplane"), QStringLiteral("solid.create.plane_points"),
          QStringLiteral("solid.create.datum_point"), QStringLiteral("solid.inspect.measure"),
          QStringLiteral("solid.inspect.interference")}}};
    return groups;
}

// The row scrolls only by the wheel; a narrow rail moves its end into the » menu (L08).
QScrollArea* railScroll(QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setObjectName(QStringLiteral("railScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // User reports 2026-10-23/25 (Windows): a resizable widget was sized by the scroll area from
    // a stale minimum and squeezed the tools; the rail alone sizes its row (followRow).
    scroll->setWidgetResizable(false);
    // Its own minimum width would push the » button out of a bar that shows only that button.
    scroll->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return scroll;
}

QFrame* card(QWidget* parent, const char* name) {
    auto* frame = new QFrame(parent);
    frame->setObjectName(QLatin1String(name));
    frame->setProperty("overlayCard", true);
    frame->setAttribute(Qt::WA_StyledBackground);
    return frame;
}

// L08: a section line with its space as one widget; a section in the » menu leaves none.
QWidget* divider(QWidget* parent) {
    auto* section = new QWidget(parent);
    section->setObjectName(QStringLiteral("railSection"));
    section->setFixedSize(13, 22);
    auto* layout = new QHBoxLayout(section);
    layout->setContentsMargins(6, 0, 6, 0);
    auto* line = new QFrame(section);
    line->setObjectName(QStringLiteral("railDivider"));
    line->setFixedSize(1, 22);
    layout->addWidget(line);
    return section;
}

using rail::boundedWidth;
using rail::leadingRow;
using rail::rowWidth;
using rail::shownWidth;

constexpr int barPadding = 7;
constexpr int pillHeight = 34;

} // namespace

ToolRail::ToolRail(viewport::CadViewport& viewport, const CommandActions& commands)
    : QFrame(&viewport), viewport_(viewport) {
    setObjectName(QStringLiteral("toolRail"));
    bar_ = card(this, "railBar");
    auto* barLayout = new QHBoxLayout(bar_);
    barLayout->setContentsMargins(barPadding, 0, barPadding, 0);
    barLayout->setSpacing(0);
    scroll_ = railScroll(bar_);
    auto* content = new QWidget(scroll_);
    content->setObjectName(QStringLiteral("railContent"));
    auto* pages = new QHBoxLayout(content);
    pages->setContentsMargins(0, 0, 0, 0);
    pages->setSpacing(0);
    solidPage_ = new QWidget(content);
    auto* layout = new QHBoxLayout(solidPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    pages->addWidget(solidPage_);
    scroll_->setWidget(content);
    barLayout->addWidget(scroll_);
    createOverflow(*barLayout);
    // The wheel scrolls a bar that does not fit sideways.
    scroll_->viewport()->installEventFilter(this);
    flyout_ = card(this, "railFlyout");
    auto* flyoutLayout = new QVBoxLayout(flyout_);
    flyoutLayout->setContentsMargins(6, 8, 6, 6);
    flyoutLayout->setSpacing(0);
    flyout_->hide();

    const auto& groups = railGroups();
    std::vector<std::vector<QAction*>> members(groups.size());
    for (QAction* action : commands.ordered()) {
        const QString id = action->property(commandIdProperty).toString();
        std::size_t index = groups.size() - 1;
        for (std::size_t group = 0; group < groups.size(); ++group)
            if (std::ranges::find(groups[group].commands, id) != groups[group].commands.end())
                index = group;
        members[index].push_back(action);
    }
    for (std::size_t group = 0; group < groups.size(); ++group) {
        groupButtons_.push_back(nullptr);
        flyoutPages_.push_back(nullptr);
        if (members[group].empty())
            continue;
        auto* button = new PillButton(solidPage_);
        button->setProperty("railPill", true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(18, 18));
        button->setFixedHeight(pillHeight);
        layout->addWidget(button);
        if (members[group].size() == 1) { // a group of one is its command
            button->setDefaultAction(members[group].front());
            continue;
        }
        button->setObjectName(QStringLiteral("railGroupButton"));
        button->setText(tr(groups[group].title));
        button->setToolTip(tr(groups[group].title));
        button->setCheckable(true);
        button->setChevron(true);
        button->setProperty("railGroup", static_cast<int>(group));
        button->setProperty("railIcon", static_cast<int>(groups[group].icon));
        const int index = static_cast<int>(group);
        connect(button, &QToolButton::clicked, this, [this, index] {
            if (openGroup_ == index)
                closeFlyout();
            else
                openFlyout(index);
        });
        groupButtons_.back() = button;

        auto* page = new QWidget(flyout_);
        auto* pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(1);
        auto* title = new QLabel(tr(groups[group].title), page);
        title->setObjectName(QStringLiteral("railGroupTitle"));
        pageLayout->addWidget(title);
        for (QAction* action : members[group]) {
            auto* command = new QToolButton(page);
            command->setDefaultAction(action);
            command->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            command->setIconSize(QSize(20, 20));
            command->setMinimumWidth(184);
            command->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            command->setAutoRaise(true);
            command->setProperty("railGroup", index);
            pageLayout->addWidget(command);
            // Choosing a command closes the flyout; the command's panel takes over.
            connect(action, &QAction::triggered, this, [this] { closeFlyout(); });
        }
        flyoutLayout->addWidget(page);
        page->hide();
        flyoutPages_.back() = page;
    }
    viewport.installEventFilter(this);
    solidPage_->installEventFilter(this);
    place();
}

void ToolRail::createOverflow(QHBoxLayout& barLayout) {
    overflow_ = new QToolButton(bar_); // no PillButton: that draws a chevron beside a menu
    overflow_->setProperty("railPill", true);
    overflow_->setObjectName(QStringLiteral("railOverflowButton"));
    overflow_->setText(QStringLiteral("»"));
    overflow_->setToolTip(tr("Weitere Werkzeuge"));
    overflow_->setAccessibleName(tr("Weitere Werkzeuge"));
    overflow_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    overflow_->setFixedSize(pillHeight, pillHeight);
    overflow_->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(overflow_);
    connect(menu, &QMenu::aboutToShow, this, [this, menu] { fillOverflow(*menu); });
    overflow_->setMenu(menu);
    barLayout.addWidget(overflow_);
    overflow_->hide();
}

bool ToolRail::scrollRow(const QWheelEvent& event) {
    const QPoint delta = event.angleDelta();
    QScrollBar* bar = scroll_->horizontalScrollBar();
    bar->setValue(bar->value() - (delta.y() != 0 ? delta.y() : delta.x()) / 3);
    return true;
}

// The bar follows the viewport and its two cards; a page measured too early is measured again.
bool ToolRail::follows(const QObject* watched, QEvent::Type type) const {
    if (watched == &viewport_)
        return type == QEvent::Resize;
    if ((watched == solidPage_ || watched == sketchPage_) && type == QEvent::LayoutRequest)
        return true;
    return type == QEvent::Resize || type == QEvent::Move;
}

bool ToolRail::eventFilter(QObject* watched, QEvent* event) {
    if (scroll_ != nullptr && watched == scroll_->viewport()) {
        if (event->type() == QEvent::Wheel)
            return scrollRow(*static_cast<QWheelEvent*>(event));
        if (event->type() == QEvent::Resize)
            followRow();
    }
    if (follows(watched, event->type()))
        place();
    // A click into the view closes an open flyout, like a menu.
    if (watched == &viewport_ && event->type() == QEvent::MouseButtonPress && openGroup_ >= 0)
        closeFlyout();
    return QFrame::eventFilter(watched, event);
}

bool ToolRail::event(QEvent* event) {
    // Sizes depend on the style sheet, which arrives after construction, and on the screen's
    // scale and font (user report 2026-10-25: moving the window to another screen mended it).
    const bool result = QFrame::event(event);
    switch (event->type()) {
    case QEvent::StyleChange:
    case QEvent::LayoutRequest:
    case QEvent::FontChange:
    case QEvent::DevicePixelRatioChange:
        place();
        break;
    default:
        break;
    }
    return result;
}

std::pair<int, int> ToolRail::freeRow() {
    int left = overlay::margin;
    int right = viewport_.width() - overlay::margin;
    for (const char* name : {"projectCard", "shellHeader"}) {
        auto* card = viewport_.findChild<QWidget*>(QLatin1String(name), Qt::FindDirectChildrenOnly);
        if (card == nullptr)
            continue;
        if (std::ranges::find(watchedCards_, card) == watchedCards_.end()) {
            card->installEventFilter(this);
            watchedCards_.push_back(card);
        }
        if (card->geometry().center().x() < viewport_.width() / 2)
            left = std::max(left, card->geometry().right() + 1 + overlay::gap);
        else
            right = std::min(right, card->geometry().left() - overlay::gap);
    }
    return {left, right};
}

void ToolRail::place() {
    if (placing_)
        return;
    placing_ = true;
    const auto [left, right] = freeRow();
    const bool sketch = sketchShown_ && sketchPage_ != nullptr;
    QWidget* page = sketch ? sketchPage_ : solidPage_;
    const int frame = 2 * barPadding + 2;
    const int room = right - left - frame;
    const int wanted = announceWidth(*page, sketch);
    if (!sketch)
        setIconOnly(wanted > room);
    // At least the » button, which then holds every tool.
    const int least = boundedWidth(*overflow_) + frame;
    setProperty("leastWidth", least);
    const int width = std::max(least, std::min(fitRow(*page, room) + frame, right - left));
    // Centred in the window when it fits there, else in the free stretch.
    const int x = std::clamp((viewport_.width() - width) / 2, left, std::max(left, right - width));
    bar_->setGeometry(0, 0, width, overlay::barHeight);
    followRow();
    const QRect area = placeFlyout(QRect(0, 0, width, overlay::barHeight));
    setGeometry(x, overlay::margin, area.width(), area.height());
    // Only the cards take clicks; the space beside a narrow flyout belongs to the view.
    QRegion cards(bar_->geometry());
    if (flyout_->isVisible())
        cards += flyout_->geometry();
    setMask(cards);
    raise();
    placing_ = false;
}

// User reports 2026-10-23/24 (Windows, sketch opened): the scroll area sized its row by Qt's
// minimum size hint, which could still be the narrower page's, so the fixed-size sketch tools
// lay on top of each other. The row's least width is now the shown tools' own width, which
// the scroll area keeps whenever it resizes the row.
// User report 2026-10-25 (Windows): the tools still lay on top of each other until the window
// moved to another screen, which lays out everything anew. The shown page now never gets less
// than its tools' width, and the row is laid out here every time, not only when Qt asks.
void ToolRail::followRow() {
    bar_->layout()->activate();
    QWidget* content = scroll_->widget();
    const bool sketch = sketchShown_ && sketchPage_ != nullptr;
    QWidget& page = sketch ? *sketchPage_ : *solidPage_;
    const int needed = shownWidth(page);
    page.setMinimumWidth(needed);
    content->setMinimumWidth(needed);
    const QSize room = scroll_->viewport()->size();
    content->resize(std::max(room.width(), needed), room.height());
    content->layout()->invalidate();
    content->layout()->activate();
}

void ToolRail::setIconOnly(bool iconOnly) {
    if (iconOnly == iconOnly_)
        return;
    iconOnly_ = iconOnly;
    for (auto* pill : solidPage_->findChildren<QToolButton*>(Qt::FindDirectChildrenOnly))
        if (pill->property("railPill").toBool())
            pill->setToolButtonStyle(iconOnly ? Qt::ToolButtonIconOnly
                                              : Qt::ToolButtonTextBesideIcon);
}

int ToolRail::announceWidth(QWidget& page, bool sketch) {
    // The width with every tool and every text, as the header and project card see it.
    if (!sketch && !iconOnly_)
        solidTextWidth_ = rowWidth(page);
    const int wanted = sketch ? rowWidth(page) : solidTextWidth_;
    // L08: the header compacts when this width does not fit beside its full size.
    const int withFrame = wanted + 2 * barPadding + 2;
    if (property("wantedWidth").toInt() != withFrame) {
        setProperty("wantedWidth", withFrame);
        if (auto* header = viewport_.findChild<QWidget*>(QStringLiteral("shellHeader"),
                                                         Qt::FindDirectChildrenOnly))
            QCoreApplication::postEvent(header, new QEvent(QEvent::LayoutRequest));
    }
    return wanted;
}

QRect ToolRail::placeFlyout(QRect area) {
    if (!flyout_->isVisible() || openGroup_ < 0 || groupButtons_[openGroup_] == nullptr)
        return area;
    flyout_->adjustSize();
    const int below = std::max(0, groupButtons_[openGroup_]->mapTo(bar_, QPoint(0, 0)).x() - 6);
    flyout_->setGeometry(below, overlay::barHeight + 6, flyout_->sizeHint().width(),
                         flyout_->sizeHint().height());
    return area.united(flyout_->geometry());
}

int ToolRail::fitRow(QWidget& page, int room) {
    QLayout& layout = *page.layout();
    const bool cut = rowWidth(page) > room;
    const int menuWidth = cut ? boundedWidth(*overflow_) : 0;
    const auto [shown, used] =
        leadingRow(layout, cut ? room - menuWidth : std::numeric_limits<int>::max());
    showLeading(layout, shown);
    if (overflow_->isHidden() == cut)
        overflow_->setVisible(cut);
    return used + menuWidth;
}

void ToolRail::showLeading(QLayout& layout, int shown) {
    hidden_.clear();
    for (int index = 0; index < layout.count(); ++index) {
        QWidget* widget = layout.itemAt(index)->widget();
        if (index >= shown)
            hidden_.push_back(widget);
        if (widget->isHidden() == (index < shown)) // only changes, no layout loop
            widget->setVisible(index < shown);
    }
}

void ToolRail::fillOverflow(QMenu& menu) {
    menu.clear();
    for (QWidget* widget : hidden_) {
        auto* button = qobject_cast<QToolButton*>(widget);
        if (button == nullptr)
            continue;
        if (button->objectName() == QStringLiteral("railGroupButton")) {
            QMenu* group = menu.addMenu(button->icon(), button->toolTip());
            const int index = button->property("railGroup").toInt();
            for (auto* command : flyoutPages_[index]->findChildren<QToolButton*>())
                group->addAction(command->defaultAction());
        } else if (button->menu() != nullptr && button->defaultAction() != nullptr) {
            QMenu* family =
                menu.addMenu(button->defaultAction()->icon(), button->defaultAction()->text());
            family->addActions(button->menu()->actions());
        } else if (button->defaultAction() != nullptr) {
            menu.addAction(button->defaultAction());
        }
    }
}

void ToolRail::openFlyout(int group) {
    for (std::size_t index = 0; index < flyoutPages_.size(); ++index) {
        const bool open = static_cast<int>(index) == group;
        if (flyoutPages_[index] != nullptr)
            flyoutPages_[index]->setVisible(open);
        if (groupButtons_[index] != nullptr)
            groupButtons_[index]->setChecked(open);
    }
    openGroup_ = group;
    flyout_->show();
    place();
}

void ToolRail::closeFlyout() {
    for (QToolButton* button : groupButtons_)
        if (button != nullptr)
            button->setChecked(false);
    const bool wasOpen = openGroup_ >= 0;
    openGroup_ = -1;
    flyout_->hide();
    if (wasOpen)
        place();
}

void ToolRail::showSolid() {
    if (!sketchShown_)
        return;
    sketchShown_ = false;
    if (sketchPage_ != nullptr)
        sketchPage_->hide();
    solidPage_->show();
    place();
}

void ToolRail::showSketch(const std::vector<SketchToolActions::Section>& sections) {
    closeFlyout();
    if (sketchPage_ == nullptr)
        buildSketchPage(sections);
    if (sketchShown_)
        return;
    sketchShown_ = true;
    solidPage_->hide();
    sketchPage_->show();
    place();
}

void ToolRail::buildSketchPage(const std::vector<SketchToolActions::Section>& sections) {
    QWidget* content = scroll_->widget();
    sketchPage_ = new QWidget(content);
    sketchPage_->setObjectName(QStringLiteral("sketchToolPage"));
    auto* layout = new QHBoxLayout(sketchPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    bool first = true;
    for (const SketchToolActions::Section& section : sections) {
        // U14: the sections follow each other in one row, parted by a line; the section's
        // name stays as the buttons' accessible description.
        if (!first)
            layout->addWidget(divider(sketchPage_));
        first = false;
        // User decision 2026-10-05: symbols only, name and shortcut in the tooltip.
        // S14 (2026-10-10): one button per family; the chevron opens the other variants and
        // the button keeps the one used last.
        for (const auto& family : section.families) {
            auto* button = new PillButton(sketchPage_);
            button->setProperty("sketchTool", true);
            button->setAccessibleDescription(section.title);
            button->setDefaultAction(family.front());
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setIconSize(QSize(20, 20));
            button->setFixedSize(family.size() > 1 ? 46 : pillHeight, pillHeight);
            button->setChevronInset(7); // middle of the 14 px menu part
            if (family.size() > 1) {
                auto* menu = new QMenu(button);
                for (QAction* action : family) {
                    menu->addAction(action);
                    const auto use = [button, action] {
                        button->setDefaultAction(action);
                        button->setPopupMode(QToolButton::MenuButtonPopup);
                    };
                    connect(action, &QAction::triggered, button, use);
                    connect(action, &QAction::toggled, button, [use](bool on) {
                        if (on)
                            use();
                    });
                }
                button->setMenu(menu);
                button->setPopupMode(QToolButton::MenuButtonPopup);
            }
            layout->addWidget(button);
        }
    }
    static_cast<QHBoxLayout*>(content->layout())->addWidget(sketchPage_);
    sketchPage_->hide();
    sketchPage_->installEventFilter(this);
}

void applyCommandIcons(QWidget& window, ThemeMode mode) {
    const ThemeTokens& tokens = themeTokens(mode);
    for (QAction* action : window.findChildren<QAction*>()) {
        const auto id = action->property(commandIdProperty).toString();
        if (const auto icon = iconFor(id))
            action->setIcon(shellIcon(*icon, tokens.ink, tokens.accent));
        else if (const auto sketch = sketchIcon(id, tokens.ink, tokens.accent))
            action->setIcon(*sketch);
    }
    // U14: the rail's group buttons carry no command, only a symbol.
    for (QToolButton* button : window.findChildren<QToolButton*>(QStringLiteral("railGroupButton")))
        button->setIcon(shellIcon(static_cast<ShellIcon>(button->property("railIcon").toInt()),
                                  tokens.ink, tokens.accent));
}

} // namespace voelund::desktop
