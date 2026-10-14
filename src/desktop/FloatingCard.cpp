#include "FloatingCard.hpp"
#include "OverlayCard.hpp"

#include <QEvent>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

namespace voelund::desktop {
namespace {

constexpr int margin = overlay::margin;
constexpr int top = overlay::rightColumnTop; // U14: below the header card and the view cube
constexpr int cardWidth = overlay::rightColumnWidth;

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
    // User report 2026-10-13: a new guidance or error text inside the scroll area changes the
    // content's layout, not the card's; the card is measured again then too.
    content_->installEventFilter(this);
    hide();
}

void FloatingCard::setTitle(const QString& title) { title_->setText(title); }

bool FloatingCard::eventFilter(QObject* watched, QEvent* event) {
    if ((watched == &host_ && event->type() == QEvent::Resize) ||
        (watched == content_ && event->type() == QEvent::LayoutRequest && isVisible()))
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
    // A short host gives up the cube's place, never the header card's.
    const int y = host_.height() > 2 * top ? top : overlay::columnTop;
    const int available = std::max(160, host_.height() - y - margin);
    // User report 2026-10-13: wrapped text (guidance, errors) is measured at the card's real
    // width; sizeHint() guessed a wider line, cut the text and hid the fields below a scroll.
    const auto heightAt = [](QLayout& layout, QWidget* owner, int width) {
        return layout.hasHeightForWidth() ? layout.totalHeightForWidth(width)
                                          : (owner != nullptr ? owner->sizeHint().height()
                                                              : layout.sizeHint().height());
    };
    const int inner = cardWidth - 14 - 6;
    const int wanted = heightAt(*body_, content_, inner - 14) +
                       (title_->hasHeightForWidth() ? title_->heightForWidth(inner)
                                                    : title_->sizeHint().height()) +
                       heightAt(*footer_, nullptr, inner - 8) +
                       48;
    setFixedHeight(std::min(available, wanted));
    move(host_.width() - cardWidth - margin, y);
    raise();
}

} // namespace voelund::desktop
