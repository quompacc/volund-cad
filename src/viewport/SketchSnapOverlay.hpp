// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "SketchEditorInteraction.hpp"

#include <QColor>
#include <QWidget>

class QPainter;

namespace voelund::viewport {

// S04a: snap markers over the viewport while sketching. Nearby snap points show as small
// rings, the point the cursor snaps to lights up in the accent color, and a label names the
// relation a click will create. Drawn by Qt, so it needs no OCCT fonts.
class SketchSnapOverlay final : public QWidget {
  public:
    explicit SketchSnapOverlay(QWidget& viewport);

    void display(const SketchEditorInteraction::SnapFeedback& feedback, QPointF cursor,
                 double devicePixelRatio);
    void clear();
    // D01: the fields at the pointer while drawing; none hides them. W12b: a field with a
    // dimension line (framebuffer pixels) sits on it instead.
    void showFields(const SketchDynamicInput& input, QPointF cursor,
                    const std::vector<std::optional<SketchEditorInteraction::FieldLine>>& lines);
    void setColors(const QColor& accent, const QColor& muted, const QColor& panel);

  protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    QWidget& viewport_;
    std::vector<QPointF> candidates_;
    std::optional<QPointF> active_;
    QString label_;
    struct FieldView final {
        QString label;
        QString value;
        bool current;
        bool typed;
        std::optional<SketchEditorInteraction::FieldLine> line; // logical pixels
    };
    std::vector<FieldView> fields_;
    QPointF fieldCursor_;
    void paintFields(QPainter& painter) const;
    void paintFieldBox(QPainter& painter, const FieldView& field, QPointF centre) const;
    // W12b: extension lines, dimension line and arrows; returns where the field box sits.
    QPointF paintFieldLine(QPainter& painter, const SketchEditorInteraction::FieldLine& line) const;
    QPointF cursor_;
    QColor accent_{0xD9, 0x62, 0x2B};
    QColor muted_{0x5F, 0x68, 0x73};
    QColor panel_{0xFF, 0xFF, 0xFF};
};

[[nodiscard]] QString snapLabel(const SketchEditorInteraction::SnapFeedback& feedback);

} // namespace voelund::viewport
