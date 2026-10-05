#include "AssistantPanel.hpp"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace voelund::desktop {
namespace {
using Kind = assistant::AssistantEvent::Kind;

QString toolLabel(const QString& tool) {
    static const QHash<QString, QString> labels{
        {"get_context", QObject::tr("Modell gelesen")},
        {"create_parameter", QObject::tr("Parameter anlegen")},
        {"set_parameter_value", QObject::tr("Parameter ändern")},
        {"rename_parameter", QObject::tr("Parameter umbenennen")},
        {"bind_sketch_dimension", QObject::tr("Maß binden")},
        {"bind_extrusion_distance", QObject::tr("Distanz binden")},
        {"create_sketch", QObject::tr("Skizze anlegen")},
        {"edit_sketch", QObject::tr("Skizze ändern")},
        {"create_profile_extrusion", QObject::tr("Extrusion anlegen")},
        {"create_sketch_extrusion", QObject::tr("Skizze und Extrusion anlegen")},
        {"set_extrusion_distance", QObject::tr("Distanz ändern")},
        {"create_box", QObject::tr("Quader anlegen")},
        {"set_box_dimensions", QObject::tr("Quadermaße ändern")},
        {"create_cylinder", QObject::tr("Zylinder anlegen")},
        {"set_cylinder_size", QObject::tr("Zylinder ändern")},
        {"set_placement", QObject::tr("Lage ändern")}};
    return labels.value(tool, tool);
}

// Why a tool call did not change anything, from Völund's structured result.
QString refusal(const QJsonObject& result, const QString& text) {
    const auto rejection = result.value("rejection").toString();
    if (!rejection.isEmpty() && rejection != QLatin1String("none"))
        return rejection;
    if (!result.value("diagnostic").toString().isEmpty())
        return result.value("diagnostic").toString();
    // A preview or commit that failed in the evaluation names the feature's problem.
    const auto issues = result.value("issues").toArray();
    if (!issues.isEmpty())
        return issues.first().toObject().value("message").toString();
    if (!result.value("message").toString().isEmpty())
        return result.value("message").toString();
    return text.left(120);
}
} // namespace

AssistantPanel::AssistantPanel(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("assistantPanel"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // The dock title already reads "Claude".
    auto* top = new QHBoxLayout;
    top->addStretch(1);
    reset_ = new QPushButton(tr("Neu"), this);
    reset_->setObjectName(QStringLiteral("assistantNewButton"));
    reset_->setToolTip(tr("Neues Gespräch"));
    top->addWidget(reset_);
    layout->addLayout(top);

    history_ = new QListWidget(this);
    history_->setObjectName(QStringLiteral("assistantHistory"));
    history_->setWordWrap(true);
    history_->setSelectionMode(QAbstractItemView::NoSelection);
    history_->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(history_, 1);

    selection_ = new QToolButton(this);
    selection_->setObjectName(QStringLiteral("assistantSelection"));
    selection_->setCheckable(true);
    selection_->setChecked(true);
    selection_->setAutoRaise(true);
    selection_->setToolTip(tr("Die Auswahl geht als Kontext mit. Klick: nicht mitschicken."));
    layout->addWidget(selection_, 0, Qt::AlignLeft);

    auto* bottom = new QHBoxLayout;
    input_ = new QLineEdit(this);
    input_->setObjectName(QStringLiteral("assistantInput"));
    input_->setPlaceholderText(tr("Nachricht an Claude …"));
    bottom->addWidget(input_, 1);
    send_ = new QPushButton(tr("Senden"), this);
    send_->setObjectName(QStringLiteral("assistantSendButton"));
    send_->setDefault(true);
    bottom->addWidget(send_);
    stop_ = new QPushButton(tr("Stopp"), this);
    stop_->setObjectName(QStringLiteral("assistantStopButton"));
    bottom->addWidget(stop_);
    layout->addLayout(bottom);

    connect(input_, &QLineEdit::returnPressed, this, [this] { submit(); });
    connect(send_, &QPushButton::clicked, this, [this] { submit(); });
    connect(stop_, &QPushButton::clicked, this, [this] {
        if (handlers_.stop)
            handlers_.stop();
    });
    connect(reset_, &QPushButton::clicked, this, [this] {
        if (handlers_.reset)
            handlers_.reset();
    });
    connect(selection_, &QToolButton::toggled, this, [this](bool included) {
        auto font = selection_->font();
        font.setStrikeOut(!included);
        selection_->setFont(font);
    });
    setSelection({});
    setRunning(false);
}

void AssistantPanel::setSelection(const QString& text) {
    selection_->setVisible(!text.isEmpty());
    selection_->setText(tr("Auswahl: %1").arg(text));
}

void AssistantPanel::setRunning(bool running) {
    send_->setEnabled(!running);
    input_->setEnabled(!running);
    stop_->setEnabled(running);
    if (!running)
        input_->setFocus();
}

void AssistantPanel::submit() {
    const auto text = input_->text().trimmed();
    if (text.isEmpty() || !send_->isEnabled() || !handlers_.send)
        return;
    input_->clear();
    handlers_.send(text, selection_->isVisible() && selection_->isChecked());
}

QListWidgetItem* AssistantPanel::addItem(const QString& text, const char* role) {
    auto* item = new QListWidgetItem(text, history_);
    item->setData(Qt::UserRole, QString::fromLatin1(role));
    if (QLatin1String(role) == QLatin1String("user")) {
        auto font = item->font();
        font.setBold(true);
        item->setFont(font);
    } else if (QLatin1String(role) == QLatin1String("tool") ||
               QLatin1String(role) == QLatin1String("note")) {
        item->setForeground(muted_);
    } else if (QLatin1String(role) == QLatin1String("error")) {
        item->setForeground(error_);
    }
    history_->scrollToBottom();
    return item;
}

void AssistantPanel::setColors(const QColor& muted, const QColor& error) {
    muted_ = muted;
    error_ = error;
    for (int row = 0; row < history_->count(); ++row) {
        auto* item = history_->item(row);
        const auto role = item->data(Qt::UserRole).toString();
        if (role == QLatin1String("tool") || role == QLatin1String("note"))
            item->setForeground(muted_);
        else if (role == QLatin1String("error"))
            item->setForeground(error_);
    }
}

void AssistantPanel::addUserMessage(const QString& text) { addItem(text, "user"); }

void AssistantPanel::present(const assistant::AssistantEvent& event) {
    switch (event.kind) {
    case Kind::started:
    case Kind::finished: // Its answer arrived as text already.
        break;
    case Kind::text:
        addItem(event.text, "claude");
        break;
    case Kind::toolStarted:
        tools_.insert(event.toolId,
                      addItem(QStringLiteral("⋯ %1").arg(toolLabel(event.tool)), "tool"));
        tools_[event.toolId]->setData(Qt::UserRole + 1, event.tool);
        break;
    case Kind::toolFinished: {
        auto* item = tools_.take(event.toolId);
        if (item == nullptr)
            break;
        const auto label = toolLabel(item->data(Qt::UserRole + 1).toString());
        const bool preview = event.toolResult.value("preview").toBool();
        item->setText(event.toolError
                          ? QStringLiteral("✗ %1 – %2").arg(label, refusal(event.toolResult,
                                                                          event.text))
                          : QStringLiteral("✓ %1%2").arg(label, preview ? tr(" (Vorschau)")
                                                                        : QString()));
        break;
    }
    case Kind::failed:
        addItem(event.text, "error");
        break;
    case Kind::cancelled:
        addItem(event.text, "note");
        break;
    }
    if (event.kind == Kind::finished || event.kind == Kind::failed ||
        event.kind == Kind::cancelled)
        tools_.clear();
}

void AssistantPanel::clear(const QString& note) {
    history_->clear();
    tools_.clear();
    if (!note.isEmpty())
        addItem(note, "note");
}

} // namespace voelund::desktop
