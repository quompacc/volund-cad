// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once
#include <voelund/viewport/CadViewport.hpp>
class QAbstractButton;
class QDoubleSpinBox;
namespace voelund::apps {
void enableSmokeLogging();
bool verifyDesktopSmoke(viewport::CadViewport& viewport);
bool captureDesktopSmoke(QWidget& window, viewport::CadViewport& viewport);
bool clickSmokeButton(QAbstractButton& button);
bool enterSmokeValue(QDoubleSpinBox& input, const QString& text);
} // namespace voelund::apps
