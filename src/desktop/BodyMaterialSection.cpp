// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "BodyMaterialSection.hpp"

#include <voelund/document/Materials.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLocale>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace voelund::desktop {
namespace {

constexpr int noMaterial = 0;
// Combo rows: none, the presets in list order, then "Eigene Dichte".
constexpr int customRow = static_cast<int>(document::materialPresets.size()) + 1;

QString number(double value, int decimals) {
    return QLocale(QLocale::German).toString(value, 'f', decimals);
}

int rowOf(const std::optional<document::BodyMaterial>& material) {
    if (!material)
        return noMaterial;
    for (std::size_t index = 0; index < document::materialPresets.size(); ++index)
        if (document::materialPresets[index].key == material->key)
            return static_cast<int>(index) + 1;
    return customRow;
}

} // namespace

QString massPropertiesText(const kernel::CombinedMass& totals, bool weighed) {
    QStringList lines;
    lines << (weighed ? QObject::tr("Masse: %1 g").arg(number(totals.mass, 3))
                      : QObject::tr("Masse: – (ohne Material)"));
    lines << QObject::tr("Volumen: %1 mm³").arg(number(totals.volume, 3));
    lines << QObject::tr("Oberfläche: %1 mm²").arg(number(totals.area, 3));
    lines << QObject::tr("Schwerpunkt: %1; %2; %3 mm")
                 .arg(number(totals.center[0], 3), number(totals.center[1], 3),
                      number(totals.center[2], 3));
    if (weighed)
        lines << QObject::tr("Trägheit (Schwerpunkt): Ixx %1, Iyy %2, Izz %3 g·mm²")
                     .arg(number(totals.inertia[0], 3), number(totals.inertia[1], 3),
                          number(totals.inertia[2], 3));
    return lines.join(QLatin1Char('\n'));
}

BodyMaterialSection::BodyMaterialSection(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(new QLabel(tr("Material"), this));
    material_ = new QComboBox(this);
    material_->setObjectName(QStringLiteral("bodyMaterial"));
    material_->addItem(tr("Kein Material"));
    for (const auto& preset : document::materialPresets)
        material_->addItem(tr("%1 (%2 g/cm³)")
                               .arg(QString::fromUtf8(preset.name.data(),
                                                      static_cast<qsizetype>(preset.name.size())),
                                    number(preset.density, 2)));
    material_->addItem(tr("Eigene Dichte"));
    connect(material_, &QComboBox::activated, this, [this](int index) { chosen(index); });
    layout->addWidget(material_);
    density_ = new QDoubleSpinBox(this);
    density_->setObjectName(QStringLiteral("bodyDensity"));
    density_->setRange(document::minimumDensity, document::maximumDensity);
    density_->setDecimals(3);
    density_->setSuffix(tr(" g/cm³"));
    density_->setValue(1.0);
    density_->setVisible(false);
    connect(density_, &QDoubleSpinBox::editingFinished, this, [this] {
        if (!showing_ && material_->currentIndex() == customRow)
            report(document::BodyMaterial{std::string(document::customMaterialKey),
                                          density_->value()});
    });
    layout->addWidget(density_);
    properties_ = new QLabel(this);
    properties_->setObjectName(QStringLiteral("bodyMassProperties"));
    properties_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    properties_->setWordWrap(true);
    layout->addWidget(properties_);
}

void BodyMaterialSection::display(const std::optional<document::BodyMaterial>& material,
                                  const kernel::CombinedMass& totals, bool weighed) {
    showing_ = true;
    const QSignalBlocker blocker(material_);
    material_->setCurrentIndex(rowOf(material));
    density_->setVisible(material_->currentIndex() == customRow);
    if (material)
        density_->setValue(material->density);
    properties_->setText(massPropertiesText(totals, weighed));
    showing_ = false;
}

void BodyMaterialSection::chosen(int index) {
    density_->setVisible(index == customRow);
    if (index == noMaterial)
        return report(std::nullopt);
    if (index == customRow)
        return report(
            document::BodyMaterial{std::string(document::customMaterialKey), density_->value()});
    const auto& preset = document::materialPresets[static_cast<std::size_t>(index - 1)];
    report(document::BodyMaterial{std::string(preset.key), preset.density});
}

void BodyMaterialSection::report(std::optional<document::BodyMaterial> material) {
    if (choice_)
        choice_(std::move(material));
}

} // namespace voelund::desktop
