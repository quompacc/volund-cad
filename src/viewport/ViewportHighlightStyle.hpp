// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/kernel/OcctCompatibility.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <AIS_InteractiveContext.hxx>
#include <Quantity_Color.hxx>

#include <QColor>

namespace voelund::viewport {

[[nodiscard]] Quantity_Color occtColor(const QColor& color);
// Hover and selection colors come from the viewport theme (U03).
void configureHighlightStyles(const occ::handle<AIS_InteractiveContext>& context,
                              const ViewportTheme& theme);

} // namespace voelund::viewport
