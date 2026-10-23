// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "SectionController.hpp"

#include "MainWindowSupport.hpp"
#include "MeasurementInput.hpp"
#include "OverlayCard.hpp"

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace voelund::desktop {
namespace {

constexpr int height = 156;

// The base planes with the normals of their sketches (document::datumSketchPlane).
std::vector<SectionController::Choice> basePlanes() {
    return {{QObject::tr("XY-Ebene"), {{0, 0, 0}, {0, 0, 1}}},
            {QObject::tr("XZ-Ebene"), {{0, 0, 0}, {0, -1, 0}}},
            {QObject::tr("YZ-Ebene"), {{0, 0, 0}, {1, 0, 0}}}};
}

double dot(const std::array<double, 3>& a, const std::array<double, 3>& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

} // namespace

SectionController::SectionController(QWidget& window, viewport::CadViewport& viewport, Hooks hooks)
    : QObject(&window), viewport_(viewport), hooks_(std::move(hooks)) {
    action_ = new QAction(tr("Schnitt"), &window);
    action_->setCheckable(true);
    action_->setToolTip(tr("Schnittansicht ein- und ausblenden"));
    setCommandId(*action_, "view.section");
    window.addAction(action_);
    connect(action_, &QAction::toggled, this, [this](bool on) { toggled(on); });
    createCard(viewport);
    viewport_.setSectionHandler(
        [this](const viewport::SectionPlane& plane, bool finished) { dragged(plane, finished); });
}

int SectionController::cardHeight() noexcept { return height; }

void SectionController::createCard(viewport::CadViewport& viewport) {
    card_ = new OverlayCard(viewport, QStringLiteral("sectionCard"));
    auto* layout = new QVBoxLayout(card_);
    layout->setContentsMargins(8, 10, 8, 8);
    layout->setSpacing(6);
    auto* head = new QHBoxLayout;
    auto* title = new QLabel(tr("Schnitt"), card_);
    title->setObjectName(QStringLiteral("overlayTitle"));
    head->addWidget(title, 1);
    auto* close = new QToolButton(card_);
    close->setObjectName(QStringLiteral("sectionClose"));
    close->setText(QStringLiteral("✕"));
    close->setToolTip(tr("Schnitt schließen"));
    close->setAutoRaise(true);
    head->addWidget(close);
    layout->addLayout(head);

    auto* planeRow = new QHBoxLayout;
    planes_ = new QComboBox(card_);
    planes_->setObjectName(QStringLiteral("sectionPlane"));
    planeRow->addWidget(planes_, 1);
    useSelection_ = new QToolButton(card_);
    useSelection_->setObjectName(QStringLiteral("sectionUseSelection"));
    useSelection_->setText(tr("Auswahl"));
    useSelection_->setToolTip(tr("Gewählte ebene Fläche oder Ebene als Schnittebene nehmen"));
    planeRow->addWidget(useSelection_);
    layout->addLayout(planeRow);

    auto* offsetRow = new QHBoxLayout;
    offsetRow->addWidget(new QLabel(tr("Versatz"), card_));
    offset_ = makeLengthInput(card_, true);
    offset_->setObjectName(QStringLiteral("sectionOffset"));
    offset_->setValue(0.0);
    offsetRow->addWidget(offset_, 1);
    layout->addLayout(offsetRow);

    flip_ = new QToolButton(card_);
    flip_->setObjectName(QStringLiteral("sectionFlip"));
    flip_->setText(tr("Seite umkehren"));
    flip_->setCheckable(true);
    layout->addWidget(flip_);

    card_->setPlacement([](const QWidget& area) {
        return QRect(overlay::margin, area.height() - overlay::bottomBand - height,
                     overlay::treeWidth, height);
    });
    card_->hide();

    connect(close, &QToolButton::clicked, action_, [this] { action_->setChecked(false); });
    connect(useSelection_, &QToolButton::clicked, this, [this] { useSelection(); });
    connect(planes_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!filling_)
            choose(index);
    });
    connect(offset_, &QDoubleSpinBox::valueChanged, this, [this] {
        if (!filling_)
            apply();
    });
    connect(flip_, &QToolButton::toggled, this, [this] {
        if (!filling_)
            apply();
    });
}

void SectionController::toggled(bool on) {
    card_->setVisible(on);
    if (on) {
        // Opened with a planar face or plane selected, the section starts there.
        taken_ = hooks_.selectedPlane ? hooks_.selectedPlane() : std::nullopt;
        fillChoices(taken_);
        choose(taken_ ? planes_->count() - 1 : 1); // otherwise XZ
    } else {
        viewport_.setSection(std::nullopt);
    }
    if (hooks_.placed)
        hooks_.placed();
}

void SectionController::fillChoices(const std::optional<Choice>& selection) {
    const QSignalBlocker blocker(planes_);
    const int index = planes_->currentIndex();
    choices_ = basePlanes();
    if (hooks_.constructionPlanes)
        for (auto& plane : hooks_.constructionPlanes())
            choices_.push_back(std::move(plane));
    if (selection)
        choices_.push_back(*selection);
    planes_->clear();
    for (const auto& choice : choices_)
        planes_->addItem(choice.name);
    planes_->setCurrentIndex(std::clamp(index, 0, planes_->count() - 1));
}

void SectionController::choose(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= choices_.size())
        return;
    filling_ = true;
    planes_->setCurrentIndex(index);
    offset_->setValue(0.0);
    // The side away from the eye stays, so the cut faces the viewer.
    flip_->setChecked(dot(choices_[static_cast<std::size_t>(index)].plane.normal,
                          {viewport_.cameraFrame().toEye[0], viewport_.cameraFrame().toEye[1],
                           viewport_.cameraFrame().toEye[2]}) > 0.0);
    filling_ = false;
    apply();
}

std::optional<viewport::SectionPlane> SectionController::plane() const {
    const int index = planes_->currentIndex();
    if (!action_->isChecked() || index < 0 || static_cast<std::size_t>(index) >= choices_.size())
        return std::nullopt;
    const auto& base = choices_[static_cast<std::size_t>(index)].plane;
    const double offset = offset_->value();
    const double side = flip_->isChecked() ? -1.0 : 1.0;
    viewport::SectionPlane plane{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        plane.origin[axis] = base.origin[axis] + base.normal[axis] * offset;
        plane.normal[axis] = base.normal[axis] * side;
    }
    return plane;
}

void SectionController::apply() { viewport_.setSection(plane()); }

void SectionController::dragged(const viewport::SectionPlane& plane, bool finished) {
    const int index = planes_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= choices_.size())
        return;
    const auto& base = choices_[static_cast<std::size_t>(index)].plane;
    const std::array<double, 3> shift{plane.origin[0] - base.origin[0],
                                      plane.origin[1] - base.origin[1],
                                      plane.origin[2] - base.origin[2]};
    {
        const QSignalBlocker blocker(offset_);
        offset_->setValue(dot(shift, base.normal));
    }
    if (finished)
        apply();
}

void SectionController::useSelection() {
    taken_ = hooks_.selectedPlane ? hooks_.selectedPlane() : std::nullopt;
    if (!taken_)
        return;
    fillChoices(taken_);
    choose(planes_->count() - 1);
}

void SectionController::sync() {
    useSelection_->setEnabled(hooks_.selectedPlane && hooks_.selectedPlane().has_value());
    if (!action_->isChecked())
        return;
    // New or removed construction planes: the list follows, the chosen plane stays.
    const QString chosen = planes_->currentText();
    std::vector<Choice> before = choices_;
    fillChoices(taken_);
    if (choices_ == before)
        return;
    const int index = planes_->findText(chosen);
    if (index < 0) {
        choose(1);
        return;
    }
    {
        const QSignalBlocker blocker(planes_);
        planes_->setCurrentIndex(index);
    }
    apply();
}

} // namespace voelund::desktop
