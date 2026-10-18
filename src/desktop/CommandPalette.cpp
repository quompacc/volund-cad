#include "CommandPalette.hpp"
#include "MainWindowSupport.hpp"

#include <QAction>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace voelund::desktop {
namespace {

QAction* findCommand(QWidget& window, const QString& id) {
    for (QAction* action : window.findChildren<QAction*>())
        if (action->property(commandIdProperty).toString() == id)
            return action;
    return nullptr;
}

QString plainText(const QAction& action) {
    QString text = action.text();
    text.remove(QLatin1Char('&'));
    return text;
}

// L03 (user decision 2026-10-18): "Über Völund" only in the main menu.
bool listedInPalette(const QString& id) {
    return !id.isEmpty() && id != QStringLiteral("view.command_search") &&
           id != QStringLiteral("app.about");
}

} // namespace

CommandPalette::CommandPalette(QWidget& window)
    : QDialog(&window, Qt::Popup | Qt::FramelessWindowHint), window_(window) {
    setObjectName(QStringLiteral("commandSearch"));
    setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    query_ = new QLineEdit(this);
    query_->setObjectName(QStringLiteral("commandSearchInput"));
    query_->setPlaceholderText(tr("Befehl oder Werkzeug suchen"));
    query_->installEventFilter(this);
    results_ = new QListWidget(this);
    results_->setObjectName(QStringLiteral("commandSearchList"));
    layout->addWidget(query_);
    layout->addWidget(results_);
    resize(460, 360);
    connect(query_, &QLineEdit::textChanged, this, [this] { refill(); });
    connect(results_, &QListWidget::itemActivated, this, [this] { runCurrent(); });
}

void CommandPalette::openSearch() {
    query_->clear();
    refill();
    const QPoint topCenter = window_.mapToGlobal(QPoint(window_.width() / 2, 56));
    move(topCenter.x() - width() / 2, topCenter.y());
    show();
    query_->setFocus();
}

std::optional<QString> CommandPalette::trigger(QWidget& window, const QString& id) {
    QAction* action = findCommand(window, id);
    if (action == nullptr)
        return tr("Unbekannter Befehl: %1").arg(id);
    if (!action->isEnabled())
        return tr("„%1“ ist für die aktuelle Auswahl nicht verfügbar").arg(plainText(*action));
    action->trigger();
    return std::nullopt;
}

bool CommandPalette::eventFilter(QObject* watched, QEvent* event) {
    if (watched == query_ && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
            const int step = key->key() == Qt::Key_Down ? 1 : -1;
            const int row = results_->currentRow() + step;
            if (row >= 0 && row < results_->count())
                results_->setCurrentRow(row);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            runCurrent();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPalette::refill() {
    results_->clear();
    const QString query = query_->text().trimmed();
    for (QAction* action : window_.findChildren<QAction*>()) {
        const QString id = action->property(commandIdProperty).toString();
        if (!listedInPalette(id))
            continue;
        const QString text = plainText(*action);
        if (!query.isEmpty() && !text.contains(query, Qt::CaseInsensitive) &&
            !action->toolTip().contains(query, Qt::CaseInsensitive))
            continue;
        QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
        shortcut.replace(QStringLiteral("Ctrl"), tr("Strg")); // German key name (ADR 0012)
        auto* item = new QListWidgetItem(
            action->icon(), shortcut.isEmpty() ? text : text + QStringLiteral("    ") + shortcut,
            results_);
        item->setData(Qt::UserRole, id);
        item->setToolTip(action->toolTip());
        if (!action->isEnabled())
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }
    for (int row = 0; row < results_->count(); ++row)
        if (results_->item(row)->flags() & Qt::ItemIsEnabled) {
            results_->setCurrentRow(row);
            break;
        }
}

void CommandPalette::runCurrent() {
    const QListWidgetItem* item = results_->currentItem();
    if (item == nullptr || !(item->flags() & Qt::ItemIsEnabled))
        return;
    const QString id = item->data(Qt::UserRole).toString();
    hide();
    static_cast<void>(trigger(window_, id));
}

} // namespace voelund::desktop
