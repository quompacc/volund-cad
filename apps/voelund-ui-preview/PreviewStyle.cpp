#include "PreviewStyle.hpp"

namespace voelund::preview {

QString applicationStyle()
{
    return QStringLiteral(R"QSS(
* { font-family: "Segoe UI", "Inter", sans-serif; font-size: 12px; color: #DEDACC; }
QMainWindow, QWidget#root { background: #0D120E; }
QWidget#topBar { background: #090D0A; border-bottom: 1px solid #293128; }
QLabel#brand { color: #F0ECE0; font-family: "Georgia", serif; font-size: 16px; font-weight: 600; letter-spacing: 3px; }
QLabel#brandTagline { color: #A88A38; font-size: 7px; letter-spacing: 2px; }
QLabel#documentTitle { color: #969C90; }
QPushButton#workspaceSelector { background: #1A2019; border: 1px solid #3C3A27; border-radius: 4px; padding: 6px 12px; color: #D6AD3B; font-weight: 600; }
QPushButton#workspaceSelector:hover { background: #24291F; border-color: #6E6032; }
QToolButton#topIcon { background: transparent; border: 0; border-radius: 5px; padding: 6px; }
QToolButton#topIcon:hover { background: #20261F; }
QWidget#workspaceTabs { background: #101510; border-bottom: 1px solid #2C342B; }
QPushButton.workspaceTab { background: transparent; color: #92988D; border: 0; border-bottom: 2px solid transparent; padding: 8px 15px 7px; font-weight: 600; }
QPushButton.workspaceTab:hover { color: #E8E3D7; background: #181E18; }
QPushButton.workspaceTab[active="true"] { color: #F0EBDD; border-bottom-color: #D3A52D; }
QWidget#ribbon { background: #171D17; border-bottom: 1px solid #333B31; }
QFrame.ribbonGroup { border: 0; border-right: 1px solid #363E34; background: transparent; }
QLabel.groupTitle { color: #7E867A; font-size: 10px; font-weight: 600; }
QToolButton.command { min-width: 54px; background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 5px 5px 3px; color: #DAD6CA; }
QToolButton.command:hover { background: #252C24; border-color: #464E43; }
QToolButton.command:pressed, QToolButton.command[active="true"] { background: #38331D; border-color: #B58D26; color: #FFF7DB; }
QToolButton#finishSketch { background: #3D5E35; border: 1px solid #78915A; border-radius: 5px; padding: 8px 14px; font-weight: 600; }
QToolButton#finishSketch:hover { background: #4C7041; }
QWidget#workArea { background: #0E130F; }
QWidget#cameraRail { background: #151B16; border-left: 1px solid #343D33; border-bottom: 1px solid #343D33; }
QFrame#browserPanel, QFrame#inspectorPanel { background: #111711; border: 0; }
QFrame#browserPanel { border-right: 1px solid #30372F; }
QFrame#inspectorPanel { border-left: 1px solid #4A4328; }
QLabel.panelTitle { color: #EEE9DD; font-size: 13px; font-weight: 650; }
QLabel.panelCaption { color: #858C80; font-size: 10px; font-weight: 600; }
QTreeWidget { background: transparent; border: 0; outline: 0; padding: 4px; }
QTreeWidget::item { height: 28px; border-radius: 4px; }
QTreeWidget::item:hover { background: #20271F; }
QTreeWidget::item:selected { background: #39331D; color: #FFF4CF; }
QTreeWidget::branch { background: transparent; }
QFrame.card { background: #192019; border: 1px solid #343D33; border-radius: 6px; }
QLabel.cardTitle { color: #F0EBDE; font-size: 13px; font-weight: 650; }
QLabel.muted { color: #8E9589; }
QLabel.valueStrong { color: #EEE9DD; font-weight: 600; }
QLineEdit, QComboBox, QDoubleSpinBox { background: #0D120E; border: 1px solid #3A4438; border-radius: 3px; padding: 6px 8px; selection-background-color: #8E6D18; }
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus { border-color: #D3A52D; }
QComboBox::drop-down { border: 0; width: 22px; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator { width: 15px; height: 15px; border-radius: 3px; border: 1px solid #505A4D; background: #0C110D; }
QCheckBox::indicator:checked { background: #B88E24; border-color: #E0B43D; }
QPushButton.primary { background: #C89520; color: #14170F; border: 1px solid #E0B43D; border-radius: 4px; padding: 7px 12px; font-weight: 700; }
QPushButton.primary:hover { background: #DBAA31; }
QPushButton.secondary { background: #20271F; border: 1px solid #3B4539; border-radius: 4px; padding: 7px 12px; }
QPushButton.secondary:hover { background: #2A3228; }
QWidget#timeline { background: #101510; border-top: 1px solid #30372F; }
QLabel#timelineTitle { color: #838A7E; font-size: 10px; font-weight: 600; }
QToolButton.timelineFeature { background: #1B221B; border: 1px solid #374036; border-radius: 4px; padding: 4px 9px; }
QToolButton.timelineFeature:hover { background: #272F25; border-color: #5D6043; }
QToolButton.timelineFeature[active="true"] { background: #39331D; border-color: #B88F25; }
QWidget#statusBar { background: #090D0A; border-top: 1px solid #282F27; }
QLabel#statusText { color: #81887D; font-size: 10px; }
QSplitter::handle { background: #30372F; width: 1px; }
QScrollArea { background: transparent; border: 0; }
QScrollBar:vertical { background: #111611; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: #3B4439; border-radius: 4px; min-height: 24px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QToolTip { background: #0A0F0B; color: #EAE5D9; border: 1px solid #5A5230; padding: 5px; }
)QSS");
}

} // namespace voelund::preview
