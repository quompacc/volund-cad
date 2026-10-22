// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "CommandActions.hpp"
#include "MainWindowSupport.hpp"

#include <QAction>
#include <QKeySequence>
#include <QWidget>

#include <functional>
#include <stdexcept>

namespace voelund::desktop {

CommandActions::CommandActions(QWidget& window, const workspaces::WorkspaceModule& module)
    : module_(module) {
    for (const workspaces::CommandDescriptor& command : module.commands()) {
        auto* action = new QAction(QString::fromStdString(command.title), &window);
        action->setToolTip(QString::fromStdString(command.toolTip));
        action->setCheckable(command.checkable);
        setCommandId(*action, command.id.c_str());
        if (!command.shortTitle.empty())
            action->setIconText(QString::fromStdString(command.shortTitle));
        if (!command.shortcut.empty())
            action->setShortcut(QKeySequence(QString::fromStdString(command.shortcut)));
        const std::function<void(bool)> trigger = command.trigger;
        QObject::connect(action, &QAction::triggered, &window,
                         [trigger](bool checked) { trigger(checked); });
        window.addAction(action);
        actions_.emplace(command.id, action);
        ordered_.push_back(action);
    }
    refreshEnabled();
}

QAction* CommandActions::action(std::string_view commandId) const {
    const auto match = actions_.find(commandId);
    if (match == actions_.end())
        throw std::out_of_range("Workspace command action is not registered");
    return match->second;
}

workspaces::CommandGroup CommandActions::group(const QAction& action) const {
    const std::string id = action.property(commandIdProperty).toString().toStdString();
    for (const workspaces::CommandDescriptor& command : module_.commands())
        if (command.id == id)
            return command.group;
    throw std::out_of_range("Action is not a workspace command");
}

void CommandActions::refreshEnabled() {
    for (const workspaces::CommandDescriptor& command : module_.commands())
        actions_.at(command.id)->setEnabled(!command.isEnabled || command.isEnabled());
}

} // namespace voelund::desktop
