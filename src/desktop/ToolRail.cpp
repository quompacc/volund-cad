#include "ToolRail.hpp"
#include "CommandActions.hpp"
#include "MainWindowSupport.hpp"
#include "ShellIcons.hpp"
#include "SketchIcons.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <optional>

namespace voelund::desktop {
namespace {

constexpr int margin = 16;

std::optional<ShellIcon> iconFor(const QString& id) {
    static const std::map<QString, ShellIcon> icons{
        {QStringLiteral("solid.create.box"), ShellIcon::box},
        {QStringLiteral("solid.create.cylinder"), ShellIcon::cylinder},
        {QStringLiteral("solid.create.sketch"), ShellIcon::sketch},
        {QStringLiteral("solid.create.extrude_profiles"), ShellIcon::extrude},
        {QStringLiteral("solid.modify.transform"), ShellIcon::transform},
        {QStringLiteral("solid.modify.edit_sketch"), ShellIcon::editSketch},
        {QStringLiteral("solid.modify.push_pull"), ShellIcon::pushPull},
        {QStringLiteral("solid.modify.fillet"), ShellIcon::fillet},
        {QStringLiteral("repair.references"), ShellIcon::repair},
        {QStringLiteral("repair.all"), ShellIcon::repair},
        {QStringLiteral("view.command_search"), ShellIcon::search},
        {QStringLiteral("view.tool_rail"), ShellIcon::rail},
        {QStringLiteral("history.undo"), ShellIcon::undo},
        {QStringLiteral("history.redo"), ShellIcon::redo}};
    const auto match = icons.find(id);
    return match == icons.end() ? std::nullopt : std::optional{match->second};
}

} // namespace

ToolRail::ToolRail(viewport::CadViewport& viewport, const CommandActions& commands)
    : QFrame(&viewport), viewport_(viewport) {
    setObjectName(QStringLiteral("toolRail"));
    setAttribute(Qt::WA_StyledBackground);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(0);
    solidPage_ = new QWidget(this);
    auto* layout = new QVBoxLayout(solidPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    outer->addWidget(solidPage_);
    std::optional<workspaces::CommandGroup> previous;
    for (QAction* action : commands.ordered()) {
        const auto group = commands.group(*action);
        if (previous.has_value() && *previous != group) {
            auto* line = new QFrame(solidPage_);
            line->setObjectName(QStringLiteral("toolRailSeparator"));
            line->setFixedHeight(1);
            layout->addWidget(line);
        }
        previous = group;
        auto* button = new QToolButton(solidPage_);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(22, 22));
        button->setFixedWidth(84);
        button->setAutoRaise(true);
        layout->addWidget(button);
    }
    adjustSize();
    viewport.installEventFilter(this);
    place();
}

bool ToolRail::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        place();
    return QFrame::eventFilter(watched, event);
}

bool ToolRail::event(QEvent* event) {
    // Sizes depend on the style sheet, which arrives after construction.
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutRequest)
        adjustSize();
    return QFrame::event(event);
}

void ToolRail::place() {
    if (sketchPage_ != nullptr && sketchPage_->widget() != nullptr) {
        // As tall as the tools need, up to the viewport's height; scroll beyond that.
        const int available = std::max(160, viewport_.height() - 2 * margin - 12);
        sketchPage_->setFixedHeight(
            std::min(available, sketchPage_->widget()->sizeHint().height() + 4));
    }
    adjustSize();
    move(margin, margin);
    raise();
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
    sketchPage_ = new QScrollArea(this);
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
    static_cast<QVBoxLayout*>(this->layout())->addWidget(sketchPage_);
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
}

} // namespace voelund::desktop
