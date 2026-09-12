#include "PreviewStyle.hpp"

namespace voelund::preview {

QString applicationStyle()
{
    return QStringLiteral(R"QSS(
* { font-family: "Segoe UI", "Inter", sans-serif; font-size: 12px; color: #D9E1EA; }
QMainWindow, QWidget#root { background: #171B20; }
QWidget#topBar { background: #12161A; border-bottom: 1px solid #30363D; }
QLabel#brand { color: #F2F6FA; font-size: 15px; font-weight: 700; letter-spacing: 2px; }
QLabel#documentTitle { color: #9AA7B5; }
QPushButton#workspaceSelector { background: #242A31; border: 1px solid #38414A; border-radius: 5px; padding: 6px 12px; font-weight: 600; }
QPushButton#workspaceSelector:hover { background: #2C343D; border-color: #4B5865; }
QToolButton#topIcon { background: transparent; border: 0; border-radius: 5px; padding: 6px; }
QToolButton#topIcon:hover { background: #2B323A; }
QWidget#workspaceTabs { background: #1C2127; border-bottom: 1px solid #343B43; }
QPushButton.workspaceTab { background: transparent; color: #919DAA; border: 0; border-bottom: 2px solid transparent; padding: 8px 15px 7px; font-weight: 600; }
QPushButton.workspaceTab:hover { color: #E7EDF4; background: #222931; }
QPushButton.workspaceTab[active="true"] { color: #F2F7FB; border-bottom-color: #35B8FF; }
QWidget#ribbon { background: #21272E; border-bottom: 1px solid #39414A; }
QFrame.ribbonGroup { border: 0; border-right: 1px solid #3A424B; background: transparent; }
QLabel.groupTitle { color: #778492; font-size: 10px; font-weight: 600; }
QToolButton.command { min-width: 54px; background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 5px 5px 3px; color: #D6DEE7; }
QToolButton.command:hover { background: #303840; border-color: #44505C; }
QToolButton.command:pressed, QToolButton.command[active="true"] { background: #153B52; border-color: #278DC3; color: white; }
QToolButton#finishSketch { background: #20784D; border: 1px solid #32A96F; border-radius: 6px; padding: 8px 14px; font-weight: 600; }
QToolButton#finishSketch:hover { background: #298B5B; }
QWidget#workArea { background: #161A1F; }
QFrame#browserPanel, QFrame#inspectorPanel { background: #1C2127; border: 0; }
QFrame#browserPanel { border-right: 1px solid #343B43; }
QFrame#inspectorPanel { border-left: 1px solid #343B43; }
QLabel.panelTitle { color: #F0F4F8; font-size: 13px; font-weight: 650; }
QLabel.panelCaption { color: #7F8C99; font-size: 10px; font-weight: 600; }
QTreeWidget { background: transparent; border: 0; outline: 0; padding: 4px; }
QTreeWidget::item { height: 28px; border-radius: 4px; }
QTreeWidget::item:hover { background: #282F37; }
QTreeWidget::item:selected { background: #173D54; color: #E9F7FF; }
QTreeWidget::branch { background: transparent; }
QFrame.card { background: #232A31; border: 1px solid #353F49; border-radius: 7px; }
QLabel.cardTitle { color: #F1F5F9; font-size: 13px; font-weight: 650; }
QLabel.muted { color: #8996A3; }
QLabel.valueStrong { color: #EAF4FA; font-weight: 600; }
QLineEdit, QComboBox, QDoubleSpinBox { background: #161B20; border: 1px solid #3B4651; border-radius: 4px; padding: 6px 8px; selection-background-color: #167CB2; }
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus { border-color: #35B8FF; }
QComboBox::drop-down { border: 0; width: 22px; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 15px; height: 15px; border-radius: 3px; border: 1px solid #53616E; background: #151A1F; }
QCheckBox::indicator:checked { background: #299EDB; border-color: #4CC1FF; }
QPushButton.primary { background: #208BC4; border: 1px solid #45B8F2; border-radius: 5px; padding: 7px 12px; font-weight: 600; }
QPushButton.primary:hover { background: #29A1DD; }
QPushButton.secondary { background: #2A323A; border: 1px solid #414C57; border-radius: 5px; padding: 7px 12px; }
QPushButton.secondary:hover { background: #343E48; }
QWidget#timeline { background: #191E23; border-top: 1px solid #343C44; }
QLabel#timelineTitle { color: #7D8995; font-size: 10px; font-weight: 600; }
QToolButton.timelineFeature { background: #262D34; border: 1px solid #3A4650; border-radius: 5px; padding: 4px 9px; }
QToolButton.timelineFeature:hover { background: #303A43; border-color: #526170; }
QToolButton.timelineFeature[active="true"] { background: #173E55; border-color: #2C9DD3; }
QWidget#statusBar { background: #111519; border-top: 1px solid #292F35; }
QLabel#statusText { color: #7E8994; font-size: 10px; }
QSplitter::handle { background: #343B43; width: 1px; }
QScrollArea { background: transparent; border: 0; }
QScrollBar:vertical { background: #1B2026; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: #434D57; border-radius: 4px; min-height: 24px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QToolTip { background: #101418; color: #EAF0F5; border: 1px solid #46515C; padding: 5px; }
)QSS");
}

} // namespace voelund::preview
