// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "TimelineBar.hpp"
#include "FeatureErrorCard.hpp"

#include <voelund/application/CadSession.hpp>

#include "ShellIcons.hpp"
#include <algorithm>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <QVariant>
#include <QWheelEvent>

#include <cmath>
#include <iterator>
#include <optional>
#include <utility>
#include <vector>

namespace voelund::desktop {
namespace {

// W04: Entf deletes the selected feature everywhere; on a focused step it belongs to the step
// (M16), so the step claims the key before the window's shortcut.
class DeleteKeyClaim final : public QObject {
  public:
    using QObject::QObject;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::ShortcutOverride &&
            static_cast<QKeyEvent*>(event)->key() == Qt::Key_Delete) {
            event->accept();
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

ShellIcon iconFor(document::FeatureKind kind) {
    // In FeatureKind order (N12a: a table instead of a switch).
    static constexpr ShellIcon icons[]{
        ShellIcon::box,           ShellIcon::cylinder,  ShellIcon::sketch,
        ShellIcon::extrude,       ShellIcon::fillet,    ShellIcon::chamfer,
        ShellIcon::hole,          ShellIcon::combine,   ShellIcon::datumPlane,
        ShellIcon::datumAxis,     ShellIcon::shell,     ShellIcon::sphere,
        ShellIcon::draft,         ShellIcon::mirror,    ShellIcon::patternLinear,
        ShellIcon::patternLinear, ShellIcon::transform, ShellIcon::importFile,
        ShellIcon::importFile,    ShellIcon::split,     ShellIcon::split,
        ShellIcon::pushPull,      ShellIcon::datumPoint};
    static_assert(std::size(icons) ==
                  static_cast<std::size_t>(document::FeatureKind::datumPoint) + 1);
    const auto index = static_cast<std::size_t>(kind);
    if (index < std::size(icons))
        return icons[index];
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
    // M14: the rollback marker stands after this many steps; none at the end.
    void setRollback(std::optional<std::size_t> after) {
        rollback_ = after;
        update();
    }
    // M15: where a dragged marker was let go: the number of steps before it.
    void setDropHandler(std::function<void(std::size_t)> handler) { drop_ = std::move(handler); }
    // M16: where a dragged step would go, drawn as a line; none hides it.
    void setDropIndicator(std::optional<double> x) {
        indicator_ = x;
        update();
    }

  protected:
    void paintEvent(QPaintEvent* event) override {
        QWidget::paintEvent(event);
        std::vector<QRect> chips;
        for (const QToolButton* chip :
             findChildren<QToolButton*>(QStringLiteral("timelineStep"), Qt::FindDirectChildrenOnly))
            if (chip->isVisibleTo(this))
                chips.push_back(chip->geometry());
        if (chips.empty())
            return;
        std::ranges::sort(chips, {}, &QRect::left);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double y = chips.front().center().y() + 0.5;
        // The end marker stands where its button is (M14), else just after the last step.
        const auto* endButton =
            findChild<QToolButton*>(QStringLiteral("timelineEnd"), Qt::FindDirectChildrenOnly);
        const double endX = endButton != nullptr && endButton->isVisibleTo(this)
                                ? endButton->geometry().center().x()
                                : chips.back().right() + endGap;
        painter.setPen(QPen(axis_, 2));
        painter.drawLine(QPointF(chips.front().center().x(), y), QPointF(endX, y));
        QColor endColor = end_;
        if (indicator_) {
            painter.setPen(QPen(end_, 2, Qt::DashLine, Qt::RoundCap));
            painter.drawLine(QPointF(*indicator_, y - 14), QPointF(*indicator_, y + 14));
        }
        if (dragX_) {
            // M15: the marker follows the mouse while it is dragged.
            painter.setPen(QPen(end_, 3, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(*dragX_, y - 11), QPointF(*dragX_, y + 11));
            endColor = axis_;
        } else if (rollback_ && *rollback_ < chips.size()) {
            // M14: the marker between the last evaluated step and the next; the end is pale.
            const double markerX = (chips[*rollback_ - 1].right() + chips[*rollback_].left()) / 2.0;
            painter.setPen(QPen(end_, 3, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(QPointF(markerX, y - 11), QPointF(markerX, y + 11));
            endColor = axis_;
        }
        painter.setPen(QPen(endColor, 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(endX, y - 9), QPointF(endX, y + 9));
        QFont small = font();
        small.setPixelSize(12);
        painter.setFont(small);
        painter.setPen(number_);
        for (std::size_t index = 0; index < chips.size(); ++index) {
            const QRect& chip = chips[index];
            painter.drawText(QRect(chip.left() - 8, chip.bottom() + 2, chip.width() + 16, 12),
                             Qt::AlignHCenter | Qt::AlignTop, QString::number(index + 1));
        }
    }

    // M15: a press on the marker (or on the end marker) starts a drag along the axis.
    void mousePressEvent(QMouseEvent* event) override {
        const auto chips = chipRects();
        if (chips.empty() || event->button() != Qt::LeftButton)
            return QWidget::mousePressEvent(event);
        const double x = event->position().x();
        if (std::abs(x - markerX(chips)) <= 8) {
            dragX_ = x;
            update();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (!dragX_)
            return QWidget::mouseMoveEvent(event);
        dragX_ = event->position().x();
        update();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        if (!dragX_)
            return QWidget::mouseReleaseEvent(event);
        const auto chips = chipRects();
        const double x = *dragX_;
        dragX_.reset();
        update();
        // The gap is the number of steps whose middle lies left of the drop; at least one.
        const auto before = static_cast<std::size_t>(
            std::ranges::count_if(chips, [x](const QRect& chip) { return chip.center().x() < x; }));
        if (drop_ && !chips.empty())
            drop_(std::max<std::size_t>(1, before));
    }

  private:
    [[nodiscard]] std::vector<QRect> chipRects() const {
        std::vector<QRect> chips;
        for (const QToolButton* chip :
             findChildren<QToolButton*>(QStringLiteral("timelineStep"), Qt::FindDirectChildrenOnly))
            if (chip->isVisibleTo(this))
                chips.push_back(chip->geometry());
        std::ranges::sort(chips, {}, &QRect::left);
        return chips;
    }
    // The marker's x: between two steps when rolled back, else on the end marker.
    [[nodiscard]] double markerX(const std::vector<QRect>& chips) const {
        if (rollback_ && *rollback_ < chips.size())
            return (chips[*rollback_ - 1].right() + chips[*rollback_].left()) / 2.0;
        const auto* endButton =
            findChild<QToolButton*>(QStringLiteral("timelineEnd"), Qt::FindDirectChildrenOnly);
        return endButton != nullptr && endButton->isVisibleTo(this)
                   ? endButton->geometry().center().x()
                   : chips.back().right() + endGap;
    }

    std::optional<double> dragX_;
    std::optional<double> indicator_;
    std::function<void(std::size_t)> drop_;
    std::optional<std::size_t> rollback_;
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
    // M14: a click on the end marker rolls forward; it is hit over the painted marker.
    end_ = new QToolButton(content_);
    end_->setObjectName(QStringLiteral("timelineEnd"));
    end_->setToolTip(tr("Ans Ende"));
    end_->setAutoRaise(true);
    end_->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    end_->setEnabled(false);
    end_->setFixedSize(16, chipSize);
    connect(end_, &QToolButton::clicked, this, [this] {
        if (rollback_)
            rollback_(std::nullopt);
    });
    static_cast<TimelineTrack*>(content_)->setDropHandler(
        [this](std::size_t stepsBefore) { markerDropped(stepsBefore); });
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
    // M16: a step pressed and moved far enough is dragged; let go in a gap it moves there.
    if (const auto step = std::ranges::find(buttons_, watched); step != buttons_.end()) {
        auto* track = static_cast<TimelineTrack*>(content_);
        const auto id = (*step)->property("featureId").toULongLong();
        if (event->type() == QEvent::MouseButtonPress &&
            static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            stepPressed(**step, *static_cast<QMouseEvent*>(event), id);
        } else if (event->type() == QEvent::MouseMove && pressedStep_ == id &&
                   (static_cast<QMouseEvent*>(event)->buttons() & Qt::LeftButton)) {
            const QPoint at =
                (*step)->mapTo(content_, static_cast<QMouseEvent*>(event)->position().toPoint());
            if (draggingStep_ || (at - pressedAt_).manhattanLength() > 10) {
                draggingStep_ = true;
                track->setDropIndicator(at.x());
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease && pressedStep_ == id) {
            const bool dragged = draggingStep_;
            const QPoint at =
                (*step)->mapTo(content_, static_cast<QMouseEvent*>(event)->position().toPoint());
            pressedStep_.reset();
            draggingStep_ = false;
            track->setDropIndicator(std::nullopt);
            if (dragged) {
                // The new place is after the last other step whose middle lies left of the drop.
                std::optional<std::uint64_t> after;
                for (QToolButton* other : buttons_)
                    if (other != *step && other->geometry().center().x() < at.x())
                        after = other->property("featureId").toULongLong();
                if (reorder_)
                    reorder_(id, after);
                return true; // no click
            }
        }
    }
    // M15: Left/Right/Home/End select a step, Enter opens it.
    if (event->type() == QEvent::KeyPress) {
        const auto button = std::ranges::find(buttons_, watched);
        if (button != buttons_.end()) {
            const auto index = button - buttons_.begin();
            const auto last = static_cast<std::ptrdiff_t>(buttons_.size()) - 1;
            const int key = static_cast<QKeyEvent*>(event)->key();
            std::optional<std::ptrdiff_t> target;
            if (key == Qt::Key_Left)
                target = std::max<std::ptrdiff_t>(0, index - 1);
            else if (key == Qt::Key_Right)
                target = std::min(last, index + 1);
            else if (key == Qt::Key_Home)
                target = 0;
            else if (key == Qt::Key_End)
                target = last;
            if (target) {
                QToolButton* chosen = buttons_[static_cast<std::size_t>(*target)];
                const auto id = chosen->property("featureId").toULongLong();
                if (select_)
                    select_(id); // rebuilds the steps
                for (QToolButton* step : buttons_)
                    if (step->property("featureId").toULongLong() == id) {
                        step->setFocus(Qt::TabFocusReason);
                        scroll_->ensureWidgetVisible(step, 8, 0);
                    }
                return true;
            }
            if ((key == Qt::Key_Return || key == Qt::Key_Enter) && open_) {
                open_((*button)->property("featureId").toULongLong());
                return true;
            }
            if (key == Qt::Key_Delete && delete_) { // M16
                delete_((*button)->property("featureId").toULongLong());
                return true;
            }
        }
    }
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

void TimelineBar::setMenuHandler(std::function<void(std::uint64_t, QPoint)> handler) {
    menu_ = std::move(handler);
}

void TimelineBar::setRollbackHandler(std::function<void(std::optional<std::uint64_t>)> handler) {
    rollback_ = std::move(handler);
}

void TimelineBar::setSelectHandler(std::function<void(std::uint64_t)> handler) {
    select_ = std::move(handler);
}

void TimelineBar::setReorderHandler(
    std::function<void(std::uint64_t, std::optional<std::uint64_t>)> handler) {
    reorder_ = std::move(handler);
}

void TimelineBar::setDeleteHandler(std::function<void(std::uint64_t)> handler) {
    delete_ = std::move(handler);
}

// M16: a pressed step may become a drag; N02b: its modifiers decide a group click.
void TimelineBar::stepPressed(QToolButton& step, const QMouseEvent& event, std::uint64_t id) {
    pressedStep_ = id;
    pressedAt_ = step.mapTo(content_, event.position().toPoint());
    draggingStep_ = false;
    pressModifiers_ = event.modifiers();
}

void TimelineBar::setGroupHandler(std::function<void(std::uint64_t, bool)> handler) {
    group_ = std::move(handler);
}

void TimelineBar::markGroup(std::span<const document::FeatureId> features) {
    for (QToolButton* button : buttons_) {
        const document::FeatureId id{button->property("featureId").toULongLong()};
        const bool grouped = std::ranges::find(features, id) != features.end();
        if (button->property("grouped").toBool() == grouped)
            continue;
        button->setProperty("grouped", grouped);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

// M15: steps before the gap; all of them means the end.
void TimelineBar::markerDropped(std::size_t stepsBefore) {
    if (!rollback_ || buttons_.empty())
        return;
    if (stepsBefore >= buttons_.size())
        rollback_(std::nullopt);
    else
        rollback_(buttons_[stepsBefore - 1]->property("featureId").toULongLong());
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
        // M10/M17: the copies of a pattern are one step, the pattern's; so are an import's parts.
        if (document::isCompanion(feature.kind))
            continue;
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
            const auto modifiers = pressModifiers_;
            pressModifiers_ = {};
            if ((modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) && group_)
                group_(id, (modifiers & Qt::ShiftModifier) != 0);
            else if (open_)
                open_(id);
        });
        button->setFocusPolicy(Qt::StrongFocus);
        button->installEventFilter(this); // M15: keyboard
        button->installEventFilter(new DeleteKeyClaim(button));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this,
                [this, id, button](const QPoint& at) {
                    if (menu_)
                        menu_(id, button->mapToGlobal(at));
                });
        steps_->addWidget(button);
        button->show();
        buttons_.push_back(button);
        const auto special = std::ranges::find(icons, feature.id, &IconOverride::first);
        icons_.push_back(special != icons.end() ? special->second : iconFor(feature.kind));
    }
    empty_->setVisible(features.empty());
    steps_->removeWidget(end_);
    steps_->addWidget(end_); // the end marker follows the last step
    end_->setVisible(!features.empty());
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
        const bool suppressed =
            state != states.end() && state->status == application::FeatureRuntimeStatus::suppressed;
        const bool rolledBack =
            state != states.end() && state->status == application::FeatureRuntimeStatus::rolledBack;
        const bool marked = !suppressed && !rolledBack && state != states.end() &&
                            state->status != application::FeatureRuntimeStatus::current;
        button->setProperty("featureState",
                            rolledBack   ? QStringLiteral("rolledBack") // M14
                            : suppressed ? QStringLiteral("suppressed")
                            : !marked    ? QString{}
                            : state->status == application::FeatureRuntimeStatus::error
                                ? QStringLiteral("error")
                                : QStringLiteral("blocked"));
        button->style()->unpolish(button);
        button->style()->polish(button);
        if (suppressed)
            button->setToolTip(button->text() + QLatin1Char('\n') + tr("Unterdrückt"));
        if (rolledBack)
            button->setToolTip(button->text() + QLatin1Char('\n') + tr("Zurückgerollt"));
        if (marked) {
            button->setIcon(featureStatusIcon(state->status));
            button->setToolTip(button->text() + QLatin1Char('\n') +
                               QString::fromStdString(state->message));
        }
    }
    // M14: the marker after the last evaluated step; the end marker then rolls forward.
    const auto first = std::ranges::find_if(buttons_, [](const QToolButton* button) {
        return button->property("featureState").toString() == QStringLiteral("rolledBack");
    });
    const bool rolledBack = first != buttons_.end() && first != buttons_.begin();
    static_cast<TimelineTrack*>(content_)->setRollback(
        rolledBack ? std::optional{static_cast<std::size_t>(first - buttons_.begin())}
                   : std::nullopt);
    end_->setEnabled(rolledBack);
    end_->setAttribute(Qt::WA_TransparentForMouseEvents, !rolledBack);
}

void TimelineBar::setTheme(ThemeMode mode) {
    mode_ = mode;
    const ThemeTokens& tokens = themeTokens(mode);
    static_cast<TimelineTrack*>(content_)->setColors(tokens.line, tokens.muted, tokens.accent);
    for (std::size_t index = 0; index < buttons_.size(); ++index)
        buttons_[index]->setIcon(shellIcon(icons_[index], tokens.ink, tokens.accent));
}

} // namespace voelund::desktop
