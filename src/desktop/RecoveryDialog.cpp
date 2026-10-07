#include "RecoveryDialog.hpp"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

namespace voelund::desktop {

QString RecoveryDialog::describe(const RecoveryEntry& entry) {
    const QString name = entry.originalFile.isEmpty()
                             ? QObject::tr("Unbenannt")
                             : QFileInfo(entry.originalFile).fileName();
    if (entry.damaged)
        return QObject::tr("%1\nbeschädigt – kann nur verworfen werden").arg(name);
    const QString time =
        QLocale().toString(entry.savedAt.toLocalTime(), QLocale::ShortFormat);
    QString detail = entry.savedFeatures
                         ? QObject::tr("gespeichert: %1 Features → Sicherung: %2")
                               .arg(*entry.savedFeatures)
                               .arg(entry.features)
                         : QObject::tr("%1 Features, nie gespeichert").arg(entry.features);
    if (entry.hasDraft)
        detail += QObject::tr("\n+ offener Skizzenentwurf");
    if (entry.olderThanFile)
        detail += QObject::tr("\nDie Datei wurde danach gespeichert; die Sicherung ist älter.");
    return QStringLiteral("%1    %2\n%3").arg(name, time, detail);
}

RecoveryDialog::RecoveryDialog(std::vector<RecoveryEntry> entries, Handler restore,
                               Handler discard, QWidget* parent)
    : QDialog(parent), entries_(std::move(entries)), restore_(std::move(restore)),
      discard_(std::move(discard)) {
    setObjectName(QStringLiteral("recoveryDialog"));
    setWindowTitle(tr("Ungespeicherte Arbeit gefunden"));
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Völund wurde nicht regulär beendet. Die Originaldateien sind unverändert."), this));
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("recoveryList"));
    layout->addWidget(list_);
    auto* buttons = new QHBoxLayout;
    discardButton_ = new QPushButton(tr("Verwerfen"), this);
    discardButton_->setObjectName(QStringLiteral("recoveryDiscard"));
    auto* later = new QPushButton(tr("Später"), this);
    later->setObjectName(QStringLiteral("recoveryLater"));
    restoreButton_ = new QPushButton(tr("Wiederherstellen"), this);
    restoreButton_->setObjectName(QStringLiteral("recoveryRestore"));
    restoreButton_->setDefault(true);
    buttons->addWidget(discardButton_);
    buttons->addStretch();
    buttons->addWidget(later);
    buttons->addWidget(restoreButton_);
    layout->addLayout(buttons);
    connect(list_, &QListWidget::currentRowChanged, this, [this] { syncButtons(); });
    connect(later, &QPushButton::clicked, this, &QDialog::reject);
    connect(discardButton_, &QPushButton::clicked, this, [this] {
        const auto* entry = selected();
        if (!entry || !discard_(*entry))
            return;
        entries_.erase(entries_.begin() + list_->currentRow());
        if (entries_.empty())
            accept();
        else
            fill();
    });
    connect(restoreButton_, &QPushButton::clicked, this, [this] {
        const auto* entry = selected();
        if (entry && restore_(*entry))
            accept();
    });
    fill();
    resize(520, 320);
}

void RecoveryDialog::fill() {
    list_->clear();
    for (const auto& entry : entries_)
        list_->addItem(describe(entry));
    list_->setCurrentRow(0);
    syncButtons();
}

const RecoveryEntry* RecoveryDialog::selected() const {
    const int row = list_->currentRow();
    return row >= 0 && row < static_cast<int>(entries_.size()) ? &entries_[row] : nullptr;
}

void RecoveryDialog::syncButtons() {
    const auto* entry = selected();
    discardButton_->setEnabled(entry != nullptr);
    restoreButton_->setEnabled(entry != nullptr && !entry->damaged);
}

} // namespace voelund::desktop
