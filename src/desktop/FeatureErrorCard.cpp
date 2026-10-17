#include "FeatureErrorCard.hpp"

#include <voelund/application/CadSession.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace voelund::desktop {
namespace {
const QColor errorColor{0xC6, 0x28, 0x28};
const QColor blockedColor{0xE0, 0x8E, 0x0B};
} // namespace

QIcon featureStatusIcon(application::FeatureRuntimeStatus status) {
    if (status == application::FeatureRuntimeStatus::current ||
        status == application::FeatureRuntimeStatus::suppressed ||
        status == application::FeatureRuntimeStatus::rolledBack) // M14: on purpose, no error
        return {};
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const bool failed = status == application::FeatureRuntimeStatus::error;
    painter.setBrush(failed ? errorColor : blockedColor);
    if (failed) {
        painter.drawEllipse(QRectF(3, 3, 26, 26));
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(11, 11), QPointF(21, 21));
        painter.drawLine(QPointF(21, 11), QPointF(11, 21));
    } else {
        QPainterPath triangle;
        triangle.moveTo(16, 3);
        triangle.lineTo(30, 28);
        triangle.lineTo(2, 28);
        triangle.closeSubpath();
        painter.drawPath(triangle);
        painter.setPen(QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(16, 11), QPointF(16, 19));
        painter.drawPoint(QPointF(16, 24));
    }
    return QIcon(pixmap);
}

FeatureErrorCard::FeatureErrorCard(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("featureErrorCard"));
    setStyleSheet(
        QStringLiteral("#featureErrorCard { border: 1px solid #b94a55; border-radius: 6px; "
                       "background: rgba(198, 40, 40, 0.08); }"
                       "#featureErrorCard[blocked=\"true\"] { border-color: #e08e0b; "
                       "background: rgba(224, 142, 11, 0.10); }"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("featureErrorTitle"));
    title_->setStyleSheet(QStringLiteral("font-weight: 600;"));
    message_ = new QLabel(this);
    message_->setObjectName(QStringLiteral("featureErrorMessage"));
    message_->setWordWrap(true);
    layout->addWidget(title_);
    layout->addWidget(message_);
    auto* buttons = new QHBoxLayout;
    const auto button = [&](const char* name, const QString& text) {
        auto* item = new QPushButton(text, this);
        item->setObjectName(QLatin1String(name));
        buttons->addWidget(item);
        return item;
    };
    cause_ = button("featureErrorCause", tr("Zur Ursache"));
    edit_ = button("featureErrorEdit", tr("Bearbeiten"));
    repair_ = button("featureErrorRepair", tr("Reparieren"));
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(cause_, &QPushButton::clicked, this, [this] {
        if (handlers_.openCause)
            handlers_.openCause();
    });
    connect(edit_, &QPushButton::clicked, this, [this] {
        if (handlers_.edit)
            handlers_.edit();
    });
    connect(repair_, &QPushButton::clicked, this, [this] {
        if (handlers_.repair)
            handlers_.repair();
    });
    hide();
}

void FeatureErrorCard::setHandlers(Handlers handlers) { handlers_ = std::move(handlers); }

// The evaluation's own text for a blocked step is technical English; the card says it in the
// UI's words.
QString FeatureErrorCard::blockedMessage(const QString& cause) {
    return cause.isEmpty()
               ? tr("Dieser Schritt wartet auf einen fehlerhaften früheren Schritt.")
               : tr("Dieser Schritt wird wieder berechnet, sobald „%1“ fehlerfrei ist.").arg(cause);
}

void FeatureErrorCard::showError(const Content& content) {
    setProperty("blocked", content.blocked);
    style()->unpolish(this);
    style()->polish(this);
    title_->setText(content.blocked ? tr("Blockiert durch %1").arg(content.cause)
                                    : tr("Fehler in diesem Feature"));
    message_->setText(content.blocked ? blockedMessage(content.cause) : content.message);
    cause_->setVisible(content.blocked && !content.cause.isEmpty());
    edit_->setVisible(content.canEdit);
    repair_->setVisible(content.canRepair);
    show();
}

void FeatureErrorCard::clear() { hide(); }

} // namespace voelund::desktop
