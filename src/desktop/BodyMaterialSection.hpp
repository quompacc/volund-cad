// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include <voelund/document/BodyAttributes.hpp>
#include <voelund/kernel/MassProperties.hpp>

#include <QWidget>

#include <functional>
#include <optional>

class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace voelund::desktop {

// N07 (agent decision 2026-10-27, as Fusion's physical material and properties): the part of
// the body card with the material - none, one of the list (Materials.hpp) or a density of its
// own - and what the selected bodies weigh: mass, volume, surface, centre of mass and the
// inertia about it. For several bodies the values are totals. Reports a choice; it changes
// nothing itself.
class BodyMaterialSection final : public QWidget {
  public:
    using Choice = std::function<void(std::optional<document::BodyMaterial>)>;

    explicit BodyMaterialSection(QWidget* parent);

    void setChoiceHandler(Choice handler) { choice_ = std::move(handler); }
    // The material shown (of the first body), and the totals; no mass when a body has none.
    void display(const std::optional<document::BodyMaterial>& material,
                 const kernel::CombinedMass& totals, bool weighed);

  private:
    void chosen(int index);
    void report(std::optional<document::BodyMaterial> material);

    QComboBox* material_{nullptr};
    QDoubleSpinBox* density_{nullptr};
    QLabel* properties_{nullptr};
    Choice choice_;
    bool showing_{false};
};

// N07: the text of the properties in German number format, e.g. for the status bar or a copy.
[[nodiscard]] QString massPropertiesText(const kernel::CombinedMass& totals, bool weighed);

} // namespace voelund::desktop
