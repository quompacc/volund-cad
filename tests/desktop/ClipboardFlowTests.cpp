#include "../../src/desktop/ModelTreePanel.hpp"
#include "IsolatedSettings.hpp"
#include "UiDriver.hpp"
#include "ViewportLifecycleSupport.hpp"

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QStatusBar>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <iostream>
#include <memory>
#include <string>
#include <voelund/application/CadSession.hpp>
#include <voelund/desktop/MainWindow.hpp>
#include <voelund/desktop/ProductShell.hpp>

// N02 (user decision 2026-10-21): a box from the rail, its body row clicked in the model tree,
// Strg+C and Strg+V: a second body, the fixed copy, appears. After File > New the
// clipboard still holds the body and Strg+V pastes it into the new document; undo in the header
// takes the paste back.
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
        viewport = dynamic_cast<viewport::CadViewport*>(main->centralWidget());
        require(viewport != nullptr, "viewport");
        test::render(*viewport);
    }
    QWidget& child(const char* name) {
        auto* widget = main->findChild<QWidget*>(QLatin1String(name));
        require(widget != nullptr && widget->isVisible(), std::string("Visible widget ") + name);
        return *widget;
    }
    void key(int code, Qt::KeyboardModifiers modifiers) {
        test::activate(*main);
        QKeyEvent press(QEvent::KeyPress, code, modifiers);
        QApplication::sendEvent(viewport, &press);
        test::settleEvents();
        test::render(*viewport);
    }
    // A left click on the first body row of the model tree.
    void clickBody() {
        auto* tree = main->findChild<QTreeWidget*>(QStringLiteral("modelTree"));
        require(tree != nullptr, "model tree");
        using Panel = desktop::ModelTreePanel;
        for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it)
            if ((*it)->data(0, Panel::kindRole).toInt() == static_cast<int>(Panel::Kind::body)) {
                const QPoint at = tree->visualItemRect(*it).center();
                const QPointF global = tree->viewport()->mapToGlobal(QPointF(at));
                for (const auto type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
                    QMouseEvent event(type, QPointF(at), global, Qt::LeftButton,
                                      type == QEvent::MouseButtonPress ? Qt::LeftButton
                                                                       : Qt::NoButton,
                                      Qt::NoModifier);
                    QApplication::sendEvent(tree->viewport(), &event);
                }
                test::settleEvents();
                return;
            }
        throw std::runtime_error("No body row in the model tree");
    }
    [[nodiscard]] QString status() const { return main->statusBar()->currentMessage(); }
};

int run() {
    test::DialogAnswers user;
    user.answer = QMessageBox::Discard;
    Window w;
    test::activate(*w.main); // keys reach only the active window
    QGuiApplication::clipboard()->clear();
    test::useRailTool(w.child("toolRail"), "solid.create.box");
    require(w.session.validBodies().size() == 1, "Box from the rail");
    w.clickBody();
    require(w.status().contains(QStringLiteral("gewählt")),
            "Body row selected: " + w.status().toStdString());

    w.key(Qt::Key_C, Qt::ControlModifier);
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    require(data != nullptr && data->hasFormat(QStringLiteral("application/x-voelund-bodies")),
            "Strg+C puts the body on the clipboard");
    w.key(Qt::Key_V, Qt::ControlModifier);
    require(w.session.validBodies().size() == 2 &&
                w.status() == QStringLiteral("Als feste Kopie eingefügt"),
            "Strg+V pastes a fixed copy: " + w.status().toStdString());

    test::chooseFromMenu(*w.main->findChild<QToolButton*>(QStringLiteral("fileMenuButton")),
                         "document.new");
    require(w.session.features().empty(), "New document");
    w.key(Qt::Key_V, Qt::ControlModifier);
    require(w.session.validBodies().size() == 1, "Pasted into the new document");
    press(test::commandButton(w.child("shellHeader"), "history.undo"));
    require(w.session.validBodies().empty(), "Undo takes the paste back");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    voelund::test::isolateSettings();
    try {
        const int result = run();
        std::cout << "PASS N02 copy and paste through the shell: Strg+C/Strg+V, fixed copy, "
                     "pasted into a new document, undo"
                  << std::endl;
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
