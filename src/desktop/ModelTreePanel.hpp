// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#pragma once

#include "ViewVisibility.hpp"

#include <voelund/application/CadSession.hpp>

#include <QIcon>
#include <QTreeWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace voelund::desktop {

// U06: the model tree of ADR 0012 — document origin with XY/XZ/YZ and the grid, then the
// bodies, each with its own origin and features. Column 1 holds the eye toggles.
class ModelTreePanel final : public QTreeWidget {
  public:
    enum class Kind { origin, plane, grid, body, bodyOrigin, feature };

    struct Handlers final {
        std::function<void(std::uint64_t)> featureSelected;
        std::function<void(int)> planeSelected; // 1 = XY, 2 = XZ, 3 = YZ
        std::function<void(Kind, std::uint64_t)> visibilityToggled;
        // M01: a body row picked, and a body row renamed in place (double-click or F2).
        std::function<void(std::uint64_t)> bodySelected;
        std::function<void(std::uint64_t, const QString&)> bodyRenamed;
    };

    explicit ModelTreePanel(QWidget* parent = nullptr);

    void setHandlers(Handlers handlers);
    void setBodyGroupHandler(std::function<void(std::uint64_t, bool)> handler) {
        bodyGrouped_ = std::move(handler);
    }
    // W09 (as in Fusion and SolidCraft): a right click on a row asks for its menu at a screen
    // position.
    void setMenuHandler(std::function<void(Kind, std::uint64_t, QPoint)> handler) {
        menuRequested_ = std::move(handler);
    }
    // Icons and the text color of hidden entries follow the theme; existing entries are
    // recolored, so a theme switch never leaves text in the old theme's color.
    void setEyeIcons(const QIcon& shown, const QIcon& hidden, const QColor& hiddenText);
    void showModel(const application::ModelOutline& outline,
                   std::span<const document::FeatureDescriptor> features,
                   std::optional<document::FeatureId> selectedFeature,
                   const ViewVisibility& visibility);
    void showStatuses(std::span<const application::FeatureRuntimeState> states);
    void selectFeature(document::FeatureId featureId);
    // M01: marks a body row as the current one, and opens its name for editing.
    void selectBody(document::BodyId body);
    void editBody(document::BodyId body);
    // N02b: marks the rows of a group of bodies (empty: none besides the current row).
    void markBodies(std::span<const document::BodyId> bodies);
    // N02b: the body IDs in the order of their rows.
    [[nodiscard]] std::vector<document::BodyId> bodyOrder() const;
    [[nodiscard]] QTreeWidgetItem* featureItem(document::FeatureId featureId) const;
    // AI04c: briefly marks features changed from outside the shell (MCP) with bold text in
    // the given color; the style sheet paints item backgrounds itself.
    void flashFeatures(std::span<const document::FeatureId> features, const QColor& color);
    // U14: height of all shown rows, so the tree card is no taller than its content.
    [[nodiscard]] int contentHeight() const;

    static constexpr int idRole = Qt::UserRole;
    static constexpr int nameRole = Qt::UserRole + 1;
    static constexpr int kindRole = Qt::UserRole + 2;

  protected:
    void mousePressEvent(QMouseEvent* event) override;

  private:
    QTreeWidgetItem* addItem(QTreeWidgetItem* parent, Kind kind, std::uint64_t id,
                             const QString& text, std::optional<bool> eye);

    void paintFlash(bool on);

    Handlers handlers_;
    // N02b: Strg-click adds or removes a body, Umschalt-click adds the bodies up to it.
    std::function<void(std::uint64_t, bool range)> bodyGrouped_;
    std::function<void(Kind, std::uint64_t, QPoint)> menuRequested_; // W09
    Qt::KeyboardModifiers pressModifiers_; // N02b: of the press that leads to a click
    std::vector<document::FeatureId> flashed_;
    QColor flashColor_;
    QIcon shownIcon_;
    QIcon hiddenIcon_;
    QColor hiddenText_;
};

} // namespace voelund::desktop
