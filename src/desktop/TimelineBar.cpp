#include "TimelineBar.hpp"
#include "FeatureErrorCard.hpp"

#include <voelund/application/CadSession.hpp>

#include <algorithm>
#include "ShellIcons.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <QWheelEvent>

#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

ShellIcon iconFor(document::FeatureKind kind) {
    switch (kind) {
    case document::FeatureKind::box:
        return ShellIcon::box;
    case document::FeatureKind::cylinder:
        return ShellIcon::cylinder;
    case document::FeatureKind::sketch:
        return ShellIcon::sketch;
    case document::FeatureKind::extrusion:
        return ShellIcon::extrude;
    case document::FeatureKind::fillet:
        return ShellIcon::fillet;
    case document::FeatureKind::chamfer:
        return ShellIcon::chamfer;
    case document::FeatureKind::hole:
        return ShellIcon::hole;
    case document::FeatureKind::combine:
        return ShellIcon::combine;
    case document::FeatureKind::datumPlane:
        return ShellIcon::datumPlane;
    case document::FeatureKind::datumAxis:
        return ShellIcon::datumAxis;
    case document::FeatureKind::shell:
        return ShellIcon::shell;
    case document::FeatureKind::primitive:
        return ShellIcon::sphere; // M27
    case document::FeatureKind::draft:
        return ShellIcon::draft; // M25
    }
    return ShellIcon::box;
}

constexpr int chipSize = 30;
constexpr int sidePadding = 10;
constexpr int endGap = 14; // from the last step to the end marker

// U14 (user report 2026-10-14): the steps sit as nodes on a time axis, numbered below, and
// the axis ends in the marker of the model's end (M15 moves it back for a rollback).
class TimelineTrack final : public QWidget {
  public:
    using QWidget::QWidget;

    void setColors(const QColor& axis, const QColor& number, const QColor& end) {
        axis_ = axis;
        number_ = number;
        end_ = end;
        update();
    }

  protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        std::vector<QRect> chips;
        for (const QToolButton* chip : findChildren<QToolButton*>(QStringLiteral("timelineStep"),
                                                                  Qt::FindDirectChildrenOnly))
            if (chip->isVisibleTo(this))
                chips.push_back(chip->geometry());
        if (chips.empty())
            return;
        std::ranges::sort(chips, {}, &QRect::left);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double y = chips.front().center().y() + 0.5;
        const double endX = chips.back().right() + endGap;
        painter.setPen(QPen(axis_, 2));
        painter.drawLine(QPointF(chips.front().center().x(), y), QPointF(endX, y));
        painter.setPen(QPen(end_, 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(endX, y - 9), QPointF(endX, y + 9));
        QFont small = font();
        small.setPixelSize(10);
        painter.setFont(small);
        painter.setPen(number_);
        for (std::size_t index = 0; index < chips.size(); ++index) {
            const QRect& chip = chips[index];
            painter.drawText(QRect(chip.left() - 8, chip.bottom() + 2, chip.width() + 16, 12),
                             Qt::AlignHCenter | Qt::AlignTop, QString::number(index + 1));
        }
    }

  private:
    QColor axis_{Qt::gray};
    QColor number_{Qt::gray};
    QColor end_{Qt::darkGray};
};

} // namespace

TimelineBar::TimelineBar(QWidget& host) : OverlayCard(host, QStringLiteral("timeline")) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(sidePadding, 0, sidePadding, 0);
    layout->setSpacing(0);
    scroll_ = new QScrollArea(this);
    scroll_->setObjectName(QStringLiteral("timelineScroll"));
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_->setWidgetResizable(true);
    content_ = new TimelineTrack(scroll_);
    content_->setObjectName(QStringLiteral("timelineContent"));
    steps_ = new QHBoxLayout(content_);
    // Room below the nodes for their numbers, right of them for the end marker.
    steps_->setContentsMargins(2, 4, endGap + 6, 14);
    steps_->setSpacing(14);
    empty_ = new QLabel(tr("Noch keine Schritte"), content_);
    empty_->setObjectName(QStringLiteral("timelineEmpty"));
    steps_->addWidget(empty_);
    scroll_->setWidget(content_);
    layout->addWidget(scroll_);
    // The wheel scrolls the steps sideways; there is nothing to scroll up or down.
    scroll_->viewport()->installEventFilter(this);
    setPlacement([this](const QWidget& area) { return placement(area); });
}

QRect TimelineBar::placement(const QWidget& host) const {
    // Centred, clear of the tool panel card on the right (and so of the tree card on the
    // left); the steps scroll when they need more room.
    const int side = overlay::margin + overlay::rightColumnWidth + overlay::gap;
    const int room = std::max(160, host.width() - 2 * side);
    const int wanted = content_->sizeHint().width() + 2 * sidePadding + 4;
    const int width = std::min(room, wanted);
    return {(host.width() - width) / 2, host.height() - overlay::timelineHeight - overlay::margin,
            width, overlay::timelineHeight};
}

bool TimelineBar::eventFilter(QObject* watched, QEvent* event) {
    if (scroll_ != nullptr && watched == scroll_->viewport() && event->type() == QEvent::Wheel) {
        const QPoint delta = static_cast<QWheelEvent*>(event)->angleDelta();
        QScrollBar* bar = scroll_->horizontalScrollBar();
        bar->setValue(bar->value() - (delta.y() != 0 ? delta.y() : delta.x()) / 3);
        return true;
    }
    return OverlayCard::eventFilter(watched, event);
}

void TimelineBar::setOpenHandler(std::function<void(std::uint64_t)> handler) {
    open_ = std::move(handler);
}

void TimelineBar::showFeatures(std::span<const document::FeatureDescriptor> features,
                               std::optional<document::FeatureId> selected,
                               std::span<const IconOverride> icons) {
    for (QToolButton* button : buttons_) {
        steps_->removeWidget(button);
        button->hide();
        button->deleteLater(); // a button may be rebuilding the bar from its own click
    }
    buttons_.clear();
    icons_.clear();
    int number = 1;
    QToolButton* current = nullptr;
    for (const document::FeatureDescriptor& feature : features) {
        auto* button = new QToolButton(content_);
        button->setObjectName(QStringLiteral("timelineStep"));
        // The text stays for accessibility and lookups; the chip shows its symbol only.
        const QString text =
            QStringLiteral("%1  %2").arg(number++).arg(QString::fromStdString(feature.name));
        button->setText(text);
        button->setAccessibleName(text);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(18, 18));
        button->setFixedSize(chipSize, chipSize);
        button->setCheckable(true);
        button->setChecked(selected == feature.id);
        if (selected == feature.id)
            current = button;
        button->setProperty("featureId", QVariant::fromValue<qulonglong>(feature.id.value()));
        button->setToolTip(text + QLatin1Char('\n') +
                           (feature.kind == document::FeatureKind::sketch
                                ? tr("Skizze bearbeiten")
                                : tr("Feature wählen und bearbeiten")));
        const std::uint64_t id = feature.id.value();
        connect(button, &QToolButton::clicked, this, [this, id] {
            if (open_)
                open_(id);
        });
        steps_->addWidget(button);
        button->show();
        buttons_.push_back(button);
        const auto special = std::ranges::find(icons, feature.id, &IconOverride::first);
        icons_.push_back(special != icons.end() ? special->second : iconFor(feature.kind));
    }
    empty_->setVisible(features.empty());
    setTheme(mode_);
    place();
    // The selected step, else the newest one, is in view.
    if (QToolButton* shown = current != nullptr ? current
                             : buttons_.empty() ? nullptr
                                                : buttons_.back())
        scroll_->ensureWidgetVisible(shown, 8, 0);
}

void TimelineBar::showStates(std::span<const application::FeatureRuntimeState> states) {
    for (QToolButton* button : buttons_) {
        const auto id = button->property("featureId").toULongLong();
        const auto state = std::ranges::find(states, document::FeatureId{id},
                                             &application::FeatureRuntimeState::id);
        const bool marked =
            state != states.end() && state->status != application::FeatureRuntimeStatus::current;
        button->setProperty("featureState",
                            !marked ? QString{}
                            : state->status == application::FeatureRuntimeStatus::error
                                ? QStringLiteral("error")
                                : QStringLiteral("blocked"));
        button->style()->unpolish(button);
        button->style()->polish(button);
        if (marked) {
            button->setIcon(featureStatusIcon(state->status));
            button->setToolTip(button->text() + QLatin1Char('\n') +
                               QString::fromStdString(state->message));
        }
    }
}

void TimelineBar::setTheme(ThemeMode mode) {
    mode_ = mode;
    const ThemeTokens& tokens = themeTokens(mode);
    static_cast<TimelineTrack*>(content_)->setColors(tokens.line, tokens.muted, tokens.accent);
    for (std::size_t index = 0; index < buttons_.size(); ++index)
        buttons_[index]->setIcon(shellIcon(icons_[index], tokens.ink, tokens.accent));
}

} // namespace voelund::desktop
