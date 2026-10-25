// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ToolPanelHost.hpp"

#include <QDoubleSpinBox>
#include <QPushButton>

// Lookups of the panel's actions and choices, and (user report 2026-10-25) the main value a
// tool types first, for the field beside its arrow in the view.
namespace voelund::desktop {

bool ToolPanelHost::isActionEnabled(const std::string& actionId) const {
    for (const auto& [id, input] : actionInputs_)
        if (id == actionId)
            return input->isEnabled();
    return false;
}

int ToolPanelHost::choiceIndex(const std::string& choiceId) const {
    for (const auto& [id, index] : choiceIndices_)
        if (id == choiceId)
            return index;
    return -1;
}

void ToolPanelHost::triggerAction(const std::string& actionId) {
    if (!isActionEnabled(actionId) || !definition_.actionTriggered)
        return;
    definition_.actionTriggered(actionId);
}

void ToolPanelHost::setMainFieldHandler(std::function<void(QDoubleSpinBox*)> handler) {
    mainFieldHandler_ = std::move(handler);
}

void ToolPanelHost::announceMainField(QDoubleSpinBox* field) {
    if (mainFieldHandler_)
        mainFieldHandler_(field);
}

// Enter in the view's field confirms like OK, when the tool allows it.
void ToolPanelHost::requestAccept() {
    if (acceptButton_->isEnabled())
        acceptButton_->click();
}

} // namespace voelund::desktop
