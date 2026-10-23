// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <voelund/desktop/MainWindow.hpp>

#include "InlineValueEdit.hpp"
#include "MainWindowState.hpp"

#include <voelund/application/CadSession.hpp>
#include <voelund/viewport/CadViewport.hpp>

#include <QCursor>
#include <QLocale>
#include <QStatusBar>

#include <cmath>
#include <numbers>

// W06 (as in Fusion and SolidCraft): after a drag of the move gizmo the dragged distance (or
// angle) stands selected in a field at the pointer. A typed value and Enter make it exact; the
// correction joins the same move ("Bewegen N", M11). Escape keeps what was dragged.
namespace voelund::desktop {
namespace {

std::optional<double> number(QString text) {
    for (const QString& unit : {QStringLiteral("mm"), QStringLiteral("°")})
        if (text.endsWith(unit))
            text = text.chopped(unit.size()).trimmed();
    bool ok = false;
    double value = QLocale::c().toDouble(text, &ok);
    if (!ok)
        value = QLocale().toDouble(text, &ok);
    return ok && std::isfinite(value) ? std::optional{value} : std::nullopt;
}

} // namespace

void MainWindow::offerMoveValue(document::FeatureId feature, std::array<double, 3> delta,
                                std::optional<int> axis) {
    const double length = std::hypot(delta[0], delta[1], delta[2]);
    if (length < 1e-9)
        return;
    if (impl_->moveValueEdit == nullptr)
        impl_->moveValueEdit = new InlineValueEdit(*impl_->viewport);
    impl_->moveValueEdit->setObjectName(QStringLiteral("moveValueEdit"));
    // A turn reports its angle in the first component.
    const bool turn = axis.has_value();
    const double shown = turn ? delta[0] : length;
    const QString hint = turn ? tr("Drehwinkel (Grad)") : tr("Verschiebung (mm)");
    impl_->moveValueEdit->open(
        impl_->viewport->mapFromGlobal(QCursor::pos()), QString::number(shown, 'g', 12), hint, {},
        {[this, feature, delta, axis, length, shown](const QString& text) {
             const auto value = number(text);
             if (!value) {
                 statusBar()->showMessage(tr("„%1“ ist keine Zahl").arg(text));
                 return;
             }
             const double change = *value - shown;
             if (std::abs(change) < 1e-12)
                 return;
             applyParameterCommand([this, feature, delta, axis, length,
                                    change](application::ErrorDisposition disposition) {
                 if (axis)
                     return impl_->session.moveBody(feature, {0.0, 0.0, 0.0}, *axis,
                                                    change * std::numbers::pi / 180.0, disposition);
                 const double scale = change / length;
                 return impl_->session.moveBody(
                     feature, {delta[0] * scale, delta[1] * scale, delta[2] * scale}, std::nullopt,
                     0.0, disposition);
             });
         },
         {},
         {}});
}

} // namespace voelund::desktop
