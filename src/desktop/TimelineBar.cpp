#include "TimelineBar.hpp"
#include "FeatureErrorCard.hpp"

#include <voelund/application/CadSession.hpp>

#include <algorithm>
#include "ShellIcons.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVariant>

#include <utility>

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
    }
    return ShellIcon::box;
}

} // namespace

TimelineBar::TimelineBar(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("timeline"));
    setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 6, 14, 6);
    layout->setSpacing(8);
    auto* title = new QLabel(tr("TIMELINE"), this);
    title->setObjectName(QStringLiteral("timelineTitle"));
    layout->addWidget(title);
    steps_ = new QHBoxLayout;
    steps_->setSpacing(6);
    layout->addLayout(steps_);
    empty_ = new QLabel(
        tr("Noch keine Schritte. Jeder Schritt erscheint hier und bleibt bearbeitbar."), this);
    empty_->setObjectName(QStringLiteral("timelineEmpty"));
    layout->addWidget(empty_);
    end_ = new QLabel(tr("Ende"), this);
    end_->setObjectName(QStringLiteral("timelineEnd"));
    layout->addWidget(end_);
    layout->addStretch(1);
}

void TimelineBar::setOpenHandler(std::function<void(std::uint64_t)> handler) {
    open_ = std::move(handler);
}

void TimelineBar::showFeatures(std::span<const document::FeatureDescriptor> features,
                               std::optional<document::FeatureId> selected,
                               std::span<const document::FeatureId> revolves) {
    for (QToolButton* button : buttons_) {
        steps_->removeWidget(button);
        button->hide();
        button->deleteLater(); // a button may be rebuilding the bar from its own click
    }
    buttons_.clear();
    icons_.clear();
    int number = 1;
    for (const document::FeatureDescriptor& feature : features) {
        auto* button = new QToolButton(this);
        button->setObjectName(QStringLiteral("timelineStep"));
        button->setText(
            QStringLiteral("%1  %2").arg(number++).arg(QString::fromStdString(feature.name)));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(16, 16));
        button->setCheckable(true);
        button->setChecked(selected == feature.id);
        button->setProperty("featureId", QVariant::fromValue<qulonglong>(feature.id.value()));
        button->setToolTip(feature.kind == document::FeatureKind::sketch
                               ? tr("Skizze bearbeiten")
                               : tr("Feature wählen und bearbeiten"));
        const std::uint64_t id = feature.id.value();
        connect(button, &QToolButton::clicked, this, [this, id] {
            if (open_)
                open_(id);
        });
        steps_->addWidget(button);
        button->show();
        buttons_.push_back(button);
        icons_.push_back(std::ranges::find(revolves, feature.id) != revolves.end()
                             ? ShellIcon::revolve
                             : iconFor(feature.kind));
    }
    empty_->setVisible(features.empty());
    end_->setVisible(!features.empty());
    setTheme(mode_);
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
        if (marked) {
            button->setIcon(featureStatusIcon(state->status));
            button->setToolTip(QString::fromStdString(state->message));
        }
    }
}

void TimelineBar::setTheme(ThemeMode mode) {
    mode_ = mode;
    const ThemeTokens& tokens = themeTokens(mode);
    for (std::size_t index = 0; index < buttons_.size(); ++index)
        buttons_[index]->setIcon(shellIcon(icons_[index], tokens.ink, tokens.accent));
}

} // namespace voelund::desktop
