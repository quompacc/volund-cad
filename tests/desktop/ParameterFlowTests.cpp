// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// PAR02 (user flow of the packet; user decisions 2026-10-16): a rectangle on XY whose bottom
// side gets "=A" and left side "=B" in the length field, each created after the question; the
// region extruded with "=C". The header button "Parameter" opens the card under the model tree:
// three rows with one use each. A changed to 30 in the table changes the shown body and the
// volume; undo in the header returns. A renamed to "Breite" keeps its use. Removing C asks;
// Cancel keeps it, "Lösen und entfernen" leaves the extrusion with its value. Ctrl+P closes the
// card.
namespace {
using namespace voelund;
using test::press;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

struct Window final {
    application::CadSession session;
    std::unique_ptr<desktop::MainWindow> main;
    viewport::CadViewport* viewport{nullptr};
    Window() {
        desktop::ProductOptions options;
        options.offerRecovery = false;
        main = desktop::startProduct(session, options);
        main->resize(1300, 860);
        main->show();
        viewport = dynamic_cast<viewport::CadViewport*>(main->centralWidget());
        require(viewport != nullptr, "viewport");
        test::render(*viewport);
        viewport->setCameraView(viewport::CameraView::top);
        test::render(*viewport);
    }
    QWidget& child(const char* name) {
        auto* widget = main->findChild<QWidget*>(QLatin1String(name));
        require(widget != nullptr && widget->isVisible(), std::string("Visible widget ") + name);
        return *widget;
    }
    QToolButton& rail(const char* id) { return test::railButton(child("toolRail"), id); }
    QToolButton& context(const char* id) { return test::commandButton(child("contextBar"), id); }
    QToolButton& header(const char* id) { return test::commandButton(child("shellHeader"), id); }
    void ok() {
        auto* button =
            child("activeToolPanel").findChild<QPushButton*>(QStringLiteral("toolAcceptButton"));
        require(button != nullptr && button->isEnabled(), "OK enabled");
        press(*button);
    }
    void click(double x, double y, double z = 0) {
        test::render(*viewport);
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, z);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    QDoubleSpinBox& field(const char* id) {
        auto* input =
            child("activeToolPanel")
                .findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_") + QLatin1String(id));
        require(input != nullptr && input->isVisible(), std::string("Field ") + id);
        return *input;
    }
    // Types `text` into a field and finishes the input, as a user does.
    void enter(const char* id, const QString& text) {
        auto& input = field(id);
        input.setFocus();
        auto* edit = input.findChild<QLineEdit*>();
        require(edit != nullptr, "line edit");
        edit->selectAll();
        edit->setText(text);
        // Enter or leaving the field ends the input; the panel's Enter shortcut is not the
        // field's, so the finish is signalled as the other UI flows do.
        emit input.editingFinished();
        test::settleEvents();
    }
    [[nodiscard]] double volume() const {
        double total = 0;
        for (const auto& body : session.validBodies()) {
            GProp_GProps properties;
            BRepGProp::VolumeProperties(body.shape, properties);
            total += properties.Mass();
        }
        return total;
    }
    [[nodiscard]] double shown() {
        test::render(*viewport);
        return viewport::ViewportTestAccess::displayedVolume(*viewport);
    }
};

// Answers the next message box called `name` with the button labelled `button`.
void answer(const QString& name, const QString& button, int* asked) {
    auto attempt = std::make_shared<std::function<void()>>();
    *attempt = [name, button, asked, attempt] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box == nullptr || box->objectName() != name) {
            QTimer::singleShot(10, *attempt);
            return;
        }
        ++*asked;
        if (button == QStringLiteral("Yes")) {
            box->button(QMessageBox::Yes)->click();
            return;
        }
        for (auto* candidate : box->buttons())
            if (candidate->text().remove(QLatin1Char('&')) == button)
                candidate->click();
    };
    QTimer::singleShot(0, *attempt);
}

bool closeTo(double value, double expected) {
    return std::abs(value - expected) < 1e-6 * std::max(1.0, std::abs(expected));
}

int run() {
    Window w;
    test::activate(*w.main);
    int asked = 0;

    // The rectangle 20 x 10 with its sides bound to new parameters A and B.
    w.click(30, 30);
    press(w.context("solid.create.sketch"));
    press(w.rail("sketch.tool.rectangle"));
    w.click(0, 0);
    w.click(20, 10);
    press(w.rail("sketch.tool.select"));
    w.click(10, 0); // bottom side
    answer(QStringLiteral("createParameterQuestion"), QStringLiteral("Yes"), &asked);
    w.enter("line_length", QStringLiteral("=A"));
    require(asked == 1 && w.field("line_length").text().startsWith(QStringLiteral("A = 20")),
            "=A asks, creates A = 20 and shows it: " + w.field("line_length").text().toStdString());
    w.click(0, 5); // left side
    answer(QStringLiteral("createParameterQuestion"), QStringLiteral("Yes"), &asked);
    w.enter("line_length", QStringLiteral("=B"));
    require(asked == 2 && w.field("line_length").text().startsWith(QStringLiteral("B = 10")),
            "=B creates B = 10");
    w.ok();
    require(w.session.parameterTable().values.size() == 2, "A and B in the document");

    // Extruded with "=C" at 8.
    w.click(10, 5);
    press(w.context("solid.create.extrude_profiles"));
    w.field("distance").setValue(8);
    answer(QStringLiteral("createParameterQuestion"), QStringLiteral("Yes"), &asked);
    w.enter("distance", QStringLiteral("=C"));
    require(asked == 3 && w.field("distance").text().startsWith(QStringLiteral("C = 8")),
            "=C creates C = 8");
    w.ok();
    require(closeTo(w.volume(), 1600), "20 x 10 x 8");

    // The card under the model tree.
    press(w.header("view.parameters"));
    auto* table = w.main->findChild<QTableWidget*>(QStringLiteral("parameterTable"));
    auto& card = w.child("parameterCard");
    auto& tree = w.child("modelCard");
    require(table != nullptr && table->isVisible() && table->rowCount() == 3 &&
                card.geometry().top() > tree.geometry().bottom() &&
                card.geometry().left() == tree.geometry().left(),
            "The card shows three parameters under the model tree");
    for (int row = 0; row < 3; ++row)
        require(table->item(row, 2)->text() == QStringLiteral("1×"), "One use each");

    // A to 30 in the table: body and volume follow; undo returns.
    table->item(0, 1)->setText(QStringLiteral("30"));
    test::settleEvents();
    require(closeTo(w.volume(), 2400) && closeTo(w.shown(), 2400), "A = 30 gives 30 x 10 x 8");
    press(w.header("history.undo"));
    require(closeTo(w.volume(), 1600) && table->item(0, 1)->text().startsWith(QStringLiteral("20")),
            "Undo returns to A = 20");

    // Renamed, still in use.
    table->item(0, 0)->setText(QStringLiteral("Breite"));
    test::settleEvents();
    require(table->item(0, 0)->text() == QStringLiteral("Breite") &&
                table->item(0, 2)->text() == QStringLiteral("1×"),
            "Renamed, the use stays");

    // Removing C asks; Cancel keeps it, unbinding keeps the extrusion's value.
    table->setCurrentCell(2, 0);
    answer(QStringLiteral("deleteParameterQuestion"), QStringLiteral("Abbrechen"), &asked);
    press(*w.main->findChild<QToolButton*>(QStringLiteral("parameterRemove")));
    require(asked == 4 && w.session.parameterTable().values.size() == 3, "Cancel keeps C");
    table->setCurrentCell(2, 0);
    answer(QStringLiteral("deleteParameterQuestion"), QStringLiteral("Lösen und entfernen"),
           &asked);
    press(*w.main->findChild<QToolButton*>(QStringLiteral("parameterRemove")));
    require(asked == 5 && w.session.parameterTable().values.size() == 2 &&
                closeTo(w.volume(), 1600) && table->rowCount() == 2,
            "C removed, the extrusion keeps 8");

    // Ctrl+P is the shortcut; the header button closes the card.
    for (auto* action : w.main->actions())
        if (action->property("commandId").toString() == QStringLiteral("view.parameters"))
            require(action->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_P), "Ctrl+P");
    press(w.header("view.parameters"));
    require(!card.isVisible(), "The header button closes the card again");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS PAR02 parameter flow: =A, =B in the sketch and =C in the extrusion "
                     "with the creation question, card under the tree with uses, A changed in "
                     "the table (view and volume), undo, rename, delete protection, Ctrl+P"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
