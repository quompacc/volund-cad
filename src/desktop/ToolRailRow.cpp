// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "ToolRailRow.hpp"

#include <QLayout>
#include <QWidget>

namespace voelund::desktop::rail {

int boundedWidth(const QWidget& widget) {
    return widget.sizeHint()
        .expandedTo(widget.minimumSize())
        .boundedTo(widget.maximumSize())
        .width();
}

int itemWidth(QLayoutItem& item) {
    return item.widget() != nullptr ? boundedWidth(*item.widget()) : item.sizeHint().width();
}

int shownWidth(QWidget& page) {
    QLayout& layout = *page.layout();
    int width = 0;
    bool first = true;
    for (int index = 0; index < layout.count(); ++index) {
        QLayoutItem& item = *layout.itemAt(index);
        if (item.widget() != nullptr && item.widget()->isHidden())
            continue;
        width += itemWidth(item) + (first ? 0 : layout.spacing());
        first = false;
    }
    return width;
}

int rowWidth(QWidget& page) {
    QLayout& layout = *page.layout();
    int width = 0;
    for (int index = 0; index < layout.count(); ++index)
        width += itemWidth(*layout.itemAt(index)) + (index > 0 ? layout.spacing() : 0);
    return width;
}

std::pair<int, int> leadingRow(QLayout& layout, int limit) {
    int used = 0;
    int shown = 0;
    while (shown < layout.count()) {
        const int next =
            used + (shown > 0 ? layout.spacing() : 0) + itemWidth(*layout.itemAt(shown));
        if (next > limit)
            break;
        used = next;
        ++shown;
    }
    if (shown > 0 && shown < layout.count() &&
        layout.itemAt(shown - 1)->widget()->objectName() == QStringLiteral("railSection")) {
        used -= itemWidth(*layout.itemAt(shown - 1)) + (shown > 1 ? layout.spacing() : 0);
        --shown;
    }
    return {shown, used};
}

} // namespace voelund::desktop::rail
