#include "PreviewWindow.hpp"

#include "PreviewCanvas.hpp"
#include "PreviewIcons.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <array>

namespace voelund::preview {
namespace {

constexpr auto margin = 12;

QToolButton* toolButton(const QString& text, Icon icon, const QString& tooltip = {})
{
    auto* button = new QToolButton;
    button->setProperty("class", QStringLiteral("command"));
    button->setText(text);
    button->setIcon(makeIcon(icon));
    button->setIconSize(QSize(25, 25));
    button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    button->setAutoRaise(false);
    button->setToolTip(tooltip.isEmpty() ? text : tooltip);
    return button;
}

QLabel* caption(const QString& text)
{
    auto* label = new QLabel(text);
    label->setProperty("class", QStringLiteral("panelCaption"));
    return label;
}

QFrame* separator()
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("background:#37414A; max-height:1px; border:0;"));
    return line;
}

} // namespace

PreviewWindow::PreviewWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("uiPreviewWindow"));
    setWindowTitle(QStringLiteral("Völund — Design Preview"));
    resize(1500, 920);
    setMinimumSize(1120, 720);

    auto* root = new QWidget;
    root->setObjectName(QStringLiteral("root"));
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(createTopBar());
    rootLayout->addWidget(createWorkspaceTabs());

    ribbonStack_ = new QStackedWidget;
    ribbonStack_->setObjectName(QStringLiteral("ribbon"));
    ribbonStack_->setFixedHeight(94);
    ribbonStack_->addWidget(createSolidRibbon());
    ribbonStack_->addWidget(createSketchRibbon());
    rootLayout->addWidget(ribbonStack_);

    auto* workArea = new QWidget;
    workArea->setObjectName(QStringLiteral("workArea"));
    auto* workLayout = new QHBoxLayout(workArea);
    workLayout->setContentsMargins(0, 0, 0, 0);
    workLayout->setSpacing(0);
    workLayout->addWidget(createBrowser());

    auto* canvasLayer = new QWidget;
    auto* overlayLayout = new QGridLayout(canvasLayer);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    canvas_ = new PreviewCanvas;
    overlayLayout->addWidget(canvas_, 0, 0);

    inspectorStack_ = new QStackedWidget;
    inspectorStack_->setObjectName(QStringLiteral("inspectorPanel"));
    inspectorStack_->setFixedWidth(288);
    inspectorStack_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    inspectorStack_->addWidget(createSolidInspector());
    inspectorStack_->addWidget(createSketchInspector());
    overlayLayout->addWidget(inspectorStack_, 0, 0, Qt::AlignTop | Qt::AlignRight);
    overlayLayout->setContentsMargins(0, 0, 14, 0);
    workLayout->addWidget(canvasLayer, 1);
    rootLayout->addWidget(workArea, 1);
    rootLayout->addWidget(createTimeline());
    rootLayout->addWidget(createStatusBar());
    setCentralWidget(root);

    connect(canvas_, &PreviewCanvas::selectionChanged, this, [this](const QString& selection) {
        selectionText_->setText(selection);
        statusText_->setText(QStringLiteral("Selected: %1").arg(selection));
    });
    setWorkspaceTab(QStringLiteral("SOLID"));
}

bool PreviewWindow::smokeTestPassed() const
{
    return isVisible() && canvas_ != nullptr && ribbonStack_ != nullptr && inspectorStack_ != nullptr
        && browserTree_ != nullptr && ribbonStack_->count() == 2 && inspectorStack_->count() == 2;
}

QWidget* PreviewWindow::createTopBar()
{
    auto* bar = new QWidget;
    bar->setObjectName(QStringLiteral("topBar"));
    bar->setFixedHeight(46);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(14, 0, 12, 0);
    layout->setSpacing(9);

    auto* mark = new QToolButton;
    mark->setObjectName(QStringLiteral("topIcon"));
    mark->setIcon(makeIcon(Icon::Logo, QColor("#44C5FF")));
    mark->setIconSize(QSize(26, 26));
    layout->addWidget(mark);
    auto* brand = new QLabel(QStringLiteral("VÖLUND"));
    brand->setObjectName(QStringLiteral("brand"));
    layout->addWidget(brand);

    auto* workspace = new QPushButton(QStringLiteral("DESIGN   ⌄"));
    workspace->setObjectName(QStringLiteral("workspaceSelector"));
    layout->addSpacing(14);
    layout->addWidget(workspace);
    layout->addStretch();

    auto* title = new QLabel(QStringLiteral("●  Bracket Assembly  /  v12"));
    title->setObjectName(QStringLiteral("documentTitle"));
    layout->addWidget(title);
    layout->addStretch();

    for (const auto& entry : std::array{
             std::pair{Icon::Search, QStringLiteral("Search")},
             std::pair{Icon::Help, QStringLiteral("Help")},
             std::pair{Icon::Settings, QStringLiteral("Settings")}}) {
        auto* button = new QToolButton;
        button->setObjectName(QStringLiteral("topIcon"));
        button->setIcon(makeIcon(entry.first));
        button->setIconSize(QSize(19, 19));
        button->setToolTip(entry.second);
        layout->addWidget(button);
    }
    auto* avatar = new QLabel(QStringLiteral("EV"));
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(29, 29);
    avatar->setStyleSheet(QStringLiteral("background:#6754C8;color:white;border-radius:14px;font-weight:700;"));
    layout->addWidget(avatar);
    return bar;
}

QWidget* PreviewWindow::createWorkspaceTabs()
{
    auto* tabs = new QWidget;
    tabs->setObjectName(QStringLiteral("workspaceTabs"));
    tabs->setFixedHeight(39);
    auto* layout = new QHBoxLayout(tabs);
    layout->setContentsMargins(136, 0, 0, 0);
    layout->setSpacing(0);
    for (const QString& name : {QStringLiteral("SOLID"), QStringLiteral("SURFACE"), QStringLiteral("MESH"),
                                QStringLiteral("SHEET METAL"), QStringLiteral("ASSEMBLY"), QStringLiteral("TOOLS")}) {
        auto* tab = new QPushButton(name);
        tab->setProperty("class", QStringLiteral("workspaceTab"));
        connect(tab, &QPushButton::clicked, this, [this, name] { setWorkspaceTab(name); });
        workspaceTabs_.append(tab);
        layout->addWidget(tab);
    }
    layout->addStretch();
    return tabs;
}

QWidget* PreviewWindow::createSolidRibbon()
{
    auto* ribbon = new QWidget;
    auto* layout = new QHBoxLayout(ribbon);
    layout->setContentsMargins(14, 4, 10, 2);
    layout->setSpacing(4);

    auto* sketch = toolButton(QStringLiteral("Sketch"), Icon::Sketch, QStringLiteral("Create sketch"));
    connect(sketch, &QToolButton::clicked, this, &PreviewWindow::enterSketch);
    auto* extrude = toolButton(QStringLiteral("Extrude"), Icon::Extrude, QStringLiteral("Extrude selected profile"));
    extrude->setProperty("active", true);
    connect(extrude, &QToolButton::clicked, this, &PreviewWindow::activateExtrude);
    layout->addWidget(createRibbonGroup(QStringLiteral("CREATE"), {sketch, extrude,
        toolButton(QStringLiteral("Revolve"), Icon::Revolve), toolButton(QStringLiteral("Hole"), Icon::Hole)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("MODIFY"), {
        toolButton(QStringLiteral("Fillet"), Icon::Fillet), toolButton(QStringLiteral("Chamfer"), Icon::Chamfer),
        toolButton(QStringLiteral("Shell"), Icon::Shell)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("ASSEMBLE"), {toolButton(QStringLiteral("Joint"), Icon::Joint)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("SELECT"), {toolButton(QStringLiteral("Select"), Icon::Select)}));
    layout->addStretch();
    return ribbon;
}

QWidget* PreviewWindow::createSketchRibbon()
{
    auto* ribbon = new QWidget;
    auto* layout = new QHBoxLayout(ribbon);
    layout->setContentsMargins(14, 4, 10, 2);
    layout->setSpacing(4);
    layout->addWidget(createRibbonGroup(QStringLiteral("CREATE"), {
        toolButton(QStringLiteral("Line"), Icon::Line), toolButton(QStringLiteral("Rectangle"), Icon::Rectangle),
        toolButton(QStringLiteral("Circle"), Icon::Circle), toolButton(QStringLiteral("Arc"), Icon::Arc)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("CONSTRAIN"), {
        toolButton(QStringLiteral("Relation"), Icon::Constraint), toolButton(QStringLiteral("Dimension"), Icon::Dimension)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("MODIFY"), {
        toolButton(QStringLiteral("Trim"), Icon::Trim), toolButton(QStringLiteral("Offset"), Icon::Offset)}));
    layout->addStretch();
    auto* finish = toolButton(QStringLiteral("Finish Sketch"), Icon::Finish);
    finish->setObjectName(QStringLiteral("finishSketch"));
    finish->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    finish->setIcon(makeIcon(Icon::Finish, QColor("#E8FFF2")));
    connect(finish, &QToolButton::clicked, this, &PreviewWindow::finishSketch);
    layout->addWidget(finish);
    return ribbon;
}

QWidget* PreviewWindow::createBrowser()
{
    auto* panel = new QFrame;
    panel->setObjectName(QStringLiteral("browserPanel"));
    panel->setFixedWidth(235);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(10, 11, 8, 8);
    layout->setSpacing(8);
    auto* title = new QLabel(QStringLiteral("BROWSER")); title->setProperty("class", QStringLiteral("panelTitle"));
    layout->addWidget(title);
    auto* filter = new QLineEdit; filter->setPlaceholderText(QStringLiteral("Filter components")); filter->setClearButtonEnabled(true);
    layout->addWidget(filter);
    browserTree_ = new QTreeWidget;
    browserTree_->setObjectName(QStringLiteral("modelBrowser"));
    browserTree_->setHeaderHidden(true);
    auto* root = new QTreeWidgetItem(browserTree_, {QStringLiteral("◉  Bracket Assembly")});
    auto* settings = new QTreeWidgetItem(root, {QStringLiteral("⚙  Document Settings")});
    auto* origin = new QTreeWidgetItem(root, {QStringLiteral("◇  Origin")});
    auto* body = new QTreeWidgetItem(root, {QStringLiteral("▣  Body")});
    new QTreeWidgetItem(body, {QStringLiteral("◇  Sketch 01")});
    auto* pad = new QTreeWidgetItem(body, {QStringLiteral("▦  Pad 01")});
    new QTreeWidgetItem(body, {QStringLiteral("◇  Sketch 02")});
    new QTreeWidgetItem(body, {QStringLiteral("▦  Fillet 01")});
    root->setExpanded(true); body->setExpanded(true); pad->setSelected(true);
    settings->setForeground(0, QColor("#9AA6B2")); origin->setForeground(0, QColor("#9AA6B2"));
    layout->addWidget(browserTree_, 1);
    auto* comments = new QPushButton(QStringLiteral("○  Comments")); comments->setProperty("class", QStringLiteral("secondary"));
    layout->addWidget(comments);
    return panel;
}

QWidget* PreviewWindow::createSolidInspector()
{
    auto* page = new QFrame;
    page->setObjectName(QStringLiteral("inspectorPanel"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(9);
    auto* header = new QLabel(QStringLiteral("EXTRUDE")); header->setProperty("class", QStringLiteral("panelTitle")); layout->addWidget(header);
    layout->addWidget(separator());

    selectionText_ = new QLabel(QStringLiteral("Top face / Profile 01")); selectionText_->setProperty("class", QStringLiteral("valueStrong"));
    auto* distance = new QDoubleSpinBox; distance->setRange(-10000, 10000); distance->setValue(24); distance->setSuffix(QStringLiteral(" mm"));
    auto* direction = new QComboBox; direction->addItems({QStringLiteral("One side"), QStringLiteral("Symmetric"), QStringLiteral("Two sides")});
    auto* operation = new QComboBox; operation->addItems({QStringLiteral("Join"), QStringLiteral("New body"), QStringLiteral("Cut"), QStringLiteral("Intersect")});
    layout->addWidget(createField(QStringLiteral("PROFILE"), selectionText_));
    layout->addWidget(createField(QStringLiteral("DISTANCE"), distance));
    layout->addWidget(createField(QStringLiteral("DIRECTION"), direction));
    layout->addWidget(createField(QStringLiteral("OPERATION"), operation));
    auto* taper = new QCheckBox(QStringLiteral("Taper angle")); layout->addWidget(taper);
    auto* buttons = new QWidget; auto* buttonsLayout = new QHBoxLayout(buttons); buttonsLayout->setContentsMargins(0, 6, 0, 0);
    auto* cancel = new QPushButton(QStringLiteral("Cancel")); cancel->setProperty("class", QStringLiteral("secondary"));
    auto* okay = new QPushButton(QStringLiteral("OK")); okay->setProperty("class", QStringLiteral("primary"));
    buttonsLayout->addWidget(cancel); buttonsLayout->addWidget(okay); layout->addWidget(buttons);
    return page;
}

QWidget* PreviewWindow::createSketchInspector()
{
    auto* page = new QFrame;
    page->setObjectName(QStringLiteral("inspectorPanel"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(8);
    auto* header = new QLabel(QStringLiteral("SKETCH PALETTE")); header->setProperty("class", QStringLiteral("panelTitle")); layout->addWidget(header);
    layout->addWidget(separator());
    auto* plane = new QLabel(QStringLiteral("Top face")); plane->setProperty("class", QStringLiteral("valueStrong"));
    layout->addWidget(createField(QStringLiteral("SKETCH PLANE"), plane));
    auto* autoConstraints = new QCheckBox(QStringLiteral("Auto constraints")); autoConstraints->setChecked(true);
    auto* grid = new QCheckBox(QStringLiteral("Sketch grid")); grid->setChecked(true);
    auto* dimensions = new QCheckBox(QStringLiteral("Show dimensions")); dimensions->setChecked(true);
    auto* projected = new QCheckBox(QStringLiteral("Projected geometry")); projected->setChecked(true);
    layout->addWidget(autoConstraints); layout->addWidget(grid); layout->addWidget(dimensions); layout->addWidget(projected);
    layout->addWidget(separator());
    auto* state = new QLabel(QStringLiteral("●  Under-constrained  ·  4 DOF"));
    state->setStyleSheet(QStringLiteral("color:#F1B54D;font-weight:600;")); layout->addWidget(state);
    auto* finish = new QPushButton(QStringLiteral("Finish Sketch")); finish->setProperty("class", QStringLiteral("primary"));
    connect(finish, &QPushButton::clicked, this, &PreviewWindow::finishSketch); layout->addWidget(finish);
    return page;
}

QWidget* PreviewWindow::createTimeline()
{
    auto* timeline = new QWidget;
    timeline->setObjectName(QStringLiteral("timeline"));
    timeline->setFixedHeight(58);
    auto* layout = new QHBoxLayout(timeline);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(7);
    auto* title = new QLabel(QStringLiteral("HISTORY")); title->setObjectName(QStringLiteral("timelineTitle")); layout->addWidget(title);
    for (const auto& entry : std::array{
             std::pair{QStringLiteral("01  Sketch"), Icon::Sketch}, std::pair{QStringLiteral("02  Pad"), Icon::Extrude},
             std::pair{QStringLiteral("03  Sketch"), Icon::Sketch}, std::pair{QStringLiteral("04  Fillet"), Icon::Fillet}}) {
        auto* feature = new QToolButton; feature->setProperty("class", QStringLiteral("timelineFeature"));
        feature->setText(entry.first); feature->setIcon(makeIcon(entry.second)); feature->setIconSize(QSize(18, 18));
        feature->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); layout->addWidget(feature);
    }
    layout->addStretch();
    auto* marker = new QLabel(QStringLiteral("◀  END")); marker->setProperty("class", QStringLiteral("muted")); layout->addWidget(marker);
    return timeline;
}

QWidget* PreviewWindow::createStatusBar()
{
    auto* bar = new QWidget;
    bar->setObjectName(QStringLiteral("statusBar"));
    bar->setFixedHeight(24);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 0, 10, 0);
    statusText_ = new QLabel(QStringLiteral("Ready  ·  Top face selected")); statusText_->setObjectName(QStringLiteral("statusText"));
    layout->addWidget(statusText_); layout->addStretch();
    auto* units = new QLabel(QStringLiteral("mm  ·  Grid 10 mm  ·  Perspective")); units->setObjectName(QStringLiteral("statusText")); layout->addWidget(units);
    return bar;
}

QWidget* PreviewWindow::createRibbonGroup(const QString& title, const QList<QWidget*>& commands)
{
    auto* group = new QFrame;
    group->setProperty("class", QStringLiteral("ribbonGroup"));
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(5, 1, 10, 0);
    layout->setSpacing(0);
    auto* row = new QWidget; auto* rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(0, 0, 0, 0); rowLayout->setSpacing(2);
    for (auto* command : commands) rowLayout->addWidget(command);
    layout->addWidget(row, 1);
    auto* label = new QLabel(title); label->setProperty("class", QStringLiteral("groupTitle")); label->setAlignment(Qt::AlignCenter); layout->addWidget(label);
    return group;
}

QWidget* PreviewWindow::createCommand(const QString& text, int icon, const QString& tooltip)
{
    return toolButton(text, static_cast<Icon>(icon), tooltip);
}

QWidget* PreviewWindow::createField(const QString& label, QWidget* editor)
{
    auto* field = new QWidget;
    auto* layout = new QVBoxLayout(field); layout->setContentsMargins(0, 2, 0, 2); layout->setSpacing(4);
    layout->addWidget(caption(label)); layout->addWidget(editor);
    return field;
}

QWidget* PreviewWindow::createCard(const QString& title, const QList<QWidget*>& rows)
{
    auto* card = new QFrame; card->setProperty("class", QStringLiteral("card"));
    auto* layout = new QVBoxLayout(card); layout->setContentsMargins(10, 10, 10, 10);
    auto* header = new QLabel(title); header->setProperty("class", QStringLiteral("cardTitle")); layout->addWidget(header);
    for (auto* row : rows) layout->addWidget(row);
    return card;
}

void PreviewWindow::enterSketch()
{
    ribbonStack_->setCurrentIndex(1);
    inspectorStack_->setCurrentIndex(1);
    canvas_->setMode(PreviewCanvas::Mode::Sketch);
    setWorkspaceTab(QStringLiteral("SOLID"));
    statusText_->setText(QStringLiteral("Editing Sketch 01  ·  Top face"));
}

void PreviewWindow::finishSketch()
{
    ribbonStack_->setCurrentIndex(0);
    inspectorStack_->setCurrentIndex(0);
    canvas_->setMode(PreviewCanvas::Mode::Solid);
    statusText_->setText(QStringLiteral("Sketch 01 finished  ·  Ready"));
}

void PreviewWindow::activateExtrude()
{
    finishSketch();
    inspectorStack_->show();
    statusText_->setText(QStringLiteral("Extrude  ·  Select a profile or face"));
}

void PreviewWindow::setWorkspaceTab(const QString& activeName)
{
    for (auto* tab : workspaceTabs_) {
        tab->setProperty("active", tab->text() == activeName);
        refreshStyle(tab);
    }
}

void PreviewWindow::refreshStyle(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace voelund::preview
