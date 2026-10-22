// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include "../../src/desktop/ModelTreePanel.hpp"
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <stdexcept>
#include <voelund/application/CadSession.hpp>

// U06: the tree groups features by body, so tests find a feature's row by its ID (in the
// session's document order), never by a fixed position.
namespace voelund::test {
inline QTreeWidgetItem* featureRow(const QTreeWidget& tree, const application::CadSession& session,
                                   std::size_t index) {
    const auto id = session.features().at(index).id.value();
    using Panel = desktop::ModelTreePanel;
    for (QTreeWidgetItemIterator it(const_cast<QTreeWidget*>(&tree)); *it != nullptr; ++it)
        if ((*it)->data(0, Panel::kindRole).toInt() == static_cast<int>(Panel::Kind::feature) &&
            (*it)->data(0, Panel::idRole).toULongLong() == id)
            return *it;
    throw std::runtime_error("Feature row missing in the model tree");
}
} // namespace voelund::test
