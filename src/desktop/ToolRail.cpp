#include "ToolRail.hpp"
#include "CommandActions.hpp"
#include "MainWindowSupport.hpp"
#include "ShellIcons.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QEvent>
#include <QToolButton>
#include <QVBoxLayout>

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
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(2);
    std::optional<workspaces::CommandGroup> previous;
    for (QAction* action : commands.ordered()) {
        const auto group = commands.group(*action);
        if (previous.has_value() && *previous != group) {
            auto* line = new QFrame(this);
            line->setObjectName(QStringLiteral("toolRailSeparator"));
            line->setFixedHeight(1);
            layout->addWidget(line);
        }
        previous = group;
        auto* button = new QToolButton(this);
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
    move(margin, margin);
    raise();
}

void applyCommandIcons(QWidget& window, ThemeMode mode) {
    const ThemeTokens& tokens = themeTokens(mode);
    for (QAction* action : window.findChildren<QAction*>()) {
        const auto icon = iconFor(action->property(commandIdProperty).toString());
        if (icon.has_value())
            action->setIcon(shellIcon(*icon, tokens.ink, tokens.accent));
    }
}

} // namespace voelund::desktop
