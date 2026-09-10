#include "WorkspaceToolbar.hpp"

#include <QAction>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>

#include <array>
#include <stdexcept>

namespace voelund::desktop {
namespace {

QString groupTitle(workspaces::CommandGroup group)
{
    switch (group) {
    case workspaces::CommandGroup::create: return QObject::tr("Create");
    case workspaces::CommandGroup::modify: return QObject::tr("Modify");
    case workspaces::CommandGroup::construct: return QObject::tr("Construct");
    }
    return {};
}

} // namespace

WorkspaceToolbar::WorkspaceToolbar(
    QMainWindow& window,
    const workspaces::WorkspaceModule& module)
    : module_(module)
{
    auto* toolbar = window.addToolBar(QString::fromStdString(module.title()));
    toolbar->setObjectName(QStringLiteral("workspaceToolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* workspaceTitle = new QLabel(
        QStringLiteral("  %1  ").arg(QString::fromStdString(module.title())).toUpper(),
        toolbar);
    toolbar->addWidget(workspaceTitle);
    toolbar->addSeparator();

    for (const workspaces::CommandDescriptor& command : module.commands()) {
        auto* action = new QAction(QString::fromStdString(command.title), toolbar);
        action->setToolTip(QString::fromStdString(command.toolTip));
        action->setCheckable(command.checkable);
        if (!command.shortcut.empty()) {
            action->setShortcut(QKeySequence(QString::fromStdString(command.shortcut)));
        }
        const std::function<void(bool)> trigger = command.trigger;
        QObject::connect(action, &QAction::triggered, toolbar,
            [trigger](bool checked) { trigger(checked); });
        actions_.emplace(command.id, action);
    }

    constexpr std::array groups{
        workspaces::CommandGroup::create,
        workspaces::CommandGroup::modify,
        workspaces::CommandGroup::construct};
    for (const workspaces::CommandGroup group : groups) {
        bool hasCommands = false;
        for (const workspaces::CommandDescriptor& command : module.commands()) {
            if (command.group == group && command.quickAccess) {
                toolbar->addAction(actions_.at(command.id));
                hasCommands = true;
            }
        }
        auto* menu = new QMenu(groupTitle(group), toolbar);
        for (const workspaces::CommandDescriptor& command : module.commands()) {
            if (command.group == group) {
                menu->addAction(actions_.at(command.id));
                hasCommands = true;
            }
        }
        if (!hasCommands) {
            delete menu;
            continue;
        }
        auto* groupButton = new QToolButton(toolbar);
        groupButton->setText(groupTitle(group) + QStringLiteral(" ▾"));
        groupButton->setPopupMode(QToolButton::InstantPopup);
        groupButton->setMenu(menu);
        toolbar->addWidget(groupButton);
        toolbar->addSeparator();
    }
    refreshEnabled();
}

QAction* WorkspaceToolbar::action(std::string_view commandId) const
{
    const auto match = actions_.find(std::string(commandId));
    if (match == actions_.end()) {
        throw std::out_of_range("Workspace command action is not registered");
    }
    return match->second;
}

void WorkspaceToolbar::refreshEnabled()
{
    for (const workspaces::CommandDescriptor& command : module_.commands()) {
        actions_.at(command.id)->setEnabled(
            !command.isEnabled || command.isEnabled());
    }
}

} // namespace voelund::desktop
