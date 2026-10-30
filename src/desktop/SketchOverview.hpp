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
class QPushButton;
class QToolButton;

namespace voelund::desktop {

// W19 (user report 2026-10-25, after SolidCraft's palette and FreeCAD's lists): the sketch
// card's overview under its status line. Loose ends are named, and ends lying on each other by
// position only are tied with one button. VP03 (user review 2026-10-30): relations, elements and
// view switches are details, folded away; a click in a list selects in the sketch.
class SketchOverview final : public QWidget {
  public:
    struct State final {
        std::span<const document::SketchElement> elements;
        std::span<const features::SketchConstraintAnnotation> relations;
        features::SketchLooseEnds looseEnds;
        std::optional<document::SketchConstraintId> selectedRelation;
        std::vector<document::SketchElementId> selectedElements;
        bool construction{false}; // W20: the selection's line type, else the drawing mode
    };
    struct Handlers final {
        std::function<void(document::SketchConstraintId)> relationPicked;
        std::function<void(document::SketchElementId)> elementPicked;
        std::function<void()> tie;
        std::function<void(bool construction)> lineType; // W20
        std::function<void(bool open)> details;          // VP03: the card's view switches
    };

    SketchOverview(Handlers handlers, QWidget* parent = nullptr);
    void display(const State& state);
    void openDetails();

  private:
    void fillRelations(const State& state);
    void fillElements(const State& state);

    Handlers handlers_;
    QWidget* endsRow_{nullptr};
    QLabel* ends_{nullptr};
    QPushButton* tie_{nullptr};
    QPushButton* construction_{nullptr}; // W20
    QToolButton* detailsToggle_{nullptr};
    QToolButton* relationsToggle_{nullptr};
    QListWidget* relations_{nullptr};
    QToolButton* elementsToggle_{nullptr};
    QListWidget* elements_{nullptr};
    bool filling_{false};
};

// The name of a sketch element in the list ("Linie 3", "Kreis 5" ...).
[[nodiscard]] QString sketchElementName(const document::SketchElement& element);

} // namespace voelund::desktop
