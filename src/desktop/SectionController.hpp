// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/viewport/CadViewport.hpp>

#include <QObject>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QToolButton;
class QWidget;

namespace voelund::desktop {

class OverlayCard;

// N05 (user decision 2026-10-23): the section view. "Schnitt" is a view switch (header, command
// search); its small card at the bottom of the left column picks the plane (XY, XZ, YZ, a
// construction plane or the selected planar face or plane), its offset along the plane's
// normal (field or the arrow in the viewport) and which side stays. The section stays while
// other tools are used; it is view state only: no document change, no undo step.
class SectionController final : public QObject {
  public:
    struct Choice final {
        QString name;
        viewport::SectionPlane plane;
        bool operator==(const Choice&) const = default;
    };
    struct Hooks final {
        std::function<std::vector<Choice>()> constructionPlanes;
        // The selected planar face, base or construction plane, if any.
        std::function<std::optional<Choice>()> selectedPlane;
        // The card was shown or hidden: the left column is measured again.
        std::function<void()> placed;
    };

    SectionController(QWidget& window, viewport::CadViewport& viewport, Hooks hooks);

    [[nodiscard]] QAction& action() const noexcept { return *action_; }
    [[nodiscard]] OverlayCard& card() const noexcept { return *card_; }
    [[nodiscard]] static int cardHeight() noexcept;
    // Construction planes or the selection changed.
    void sync();
    // The section shown now; nullopt while it is off.
    [[nodiscard]] std::optional<viewport::SectionPlane> plane() const;

  private:
    void createCard(viewport::CadViewport& viewport);
    void toggled(bool on);
    void fillChoices(const std::optional<Choice>& selection);
    void choose(int index);
    void apply();
    void dragged(const viewport::SectionPlane& plane, bool finished);
    void useSelection();

    viewport::CadViewport& viewport_;
    Hooks hooks_;
    QAction* action_{nullptr};
    OverlayCard* card_{nullptr};
    QComboBox* planes_{nullptr};
    QToolButton* useSelection_{nullptr};
    QDoubleSpinBox* offset_{nullptr};
    QToolButton* flip_{nullptr};
    std::vector<Choice> choices_;
    std::optional<Choice> taken_; // the selection taken as plane, listed last
    bool filling_{false};
};

} // namespace voelund::desktop
