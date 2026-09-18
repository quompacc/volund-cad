#include "ReferenceRepairDialog.hpp"
#include "CommandFeedback.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

namespace voelund::desktop {
namespace {
class RepairPreview final : public QGraphicsView {
  public:
    explicit RepairPreview(QWidget* parent) : QGraphicsView(parent) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

  protected:
    void resizeEvent(QResizeEvent* event) override {
        QGraphicsView::resizeEvent(event);
        if (scene())
            fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
    }
};
QString role(const document::SubshapeOrigin& origin) {
    if (!origin.role)
        return QObject::tr("Extrusion end face");
    using kernel::PrimitiveRole;
    switch (*origin.role) {
    case PrimitiveRole::boxNegativeX:
        return QObject::tr("Left (−X)");
    case PrimitiveRole::boxPositiveX:
        return QObject::tr("Right (+X)");
    case PrimitiveRole::boxNegativeY:
        return QObject::tr("Front (−Y)");
    case PrimitiveRole::boxPositiveY:
        return QObject::tr("Back (+Y)");
    case PrimitiveRole::boxNegativeZ:
        return QObject::tr("Bottom (−Z)");
    case PrimitiveRole::boxPositiveZ:
        return QObject::tr("Top (+Z)");
    case PrimitiveRole::cylinderStart:
        return QObject::tr("Cylinder start");
    case PrimitiveRole::cylinderEnd:
        return QObject::tr("Cylinder end");
    case PrimitiveRole::cylinderSide:
        return QObject::tr("Cylinder side");
    }
    return {};
}
bool attachment(const document::SketchConstraint& c) {
    return c.projectedEdge.has_value() ||
           ((c.kind == document::SketchConstraintKind::coordinateX ||
             c.kind == document::SketchConstraintKind::coordinateY) &&
            c.auxiliaryValues.size() == 2);
}
} // namespace
ReferenceRepairDialog::ReferenceRepairDialog(application::CadSession& session,
                                             document::FeatureId sketch,
                                             std::function<void()> changed, QWidget* parent)
    : QDialog(parent), session_(session), sketch_(sketch), changed_(std::move(changed)) {
    setObjectName("referenceRepairDialog");
    setWindowTitle(tr("Repair sketch references"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    resize(760, 700);
    auto* layout = new QVBoxLayout(this);
    original_ = new QLabel(this);
    original_->setWordWrap(true);
    layout->addWidget(original_);
    auto* instructions = new QLabel(tr("Choose a reference and a replacement. Matching geometry is "
                                       "only a suggestion. Cancel leaves the document unchanged."),
                                    this);
    instructions->setWordWrap(true);
    layout->addWidget(instructions);
    target_ = new QComboBox(this);
    target_->setObjectName("repairTarget");
    layout->addWidget(target_);
    candidates_ = new QListWidget(this);
    candidates_->setObjectName("repairCandidates");
    candidates_->setWordWrap(true);
    layout->addWidget(candidates_, 2);
    auto* caption = new QLabel(
        tr("Support outline in the selected face plane; orange marks the chosen edge point."),
        this);
    caption->setWordWrap(true);
    layout->addWidget(caption);
    preview_ = new RepairPreview(this);
    preview_->setMinimumHeight(170);
    layout->addWidget(preview_, 2);
    previewInfo_ = new QLabel(this);
    previewInfo_->setObjectName("repairPreviewCoordinates");
    previewInfo_->setWordWrap(true);
    layout->addWidget(previewInfo_);
    message_ = new QLabel(this);
    message_->setObjectName("repairMessage");
    message_->setWordWrap(true);
    layout->addWidget(message_);
    keepErrors_ = new QCheckBox(
        tr("Keep this repair with the remaining model errors (Undo available)"), this);
    keepErrors_->setObjectName("repairKeepErrors");
    keepErrors_->hide();
    layout->addWidget(keepErrors_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    apply_ = buttons->addButton(tr("Apply repair"), QDialogButtonBox::ActionRole);
    apply_->setObjectName("repairApply");
    apply_->setAutoDefault(false);
    auto* refresh = buttons->addButton(tr("Refresh candidates"), QDialogButtonBox::ActionRole);
    refresh->setObjectName("repairRefresh");
    refresh->setAutoDefault(false);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(refresh, &QPushButton::clicked, this, [this] { reload(); });
    connect(apply_, &QPushButton::clicked, this, [this] { apply(); });
    connect(target_, &QComboBox::currentIndexChanged, this, [this] { populate(); });
    connect(candidates_, &QListWidget::currentRowChanged, this, [this] { selectionChanged(); });
    reload();
}
void ReferenceRepairDialog::reload() {
    const QSignalBlocker blocker(target_);
    target_->clear();
    slots_.clear();
    inspection_.reset();
    try {
        inspection_ = session_.inspectSketchReferences(sketch_);
        const auto& p = inspection_->review.original.plane;
        original_->setText(tr("Sketch %1 — original plane origin (%2, %3, %4), normal (%5, %6, %7)")
                               .arg(sketch_.value())
                               .arg(p.originX)
                               .arg(p.originY)
                               .arg(p.originZ)
                               .arg(p.normalX)
                               .arg(p.normalY)
                               .arg(p.normalZ));
        target_->addItem(tr("Support face"));
        const auto& constraints = inspection_->review.original.constraints.values;
        for (const auto& c : constraints) {
            if (!attachment(c))
                continue;
            bool included = false;
            for (const auto& group : slots_)
                if (std::ranges::find(group, c.id) != group.end())
                    included = true;
            if (included)
                continue;
            std::vector<document::SketchConstraintId> group;
            QStringList ids;
            for (const auto& other : constraints)
                if (attachment(other) && other.first == c.first &&
                    other.auxiliaryValues == c.auxiliaryValues &&
                    other.projectedEdge == c.projectedEdge) {
                    group.push_back(other.id);
                    ids.push_back(QString::number(other.id.value()));
                }
            slots_.push_back(std::move(group));
            QString label = tr("Edge point — constraints %1").arg(ids.join(", "));
            if (c.auxiliaryValues.size() == 2)
                label += tr("; original segment %1, parameter %2")
                             .arg(c.auxiliaryValues[0])
                             .arg(c.auxiliaryValues[1]);
            target_->addItem(label);
        }
        target_->setCurrentIndex(0);
        populate();
    } catch (const std::exception& e) {
        candidates_->clear();
        apply_->setEnabled(false);
        message_->setText(QString::fromUtf8(e.what()));
    }
}
void ReferenceRepairDialog::populate() {
    const QSignalBlocker blocker(candidates_);
    candidates_->clear();
    points_.clear();
    apply_->setEnabled(false);
    keepErrors_->setChecked(false);
    keepErrors_->hide();
    if (preview_->scene())
        preview_->scene()->clear();
    previewInfo_->clear();
    if (!inspection_)
        return;
    try {
        if (target_->currentIndex() == 0) {
            for (const auto& c : inspection_->review.candidates) {
                QString title = tr("%1 — feature %2; origin (%3, %4, %5)")
                                    .arg(role(c.binding.reference.origin))
                                    .arg(c.binding.reference.origin.feature.value())
                                    .arg(c.plane.originX)
                                    .arg(c.plane.originY)
                                    .arg(c.plane.originZ);
                if (!c.binding.reference.origin.extrusionBoundary.empty()) {
                    QStringList ids;
                    for (auto id : c.binding.reference.origin.extrusionBoundary)
                        ids.push_back(QString::number(id.value()));
                    title += tr("; boundary %1").arg(ids.join(", "));
                }
                if (c.matchesOriginalPlane)
                    title += tr(" — matches original plane (unconfirmed)");
                candidates_->addItem(title);
            }
        } else {
            if (!inspection_->review.original.supportBinding)
                throw std::invalid_argument(
                    "Repair the support face before selecting an edge point.");
            const auto projection = session_.projectSketchSupport(
                inspection_->support, *inspection_->review.original.supportBinding);
            points_ = projection.points;
            for (std::size_t i = 0; i < points_.size(); ++i) {
                const auto& p = points_[i];
                const auto kind = p.reference.selection.kind;
                const QString position = kind == document::EdgePointKind::start ? tr("Start")
                                         : kind == document::EdgePointKind::end ? tr("End")
                                                                                : tr("Midpoint");
                candidates_->addItem(tr("%1 — local (%2, %3), candidate %4")
                                         .arg(position)
                                         .arg(p.local.x)
                                         .arg(p.local.y)
                                         .arg(i + 1));
            }
        }
        message_->setText(
            candidates_->count()
                ? tr("Select a candidate explicitly to enable Apply.")
                : tr("No uniquely named current candidates. Repair the upstream feature first."));
    } catch (const std::exception& e) {
        message_->setText(QString::fromUtf8(e.what()));
    }
    candidates_->setCurrentRow(-1);
}
void ReferenceRepairDialog::selectionChanged() {
    keepErrors_->setChecked(false);
    keepErrors_->hide();
    apply_->setEnabled(false);
    const auto row = candidates_->currentRow();
    if (!inspection_ || row < 0)
        return;
    try {
        const bool face = target_->currentIndex() == 0;
        const auto& binding =
            face ? inspection_->review.candidates.at(static_cast<std::size_t>(row)).binding
                 : *inspection_->review.original.supportBinding;
        const auto projection = session_.projectSketchSupport(inspection_->support, binding);
        preview(projection, face ? nullptr : &points_.at(static_cast<std::size_t>(row)));
        message_->setText(
            tr("Review the replacement, then apply. Original data changes only on Apply."));
        apply_->setEnabled(true);
    } catch (const std::exception& e) {
        message_->setText(QString::fromUtf8(e.what()));
    }
}
void ReferenceRepairDialog::apply() {
    if (!inspection_ || candidates_->currentRow() < 0)
        return;
    try {
        const auto disposition = keepErrors_->isVisible() && keepErrors_->isChecked()
                                     ? application::ErrorDisposition::keepEditableErrors
                                     : application::ErrorDisposition::reject;
        const auto row = candidates_->currentRow();
        const auto result =
            target_->currentIndex() == 0
                ? session_.repairSketchSupport(
                      *inspection_,
                      inspection_->review.candidates.at(static_cast<std::size_t>(row)), disposition)
                : session_.repairSketchEdgeReferences(
                      *inspection_,
                      slots_.at(static_cast<std::size_t>(target_->currentIndex() - 1)),
                      points_.at(static_cast<std::size_t>(row)).reference, disposition);
        if (result.status() != application::CommandStatus::rejected) {
            changed_();
            accept();
            return;
        }
        message_->setText(commandFeedbackMessage(result));
        keepErrors_->setVisible(result.reason() == application::RejectionReason::evaluationFailed);
        if (result.reason() != application::RejectionReason::evaluationFailed) {
            apply_->setEnabled(false);
            message_->setText(message_->text() + tr(" Refresh candidates before choosing again."));
        }
    } catch (const std::exception& e) {
        message_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace voelund::desktop
