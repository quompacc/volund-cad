// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QColor>
#include <QRectF>
#include <QString>
#include <QWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class QPainter;

namespace voelund::viewport {

// W18 (user report 2026-10-25, as SolidCraft): a sketch's relations as small symbol boxes on
// the geometry instead of text beside a point. A relation between two curves has a box on
// each. Boxes avoid each other along their curve, with a thin leader when pushed far; the box
// under the pointer lights up and names its relation. Drawn by Qt in logical pixels, so the
// boxes keep their size on a scaled screen. Clicks go through the viewport (glyphAt).
class SketchGlyphOverlay final : public QWidget {
  public:
    struct Glyph final {
        std::uint64_t id;
        std::string key; // the relation's symbol: "H", "V", "C", "||", "90", "=", "T" ...
        std::vector<SceneGlyph> anchors;
        bool conflicting{false};
        bool redundant{false};
        bool selected{false};
        std::vector<std::uint64_t> elements{};
    };
    // W21 (as SolidCraft): a dimension's value in a box of the panel colour, so lines behind
    // it do not cross it. The baseline starts `offset` framebuffer pixels from the projected
    // anchor (x right, y up), as OCCT placed its text.
    struct Value final {
        QString text;
        double x;
        double y;
        double z;
        QPointF offset;
        QColor color;
        double height; // logical pixels
    };
    // World point to logical viewport pixels; none while the view is not ready.
    using Projector = std::function<std::optional<QPointF>(double, double, double)>;

    explicit SketchGlyphOverlay(QWidget& viewport);

    void setGlyphs(std::vector<Glyph> glyphs);
    void setValues(std::vector<Value> values);
    [[nodiscard]] std::size_t valueCount() const noexcept { return values_.size(); }
    void setProjector(Projector projector) { projector_ = std::move(projector); }
    // The label text boxes the symbols keep clear of (logical pixels), read at each placing.
    void setObstacles(std::function<std::vector<QRectF>()> obstacles) {
        obstacles_ = std::move(obstacles);
    }
    void setColors(const QColor& text, const QColor& accent, const QColor& panel);
    // The relation whose box lies under a logical pixel (as last painted).
    [[nodiscard]] std::optional<std::uint64_t> glyphAt(QPointF position) const;
    // The pointer moved; true when another box lights up.
    bool hover(QPointF position);
    [[nodiscard]] std::optional<std::uint64_t> hovered() const noexcept { return hovered_; }
    // The elements the hovered relation binds; none while no box is hovered.
    [[nodiscard]] std::vector<std::uint64_t> hoveredElements() const;
    [[nodiscard]] std::size_t boxCount() const noexcept { return boxes_.size(); }
    // The boxes as last painted (logical pixels), for tests.
    [[nodiscard]] std::vector<QRectF> boxes() const;

  protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    struct Box final {
        std::size_t glyph; // index into glyphs_
        QRectF rect;
        QPointF anchor;
    };
    void place();
    void paintBox(QPainter& painter, const Glyph& glyph, const Box& box) const;
    [[nodiscard]] std::vector<QRectF> paintValues(QPainter& painter) const;
    void showWhenUsed();

    QWidget& viewport_;
    std::vector<Glyph> glyphs_;
    std::vector<Value> values_;
    std::vector<Box> boxes_;
    Projector projector_;
    std::function<std::vector<QRectF>()> obstacles_;
    std::optional<std::uint64_t> hovered_;
    QColor text_{0xCA, 0x47, 0x3D};
    QColor accent_{0xD9, 0x62, 0x2B};
    QColor panel_{0xFF, 0xFF, 0xFF};
};

// The German name of a relation symbol, as the tooltip shows it.
[[nodiscard]] QString glyphName(const std::string& key);

} // namespace voelund::viewport
