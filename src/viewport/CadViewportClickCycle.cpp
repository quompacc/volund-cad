// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#if defined(VOELUND_VIEWPORT_QT_OPENGL)
#include "CadViewportQtOpenGlBackend.hpp"
#else
#include "CadViewportNativeBackend.hpp"
#endif

#include "ViewportContextGuard.hpp"

#include <QMouseEvent>

namespace voelund::viewport {

// W07 (as the pick list in Fusion and "Select Other" elsewhere): a plain click again at the
// same place with the view unchanged takes the next body candidate under the pointer - after
// an edge the face behind it, then the faces behind that, and round again. The first click
// only notes the place; it picks as before. Not while a tool is open: there a click again at
// the same place means the same pick (measure, push/pull).
void CadViewport::setClickCycleCheck(std::function<bool()> allowed) {
    impl_->clickCycleAllowed = std::move(allowed);
}

bool CadViewport::cycleClick(const QMouseEvent& event, QPoint position) {
    auto& cycle = impl_->clickCycle;
    const gp_Pnt eye = impl_->view->Camera()->Eye();
    const bool again = impl_->repeatsClick(position, eye) && event.modifiers() == Qt::NoModifier;
    cycle = {position, eye, again ? cycle.index + 1 : 0, impl_->releases};
    const auto owners = impl_->detectedBodyOwners();
    if (!again || owners.size() < 2)
        return false;
    const std::size_t index = cycle.index % owners.size();
    if (impl_->pickBodyEdge(owners[index], false))
        return true;
    impl_->notifyBodyOwner(owners[index], true);
    // The callback may have rebuilt the presentations: detect again before selecting.
    impl_->context->MoveTo(position.x(), position.y(), impl_->view, false);
    const auto current = impl_->detectedBodyOwners();
    impl_->context->ClearSelected(false);
    if (index < current.size())
        impl_->context->SetSelected(current[index], false);
    return true;
}

} // namespace voelund::viewport
