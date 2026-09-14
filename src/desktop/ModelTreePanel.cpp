#include "ModelTreePanel.hpp"

#include <QSignalBlocker>
#include <QTreeWidgetItem>
#include <QVariant>

#include <utility>

namespace voelund::desktop {

ModelTreePanel::ModelTreePanel(QWidget* parent) : QTreeWidget(parent) {
    setHeaderHidden(true);
    connect(this, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
                if (current != nullptr && selectionHandler_) {
                    selectionHandler_(current->data(0, Qt::UserRole).toULongLong());
                }
            });
}

void ModelTreePanel::setFeatureSelectionHandler(std::function<void(std::uint64_t)> handler) {
    selectionHandler_ = std::move(handler);
}

void ModelTreePanel::showFeatures(std::span<const document::FeatureDescriptor> features,
                                  std::optional<document::FeatureId> selectedFeature) {
    const QSignalBlocker blocker(this);
    clear();
    auto* bodyItem = new QTreeWidgetItem(this);
    bodyItem->setText(0, tr("Body"));
    bodyItem->setFlags(bodyItem->flags() & ~Qt::ItemIsSelectable);
    for (const document::FeatureDescriptor& feature : features) {
        auto* item = new QTreeWidgetItem(bodyItem);
        const QString name = QString::fromStdString(feature.name);
        item->setText(0, name);
        item->setData(0, Qt::UserRole, QVariant::fromValue<qulonglong>(feature.id.value()));
        item->setData(0, Qt::UserRole + 1, name);
        if (selectedFeature == feature.id) {
            setCurrentItem(item);
        }
    }
    bodyItem->setExpanded(true);
}

void ModelTreePanel::showStatuses(std::span<const application::FeatureRuntimeState> states) {
    for (const application::FeatureRuntimeState& state : states) {
        QTreeWidgetItem* item = featureItem(state.id);
        if (item == nullptr) {
            continue;
        }
        const QString name = item->data(0, Qt::UserRole + 1).toString();
        const bool hasError = state.status == application::FeatureRuntimeStatus::error;
        const bool blocked = state.status == application::FeatureRuntimeStatus::blocked;
        item->setText(0, hasError  ? tr("%1 [error]").arg(name)
                         : blocked ? tr("%1 [blocked]").arg(name)
                                   : name);
        item->setToolTip(0,
                         hasError || blocked ? QString::fromStdString(state.message) : QString{});
    }
}

void ModelTreePanel::selectFeature(document::FeatureId featureId) {
    const QSignalBlocker blocker(this);
    setCurrentItem(featureItem(featureId));
}

QTreeWidgetItem* ModelTreePanel::featureItem(document::FeatureId featureId) const {
    QTreeWidgetItem* body = topLevelItem(0);
    if (body == nullptr) {
        return nullptr;
    }
    for (int index = 0; index < body->childCount(); ++index) {
        QTreeWidgetItem* item = body->child(index);
        if (item->data(0, Qt::UserRole).toULongLong() == featureId.value()) {
            return item;
        }
    }
    return nullptr;
}

} // namespace voelund::desktop
