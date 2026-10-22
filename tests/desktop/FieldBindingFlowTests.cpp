// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include "IsolatedSettings.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>

// N01b (user decision 2026-10-21): a box 20 x 20 x 10. "Lineares Muster" with "=N" (N = 3) in
// the count and "=Abstand" in the spacing, created after the question with the field's 10: three
// cubes, two bindings. N = 5 in the parameter table makes five. The pattern reopened from the
// timeline shows "N = 5"; 4 typed into it unbinds it and stays. "=B" in the box's length field of
// the properties creates B = 20 and binds at once; B = 30 widens every cube.
namespace {
using namespace voelund;
void require(bool condition, const std::string& message) {
    test::require(condition, message.c_str());
}

double volume(const application::CadSession& session) {
    double total = 0;
    for (const auto& body : session.validBodies()) {
        GProp_GProps properties;
        BRepGProp::VolumeProperties(body.shape, properties);
        total += properties.Mass();
    }
    return total;
}
bool matches(double a, double b) { return std::abs(a - b) < 1e-6 * std::max(1.0, std::abs(b)); }
void settle() {
    for (int i = 0; i < 3; ++i)
        QApplication::processEvents();
}
QAction& command(QWidget& window, const char* id) {
    for (auto* action : window.findChildren<QAction*>())
        if (action->property("commandId").toString() == QLatin1String(id))
            return *action;
    throw std::runtime_error(std::string("Command is missing: ") + id);
}
QDoubleSpinBox& visibleField(QWidget& window, const char* name) {
    for (auto* candidate : window.findChildren<QDoubleSpinBox*>(QLatin1String(name)))
        if (candidate->isVisible())
            return *candidate;
    throw std::runtime_error(std::string("Field missing: ") + name);
}
// Types `text` into a field and ends the input, as a user does.
void enter(QDoubleSpinBox& field, const QString& text) {
    field.setFocus();
    auto* edit = field.findChild<QLineEdit*>();
    edit->selectAll();
    edit->setText(text);
    if (!text.startsWith(QLatin1Char('=')))
        field.interpretText(); // as Enter does: a number becomes the value
    emit field.editingFinished();
    settle();
}
void accept(QWidget& window) {
    auto* panel = window.findChild<QWidget*>(QStringLiteral("activeToolPanel"));
    panel->findChild<QPushButton*>(QStringLiteral("toolAcceptButton"))->click();
    settle();
}
// Answers the next question called `name` with Yes and counts it.
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
document::ParameterId named(const application::CadSession& session, const std::string& name) {
    for (const auto& entry : session.parameterTable().values)
        if (entry.name == name)
            return entry.id;
    throw std::runtime_error("Parameter " + name);
}

void patternBound(desktop::MainWindow& window, application::CadSession& session,
                  document::FeatureId box, int& asked) {
    require(session.createParameter("N", document::ParameterUnit::number, 3).status() ==
                application::CommandStatus::committed,
            "N = 3");
    require(session.selectFeature(box), "Box selected");
    window.refreshAfterExternalChange();
    command(window, "solid.modify.pattern_linear").trigger();
    settle();
    enter(visibleField(window, "toolLength_count"), QStringLiteral("=N"));
    require(visibleField(window, "toolLength_count").text().startsWith(QStringLiteral("N = 3")),
            "The count shows N = 3: " +
                visibleField(window, "toolLength_count").text().toStdString());
    visibleField(window, "toolLength_spacing").setValue(10);
    settle();
    answerYes(QStringLiteral("createParameterQuestion"), &asked);
    enter(visibleField(window, "toolLength_spacing"), QStringLiteral("=Abstand"));
    require(asked == 1, "=Abstand asks first");
    accept(window);
    require(session.validBodies().size() == 3 && session.parameterTable().bindings.size() == 2 &&
                matches(volume(session), 3 * 4000),
            "Three cubes, count and spacing bound");
}

void tableAndReopen(desktop::MainWindow& window, application::CadSession& session) {
    command(window, "view.parameters").trigger();
    settle();
    auto* table = window.findChild<QTableWidget*>(QStringLiteral("parameterTable"));
    for (int row = 0; row < table->rowCount(); ++row)
        if (table->item(row, 0)->text() == QStringLiteral("N"))
            table->item(row, 1)->setText(QStringLiteral("5"));
    settle();
    require(session.validBodies().size() == 5 && matches(volume(session), 5 * 4000),
            "N = 5 in the table makes five cubes");
    command(window, "view.parameters").trigger();
    settle();

    int seen = 0;
    for (auto* step : window.findChildren<QToolButton*>(QStringLiteral("timelineStep")))
        if (step->isVisible() && seen++ == 1)
            step->click();
    settle();
    auto& count = visibleField(window, "toolLength_count");
    require(count.text().startsWith(QStringLiteral("N = 5")),
            "Reopened, the count shows N = 5: " + count.text().toStdString());
    enter(count, QStringLiteral("4"));
    accept(window);
    require(session.validBodies().size() == 4 && session.parameterTable().bindings.size() == 1,
            "4 typed: unbound, four cubes: " + std::to_string(session.validBodies().size()) +
                " bodies, " + std::to_string(session.parameterTable().bindings.size()) +
                " bindings, panel " + std::to_string(count.isVisible()));
}

void propertiesBound(desktop::MainWindow& window, application::CadSession& session,
                     document::FeatureId box, int& asked) {
    require(session.selectFeature(box), "Box selected again");
    window.refreshAfterExternalChange();
    settle();
    answerYes(QStringLiteral("createParameterQuestion"), &asked);
    enter(visibleField(window, "boxLengthInput"), QStringLiteral("=B"));
    require(asked == 2 && session.parameterTable().bindings.size() == 2,
            "=B in the box's length asks and binds at once");
    require(session.setParameterValue(named(session, "B"), 30).status() ==
                    application::CommandStatus::committed &&
                matches(volume(session), 4 * 6000),
            "B = 30 widens every cube: " + std::to_string(volume(session)));
    window.refreshAfterExternalChange();
    settle();
    require(visibleField(window, "boxLengthInput").text().startsWith(QStringLiteral("B = 30")),
            "The length field shows B = 30: " +
                visibleField(window, "boxLengthInput").text().toStdString());
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
    const auto box = *session.createBoxFeature({20, 20, 10}).feature();
    window.refreshAfterExternalChange();
    int asked = 0;
    patternBound(window, session, box, asked);
    tableAndReopen(window, session);
    propertiesBound(window, session, box, asked);
}
} // namespace

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        run();
        std::cout << "PASS N01b field bindings through the shell: count and spacing of a new "
                     "pattern, the table drives the count, a typed number unbinds, a box length "
                     "bound in the properties"
                  << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
