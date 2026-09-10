#pragma once

#include <voelund/application/CadSession.hpp>

#include <QTreeWidget>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace voelund::desktop {

class ModelTreePanel final : public QTreeWidget {
public:
    explicit ModelTreePanel(QWidget* parent = nullptr);

    void setFeatureSelectionHandler(std::function<void(std::uint64_t)> handler);
    void showFeatures(
        std::span<const document::FeatureDescriptor> features,
        std::optional<document::FeatureId> selectedFeature);
    void showStatuses(std::span<const application::FeatureRuntimeState> states);
    void selectFeature(document::FeatureId featureId);

private:
    [[nodiscard]] QTreeWidgetItem* featureItem(document::FeatureId featureId) const;

    std::function<void(std::uint64_t)> selectionHandler_;
};

} // namespace voelund::desktop
