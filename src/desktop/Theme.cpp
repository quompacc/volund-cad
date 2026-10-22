// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "Theme.hpp"

#include <QFontDatabase>
#include <QSettings>
#include <QStringList>

namespace voelund::desktop {
namespace {

QString hex(const QColor& color) { return color.name(QColor::HexRgb); }

QString registeredFamily(const QString& resource, const QString& fallback) {
    const int id = QFontDatabase::addApplicationFont(resource);
    const QStringList families =
        id < 0 ? QStringList{} : QFontDatabase::applicationFontFamilies(id);
    return families.isEmpty() ? fallback : families.front();
}

} // namespace

const ThemeTokens& themeTokens(ThemeMode mode) {
    static const ThemeTokens light{
        QColor(0xE9, 0xEC, 0xEF), QColor(0xFF, 0xFF, 0xFF), QColor(0x1E, 0x23, 0x28),
        QColor(0x5F, 0x68, 0x73), QColor(0xA3, 0xAB, 0xB4), QColor(0xDA, 0xDF, 0xE4),
        QColor(0xEE, 0xF1, 0xF4), QColor(0xF6, 0xF7, 0xF9), QColor(0xD9, 0x62, 0x2B),
        QColor(0xC0, 0x53, 0x1F), QColor(0xFB, 0xE9, 0xDF), QColor(0x8F, 0x3C, 0x14),
        QColor(0xFF, 0xFF, 0xFF), QColor(0xB4, 0x2E, 0x2E), QColor(0xFB, 0xE4, 0xE2),
        QColor(0xFF, 0xFF, 0xFF), QColor(0xF1, 0xF3, 0xF5), QColor(0xE4, 0xE8, 0xEC),
        QColor(0xB9, 0xC1, 0xCA)};
    static const ThemeTokens dark{
        QColor(0x14, 0x18, 0x1C), QColor(0x1D, 0x22, 0x27), QColor(0xE7, 0xEA, 0xED),
        QColor(0xA0, 0xAA, 0xB4), QColor(0x5D, 0x67, 0x72), QColor(0x2E, 0x35, 0x3C),
        QColor(0x27, 0x2D, 0x33), QColor(0x22, 0x28, 0x2E), QColor(0xEF, 0x7A, 0x3E),
        QColor(0xF6, 0x8B, 0x54), QColor(0x3B, 0x25, 0x19), QColor(0xF7, 0xB8, 0x93),
        QColor(0x1A, 0x0F, 0x08), QColor(0xF0, 0x8A, 0x84), QColor(0x3D, 0x1F, 0x1E),
        QColor(0x2A, 0x30, 0x36), QColor(0x23, 0x29, 0x2F), QColor(0x1E, 0x24, 0x29),
        QColor(0x46, 0x50, 0x5A)};
    return mode == ThemeMode::dark ? dark : light;
}

viewport::ViewportTheme viewportTheme(ThemeMode mode) {
    if (mode == ThemeMode::light)
        return {};
    return {QColor(0x14, 0x18, 0x1C), QColor(0x22, 0x29, 0x30), QColor(0x34, 0x3E, 0x48),
            QColor(0x8A, 0x96, 0xA3), QColor(0x4A, 0x54, 0x5F), QColor(0xF6, 0x8B, 0x54),
            QColor(0xEF, 0x7A, 0x3E), QColor(0xA0, 0xAA, 0xB4), QColor(0x5A, 0x9C, 0xFF),
            QColor(0xE8, 0xEB, 0xEE), QColor(0x7F, 0xD4, 0xFF)};
}

QString uiFontFamily() {
    static const QString family = registeredFamily(
        QStringLiteral(":/voelund/fonts/AlbertSans-Variable.ttf"), QStringLiteral("Sans Serif"));
    return family;
}

QString valueFontFamily() {
    static const QString family = registeredFamily(
        QStringLiteral(":/voelund/fonts/JetBrainsMono-Variable.ttf"), QStringLiteral("Monospace"));
    return family;
}

ThemeMode storedThemeMode() {
    return QSettings().value(QStringLiteral("ui/theme")).toString() == QStringLiteral("dark")
               ? ThemeMode::dark
               : ThemeMode::light;
}

void storeThemeMode(ThemeMode mode) {
    QSettings().setValue(QStringLiteral("ui/theme"), mode == ThemeMode::dark
                                                         ? QStringLiteral("dark")
                                                         : QStringLiteral("light"));
}

QString themeStyleSheet(const ThemeTokens& t) {
    QString sheet = QStringLiteral(R"(
QMainWindow, QDialog { background: @bg; color: @ink; }
QWidget { color: @ink; font-family: "@ui"; font-size: 13px; }
QMainWindow::separator { background: @line; width: 1px; height: 1px; }
QToolTip { background: @ink; color: @panel; border: 0; padding: 4px 8px; }

/* U14: the shell floats over the viewport as rounded cards. */
QFrame[overlayCard="true"] { background: @panel; border: 1px solid @line; border-radius: 14px; }
#overlayTitle { color: @muted; font-size: 11px; font-weight: 600; letter-spacing: 1px;
    padding: 0 6px; }
#projectCard QLabel#brandName { font-weight: 700; font-size: 15px; }
#projectCard QLabel#documentName { font-weight: 600; }
#projectCard QLabel#pathSeparator { color: @faint; }
#projectCard QLabel#saveState { color: @muted; background: @soft; border-radius: 9px;
    padding: 2px 10px; font-size: 12px; }
#projectCard QLabel#saveState[modified="true"] { color: @accentInk; background: @accentSoft; }
QLabel#toolStatus { font-weight: 600; color: @ink; }
QLabel#toolStatus[good="true"] { color: @accentInk; }

QToolButton { background: transparent; border: 1px solid transparent; border-radius: 8px;
    padding: 5px 10px; min-height: 26px; }
QToolButton:hover { background: @soft; }
QToolButton:pressed, QToolButton:checked { background: @accentSoft; color: @accentInk;
    border-color: @accentSoft; }
QToolButton:disabled { color: @faint; }
QToolButton::menu-indicator { image: none; width: 0; }
QToolButton::menu-arrow { image: none; width: 0; height: 0; } /* PillButton paints it */
QToolButton:focus { border-color: @accent; }
/* AI04c: Claude access chip; it lights up once for every change made over MCP. */
#shellHeader QToolButton#assistantChip { color: @muted; background: transparent;
    border: 1px solid @line; border-radius: 16px; padding: 0 26px 0 6px; min-height: 0;
    font-size: 12px; font-weight: 600; }
#shellHeader QToolButton#assistantChip:hover { background: @soft; }
#shellHeader QToolButton#assistantChip::menu-button { border: 0; width: 22px; }
#shellHeader QToolButton#assistantChip[state="ready"] { color: @ink; }
#shellHeader QToolButton#assistantChip[state="connected"] { color: @accentInk;
    background: @accentSoft; border-color: @accentSoft; }
#shellHeader QToolButton#assistantChip[flash="true"] { color: @onAccent; background: @accent;
    border-color: @accent; }
/* U14 (user decision 2026-10-14): the pill of the Claude chip is the button look of the
   shell: a thin line around, fully rounded, one height. */
QToolButton[cardIcon="true"] { padding: 0; min-height: 0; border: 1px solid @line;
    border-radius: 16px; }
QToolButton[cardIcon="true"]:checked { background: @accentSoft; border-color: @accent; }
QToolButton[railPill="true"] { border: 1px solid @line; border-radius: 17px;
    padding: 0 14px 0 10px; min-height: 0; font-size: 12px; font-weight: 600; }
QToolButton[railPill="true"][chevron="true"] { padding-right: 28px; }
QToolButton[railPill="true"]:checked { background: @accentSoft; border-color: @accent;
    color: @accentInk; }
QToolButton[sketchTool="true"] { border: 1px solid transparent; border-radius: 17px;
    padding: 0; min-height: 0; }
QToolButton[sketchTool="true"]:hover { border-color: @line; background: @soft; }
QToolButton[sketchTool="true"]:checked { border-color: @accent; background: @accentSoft; }
QToolButton[sketchTool="true"]::menu-button { border: 0; width: 14px; }
#contextBar QToolButton { border: 1px solid @line; border-radius: 16px; padding: 0 14px;
    min-height: 30px; }
#contextBar QToolButton[primary="true"] { border-color: @accent; }
#headerDivider, #railDivider { background: @line; }

QFrame[floatingCard="true"] { background: @panel; border: 1px solid @line; border-radius: 14px; }
QFrame[floatingCard="true"] QScrollArea, #cardContent { background: transparent; border: 0; }
#cardTitle { font-weight: 700; font-size: 15px; }
QGroupBox { border: 0; margin-top: 18px; padding: 0; }
QFrame[floatingCard="true"] QTabBar::tab { padding: 6px 8px; }
QFrame[floatingCard="true"] QPushButton { padding: 6px 14px; }
QFrame[floatingCard="true"] QToolButton { padding: 5px 4px; }
QGroupBox::title { subcontrol-origin: margin; left: 0; color: @muted; font-size: 12px; }
#toolGuidance { color: @muted; }
#timelineScroll, #timelineContent { background: transparent; border: 0; }
#timelineEmpty { color: @muted; padding: 0 8px; }
QToolButton#timelineStep { background: @panel; border: 1px solid @line; border-radius: 15px;
    padding: 0; min-height: 0; }
QToolButton#timelineStep:hover { background: @soft; }
QToolButton#timelineStep:checked, QToolButton#timelineStep[grouped="true"] { border-color: @accent; background: @accentSoft; }
QToolButton#timelineStep[featureState="error"] { border-color: @error; background: @errorSoft; }
QToolButton#timelineStep[featureState="blocked"] { border-color: @faint; }
QToolButton#timelineStep[featureState="suppressed"] { border-style: dashed; border-color: @faint; background: transparent; }
QToolButton#timelineStep[featureState="rolledBack"] { border-color: @faint; background: transparent; color: @faint; }
#timeline QScrollBar:horizontal { height: 4px; }
#toolRail { background: transparent; border: 0; }
#contextBar, #commandSearch { background: @panel; border: 1px solid @line;
    border-radius: 14px; }
#railScroll, #railContent, #sketchToolPage { background: transparent; border: 0; }
#railFlyout QToolButton { font-size: 13px; padding: 5px 10px; text-align: left;
    border-radius: 16px; }
#toolRailSeparator, #contextDivider { background: @line; }
#railGroupTitle { color: @muted; font-size: 11px; padding: 6px 4px 0 4px; }
#contextTitle { font-weight: 600; padding: 0 8px; }
QToolButton[primary="true"] { background: @accent; color: @onAccent; border-color: @accent; }
QToolButton[primary="true"]:hover { background: @accentHover; }
QToolButton[primary="true"]:disabled { background: @accentSoft; color: @muted; }
#commandSearchButton { background: @field; border: 1px solid @line; color: @muted;
    text-align: left; padding: 0 12px; min-height: 0; border-radius: 16px; }
#commandSearchButton[compact="true"] { padding: 0; }
#commandSearch QListWidget { background: @panel; border: 0; outline: 0; }
#commandSearch QListWidget::item { min-height: 30px; border-radius: 6px; padding: 0 6px; }
#commandSearch QListWidget::item:selected { background: @accentSoft; color: @accentInk; }
#commandSearch QListWidget::item:disabled { color: @faint; }

QPushButton { background: @panel; border: 1px solid @line; border-radius: 16px;
    padding: 6px 16px; min-height: 20px; }
QPushButton:hover { background: @soft; }
QPushButton:default { background: @accent; color: @onAccent; border-color: @accent; }
QPushButton:default:hover { background: @accentHover; }
QPushButton:disabled { color: @faint; }
QPushButton:focus { border-color: @accent; }

QToolBar { background: @panel; border: 0; border-bottom: 1px solid @line; spacing: 2px;
    padding: 3px 8px; }
QToolBar::separator { background: @line; width: 1px; margin: 6px 6px; }
QToolBar QLabel { color: @muted; font-size: 12px; letter-spacing: 1px; }

QDockWidget { color: @muted; font-size: 12px; }
QDockWidget::title { background: @panel; padding: 10px 12px 6px 12px; text-align: left; }
QDockWidget > QWidget { background: @panel; }
QTabBar { qproperty-drawBase: 0; background: @panel; }
QTabBar::tab { background: @panel; color: @muted; padding: 6px 14px; border: 0;
    border-top: 2px solid transparent; }
QTabBar::tab:selected { color: @ink; border-top-color: @accent; }

QTreeView, QTreeWidget, QListView, QTableView, QTableWidget { background: @panel;
    border: 0; outline: 0; }
QTreeView::item { min-height: 26px; border-radius: 6px; padding: 0 4px; }
QTreeView::item:hover { background: @soft; }
QTreeView::branch { background: transparent; border-image: none; image: none; }
QTreeView::item:selected { background: @accentSoft; color: @accentInk; }
QHeaderView::section { background: @panel; color: @muted; border: 0;
    border-bottom: 1px solid @line; padding: 4px 6px; }

QLineEdit, QAbstractSpinBox, QComboBox { background: @field; border: 1px solid @line;
    border-radius: 7px; padding: 4px 8px; min-height: 22px; selection-background-color: @accent;
    selection-color: @onAccent; }
QAbstractSpinBox { font-family: "@value"; }
QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus { border-color: @accent; }
QLineEdit:disabled, QAbstractSpinBox:disabled, QComboBox:disabled { color: @faint; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 0; border: 0; }
QComboBox::drop-down { border: 0; width: 18px; }
QComboBox QAbstractItemView { background: @panel; border: 1px solid @line;
    selection-background-color: @accentSoft; selection-color: @accentInk; }

QMenu { background: @panel; border: 1px solid @line; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 6px; }
QMenu::item:selected { background: @soft; }
QMenu::item:disabled { color: @faint; }
QMenu::separator { height: 1px; background: @line; margin: 4px 8px; }

QStatusBar { background: @bg; color: @muted; border: 0; min-height: 26px; }
QStatusBar QLabel#geometryStatus { color: @accentInk; background: @accentSoft;
    border-radius: 6px; padding: 2px 8px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: @line; border-radius: 4px; min-height: 24px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: @line; border-radius: 4px; min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
)");
    const std::pair<const char*, QString> tokens[] = {{"@bg", hex(t.background)},
                                                      {"@panel", hex(t.panel)},
                                                      {"@ink", hex(t.ink)},
                                                      {"@muted", hex(t.muted)},
                                                      {"@faint", hex(t.faint)},
                                                      {"@line", hex(t.line)},
                                                      {"@soft", hex(t.soft)},
                                                      {"@field", hex(t.field)},
                                                      {"@accentHover", hex(t.accentHover)},
                                                      {"@accentSoft", hex(t.accentSoft)},
                                                      {"@accentInk", hex(t.accentInk)},
                                                      {"@accent", hex(t.accent)},
                                                      {"@onAccent", hex(t.onAccent)},
                                                      {"@errorSoft", hex(t.errorSoft)},
                                                      {"@error", hex(t.error)},
                                                      {"@ui", uiFontFamily()},
                                                      {"@value", valueFontFamily()}};
    // Longer names first so "@accent" does not clobber "@accentSoft".
    for (const auto& [name, value] : tokens)
        sheet.replace(QLatin1String(name), value);
    return sheet;
}

} // namespace voelund::desktop
