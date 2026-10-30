// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>
#include <iostream>
#include <stdexcept>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// W19 (user report 2026-10-25, after SolidCraft's palette and FreeCAD's lists): the sketch
// card shows how much is determined, names loose ends and ties corners lying on each other,
// and lists relations and elements; a click in a list selects in the sketch.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
void settle() {
    for (int i = 0; i < 6; ++i)
        QApplication::processEvents();
}
void click(viewport::CadViewport& viewport, double x, double y) {
    const QPoint point = viewport::ViewportTestAccess::project(viewport, x, y, 0);
    test::mouse(viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
    test::mouse(viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
    test::render(viewport);
    settle();
}
template <class Widget> Widget& part(QWidget& window, const char* name) {
    auto* found = window.findChild<Widget*>(QLatin1String(name));
    require(found != nullptr, std::string("Overview part missing: ") + name);
    return *found;
}
QString status(QWidget& window) { return window.findChild<QStatusBar*>()->currentMessage(); }

void lists(QWidget& window) {
    // VP03: the lists wait behind "Details und Ansicht", as do the view switches.
    part<QToolButton>(window, "sketchDetailsToggle").setChecked(true);
    settle();
    auto& relations = part<QToolButton>(window, "sketchRelationsToggle");
    auto& elements = part<QToolButton>(window, "sketchElementsToggle");
    require(relations.text().startsWith(QStringLiteral("Beziehungen (")) &&
                elements.text() == QStringLiteral("Elemente (4)"),
            "The lists name their counts: " + relations.text().toStdString() + ", " +
                elements.text().toStdString());
    relations.setChecked(true);
    auto& relationList = part<QListWidget>(window, "sketchRelationList");
    require(relationList.isVisible() && relationList.count() >= 4, "The relations unfold");
    emit relationList.itemClicked(relationList.item(0));
    settle();
    require(status(window).startsWith(QStringLiteral("Bedingung gewählt")),
            "A relation in the list is selected in the sketch, got " +
                status(window).toStdString());
    elements.setChecked(true);
    auto& elementList = part<QListWidget>(window, "sketchElementList");
    require(elementList.item(0)->text().startsWith(QStringLiteral("Linie")),
            "The rectangle's sides are named as lines");
    emit elementList.itemClicked(elementList.item(0));
    settle();
    require(elementList.item(0)->isSelected(), "The element in the list is selected");
}

// Two lines drawn without automatic relations meet at (40, 0) by position only.
void looseEnds(QWidget& window, viewport::CadViewport& viewport) {
    part<QComboBox>(window, "toolChoice_relations").setCurrentIndex(1); // Keine
    for (const double start : {20.0, 40.0}) {
        command(window, "sketch.tool.line").trigger();
        settle();
        click(viewport, start, 0);
        click(viewport, start + 20, 0);
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QApplication::sendEvent(&viewport, &escape);
        settle();
    }
    auto& ends = part<QLabel>(window, "sketchLooseEnds");
    auto& tie = part<QPushButton>(window, "sketchTieButton");
    require(ends.isVisible() && ends.text().contains(QStringLiteral("offene")) &&
                ends.text().contains(QStringLiteral("aufeinandergelegt")) && tie.isVisible(),
            "Loose ends are named and can be tied: " + ends.text().toStdString());
    tie.click();
    settle();
    require(!tie.isVisible() && !ends.text().contains(QStringLiteral("aufeinandergelegt")),
            "Tied, no corner is left lying loose: " + ends.text().toStdString());
}

// W20: a right click ends a drawing tool; in selection it opens the sketch's menu with the
// relations that fit the selection.
void rightClick(viewport::CadViewport& viewport, QPoint at) {
    test::mouse(viewport, QEvent::MouseButtonPress, at, Qt::RightButton, Qt::RightButton);
    test::mouse(viewport, QEvent::MouseButtonRelease, at, Qt::RightButton, Qt::NoButton);
    settle();
}
void contextMenu(QWidget& window, viewport::CadViewport& viewport) {
    command(window, "sketch.tool.line").trigger();
    settle();
    const QPoint empty = viewport::ViewportTestAccess::project(viewport, 60, 40, 0);
    rightClick(viewport, empty);
    require(command(window, "sketch.tool.select").isChecked(),
            "A right click ends the drawing tool");
    click(viewport, -30, 0); // the rectangle's left side
    QTimer::singleShot(200, [] {
        if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
            menu->close();
    });
    rightClick(viewport, empty);
    const QStringList offered = viewport.property("sketchMenuEntries").toStringList();
    require(offered.contains(QStringLiteral("Skizze beenden")) &&
                offered.contains(QStringLiteral("Linie\tL")),
            "The menu offers tools and finishing: " + offered.join(',').toStdString());
    require(offered.contains(QStringLiteral("Horizontal")) ||
                offered.contains(QStringLiteral("Vertikal")),
            "A selected line is offered its direction");
}

// W20 (as SolidCraft's circle family): a circle through three clicked points and one across
// two clicked ends of a diameter.
void circleVariants(QWidget& window, application::CadSession& session,
                    viewport::CadViewport& viewport) {
    auto& elements = part<QToolButton>(window, "sketchElementsToggle");
    const QString before = elements.text();
    command(window, "sketch.tool.circle_three_point").trigger();
    settle();
    click(viewport, 60, 50);
    click(viewport, 50, 60);
    click(viewport, 40, 50);
    command(window, "sketch.tool.circle_two_point").trigger();
    settle();
    click(viewport, 80, -40);
    click(viewport, 100, -40);
    command(window, "sketch.tool.select").trigger();
    settle();
    require(elements.text() != before, "Two circles were added: " + elements.text().toStdString());
    auto& list = part<QListWidget>(window, "sketchElementList");
    require(list.count() >= 2 && list.item(list.count() - 1)->text().startsWith("Kreis") &&
                list.item(list.count() - 2)->text().startsWith("Kreis"),
            "Both are circles");
    static_cast<void>(session);
}

// W20 (as SolidCraft): without a selection X switches the drawing mode; a line drawn then is
// construction geometry, shown by the card's line type row.
void constructionMode(QWidget& window, viewport::CadViewport& viewport) {
    click(viewport, 90, 70); // empty space: nothing selected
    command(window, "sketch.construction").trigger();
    settle();
    require(part<QPushButton>(window, "sketchLineTypeConstruction").isChecked(),
            "X without a selection switches to construction drawing");
    command(window, "sketch.tool.line").trigger();
    settle();
    click(viewport, 70, -60);
    click(viewport, 90, -60);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &escape);
    settle();
    command(window, "sketch.tool.select").trigger();
    settle();
    auto& list = part<QListWidget>(window, "sketchElementList");
    require(list.item(list.count() - 1)->text().endsWith(QStringLiteral("(Hilfsgeometrie)")),
            "The new line is construction: " + list.item(list.count() - 1)->text().toStdString());
    click(viewport, 90, 70); // nothing selected: the button sets the drawing mode
    part<QPushButton>(window, "sketchLineTypeConstruction").click();
    settle();
    require(!part<QPushButton>(window, "sketchLineTypeConstruction").isChecked(),
            "Normal switches back");
}

// W20 (as SolidCraft): a rectangle at an angle from three clicks, kept square by relations.
void threePointRectangle(QWidget& window, viewport::CadViewport& viewport) {
    auto& elements = part<QListWidget>(window, "sketchElementList");
    const int before = elements.count();
    command(window, "sketch.tool.three_point_rectangle").trigger();
    settle();
    click(viewport, 100, 0);
    click(viewport, 120, 12);
    click(viewport, 112, 26);
    command(window, "sketch.tool.select").trigger();
    settle();
    require(elements.count() == before + 4,
            "Four sides: " + std::to_string(elements.count() - before));
    auto& relations = part<QListWidget>(window, "sketchRelationList");
    bool perpendicular = false, parallel = false;
    for (int row = 0; row < relations.count(); ++row) {
        perpendicular = perpendicular || relations.item(row)->text() == QStringLiteral("Senkrecht");
        parallel = parallel || relations.item(row)->text() == QStringLiteral("Parallel");
    }
    require(perpendicular && parallel, "Kept square by perpendicular and parallel relations");
}

// W20 (as Fusion): an arc that goes on from the end of a line without a kink.
void tangentArc(QWidget& window, viewport::CadViewport& viewport) {
    command(window, "sketch.tool.line").trigger();
    settle();
    click(viewport, 20, 80);
    click(viewport, 40, 80);
    QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(&viewport, &escape);
    settle();
    command(window, "sketch.tool.tangent_arc").trigger();
    settle();
    click(viewport, 40, 80); // the line's end
    click(viewport, 50, 90);
    command(window, "sketch.tool.select").trigger();
    settle();
    auto& elements = part<QListWidget>(window, "sketchElementList");
    require(elements.item(elements.count() - 1)->text().startsWith(QStringLiteral("Bogen")),
            "An arc was added: " + elements.item(elements.count() - 1)->text().toStdString());
    auto& relations = part<QListWidget>(window, "sketchRelationList");
    bool tangent = false;
    for (int row = 0; row < relations.count(); ++row)
        tangent = tangent || relations.item(row)->text() == QStringLiteral("Tangential");
    require(tangent, "The arc is tangent to the line");
}

void run() {
    application::CadSession session;
    session.newDocument();
    desktop::MainWindow window(session);
    window.resize(1500, 950);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    viewport->setCameraView(viewport::CameraView::top);
    test::render(*viewport);
    click(*viewport, 30, 30);
    command(window, "solid.create.sketch").trigger();
    settle();
    command(window, "sketch.tool.rectangle").trigger();
    click(*viewport, -30, -15);
    click(*viewport, 10, 15);
    command(window, "sketch.tool.select").trigger();
    settle();
    lists(window);
    contextMenu(window, *viewport);
    circleVariants(window, session, *viewport);
    constructionMode(window, *viewport);
    threePointRectangle(window, *viewport);
    tangentArc(window, *viewport);
    looseEnds(window, *viewport);
    std::cout << "PASS sketch overview: lists, selection, loose ends, tie\n";
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
