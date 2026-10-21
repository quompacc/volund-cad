#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QApplication>
#include <QDoubleSpinBox>
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

// N01 (user decisions 2026-10-21): a rectangle 20 x 10 extruded with "=10*2" in the distance
// field: the question offers "Ausdruck1", the depth is 20. In the parameter card the row shows
// "=10*2 → 20,000 mm"; "=A*3" typed into it with A = 5 gives 15; "=Q*2" is refused with the
// reason and the depth stays.
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
    QToolButton& context(const char* id) { return test::commandButton(child("contextBar"), id); }
    QToolButton& header(const char* id) { return test::commandButton(child("shellHeader"), id); }
    void click(double x, double y) {
        test::render(*viewport);
        const QPoint point = viewport::ViewportTestAccess::project(*viewport, x, y, 0);
        test::mouse(*viewport, QEvent::MouseMove, point, Qt::NoButton, Qt::NoButton);
        test::mouse(*viewport, QEvent::MouseButtonPress, point, Qt::LeftButton, Qt::LeftButton);
        test::mouse(*viewport, QEvent::MouseButtonRelease, point, Qt::LeftButton, Qt::NoButton);
        test::render(*viewport);
        test::settleEvents();
    }
    void enter(const char* id, const QString& text) {
        auto* input =
            child("activeToolPanel")
                .findChild<QDoubleSpinBox*>(QStringLiteral("toolLength_") + QLatin1String(id));
        require(input != nullptr && input->isVisible(), std::string("Field ") + id);
        input->setFocus();
        auto* edit = input->findChild<QLineEdit*>();
        edit->selectAll();
        edit->setText(text);
        emit input->editingFinished();
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
};

// Answers the next message box called `name` with Yes and counts it.
void answerYes(const QString& name, int* asked) {
    auto attempt = std::make_shared<std::function<void()>>();
    *attempt = [name, asked, attempt] {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (box == nullptr || box->objectName() != name) {
            QTimer::singleShot(10, *attempt);
            return;
        }
        ++*asked;
        box->button(QMessageBox::Yes)->click();
    };
    QTimer::singleShot(0, *attempt);
}

bool closeTo(double value, double expected) {
    return std::abs(value - expected) < 1e-6 * std::max(1.0, std::abs(expected));
}

QTableWidgetItem* valueCell(QTableWidget& table, const QString& name) {
    for (int row = 0; row < table.rowCount(); ++row)
        if (table.item(row, 0)->text() == name)
            return table.item(row, 1);
    throw std::runtime_error("Row missing: " + name.toStdString());
}

int run() {
    Window w;
    test::activate(*w.main);
    int asked = 0;
    w.click(30, 30);
    press(w.context("solid.create.sketch"));
    press(test::railButton(w.child("toolRail"), "sketch.tool.rectangle"));
    w.click(0, 0);
    w.click(20, 10);
    press(*w.child("activeToolPanel").findChild<QPushButton*>(QStringLiteral("toolAcceptButton")));
    w.click(10, 5);
    press(w.context("solid.create.extrude_profiles"));
    answerYes(QStringLiteral("createParameterQuestion"), &asked);
    w.enter("distance", QStringLiteral("=10*2"));
    press(*w.child("activeToolPanel").findChild<QPushButton*>(QStringLiteral("toolAcceptButton")));
    require(asked == 1 && closeTo(w.volume(), 20 * 10 * 20),
            "=10*2 creates Ausdruck1 and extrudes 20: " + std::to_string(w.volume()));

    press(w.header("view.parameters"));
    auto* table = w.main->findChild<QTableWidget*>(QStringLiteral("parameterTable"));
    require(table != nullptr && valueCell(*table, QStringLiteral("Ausdruck1"))
                                    ->text()
                                    .startsWith(QStringLiteral("=10*2 → 20")),
            "The row shows the expression and its value");

    require(w.session.createParameter("A", document::ParameterUnit::length, 5).status() ==
                application::CommandStatus::committed,
            "A = 5");
    w.main->refreshAfterExternalChange();
    valueCell(*table, QStringLiteral("Ausdruck1"))->setText(QStringLiteral("=A*3"));
    test::settleEvents();
    require(closeTo(w.volume(), 20 * 10 * 15), "=A*3 in the table: depth 15");

    valueCell(*table, QStringLiteral("Ausdruck1"))->setText(QStringLiteral("=Q*2"));
    test::settleEvents();
    require(closeTo(w.volume(), 20 * 10 * 15) &&
                w.main->statusBar()->currentMessage().contains(QStringLiteral("Q")),
            "=Q*2 is refused with the reason: " +
                w.main->statusBar()->currentMessage().toStdString());
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS N01 expressions through the shell: =10*2 in a field creates "
                     "Ausdruck1, the table shows and edits it, an unknown name is refused"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
