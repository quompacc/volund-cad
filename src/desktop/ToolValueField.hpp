// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <QLineEdit>
#include <QPointer>

#include <functional>

class QDoubleSpinBox;

namespace voelund::viewport {
class CadViewport;
}

namespace voelund::desktop {

// User report 2026-10-25 (as in Fusion): a tool's main value (fillet radius, chamfer distance,
// extrusion distance, shell thickness ...) is typed in a field beside its arrow in the view, or
// at the pointer without one, not only in the panel. The field mirrors the panel's field both
// ways: a typed number shows in the preview at once, a drag of the arrow shows in the field.
// Enter confirms like OK, Escape cancels the tool (the panel's keys).
class ToolValueField final : public QLineEdit {
  public:
    explicit ToolValueField(viewport::CadViewport& viewport);

    // The panel field to mirror; nullptr hides the field (the tool ended).
    void attach(QDoubleSpinBox* field, std::function<void()> accept);

  protected:
    void keyPressEvent(QKeyEvent* event) override;

  private:
    void typed(const QString& text);
    void follow();
    void place();

    viewport::CadViewport& viewport_;
    QPointer<QDoubleSpinBox> field_;
    std::function<void()> accept_;
    bool typing_{false};
};

} // namespace voelund::desktop
