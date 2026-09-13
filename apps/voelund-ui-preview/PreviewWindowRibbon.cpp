#include "PreviewWindow.hpp"

#include "PreviewCanvas.hpp"
#include "PreviewIcons.hpp"

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QToolButton>

namespace voelund::preview {
namespace {

QToolButton* ribbonButton(const QString& text, Icon icon, const QString& tooltip = {})
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

} // namespace

QWidget* PreviewWindow::createSolidRibbon()
{
    auto* ribbon = new QWidget;
    auto* layout = new QHBoxLayout(ribbon);
    layout->setContentsMargins(14, 4, 10, 2);
    layout->setSpacing(4);
    auto* sketch = ribbonButton(QStringLiteral("Sketch"), Icon::Sketch,
                                QStringLiteral("Create sketch"));
    sketch->setObjectName(QStringLiteral("previewSketchAction"));
    connect(sketch, &QToolButton::clicked, this, &PreviewWindow::enterSketch);
    extrudeButton_ = ribbonButton(QStringLiteral("Extrude"), Icon::Extrude,
                                  QStringLiteral("Extrude selected profile"));
    extrudeButton_->setObjectName(QStringLiteral("previewExtrudeAction"));
    connect(extrudeButton_, &QToolButton::clicked, this, &PreviewWindow::activateExtrude);
    layout->addWidget(createRibbonGroup(QStringLiteral("CREATE"), {sketch, extrudeButton_,
        ribbonButton(QStringLiteral("Revolve"), Icon::Revolve),
        ribbonButton(QStringLiteral("Hole"), Icon::Hole)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("MODIFY"), {
        ribbonButton(QStringLiteral("Fillet"), Icon::Fillet),
        ribbonButton(QStringLiteral("Chamfer"), Icon::Chamfer),
        ribbonButton(QStringLiteral("Shell"), Icon::Shell)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("ASSEMBLE"), {
        ribbonButton(QStringLiteral("Joint"), Icon::Joint)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("SELECT"), {
        ribbonButton(QStringLiteral("Select"), Icon::Select)}));
    layout->addStretch();
    return ribbon;
}

QWidget* PreviewWindow::createSketchRibbon()
{
    auto* ribbon = new QWidget;
    auto* layout = new QHBoxLayout(ribbon);
    layout->setContentsMargins(14, 4, 10, 2);
    layout->setSpacing(4);
    const auto addSketchTool = [this](const QString& label, Icon icon,
                                      PreviewCanvas::SketchTool tool) {
        auto* button = ribbonButton(label, icon);
        button->setProperty("sketchTool", static_cast<int>(tool));
        sketchToolButtons_.append(button);
        connect(button, &QToolButton::clicked, this, [this, button] {
            activateSketchTool(button->property("sketchTool").toInt(), button);
        });
        return button;
    };
    const auto addToolMenu = [this](QToolButton* button, Icon icon,
                                    const QList<QPair<QString, PreviewCanvas::SketchTool>>& tools) {
        auto* menu = new QMenu(button);
        for (const auto& [label, tool] : tools) {
            auto* action = menu->addAction(makeIcon(icon), label);
            connect(action, &QAction::triggered, this, [this, button, label, tool] {
                button->setText(label);
                button->setProperty("sketchTool", static_cast<int>(tool));
                activateSketchTool(static_cast<int>(tool), button);
            });
        }
        button->setMenu(menu);
        button->setPopupMode(QToolButton::MenuButtonPopup);
    };
    auto* select = addSketchTool(QStringLiteral("Select"), Icon::Select,
                                 PreviewCanvas::SketchTool::Select);
    select->setProperty("active", true);
    auto* line = addSketchTool(QStringLiteral("Line"), Icon::Line,
                               PreviewCanvas::SketchTool::Line);
    addToolMenu(line, Icon::Line, {{QStringLiteral("Line"), PreviewCanvas::SketchTool::Line},
                                  {QStringLiteral("Polyline"), PreviewCanvas::SketchTool::Polyline}});
    auto* rectangle = addSketchTool(QStringLiteral("Rectangle"), Icon::Rectangle,
                                    PreviewCanvas::SketchTool::Rectangle);
    addToolMenu(rectangle, Icon::Rectangle, {
        {QStringLiteral("Rectangle"), PreviewCanvas::SketchTool::Rectangle},
        {QStringLiteral("Slot"), PreviewCanvas::SketchTool::Slot},
        {QStringLiteral("Polygon"), PreviewCanvas::SketchTool::Polygon}});
    auto* circle = addSketchTool(QStringLiteral("Circle"), Icon::Circle,
                                 PreviewCanvas::SketchTool::Circle);
    addToolMenu(circle, Icon::Circle, {{QStringLiteral("Circle"), PreviewCanvas::SketchTool::Circle},
                                      {QStringLiteral("Ellipse"), PreviewCanvas::SketchTool::Ellipse}});
    auto* arc = addSketchTool(QStringLiteral("Center Arc"), Icon::Arc,
                              PreviewCanvas::SketchTool::Arc);
    addToolMenu(arc, Icon::Arc, {{QStringLiteral("Center Arc"), PreviewCanvas::SketchTool::Arc},
                                {QStringLiteral("3-Point Arc"), PreviewCanvas::SketchTool::ThreePointArc}});
    layout->addWidget(createRibbonGroup(QStringLiteral("CREATE"),
                                        {select, line, rectangle, circle, arc}));
    layout->addWidget(createRibbonGroup(QStringLiteral("CONSTRAIN"), {
        addSketchTool(QStringLiteral("Relation"), Icon::Constraint,
                      PreviewCanvas::SketchTool::Constraint),
        addSketchTool(QStringLiteral("Dimension"), Icon::Dimension,
                      PreviewCanvas::SketchTool::Dimension)}));
    layout->addWidget(createRibbonGroup(QStringLiteral("MODIFY"), {
        addSketchTool(QStringLiteral("Trim"), Icon::Trim, PreviewCanvas::SketchTool::Trim),
        addSketchTool(QStringLiteral("Offset"), Icon::Offset, PreviewCanvas::SketchTool::Offset)}));
    layout->addStretch();
    auto* finish = ribbonButton(QStringLiteral("Finish Sketch"), Icon::Finish);
    finish->setObjectName(QStringLiteral("finishSketch"));
    finish->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    finish->setIcon(makeIcon(Icon::Finish, QColor("#E8FFF2")));
    connect(finish, &QToolButton::clicked, this, &PreviewWindow::finishSketch);
    layout->addWidget(finish);
    return ribbon;
}

} // namespace voelund::preview
