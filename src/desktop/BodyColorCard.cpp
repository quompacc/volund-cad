// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "BodyColorCard.hpp"

#include "BodyMaterialSection.hpp"

#include <QColor>
#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace voelund::desktop {
namespace {

QColor qColor(std::uint32_t rgb) { return QColor::fromRgb(static_cast<QRgb>(rgb)); }

QString swatchStyle(std::uint32_t rgb) {
    return QStringLiteral("QToolButton { background: %1; border-radius: 6px; }"
                          "QToolButton:checked { border: 2px solid palette(highlight); }")
        .arg(qColor(rgb).name());
}

} // namespace

BodyColorCard::BodyColorCard(QWidget& host) : FloatingCard(host, QStringLiteral("bodyCard")) {
    auto* label = new QLabel(tr("Farbe"), this);
    body().addWidget(label);
    auto* grid = new QGridLayout;
    grid->setSpacing(6);
    for (std::size_t index = 0; index < palette().size(); ++index) {
        const auto rgb = palette()[index];
        auto* swatch = new QToolButton(this);
        swatch->setObjectName(QStringLiteral("bodyColor%1").arg(index));
        swatch->setCheckable(true);
        swatch->setFixedSize(28, 28);
        swatch->setStyleSheet(swatchStyle(rgb));
        swatch->setToolTip(qColor(rgb).name());
        connect(swatch, &QToolButton::clicked, this, [this, rgb] {
            if (choice_)
                choice_(rgb);
        });
        grid->addWidget(swatch, static_cast<int>(index / 5), static_cast<int>(index % 5));
        swatches_.push_back(swatch);
    }
    body().addLayout(grid);
    auto* row = new QHBoxLayout;
    custom_ = new QToolButton(this);
    custom_->setObjectName(QStringLiteral("bodyColorCustom"));
    custom_->setText(tr("Eigene…"));
    connect(custom_, &QToolButton::clicked, this, [this] { chooseCustom(); });
    row->addWidget(custom_);
    standard_ = new QToolButton(this);
    standard_->setObjectName(QStringLiteral("bodyColorDefault"));
    standard_->setText(tr("Standard"));
    standard_->setToolTip(tr("Die Körperfarbe des Farbschemas"));
    connect(standard_, &QToolButton::clicked, this, [this] {
        if (choice_)
            choice_(std::nullopt);
    });
    row->addWidget(standard_);
    row->addStretch(1);
    body().addLayout(row);
    material_ = new BodyMaterialSection(this); // N07
    body().addWidget(material_);
}

const std::array<std::uint32_t, 10>& BodyColorCard::palette() noexcept {
    // Steel, red, orange, yellow, green, teal, blue, violet, brown, slate.
    static constexpr std::array<std::uint32_t, 10> colors{0x8A9BB0, 0xC0392B, 0xE67E22, 0xF1C40F,
                                                          0x27AE60, 0x16A085, 0x2E86C1, 0x8E44AD,
                                                          0x7B5B45, 0x2C3E50};
    return colors;
}

void BodyColorCard::display(const QString& bodyName, std::optional<std::uint32_t> color) {
    setTitle(bodyName);
    color_ = color;
    for (std::size_t index = 0; index < swatches_.size(); ++index)
        swatches_[index]->setChecked(color && palette()[index] == *color);
    standard_->setEnabled(color.has_value());
    custom_->setToolTip(color ? qColor(*color).name() : tr("Eine eigene Farbe wählen"));
}

void BodyColorCard::chooseCustom() {
    const QColor chosen = QColorDialog::getColor(color_ ? qColor(*color_) : QColor(Qt::gray), this,
                                                 tr("Körperfarbe"));
    if (chosen.isValid() && choice_)
        choice_(static_cast<std::uint32_t>(chosen.rgb() & 0xFFFFFFU));
}

} // namespace voelund::desktop
