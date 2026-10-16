#include "ShellHeader.hpp"
#include "PillButton.hpp"
#include "ShellIcons.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QStyle>
#include <QTimer>
#include <QToolButton>

#include <algorithm>

namespace voelund::desktop {
namespace {

// U14 (user report 2026-10-14): every control of the cards is one square size, so the
// symbols sit in the middle and the row lines up.
constexpr int controlSize = 32;

QToolButton* iconButton(QWidget* parent, const char* name) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QLatin1String(name));
    button->setProperty("cardIcon", true);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setIconSize(QSize(18, 18));
    button->setFixedSize(controlSize, controlSize);
    button->setAutoRaise(true);
    return button;
}

QFrame* divider(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setObjectName(QStringLiteral("headerDivider"));
    line->setFixedSize(1, 20);
    return line;
}

QIcon statusDot(const QColor& color, bool filled) {
    QPixmap pixmap(QSize(20, 20) * 2);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.6));
    painter.setBrush(filled ? QBrush(color) : QBrush(Qt::NoBrush));
    painter.drawEllipse(QPointF(10, 10), 4, 4);
    return QIcon(pixmap);
}

} // namespace

ShellHeader::ShellHeader(QWidget& host) : OverlayCard(host, QStringLiteral("shellHeader")) {
    project_ = new OverlayCard(host, QStringLiteral("projectCard"), [this](const QWidget& area) {
        const int width = std::min(project_->sizeHint().width(), area.width() / 2 - 40);
        return QRect(overlay::margin, overlay::margin, std::max(120, width), overlay::barHeight);
    });
    auto* projectLayout = new QHBoxLayout(project_);
    projectLayout->setContentsMargins(8, 6, 12, 6);
    projectLayout->setSpacing(8);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(4);

    QWidget* const card = project_;
    fileButton_ = iconButton(card, "fileMenuButton");
    fileButton_->setPopupMode(QToolButton::InstantPopup);
    fileButton_->setToolTip(tr("Datei"));
    fileButton_->setAccessibleName(tr("Datei"));
    projectLayout->addWidget(fileButton_);

    brand_ = new QLabel(card);
    brand_->setObjectName(QStringLiteral("brandMark"));
    projectLayout->addWidget(brand_);
    auto* brandName = new QLabel(QStringLiteral("Völund"), card);
    brandName->setObjectName(QStringLiteral("brandName"));
    projectLayout->addWidget(brandName);
    auto* separator = new QLabel(QStringLiteral("/"), card);
    separator->setObjectName(QStringLiteral("pathSeparator"));
    projectLayout->addWidget(separator);
    documentName_ = new QLabel(card);
    documentName_->setObjectName(QStringLiteral("documentName"));
    documentName_->setMaximumWidth(260);
    projectLayout->addWidget(documentName_, 1);
    saveState_ = new QLabel(card);
    saveState_->setObjectName(QStringLiteral("saveState"));
    saveState_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    projectLayout->addWidget(saveState_, 0, Qt::AlignVCenter);

    undoButton_ = iconButton(this, "undoButton");
    redoButton_ = iconButton(this, "redoButton");
    layout->addWidget(undoButton_);
    layout->addWidget(redoButton_);
    layout->addSpacing(4);
    layout->addWidget(divider(this));
    layout->addSpacing(4);
    searchButton_ = new QToolButton(this);
    searchButton_->setObjectName(QStringLiteral("commandSearchButton"));
    searchButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    searchButton_->setIconSize(QSize(16, 16));
    searchButton_->setFixedHeight(controlSize);
    searchButton_->setMinimumWidth(240);
    layout->addWidget(searchButton_);
    // PAR02 (user decision 2026-10-16): "Parameter" opens the table under the model tree.
    parametersButton_ = new QToolButton(this);
    parametersButton_->setObjectName(QStringLiteral("parametersButton"));
    parametersButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    parametersButton_->setIconSize(QSize(16, 16));
    parametersButton_->setFixedHeight(controlSize);
    parametersButton_->setAutoRaise(true);
    layout->addWidget(parametersButton_);
    layout->addSpacing(4);
    layout->addWidget(divider(this));
    layout->addSpacing(4);
    railButton_ = iconButton(this, "toolRailButton");
    layout->addWidget(railButton_);
    themeButton_ = iconButton(this, "themeButton");
    layout->addWidget(themeButton_);
    layout->addSpacing(4);
    assistantButton_ = new PillButton(this); // U14: status dot, name and menu chevron
    assistantButton_->setObjectName(QStringLiteral("assistantChip"));
    assistantButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    assistantButton_->setIconSize(QSize(20, 20));
    assistantButton_->setFixedHeight(controlSize);
    assistantButton_->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(assistantButton_);
    showAssistant(AssistantState::off);
    setTheme(ThemeMode::light);
    setPlacement([this](const QWidget& area) {
        setCompact(area.width() < 1100);
        const int width = sizeHint().width();
        return QRect(area.width() - width - overlay::margin, overlay::margin, width,
                     overlay::barHeight);
    });
}

void ShellHeader::setFileMenu(QMenu& menu) { fileButton_->setMenu(&menu); }

// U11 (A-VIS-01): in narrow windows the search shows its symbol only, so the header card
// does not reach the project card.
void ShellHeader::setCompact(bool compact) {
    if (compact == compact_)
        return;
    compact_ = compact;
    searchButton_->setToolButtonStyle(compact ? Qt::ToolButtonIconOnly
                                              : Qt::ToolButtonTextBesideIcon);
    searchButton_->setMinimumWidth(compact ? controlSize : 240);
    searchButton_->setMaximumWidth(compact ? controlSize : QWIDGETSIZE_MAX);
    searchButton_->setProperty("compact", compact); // no side padding around the symbol
    parametersButton_->setToolButtonStyle(compact ? Qt::ToolButtonIconOnly
                                                  : Qt::ToolButtonTextBesideIcon);
    searchButton_->style()->unpolish(searchButton_);
    searchButton_->style()->polish(searchButton_);
}

void ShellHeader::setActions(QAction& undo, QAction& redo, QAction& theme) {
    undoButton_->setDefaultAction(&undo);
    redoButton_->setDefaultAction(&redo);
    themeButton_->setDefaultAction(&theme);
}

void ShellHeader::setViewActions(QAction& search, QAction& toolRail) {
    searchButton_->setDefaultAction(&search);
    railButton_->setDefaultAction(&toolRail);
}

void ShellHeader::setParametersAction(QAction& parameters) {
    parametersButton_->setDefaultAction(&parameters);
    setTheme(mode_);
}

void ShellHeader::display(const ShellState& state) {
    documentName_->setText(state.documentName);
    saveState_->setText(state.modified ? tr("Ungespeichert") : tr("Gespeichert"));
    saveState_->setProperty("modified", state.modified);
    // Re-polish so the [modified="true"] style rule follows the property.
    saveState_->style()->unpolish(saveState_);
    saveState_->style()->polish(saveState_);
}

void ShellHeader::setAssistantMenu(QMenu& menu) { assistantButton_->setMenu(&menu); }

void ShellHeader::setAssistantClicked(std::function<void()> clicked) {
    assistantButton_->setPopupMode(QToolButton::MenuButtonPopup);
    connect(assistantButton_, &QToolButton::clicked, this, [clicked] { clicked(); });
}

void ShellHeader::showAssistant(AssistantState state) {
    static constexpr const char* names[] = {"off", "ready", "connected"};
    const QString texts[] = {tr("Claude: aus"), tr("Claude: bereit"), tr("Claude: verbunden")};
    const auto index = static_cast<std::size_t>(state);
    assistant_ = state;
    // U14: the chip names Claude; the dot shows the state, the tooltip says it.
    assistantButton_->setText(QStringLiteral("Claude"));
    assistantButton_->setAccessibleName(texts[index]);
    assistantButton_->setToolTip(texts[index] + QStringLiteral(" – ") +
                                 tr("Klick öffnet Claude, der Pfeil den Zugang (MCP)"));
    assistantButton_->setProperty("state", QString::fromLatin1(names[index]));
    assistantButton_->style()->unpolish(assistantButton_);
    assistantButton_->style()->polish(assistantButton_);
    showAssistantDot();
}

void ShellHeader::showAssistantDot() {
    const ThemeTokens& tokens = themeTokens(mode_);
    const bool connected = assistant_ == AssistantState::connected;
    assistantButton_->setIcon(assistant_ == AssistantState::off
                                  ? statusDot(tokens.faint, false)
                                  : statusDot(tokens.accent, connected));
}

void ShellHeader::setAssistantVisible(bool visible) { assistantButton_->setVisible(visible); }

void ShellHeader::flashAssistant() {
    const auto repolish = [button = assistantButton_](bool on) {
        button->setProperty("flash", on);
        button->style()->unpolish(button);
        button->style()->polish(button);
    };
    repolish(true);
    QTimer::singleShot(900, assistantButton_, [repolish] { repolish(false); });
}

void ShellHeader::setTheme(ThemeMode mode) {
    mode_ = mode;
    showAssistantDot();
    const ThemeTokens& tokens = themeTokens(mode);
    const auto icon = [&](ShellIcon kind) { return shellIcon(kind, tokens.ink, tokens.accent); };
    brand_->setPixmap(icon(ShellIcon::brand).pixmap(QSize(24, 24), devicePixelRatioF()));
    fileButton_->setIcon(icon(ShellIcon::menu));
    if (auto* action = undoButton_->defaultAction())
        action->setIcon(icon(ShellIcon::undo));
    if (auto* action = redoButton_->defaultAction())
        action->setIcon(icon(ShellIcon::redo));
    if (auto* action = parametersButton_->defaultAction())
        action->setIcon(icon(ShellIcon::parameters));
    // The switch shows the theme it leads to.
    if (auto* action = themeButton_->defaultAction())
        action->setIcon(icon(mode == ThemeMode::dark ? ShellIcon::sun : ShellIcon::moon));
}

} // namespace voelund::desktop
