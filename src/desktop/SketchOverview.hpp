// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/SketchGeometry.hpp>
#include <voelund/features/SketchConstraintAnnotations.hpp>
#include <voelund/features/SketchLooseEnds.hpp>

#include <QWidget>

#include <functional>
#include <optional>
#include <span>
#include <vector>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QToolButton;

namespace voelund::desktop {

// W19 (user report 2026-10-25, after SolidCraft's palette and FreeCAD's lists): the sketch
// card's overview under its status line. A bar shows how much of the sketch is determined;
// loose ends are named, and ends lying on each other by position only are tied with one
// button; the relations and elements are listed (folded), a click selects one in the sketch.
class SketchOverview final : public QWidget {
  public:
    struct State final {
        std::span<const document::SketchElement> elements;
        std::span<const features::SketchConstraintAnnotation> relations;
        int degreesOfFreedom; // -1 while the sketch does not solve
        features::SketchLooseEnds looseEnds;
        std::optional<document::SketchConstraintId> selectedRelation;
        std::vector<document::SketchElementId> selectedElements;
    };
    struct Handlers final {
        std::function<void(document::SketchConstraintId)> relationPicked;
        std::function<void(document::SketchElementId)> elementPicked;
        std::function<void()> tie;
    };

    SketchOverview(Handlers handlers, QWidget* parent = nullptr);
    void display(const State& state);

  private:
    void fillRelations(const State& state);
    void fillElements(const State& state);

    Handlers handlers_;
    QProgressBar* progress_{nullptr};
    QWidget* endsRow_{nullptr};
    QLabel* ends_{nullptr};
    QPushButton* tie_{nullptr};
    QToolButton* relationsToggle_{nullptr};
    QListWidget* relations_{nullptr};
    QToolButton* elementsToggle_{nullptr};
    QListWidget* elements_{nullptr};
    bool filling_{false};
};

// The name of a sketch element in the list ("Linie 3", "Kreis 5" ...).
[[nodiscard]] QString sketchElementName(const document::SketchElement& element);

} // namespace voelund::desktop
