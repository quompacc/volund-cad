#include "ContextBar.hpp"
#include "OverlayCard.hpp"

#include <voelund/viewport/CadViewport.hpp>

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>

#include <algorithm>
#include <utility>

namespace voelund::desktop {
ContextBar::ContextBar(viewport::CadViewport& viewport, ActionLookup lookup)
    : QFrame(&viewport), viewport_(viewport), lookup_(std::move(lookup)) {
    setObjectName(QStringLiteral("contextBar"));
    setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);
    title_ = new QLabel(this);
    title_->setObjectName(QStringLiteral("contextTitle"));
    layout->addWidget(title_);
    auto* divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("contextDivider"));
    divider->setFixedWidth(1);
    layout->addWidget(divider);
    buttons_ = new QHBoxLayout;
    buttons_->setSpacing(4);
    layout->addLayout(buttons_);
    transformSettings_ = new QWidget(this);
    auto* settings = new QHBoxLayout(transformSettings_);
    settings->setContentsMargins(0, 0, 0, 0);
    settings->setSpacing(6);
    layout->addWidget(transformSettings_);
    viewport.installEventFilter(this);
}

void ContextBar::display(const ContextCommands& content) {
    // A button may be rebuilding the bar from its own click, so it is released later.
    for (QToolButton* button : current_) {
        buttons_->removeWidget(button);
        button->hide();
        button->deleteLater();
    }
    current_.clear();
    setVisible(content.visible);
    if (!content.visible)
        return;
    title_->setText(content.title);
    for (const std::string& id : content.commands) {
        QAction* action = lookup_(id);
        if (action == nullptr)
            continue;
        auto* button = new QToolButton(this);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(18, 18));
        button->setProperty("primary", id == content.primary);
        button->style()->unpolish(button);
        button->style()->polish(button);
        buttons_->addWidget(button);
        button->show(); // Children added to a visible bar stay hidden otherwise.
        current_.push_back(button);
    }
    transformSettings_->setVisible(content.transformSettings);
    adjustSize();
    place();
}

bool ContextBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == &viewport_ && event->type() == QEvent::Resize)
        place();
    return QFrame::eventFilter(watched, event);
}

bool ContextBar::event(QEvent* event) {
    const bool result = QFrame::event(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::LayoutRequest)
        place();
    return result;
}

void ContextBar::place() {
    adjustSize();
    // U14: right above the timeline, centred but clear of an open tool panel card.
    bool cardShown = false;
    for (const QObject* child : viewport_.children())
        if (const auto* widget = qobject_cast<const QWidget*>(child))
            cardShown =
                cardShown || (widget->property("floatingCard").toBool() && widget->isVisible());
    const int limit = cardShown ? viewport_.width() - overlay::margin - overlay::rightColumnWidth -
                                      overlay::gap - width()
                                : viewport_.width();
    const int x = std::max(overlay::margin, std::min((viewport_.width() - width()) / 2, limit));
    move(x,
         viewport_.height() - height() - overlay::margin - overlay::timelineHeight - overlay::gap);
    raise();
}

} // namespace voelund::desktop
