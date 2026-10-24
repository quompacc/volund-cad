// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <utility>

class QLayout;
class QLayoutItem;
class QWidget;

// L08: the widths of the tool rail's row, as its layout lays the widgets out.
namespace voelund::desktop::rail {

// A widget's width whether it is shown or not, bounded like a layout bounds it.
[[nodiscard]] int boundedWidth(const QWidget& widget);
[[nodiscard]] int itemWidth(QLayoutItem& item);
// The width the shown widgets of a page take side by side (hidden ones are in the » menu).
[[nodiscard]] int shownWidth(QWidget& page);
// The width of every widget of a page side by side.
[[nodiscard]] int rowWidth(QWidget& page);
// The leading widgets within `limit` and the width they take; a divider never ends the row.
[[nodiscard]] std::pair<int, int> leadingRow(QLayout& layout, int limit);

} // namespace voelund::desktop::rail
