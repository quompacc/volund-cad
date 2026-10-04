#include "FloatingCard.hpp"

#include <QEvent>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace voelund::desktop {
namespace {

constexpr int margin = 16;
constexpr int top = 196; // below the view cube and its buttons
constexpr int cardWidth = 344;

} // namespace

FloatingCard::FloatingCard(QWidget& host, const QString& objectName) : QFrame(&host), host_(host) {
    setObjectName(objectName);
    setProperty("floatingCard", true);
    setAttribute(Qt::WA_StyledBackground);
    setFixedWidth(cardWidth);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 6, 12);
    layout->setSpacing(8);
    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("cardTitle"));
    layout->addWidget(title_);
    scroll_ = new QScrollArea(this);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setWidgetResizable(true);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content_ = new QWidget(scroll_);
    content_->setObjectName(QStringLiteral("cardContent"));
    body_ = new QVBoxLayout(content_);
    body_->setContentsMargins(0, 0, 14, 0);
    body_->setSpacing(8);
    scroll_->setWidget(content_);
    layout->addWidget(scroll_);
    footer_ = new QVBoxLayout;
    footer_->setContentsMargins(0, 0, 8, 0);
    footer_->setSpacing(6);
    layout->addLayout(footer_);
    host.installEventFilter(this);
    hide();
}

void FloatingCard::setTitle(const QString& title) { title_->setText(title); }

bool FloatingCard::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &host_ && event->type() == QEvent::Resize)
        place();
    return QFrame::eventFilter(watched, event);
}

bool FloatingCard::event(QEvent* event) {
    const bool result = QFrame::event(event);
    if (event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest ||
        event->type() == QEvent::StyleChange)
        place();
    return result;
}

void FloatingCard::place() {
    // As tall as the content needs, but never past the bottom of the host.
    const int y = host_.height() > 2 * top ? top : margin;
    const int available = std::max(160, host_.height() - y - margin);
    const int wanted = content_->sizeHint().height() + title_->sizeHint().height() +
                       footer_->sizeHint().height() + 48;
    setFixedHeight(std::min(available, wanted));
    move(host_.width() - cardWidth - margin, y);
    raise();
}

} // namespace voelund::desktop
