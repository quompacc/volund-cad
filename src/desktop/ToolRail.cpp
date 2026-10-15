#include "ToolRail.hpp"
#include "OverlayCard.hpp"
#include "PillButton.hpp"
#include "CommandActions.hpp"
#include "MainWindowSupport.hpp"
#include "ShellIcons.hpp"
#include "SketchIcons.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QRegion>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
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
        {QStringLiteral("solid.modify.transform"), ShellIcon::transform},
        {QStringLiteral("solid.modify.edit_sketch"), ShellIcon::editSketch},
        {QStringLiteral("solid.modify.push_pull"), ShellIcon::pushPull},
        {QStringLiteral("solid.modify.fillet"), ShellIcon::fillet},
        {QStringLiteral("solid.modify.chamfer"), ShellIcon::chamfer},
        {QStringLiteral("solid.modify.hole"), ShellIcon::hole},
        {QStringLiteral("solid.modify.combine"), ShellIcon::combine},
        {QStringLiteral("solid.modify.shell"), ShellIcon::shell},
        {QStringLiteral("solid.modify.draft"), ShellIcon::draft}, // M25
        {QStringLiteral("solid.modify.mirror"), ShellIcon::mirror}, // M09
        {QStringLiteral("solid.modify.pattern_linear"), ShellIcon::patternLinear}, // M10
        {QStringLiteral("solid.modify.pattern_circular"), ShellIcon::patternCircular},
        {QStringLiteral("solid.create.datum_plane"), ShellIcon::datumPlane},
        {QStringLiteral("solid.create.datum_axis"), ShellIcon::datumAxis},
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
          QStringLiteral("solid.create.helix")}},
        {QT_TRANSLATE_NOOP("ToolRail", "Bearbeiten"),
         ShellIcon::fillet,
         {QStringLiteral("solid.modify.push_pull"), QStringLiteral("solid.modify.fillet"),
          QStringLiteral("solid.modify.chamfer"), QStringLiteral("solid.modify.hole"),
          QStringLiteral("solid.modify.shell"), QStringLiteral("solid.modify.draft"),
          QStringLiteral("solid.modify.combine"), QStringLiteral("solid.modify.mirror"),
          QStringLiteral("solid.modify.pattern_linear"),
          QStringLiteral("solid.modify.pattern_circular")}},
        {QT_TRANSLATE_NOOP("ToolRail", "Bewegen"),
         ShellIcon::transform,
         {QStringLiteral("solid.modify.transform")}},
        {QT_TRANSLATE_NOOP("ToolRail", "Mehr"),
         ShellIcon::datumPlane,
         {QStringLiteral("solid.create.datum_plane"), QStringLiteral("solid.create.datum_axis")}}};
    return groups;
}

QFrame* card(QWidget* parent, const char* name) {
    auto* frame = new QFrame(parent);
    frame->setObjectName(QLatin1String(name));
    frame->setProperty("overlayCard", true);
    frame->setAttribute(Qt::WA_StyledBackground);
    return frame;
}

QFrame* divider(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setObjectName(QStringLiteral("railDivider"));
    line->setFixedSize(1, 22);
    return line;
}

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
    scroll_ = new QScrollArea(bar_);
    scroll_->setObjectName(QStringLiteral("railScroll"));
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setWidgetResizable(true);
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
    place();
}

bool ToolRail::eventFilter(QObject* watched, QEvent* event) {
    if (scroll_ != nullptr && watched == scroll_->viewport() && event->type() == QEvent::Wheel) {
        const QPoint delta = static_cast<QWheelEvent*>(event)->angleDelta();
        QScrollBar* bar = scroll_->horizontalScrollBar();
        bar->setValue(bar->value() - (delta.y() != 0 ? delta.y() : delta.x()) / 3);
        return true;
    }
    // The bar follows the viewport and the two cards it sits between.
    if ((watched == &viewport_ && event->type() == QEvent::Resize) ||
        (watched != &viewport_ &&
         (event->type() == QEvent::Resize || event->type() == QEvent::Move)))
        place();
    // A click into the view closes an open flyout, like a menu.
    if (watched == &viewport_ && event->type() == QEvent::MouseButtonPress && openGroup_ >= 0)
        closeFlyout();
    return QFrame::eventFilter(watched, event);
}

bool ToolRail::event(QEvent* event) {
    // Sizes depend on the style sheet, which arrives after construction.
    const bool result = QFrame::event(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutRequest)
        place();
    return result;
}

std::pair<int, int> ToolRail::freeRow() {
    int left = overlay::margin;
    int right = viewport_.width() - overlay::margin;
    for (const char* name : {"projectCard", "shellHeader"}) {
        auto* card = viewport_.findChild<QWidget*>(QLatin1String(name),
                                                    Qt::FindDirectChildrenOnly);
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
    QWidget* page = sketchShown_ && sketchPage_ != nullptr ? sketchPage_ : solidPage_;
    const int wanted = page->sizeHint().width() + 2 * barPadding + 2;
    const int width = std::max(120, std::min(wanted, right - left));
    // Centred in the window when it fits there, else in the free stretch.
    const int x = std::clamp((viewport_.width() - width) / 2, left, std::max(left, right - width));
    bar_->setGeometry(0, 0, width, overlay::barHeight);
    QRect area(0, 0, width, overlay::barHeight);
    if (flyout_->isVisible() && openGroup_ >= 0 && groupButtons_[openGroup_] != nullptr) {
        flyout_->adjustSize();
        const int below =
            std::max(0, groupButtons_[openGroup_]->mapTo(bar_, QPoint(0, 0)).x() - 6);
        flyout_->setGeometry(below, overlay::barHeight + 6, flyout_->sizeHint().width(),
                             flyout_->sizeHint().height());
        area = area.united(flyout_->geometry());
    }
    setGeometry(x, overlay::margin, area.width(), area.height());
    // Only the cards take clicks; the space beside a narrow flyout belongs to the view.
    QRegion cards(bar_->geometry());
    if (flyout_->isVisible())
        cards += flyout_->geometry();
    setMask(cards);
    raise();
    placing_ = false;
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
        if (!first) {
            layout->addSpacing(6);
            layout->addWidget(divider(sketchPage_));
            layout->addSpacing(6);
        }
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
