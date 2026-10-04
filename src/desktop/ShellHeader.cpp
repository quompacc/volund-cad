#include "ShellHeader.hpp"
#include "ShellIcons.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QStyle>
#include <QToolButton>

namespace voelund::desktop {
namespace {

QToolButton* iconButton(QWidget* parent, const char* name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QLatin1String(name));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(18, 18));
    button->setAutoRaise(true);
    return button;
}

} // namespace

ShellHeader::ShellHeader(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("shellHeader"));
    setAttribute(Qt::WA_StyledBackground);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(8);

    fileButton_ = iconButton(this, "fileMenuButton");
    fileButton_->setPopupMode(QToolButton::InstantPopup);
    fileButton_->setToolTip(tr("Datei"));
    fileButton_->setAccessibleName(tr("Datei"));
    layout->addWidget(fileButton_);

    brand_ = new QLabel(this);
    brand_->setObjectName(QStringLiteral("brandMark"));
    layout->addWidget(brand_);
    auto* brandName = new QLabel(QStringLiteral("Völund"), this);
    brandName->setObjectName(QStringLiteral("brandName"));
    layout->addWidget(brandName);
    auto* separator = new QLabel(QStringLiteral("/"), this);
    separator->setObjectName(QStringLiteral("pathSeparator"));
    layout->addWidget(separator);
    documentName_ = new QLabel(this);
    documentName_->setObjectName(QStringLiteral("documentName"));
    layout->addWidget(documentName_);
    saveState_ = new QLabel(this);
    saveState_->setObjectName(QStringLiteral("saveState"));
    saveState_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(saveState_, 0, Qt::AlignVCenter);
    layout->addSpacing(12);

    undoButton_ = iconButton(this, "undoButton");
    redoButton_ = iconButton(this, "redoButton");
    layout->addWidget(undoButton_);
    layout->addWidget(redoButton_);
    layout->addStretch(1);
    searchButton_ = new QToolButton(this);
    searchButton_->setObjectName(QStringLiteral("commandSearchButton"));
    searchButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    searchButton_->setIconSize(QSize(16, 16));
    searchButton_->setMinimumWidth(320);
    layout->addWidget(searchButton_);
    layout->addStretch(1);
    railButton_ = new QToolButton(this);
    railButton_->setObjectName(QStringLiteral("toolRailButton"));
    railButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    railButton_->setIconSize(QSize(18, 18));
    layout->addWidget(railButton_);
    themeButton_ = iconButton(this, "themeButton");
    layout->addWidget(themeButton_);
    setTheme(ThemeMode::light);
}

void ShellHeader::setFileMenu(QMenu& menu) { fileButton_->setMenu(&menu); }

void ShellHeader::setActions(QAction& undo, QAction& redo, QAction& theme) {
    undoButton_->setDefaultAction(&undo);
    redoButton_->setDefaultAction(&redo);
    themeButton_->setDefaultAction(&theme);
}

void ShellHeader::setViewActions(QAction& search, QAction& toolRail) {
    searchButton_->setDefaultAction(&search);
    railButton_->setDefaultAction(&toolRail);
}

void ShellHeader::display(const ShellState& state) {
    documentName_->setText(state.documentName);
    saveState_->setText(state.modified ? tr("Ungespeichert") : tr("Gespeichert"));
    saveState_->setProperty("modified", state.modified);
    // Re-polish so the [modified="true"] style rule follows the property.
    saveState_->style()->unpolish(saveState_);
    saveState_->style()->polish(saveState_);
}

void ShellHeader::setTheme(ThemeMode mode) {
    const ThemeTokens& tokens = themeTokens(mode);
    const auto icon = [&](ShellIcon kind) { return shellIcon(kind, tokens.ink, tokens.accent); };
    brand_->setPixmap(icon(ShellIcon::brand).pixmap(QSize(24, 24), devicePixelRatioF()));
    fileButton_->setIcon(icon(ShellIcon::menu));
    if (auto* action = undoButton_->defaultAction())
        action->setIcon(icon(ShellIcon::undo));
    if (auto* action = redoButton_->defaultAction())
        action->setIcon(icon(ShellIcon::redo));
    // The switch shows the theme it leads to.
    if (auto* action = themeButton_->defaultAction())
        action->setIcon(icon(mode == ThemeMode::dark ? ShellIcon::sun : ShellIcon::moon));
}

} // namespace voelund::desktop
