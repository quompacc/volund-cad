// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSettings>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// N06 (decision 2026-10-23): the four display styles from the commands beside the view cube,
// kept per machine and never in the document; a body row shows the colour card, a swatch
// colours the body in the view (one undo step), "Standard" returns to the theme colour.
namespace {
using namespace voelund;
using test::require;
using viewport::DisplayStyle;

QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
template <typename Widget> Widget& child(QWidget& window, const char* name) {
    auto* found = window.findChild<Widget*>(QLatin1String(name));
    if (found == nullptr)
        throw std::runtime_error(std::string("Widget is missing: ") + name);
    return *found;
}
void capture(QWidget& window, const char* name) {
    const QString directory = qEnvironmentVariable("VOELUND_N06_ARTIFACT_DIR");
    if (directory.isEmpty())
        return;
    require(QDir().mkpath(directory), "capture directory");
    test::settleEvents();
    require(window.grab().save(directory + QLatin1Char('/') + QLatin1String(name)), "capture");
}

// 1. Each style reaches the view and the settings; the document stays untouched.
void styles(QWidget& window, viewport::CadViewport& viewport,
            const application::CadSession& session) {
    const bool modified = session.isModified();
    const bool canUndo = session.canUndo();
    require(viewport.displayStyle() == DisplayStyle::shadedWithEdges &&
                child<QToolButton>(window, "displayStyleButton").isVisible(),
            "Shaded with edges by default; the style button beside the view cube");
    const std::pair<const char*, DisplayStyle> steps[] = {
        {"view.style.wireframe", DisplayStyle::wireframe},
        {"view.style.hidden_edges", DisplayStyle::hiddenEdges},
        {"view.style.shaded", DisplayStyle::shaded},
        {"view.style.shaded_edges", DisplayStyle::shadedWithEdges}};
    for (const auto& [id, style] : steps) {
        command(window, id).trigger();
        test::render(viewport);
        require(viewport.displayStyle() == style && command(window, id).isChecked(),
                (std::string("Style ") + id).c_str());
        capture(window, (std::string("n06-") + id + ".png").c_str());
    }
    require(QSettings().value(QStringLiteral("view/displayStyle")).toString() ==
                QStringLiteral("shadedEdges"),
            "The style is kept in the settings");
    require(session.isModified() == modified && session.canUndo() == canUndo,
            "Styles leave the document untouched");
}

// 2. The body row shows the card; a swatch colours the body, Standard and Undo go back.
void colors(QWidget& window, viewport::CadViewport& viewport, application::CadSession& session) {
    auto& tree = child<QTreeWidget>(window, "modelTree");
    QTreeWidgetItem* bodyRow = nullptr;
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->data(0, Qt::UserRole + 2).toInt() == 3)
            bodyRow = *it;
    require(bodyRow != nullptr, "Body row");
    tree.setCurrentItem(bodyRow); // a click on the row makes it current, which selects the body
    test::settleEvents();
    auto& card = child<QWidget>(window, "bodyCard");
    require(card.isVisible(), "A body row shows the colour card");
    const auto bodyColor = [&] { return session.modelOutline().bodies.front().color; };
    child<QToolButton>(window, "bodyColor1").click();
    test::settleEvents();
    test::render(viewport);
    require(bodyColor() == std::optional<std::uint32_t>{0xC0392B} &&
                viewport::ViewportTestAccess::bodyColor(viewport, 0) == bodyColor() &&
                session.isModified() && session.canUndo(),
            "The red swatch colours the body in the document and the view");
    require(card.isVisible() && child<QToolButton>(window, "bodyColor1").isChecked(),
            "The card stays and marks the chosen colour");
    capture(window, "n06-body-red.png");
    child<QToolButton>(window, "bodyColorDefault").click();
    test::settleEvents();
    require(!bodyColor() && !viewport::ViewportTestAccess::bodyColor(viewport, 0),
            "Standard returns to the theme colour");
    command(window, "history.undo").trigger();
    test::settleEvents();
    require(bodyColor() == std::optional<std::uint32_t>{0xC0392B}, "Undo brings the red back");
}

// N07: the body card gives the 40 x 20 x 10 plate steel: 62.8 g, centroid (20, 10, 5); a
// density of its own weighs it again; the choice is one undo step.
void materials(QWidget& window, application::CadSession& session) {
    auto& tree = child<QTreeWidget>(window, "modelTree");
    for (QTreeWidgetItemIterator it(&tree); *it != nullptr; ++it)
        if ((*it)->data(0, Qt::UserRole + 2).toInt() == 3)
            tree.setCurrentItem(*it);
    test::settleEvents();
    auto& material = child<QComboBox>(window, "bodyMaterial");
    auto& properties = child<QLabel>(window, "bodyMassProperties");
    require(material.isVisible() && properties.text().contains(QStringLiteral("ohne Material")) &&
                properties.text().contains(QStringLiteral("Volumen: 8.000,000 mm³")),
            "Without material: volume, no mass");
    material.setCurrentIndex(1);
    emit material.activated(1); // Stahl
    test::settleEvents();
    const auto given = session.modelOutline().bodies.front().material;
    require(given && given->key == "steel" && session.canUndo(), "Steel in the document");
    require(properties.text().contains(QStringLiteral("Masse: 62,800 g")) &&
                properties.text().contains(QStringLiteral("Schwerpunkt: 20,000; 10,000; 5,000")),
            "Steel plate: 62.8 g at its centre");
    const int custom = material.count() - 1;
    material.setCurrentIndex(custom);
    emit material.activated(custom);
    auto& density = child<QDoubleSpinBox>(window, "bodyDensity");
    density.setValue(2.0);
    emit density.editingFinished();
    test::settleEvents();
    require(properties.text().contains(QStringLiteral("Masse: 16,000 g")),
            "Own density 2 g/cm³: 16 g");
    capture(window, "n07-body-material.png");
}

void run() {
    application::CadSession session;
    desktop::MainWindow window(session);
    window.resize(1300, 860);
    window.show();
    auto* viewport = dynamic_cast<viewport::CadViewport*>(window.centralWidget());
    require(viewport != nullptr, "viewport");
    test::render(*viewport);
    session.newDocument();
    require(session.createBoxFeature({40, 20, 10}).feature().has_value(), "Box");
    window.refreshAfterExternalChange();
    viewport->setCameraView(viewport::CameraView::isometric);
    test::render(*viewport);
    styles(window, *viewport, session);
    colors(window, *viewport, session);
    materials(window, session);
    std::cout << "PASS N06 display styles and body colour, N07 material and mass through the shell"
              << std::endl;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
