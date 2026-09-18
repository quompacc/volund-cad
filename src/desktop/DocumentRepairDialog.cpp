#include "DocumentRepairDialog.hpp"
#include "CommandFeedback.hpp"
#include "ReferenceRepairDialog.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
namespace voelund::desktop {
DocumentRepairDialog::DocumentRepairDialog(application::CadSession& session,
                                           std::function<void()> changed, QWidget* parent)
    : QDialog(parent), session_(session), changed_(std::move(changed)) {
    setObjectName("documentRepairDialog");
    setWindowTitle(tr("Repair document references"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    resize(1000, 600);
    auto* layout = new QVBoxLayout(this);
    auto* info = new QLabel(tr("Auto repair applies unique matching support faces, including "
                               "dependent sketches. Ambiguous faces and edge references remain for "
                               "review. One Undo reverses the entire batch."),
                            this);
    info->setWordWrap(true);
    layout->addWidget(info);
    auto* automatic = new QPushButton(tr("Auto repair unique matches"), this);
    automatic->setObjectName("repairAuto");
    layout->addWidget(automatic);
    table_ = new QTableWidget(this);
    table_->setObjectName("repairBatchTable");
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels(
        {tr("Apply"), tr("Sketch"), tr("Replacement support face"), tr("Review")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_, 1);
    message_ = new QLabel(this);
    message_->setObjectName("repairBatchMessage");
    message_->setWordWrap(true);
    layout->addWidget(message_);
    keep_ =
        new QCheckBox(tr("Keep selected repairs with the remaining errors (Undo available)"), this);
    keep_->setObjectName("repairBatchKeepErrors");
    keep_->hide();
    layout->addWidget(keep_);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* applyButton = buttons->addButton(tr("Apply checked"), QDialogButtonBox::ActionRole);
    applyButton->setObjectName("repairBatchApply");
    auto* select = buttons->addButton(tr("Select unique proposals"), QDialogButtonBox::ActionRole);
    select->setObjectName("repairBatchSelect");
    auto* refresh = buttons->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
    refresh->setObjectName("repairBatchRefresh");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(refresh, &QPushButton::clicked, this, [this] { reload(); });
    connect(select, &QPushButton::clicked, this, [this] { propose(); });
    connect(applyButton, &QPushButton::clicked, this, [this] { apply(); });
    connect(automatic, &QPushButton::clicked, this, [this] {
        reload();
        propose();
        apply();
    });
    reload();
}
void DocumentRepairDialog::reload() {
    keep_->setChecked(false);
    keep_->hide();
    table_->setRowCount(0);
    suggestions_.clear();
    try {
        suggestions_ = session_.suggestSketchSupportRepairs();
        const auto features = session_.features();
        for (std::size_t i = 0; i < suggestions_.size(); ++i) {
            const int row = static_cast<int>(i);
            table_->insertRow(row);
            const auto& item = suggestions_[i];
            auto* checked = new QTableWidgetItem;
            checked->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            checked->setCheckState(Qt::Unchecked);
            table_->setItem(row, 0, checked);
            const auto feature = std::ranges::find(features, item.inspection.feature,
                                                   &document::FeatureDescriptor::id);
            auto* name = new QTableWidgetItem(feature == features.end()
                                                  ? QString::number(item.inspection.feature.value())
                                                  : QString::fromStdString(feature->name));
            name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            table_->setItem(row, 1, name);
            auto* choices = new QComboBox(table_);
            choices->addItem(tr("Choose replacement…"));
            for (const auto& c : item.inspection.review.candidates) {
                const auto& p = c.plane;
                QString label = tr("Feature %1 · normal (%2,%3,%4) · origin (%5,%6,%7)")
                                    .arg(c.binding.reference.origin.feature.value())
                                    .arg(p.normalX)
                                    .arg(p.normalY)
                                    .arg(p.normalZ)
                                    .arg(p.originX)
                                    .arg(p.originY)
                                    .arg(p.originZ);
                if (!c.binding.reference.origin.extrusionBoundary.empty()) {
                    QStringList ids;
                    for (auto id : c.binding.reference.origin.extrusionBoundary)
                        ids.push_back(QString::number(id.value()));
                    label += tr(" · boundary %1").arg(ids.join(","));
                }
                if (c.matchesOriginalPlane)
                    label += tr(" · plane match");
                choices->addItem(label);
            }
            table_->setCellWidget(row, 2, choices);
            connect(choices, &QComboBox::currentIndexChanged, this, [this, row](int index) {
                table_->item(row, 0)->setCheckState(index > 0 ? Qt::Checked : Qt::Unchecked);
                keep_->setChecked(false);
                keep_->hide();
            });
            auto* detail = new QPushButton(tr("Details…"), table_);
            detail->setToolTip(item.proposal ? tr("Unique proposal") : tr("Needs review"));
            table_->setCellWidget(row, 3, detail);
            connect(detail, &QPushButton::clicked, this, [this, id = item.inspection.feature] {
                auto* dialog = new ReferenceRepairDialog(
                    session_, id,
                    [this] {
                        changed_();
                        reload();
                    },
                    this);
                dialog->open();
            });
        }
        message_->setText(suggestions_.empty() ? tr("No unresolved references remain.")
                                               : tr("%1 sketches need review. Check several rows "
                                                    "to apply them together, or use Auto repair.")
                                                     .arg(suggestions_.size()));
    } catch (const std::exception& e) {
        message_->setText(QString::fromUtf8(e.what()));
    }
}
void DocumentRepairDialog::propose() {
    for (std::size_t i = 0; i < suggestions_.size(); ++i) {
        const auto& item = suggestions_[i];
        auto* combo = static_cast<QComboBox*>(table_->cellWidget(static_cast<int>(i), 2));
        combo->setCurrentIndex(0);
        if (!item.proposal)
            continue;
        const auto found =
            std::ranges::find_if(item.inspection.review.candidates, [&](const auto& c) {
                return c.binding == item.proposal->binding && c.plane == item.proposal->plane;
            });
        if (found != item.inspection.review.candidates.end())
            combo->setCurrentIndex(
                static_cast<int>(found - item.inspection.review.candidates.begin()) + 1);
    }
}
void DocumentRepairDialog::apply() {
    try {
        std::vector<application::SketchSupportRepair> requests;
        for (std::size_t i = 0; i < suggestions_.size(); ++i) {
            const int row = static_cast<int>(i);
            if (table_->item(row, 0)->checkState() != Qt::Checked)
                continue;
            const int index = static_cast<QComboBox*>(table_->cellWidget(row, 2))->currentIndex();
            if (index <= 0)
                continue;
            requests.push_back(
                {suggestions_[i].inspection, suggestions_[i].inspection.review.candidates.at(
                                                 static_cast<std::size_t>(index - 1))});
        }
        if (requests.empty()) {
            message_->setText(tr("No unique matches selected. Review the remaining references."));
            return;
        }
        const auto result = session_.repairSketchSupports(
            requests, keep_->isVisible() && keep_->isChecked()
                          ? application::ErrorDisposition::keepEditableErrors
                          : application::ErrorDisposition::reject);
        if (result.status() != application::CommandStatus::rejected) {
            changed_();
            reload();
            message_->setText(
                tr("%1 references repaired in one Undo step. %2 sketches remain for review.")
                    .arg(requests.size())
                    .arg(suggestions_.size()));
            return;
        }
        message_->setText(commandFeedbackMessage(result));
        keep_->setVisible(result.reason() == application::RejectionReason::evaluationFailed);
    } catch (const std::exception& e) {
        message_->setText(QString::fromUtf8(e.what()));
    }
}
} // namespace voelund::desktop
