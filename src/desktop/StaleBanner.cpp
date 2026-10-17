#include "StaleBanner.hpp"
#include "OverlayCard.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace voelund::desktop {

StaleBanner::StaleBanner(QWidget& viewport) : QFrame(&viewport) {
    setObjectName(QStringLiteral("staleGeometryBanner"));
    setStyleSheet(
        QStringLiteral("#staleGeometryBanner { background: #fff4e5; border: 1px solid #e08e0b; "
                       "border-radius: 8px; } #staleGeometryBanner QLabel { color: #5c3b00; }"));
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 4, 6, 4);
    text_ = new QLabel(this);
    auto* open = new QPushButton(tr("Zum Fehler"), this);
    open->setObjectName(QStringLiteral("staleGeometryOpenError"));
    layout->addWidget(text_);
    layout->addWidget(open);
    connect(open, &QPushButton::clicked, this, [this] {
        if (openError_)
            openError_();
    });
    viewport.installEventFilter(this);
    hide();
}

void StaleBanner::setOpenErrorHandler(std::function<void()> handler) {
    openError_ = std::move(handler);
}

void StaleBanner::showText(const QString& text) {
    text_->setText(text);
    setVisible(!text.isEmpty());
    if (!text.isEmpty()) {
        adjustSize();
        place();
        raise();
    }
}

bool StaleBanner::eventFilter(QObject* watched, QEvent* event) {
    if (watched == parent() && event->type() == QEvent::Resize)
        place();
    return QFrame::eventFilter(watched, event);
}

void StaleBanner::place() {
    const auto* host = parentWidget();
    move(std::max(0, (host->width() - width()) / 2), overlay::columnTop); // U14: below the cards
}

} // namespace voelund::desktop
