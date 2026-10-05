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
    };

    explicit ModelTreePanel(QWidget* parent = nullptr);

    void setHandlers(Handlers handlers);
    void setEyeIcons(const QIcon& shown, const QIcon& hidden);
    void showModel(const application::ModelOutline& outline,
                   std::span<const document::FeatureDescriptor> features,
                   std::optional<document::FeatureId> selectedFeature,
                   const ViewVisibility& visibility);
    void showStatuses(std::span<const application::FeatureRuntimeState> states);
    void selectFeature(document::FeatureId featureId);
    [[nodiscard]] QTreeWidgetItem* featureItem(document::FeatureId featureId) const;
    // AI04c: briefly marks features changed from outside the shell (MCP) with bold text in
    // the given color; the style sheet paints item backgrounds itself.
    void flashFeatures(std::span<const document::FeatureId> features, const QColor& color);
    [[nodiscard]] bool isFlashed(document::FeatureId feature) const;

    static constexpr int idRole = Qt::UserRole;
    static constexpr int nameRole = Qt::UserRole + 1;
    static constexpr int kindRole = Qt::UserRole + 2;

  private:
    QTreeWidgetItem* addItem(QTreeWidgetItem* parent, Kind kind, std::uint64_t id,
                             const QString& text, std::optional<bool> eye);

    void paintFlash(bool on);

    Handlers handlers_;
    std::vector<document::FeatureId> flashed_;
    QColor flashColor_;
    QIcon shownIcon_;
    QIcon hiddenIcon_;
};

} // namespace voelund::desktop
