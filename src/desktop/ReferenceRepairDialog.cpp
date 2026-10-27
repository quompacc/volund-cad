// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

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
#include <array>

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
        return QObject::tr("Endfläche der Extrusion");
    // In the order of kernel::PrimitiveRole.
    static constexpr std::array<const char*, 15> names{
        QT_TRANSLATE_NOOP("QObject", "Links (−X)"),
        QT_TRANSLATE_NOOP("QObject", "Rechts (+X)"),
        QT_TRANSLATE_NOOP("QObject", "Vorne (−Y)"),
        QT_TRANSLATE_NOOP("QObject", "Hinten (+Y)"),
        QT_TRANSLATE_NOOP("QObject", "Unten (−Z)"),
        QT_TRANSLATE_NOOP("QObject", "Oben (+Z)"),
        QT_TRANSLATE_NOOP("QObject", "Zylinderanfang"),
        QT_TRANSLATE_NOOP("QObject", "Zylinderende"),
        QT_TRANSLATE_NOOP("QObject", "Zylindermantel"),
        QT_TRANSLATE_NOOP("QObject", "Kugelfläche"), // M27
        QT_TRANSLATE_NOOP("QObject", "Torusfläche"),
        QT_TRANSLATE_NOOP("QObject", "Flachsenkung, Wand"), // N09
        QT_TRANSLATE_NOOP("QObject", "Flachsenkung, Boden"),
        QT_TRANSLATE_NOOP("QObject", "Senkung"),
        QT_TRANSLATE_NOOP("QObject", "Bohrspitze")};
    const auto index = static_cast<std::size_t>(*origin.role);
    return index < names.size() ? QObject::tr(names[index]) : QString();
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
    setWindowTitle(tr("Skizzenreferenzen reparieren"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    resize(760, 700);
    auto* layout = new QVBoxLayout(this);
    original_ = new QLabel(this);
    original_->setWordWrap(true);
    layout->addWidget(original_);
    auto* instructions =
        new QLabel(tr("Wähle eine Referenz und einen Ersatz. Passende Geometrie ist nur "
                      "ein Vorschlag. Abbrechen lässt das Dokument unverändert."),
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
        tr("Umriss der Stützfläche in ihrer Ebene; Orange markiert den gewählten Kantenpunkt."),
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
        tr("Diese Reparatur mit den übrigen Modellfehlern behalten (Rückgängig möglich)"), this);
    keepErrors_->setObjectName("repairKeepErrors");
    keepErrors_->hide();
    layout->addWidget(keepErrors_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    apply_ = buttons->addButton(tr("Reparatur übernehmen"), QDialogButtonBox::ActionRole);
    apply_->setObjectName("repairApply");
    apply_->setAutoDefault(false);
    auto* refresh =
        buttons->addButton(tr("Kandidaten aktualisieren"), QDialogButtonBox::ActionRole);
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
        original_->setText(
            tr("Skizze %1 — ursprüngliche Ebene: Ursprung (%2, %3, %4), Normale (%5, %6, %7)")
                .arg(sketch_.value())
                .arg(p.originX)
                .arg(p.originY)
                .arg(p.originZ)
                .arg(p.normalX)
                .arg(p.normalY)
                .arg(p.normalZ));
        target_->addItem(tr("Stützfläche"));
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
            QString label = tr("Kantenpunkt — Bedingungen %1").arg(ids.join(", "));
            if (c.auxiliaryValues.size() == 2)
                label += tr("; ursprüngliches Segment %1, Parameter %2")
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
                QString title = tr("%1 — Feature %2; Ursprung (%3, %4, %5)")
                                    .arg(role(c.binding.reference.origin))
                                    .arg(c.binding.reference.origin.feature.value())
                                    .arg(c.plane.originX)
                                    .arg(c.plane.originY)
                                    .arg(c.plane.originZ);
                if (!c.binding.reference.origin.extrusionBoundary.empty()) {
                    QStringList ids;
                    for (auto id : c.binding.reference.origin.extrusionBoundary)
                        ids.push_back(QString::number(id.value()));
                    title += tr("; Rand %1").arg(ids.join(", "));
                }
                if (c.matchesOriginalPlane)
                    title += tr(" — passt zur ursprünglichen Ebene (unbestätigt)");
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
                const QString position = kind == document::EdgePointKind::start ? tr("Anfang")
                                         : kind == document::EdgePointKind::end ? tr("Ende")
                                                                                : tr("Mittelpunkt");
                candidates_->addItem(tr("%1 — lokal (%2, %3), Kandidat %4")
                                         .arg(position)
                                         .arg(p.local.x)
                                         .arg(p.local.y)
                                         .arg(i + 1));
            }
        }
        message_->setText(candidates_->count()
                              ? tr("Wähle ausdrücklich einen Kandidaten, um zu übernehmen.")
                              : tr("Keine eindeutig benannten aktuellen Kandidaten. Repariere "
                                   "zuerst das vorgelagerte Feature."));
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
        message_->setText(tr(
            "Prüfe den Ersatz und übernimm ihn dann. Die Daten ändern sich erst beim Übernehmen."));
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
            message_->setText(message_->text() +
                              tr(" Aktualisiere die Kandidaten, bevor du erneut wählst."));
        }
    } catch (const std::exception& e) {
        message_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace voelund::desktop
