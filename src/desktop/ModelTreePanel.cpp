#include "ModelTreePanel.hpp"
#include "FeatureErrorCard.hpp"

#include <QBrush>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVariant>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

constexpr int eyeColumn = 1;

QString planeName(int index) {
    static const char* const names[] = {QT_TRANSLATE_NOOP("QObject", "XY-Ebene"),
                                        QT_TRANSLATE_NOOP("QObject", "XZ-Ebene"),
                                        QT_TRANSLATE_NOOP("QObject", "YZ-Ebene")};
    return QObject::tr(names[index]);
}

} // namespace

ModelTreePanel::ModelTreePanel(QWidget* parent) : QTreeWidget(parent) {
    setObjectName(QStringLiteral("modelTree"));
    setMinimumWidth(190); // U11: names stay readable in the smallest window
    setHeaderHidden(true);
    setColumnCount(2);
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(eyeColumn, QHeaderView::Fixed);
    header()->resizeSection(eyeColumn, 34);
    setRootIsDecorated(false);
    setIndentation(16);
    connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int column) {
        const auto kind = static_cast<Kind>(item->data(0, kindRole).toInt());
        const auto id = item->data(0, idRole).toULongLong();
        if (column == eyeColumn && item->data(eyeColumn, Qt::UserRole).isValid()) {
            if (handlers_.visibilityToggled)
                handlers_.visibilityToggled(kind, id);
            return;
        }
        if (kind == Kind::plane && handlers_.planeSelected)
            handlers_.planeSelected(static_cast<int>(id));
    });
    connect(this, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
                if (current == nullptr)
                    return;
                const auto kind = static_cast<Kind>(current->data(0, kindRole).toInt());
                const auto id = current->data(0, idRole).toULongLong();
                if (kind == Kind::feature && handlers_.featureSelected)
                    handlers_.featureSelected(id);
                else if (kind == Kind::body && handlers_.bodySelected)
                    handlers_.bodySelected(id);
            });
    // M01: a body row is renamed in place; an unchanged or emptied text changes nothing
    // (an empty name would bring back the numbered default only through the command).
    setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column != 0 || static_cast<Kind>(item->data(0, kindRole).toInt()) != Kind::body ||
            !handlers_.bodyRenamed)
            return;
        const QString name = item->text(0).trimmed();
        if (name.isEmpty() || name == item->data(0, nameRole).toString()) {
            const QSignalBlocker blocker(this);
            item->setText(0, item->data(0, nameRole).toString());
            return;
        }
        handlers_.bodyRenamed(item->data(0, idRole).toULongLong(), name);
    });
}

void ModelTreePanel::setHandlers(Handlers handlers) { handlers_ = std::move(handlers); }

void ModelTreePanel::setEyeIcons(const QIcon& shown, const QIcon& hidden,
                                 const QColor& hiddenText) {
    shownIcon_ = shown;
    hiddenIcon_ = hidden;
    hiddenText_ = hiddenText;
    for (QTreeWidgetItemIterator it(this); *it != nullptr; ++it) {
        const QVariant eye = (*it)->data(eyeColumn, Qt::UserRole);
        if (!eye.isValid())
            continue;
        (*it)->setIcon(eyeColumn, eye.toBool() ? shownIcon_ : hiddenIcon_);
        (*it)->setForeground(0, eye.toBool() ? QBrush() : QBrush(hiddenText_));
    }
}

QTreeWidgetItem* ModelTreePanel::addItem(QTreeWidgetItem* parent, Kind kind, std::uint64_t id,
                                         const QString& text, std::optional<bool> eye) {
    auto* item = parent == nullptr ? new QTreeWidgetItem(this) : new QTreeWidgetItem(parent);
    item->setText(0, text);
    item->setData(0, idRole, QVariant::fromValue<qulonglong>(id));
    item->setData(0, nameRole, text);
    item->setData(0, kindRole, static_cast<int>(kind));
    if (eye.has_value()) {
        item->setIcon(eyeColumn, *eye ? shownIcon_ : hiddenIcon_);
        item->setToolTip(eyeColumn, *eye ? tr("Ausblenden") : tr("Einblenden"));
        item->setData(eyeColumn, Qt::UserRole, *eye);
        if (!*eye)
            item->setForeground(0, hiddenText_);
    }
    return item;
}

void ModelTreePanel::showModel(const application::ModelOutline& outline,
                               std::span<const document::FeatureDescriptor> features,
                               std::optional<document::FeatureId> selectedFeature,
                               const ViewVisibility& visibility) {
    const QSignalBlocker blocker(this);
    clear();
    std::map<std::uint64_t, const document::FeatureDescriptor*> byId;
    for (const document::FeatureDescriptor& feature : features)
        byId.emplace(feature.id.value(), &feature);
    const bool hasBodies = !outline.bodies.empty();
    auto* origin =
        addItem(nullptr, Kind::origin, 0, tr("Ursprung"), visibility.originShown(hasBodies));
    for (int index = 0; index < 3; ++index)
        addItem(origin, Kind::plane, static_cast<std::uint64_t>(index + 1), planeName(index),
                visibility.planes[static_cast<std::size_t>(index)]);
    addItem(origin, Kind::grid, 0, tr("Raster auf XY"), visibility.grid);
    origin->setExpanded(true);

    const auto addFeature = [&](QTreeWidgetItem* parent, document::FeatureId id) {
        const auto match = byId.find(id.value());
        if (match == byId.end())
            return;
        auto* item = addItem(parent, Kind::feature, id.value(),
                             QString::fromStdString(match->second->name), std::nullopt);
        if (selectedFeature == id)
            setCurrentItem(item);
    };
    int number = 1;
    for (const application::OutlineBody& body : outline.bodies) {
        const auto bodyId = body.id.value();
        // M01: the user's name and the stored visibility.
        QString name =
            body.name.empty() ? tr("Körper %1").arg(number) : QString::fromStdString(body.name);
        if (body.consumed) // M21: taken into another body by a combination
            name += tr(" (aufgenommen)");
        ++number;
        auto* item = addItem(nullptr, Kind::body, bodyId, name, !body.hidden);
        if (!body.consumed) // the suffix is no part of the name
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        addItem(item, Kind::bodyOrigin, bodyId, tr("Ursprung"),
                visibility.shownBodyOrigins.contains(bodyId));
        for (const document::FeatureId feature : body.features)
            addFeature(item, feature);
        item->setExpanded(true);
    }
    for (const document::FeatureId feature : outline.loose)
        addFeature(nullptr, feature);
    paintFlash(true); // A rebuild during a running mark keeps it.
}

void ModelTreePanel::selectBody(document::BodyId body) {
    for (QTreeWidgetItemIterator it(this); *it != nullptr; ++it)
        if (static_cast<Kind>((*it)->data(0, kindRole).toInt()) == Kind::body &&
            (*it)->data(0, idRole).toULongLong() == body.value()) {
            const QSignalBlocker blocker(this);
            setCurrentItem(*it);
            return;
        }
}

void ModelTreePanel::editBody(document::BodyId body) {
    selectBody(body);
    if (QTreeWidgetItem* item = currentItem();
        item != nullptr && static_cast<Kind>(item->data(0, kindRole).toInt()) == Kind::body)
        editItem(item, 0);
}

void ModelTreePanel::showStatuses(std::span<const application::FeatureRuntimeState> states) {
    for (const application::FeatureRuntimeState& state : states) {
        QTreeWidgetItem* item = featureItem(state.id);
        if (item == nullptr)
            continue;
        const QString name = item->data(0, nameRole).toString();
        const bool hasError = state.status == application::FeatureRuntimeStatus::error;
        const bool blocked = state.status == application::FeatureRuntimeStatus::blocked;
        const bool suppressed = state.status == application::FeatureRuntimeStatus::suppressed;
        const bool rolledBack = state.status == application::FeatureRuntimeStatus::rolledBack;
        item->setText(0, hasError     ? tr("%1 [Fehler]").arg(name)
                         : blocked    ? tr("%1 [blockiert]").arg(name)
                         : suppressed ? tr("%1 [unterdrückt]").arg(name) // M14
                         : rolledBack ? tr("%1 [zurückgerollt]").arg(name)
                                      : name);
        item->setToolTip(0,
                         hasError || blocked ? QString::fromStdString(state.message) : QString{});
        item->setIcon(0, featureStatusIcon(state.status)); // U10
    }
}

void ModelTreePanel::selectFeature(document::FeatureId featureId) {
    const QSignalBlocker blocker(this);
    setCurrentItem(featureItem(featureId));
}

void ModelTreePanel::flashFeatures(std::span<const document::FeatureId> features,
                                   const QColor& color) {
    paintFlash(false);
    flashed_.assign(features.begin(), features.end());
    flashColor_ = color;
    paintFlash(true);
    const auto marked = flashed_;
    QTimer::singleShot(1500, this, [this, marked] {
        if (flashed_ != marked)
            return; // A newer change owns the marks.
        paintFlash(false);
        flashed_.clear();
    });
}

void ModelTreePanel::paintFlash(bool on) {
    for (const auto id : flashed_) {
        auto* item = featureItem(id);
        if (item == nullptr)
            continue;
        auto font = item->font(0);
        font.setBold(on);
        item->setFont(0, font);
        item->setForeground(0, on ? QBrush(flashColor_) : QBrush());
    }
}

QTreeWidgetItem* ModelTreePanel::featureItem(document::FeatureId featureId) const {
    for (QTreeWidgetItemIterator it(const_cast<ModelTreePanel*>(this)); *it != nullptr; ++it) {
        QTreeWidgetItem* item = *it;
        if (static_cast<Kind>(item->data(0, kindRole).toInt()) == Kind::feature &&
            item->data(0, idRole).toULongLong() == featureId.value())
            return item;
    }
    return nullptr;
}

int ModelTreePanel::contentHeight() const {
    const int row = sizeHintForRow(0) > 0 ? sizeHintForRow(0) : 28;
    int rows = 0;
    for (QTreeWidgetItemIterator it(const_cast<ModelTreePanel*>(this),
                                    QTreeWidgetItemIterator::NotHidden);
         *it != nullptr; ++it) {
        bool shown = true;
        for (const QTreeWidgetItem* parent = (*it)->parent(); parent != nullptr;
             parent = parent->parent())
            shown = shown && parent->isExpanded();
        rows += shown ? 1 : 0;
    }
    return rows * row + 2 * frameWidth() + 4;
}

} // namespace voelund::desktop
