#include "ToolRail.hpp"
#include "OverlayCard.hpp"
#include "CommandActions.hpp"
#include "MainWindowSupport.hpp"
#include "ShellIcons.hpp"
#include "SketchIcons.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QToolButton>
#include <QRegion>
#include <QVBoxLayout>

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
          QStringLiteral("solid.modify.combine")}},
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

} // namespace

ToolRail::ToolRail(viewport::CadViewport& viewport, const CommandActions& commands)
    : QFrame(&viewport), viewport_(viewport) {
    setObjectName(QStringLiteral("toolRail"));
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(8);
    column_ = card(this, "railColumn");
    auto* columnLayout = new QVBoxLayout(column_);
    columnLayout->setContentsMargins(5, 5, 5, 5);
    columnLayout->setSpacing(0);
    solidPage_ = new QWidget(column_);
    auto* layout = new QVBoxLayout(solidPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    columnLayout->addWidget(solidPage_);
    outer->addWidget(column_, 0, Qt::AlignTop);
    flyout_ = card(this, "railFlyout");
    auto* flyoutLayout = new QVBoxLayout(flyout_);
    flyoutLayout->setContentsMargins(6, 8, 6, 6);
    flyoutLayout->setSpacing(0);
    outer->addWidget(flyout_, 0, Qt::AlignTop);
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
        auto* button = new QToolButton(solidPage_);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(22, 22));
        button->setFixedWidth(66);
        button->setAutoRaise(true);
        layout->addWidget(button);
        if (members[group].size() == 1) { // a group of one is its command
            button->setDefaultAction(members[group].front());
            continue;
        }
        button->setObjectName(QStringLiteral("railGroupButton"));
        button->setText(tr(groups[group].title));
        button->setToolTip(tr(groups[group].title));
        button->setCheckable(true);
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
    if (watched == &viewport_ && event->type() == QEvent::Resize)
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

void ToolRail::place() {
    if (sketchPage_ != nullptr && sketchPage_->widget() != nullptr) {
        // As tall as the tools need, up to the viewport's height; scroll beyond that.
        const int available =
            std::max(160, viewport_.height() - overlay::columnTop - overlay::margin - 12);
        sketchPage_->setFixedHeight(
            std::min(available, sketchPage_->widget()->sizeHint().height() + 4));
    }
    layout()->activate();
    adjustSize();
    // Only the cards take clicks; the gap beside a short flyout belongs to the view.
    QRegion cards(column_->geometry());
    if (flyout_->isVisible())
        cards += flyout_->geometry();
    setMask(cards);
    move(overlay::railLeft, overlay::columnTop);
    raise();
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
    sketchPage_ = new QScrollArea(column_);
    sketchPage_->setObjectName(QStringLiteral("sketchToolPage"));
    sketchPage_->setFrameShape(QFrame::NoFrame);
    sketchPage_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sketchPage_->setWidgetResizable(true);
    auto* content = new QWidget(sketchPage_);
    content->setObjectName(QStringLiteral("sketchToolContent"));
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 4, 0);
    layout->setSpacing(4);
    for (const SketchToolActions::Section& section : sections) {
        auto* title = new QLabel(section.title, content);
        title->setObjectName(QStringLiteral("railGroupTitle"));
        layout->addWidget(title);
        // User decision 2026-10-05: symbols only, name and shortcut in the tooltip.
        // S14 (2026-10-10): one button per family; the arrow opens the other variants and
        // the button keeps the one used last.
        auto* grid = new QGridLayout;
        grid->setSpacing(2);
        constexpr int columns = 4;
        for (std::size_t index = 0; index < section.families.size(); ++index) {
            const auto& family = section.families[index];
            auto* button = new QToolButton(content);
            button->setDefaultAction(family.front());
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
            button->setIconSize(QSize(22, 22));
            button->setFixedSize(48, 38);
            button->setAutoRaise(true);
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
            grid->addWidget(button, static_cast<int>(index / columns),
                            static_cast<int>(index % columns));
        }
        layout->addLayout(grid);
    }
    layout->addStretch(1);
    sketchPage_->setWidget(content);
    sketchPage_->setFixedWidth(content->sizeHint().width() + 14);
    static_cast<QVBoxLayout*>(column_->layout())->addWidget(sketchPage_);
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
